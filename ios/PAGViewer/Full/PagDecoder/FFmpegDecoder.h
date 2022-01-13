#ifndef PAG_FFMPEGDECODER_H
#define PAG_FFMPEGDECODER_H

#include "SoftwareDecoder.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

namespace pag {
    
    class FFmpegDecoderFactory : public SoftwareDecoderFactory {
    public:
        /**
         * Create a software decoder
         */
        std::unique_ptr<SoftwareDecoder> createSoftwareDecoder() override;
    };

    class FFmpegDecoder : public SoftwareDecoder {
    public:
        ~FFmpegDecoder() override;
        
        bool onConfigure(const std::vector<CodecHeader>& headers, std::string mime, int width, int height) override;

        SoftwareDecodingResult onSendBytes(void* bytes, size_t length, int64_t frame) override;

        SoftwareDecodingResult onDecodeFrame() override;

        SoftwareDecodingResult onEndOfStream() override;

        void onFlush() override;

        std::unique_ptr<OutputFrame> onRenderFrame() override;

    private:
        const AVCodec* codec = nullptr;
        AVCodecContext* context = nullptr;
        AVFrame* frame = nullptr;
        AVPacket* packet = nullptr;
        std::vector<CodecHeader> headers;

        bool openDecoder();
        void closeDecoder();
        bool initFFmpeg();
        void sendHeaders();
    };

}

#endif //PAG_FFMPEGDECODER_H
