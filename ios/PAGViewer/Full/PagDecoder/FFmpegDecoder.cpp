#include "FFmpegDecoder.h"

namespace pag {

    std::unique_ptr<SoftwareDecoder> FFmpegDecoderFactory::createSoftwareDecoder() {
        av_log_set_level(AV_LOG_FATAL);
        return std::unique_ptr<SoftwareDecoder>(new FFmpegDecoder());
    }

    FFmpegDecoder::~FFmpegDecoder() {
        closeDecoder();

        if (frame != nullptr) {
            av_frame_free(&frame);
        }
        if (packet != nullptr) {
            av_packet_free(&packet);
        }
    }
    
    bool FFmpegDecoder::onConfigure(const std::vector<CodecHeader>& headers, std::string mime, int width, int height) {
        this->headers = headers;
        auto isValid = initFFmpeg();
        if(isValid){
            sendHeaders();
        }
        return isValid;
    }

    SoftwareDecodingResult FFmpegDecoder::onSendBytes(void* bytes, size_t length, int64_t frame) {
        if (context == nullptr) {
            return SoftwareDecodingResult::Error;
        }
        packet->data = static_cast<uint8_t*>(bytes);
        packet->size = static_cast<int>(length);
        auto result = avcodec_send_packet(context, packet);
        if (result >= 0 || result == AVERROR_EOF) {
            return SoftwareDecodingResult::Success;
        } else if (result == AVERROR(EAGAIN)) {
            return SoftwareDecodingResult::TryAgainLater;
        } else {
            return SoftwareDecodingResult::Error;
        }
    }

    SoftwareDecodingResult FFmpegDecoder::onDecodeFrame() {
        auto result = avcodec_receive_frame(context, frame);
        if (result >= 0 && frame->data[0] != nullptr) {
            return SoftwareDecodingResult::Success;
        } else if (result == AVERROR(EAGAIN)) {
            return SoftwareDecodingResult::TryAgainLater;
        } else {
            return SoftwareDecodingResult::Error;
        }
    }

    SoftwareDecodingResult FFmpegDecoder::onEndOfStream() {
        return onSendBytes(nullptr, 0, -1);
    }

    void FFmpegDecoder::onFlush() {
        closeDecoder();
        if (openDecoder()) {
            sendHeaders();
        }
    }

    std::unique_ptr<OutputFrame> FFmpegDecoder:: onRenderFrame() {
        auto outputFrame = new OutputFrame();
        for(int i=0; i<3; i++){
            outputFrame->data[i] = frame->data[i];
            outputFrame->lineSize[i] = frame->linesize[i];
        }
        return std::unique_ptr<OutputFrame>(outputFrame);
    }
    
    void FFmpegDecoder::sendHeaders() {
        for (auto& head : headers) {
            onSendBytes(head.data, head.length, 0);
        }
    }
    
    bool FFmpegDecoder::initFFmpeg() {
        packet = av_packet_alloc();
        if (!packet) {
            return false;
        }
        codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        if (!codec) {
            return false;
        }
        if (!openDecoder()) {
            return false;
        }
        frame = av_frame_alloc();
        return frame != nullptr;
    }
    
    bool FFmpegDecoder::openDecoder() {
        context = avcodec_alloc_context3(codec);
        if (!context) {
            return false;
        }
        return avcodec_open2(context, codec, nullptr) >= 0;
    }
    
    void FFmpegDecoder::closeDecoder() {
        if (context != nullptr) {
            avcodec_free_context(&context);
            context = nullptr;
        }
    }

}



