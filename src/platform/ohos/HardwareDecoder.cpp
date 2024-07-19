//
// Created on 2024/7/9.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "HardwareDecoder.h"


#include <multimedia/player_framework/native_avcapability.h>
#include <multimedia/player_framework/native_avcodec_base.h>
#include <multimedia/player_framework/native_avformat.h>
#include <multimedia/player_framework/native_avbuffer.h>
#include "hilog/log.h"
#include <mutex>

#include "base/utils/Log.h"

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x3200  // 全局domain宏，标识业务领域
#define LOG_TAG "PAG"   // 全局tag宏，标识模块日志tag

namespace pag {

void OH_AVCodecOnError(OH_AVCodec *codec, int32_t errorCode, void *userData) {
    (void)codec;
    (void)errorCode;
    (void)userData;
}

void OH_AVCodecOnStreamChanged(OH_AVCodec *codec, OH_AVFormat *format, void *userData) {
    (void)codec;
    (void)format;
    (void)userData;
}

void OH_AVCodecOnNeedInputBuffer(OH_AVCodec *codec, uint32_t index, OH_AVBuffer *buffer, void *userData) {
    if (userData == nullptr) {
        return;
    }
    (void)codec;
    OH_LOG_INFO(LOG_APP, "------OH_AVCodecOnNeedInputBuffer---------");
    CodecUserData *codecUserData = static_cast<CodecUserData*>(userData);
    std::unique_lock<std::mutex> lock(codecUserData->inputMutex);
    codecUserData->inputBufferInfoQueue.emplace(index, buffer);
    codecUserData->inputCondition.notify_all();
}

void OH_AVCodecOnNewOutputBuffer(OH_AVCodec *codec, uint32_t index, OH_AVBuffer *buffer, void *userData) {
    if (userData == nullptr) {
        return;
    }
    (void)codec;
    (void)index;
    OH_LOG_INFO(LOG_APP, "------OH_AVCodecOnNewOutputBuffer---------");
    CodecUserData *codecUserData = static_cast<CodecUserData*>(userData);
    std::unique_lock<std::mutex> lock(codecUserData->outputMutex);
    codecUserData->outputBufferInfoQueue.emplace(index, buffer);
    codecUserData->outputCondition.notify_all();
}



HardwareDecoder::HardwareDecoder(const VideoFormat& format) {
    isValid = initDecoder(format);
    
}

HardwareDecoder::~HardwareDecoder()
{
    if (videoDec != nullptr) {
        OH_VideoDecoder_Flush(videoDec);
        OH_VideoDecoder_Stop(videoDec);
        OH_VideoDecoder_Destroy(videoDec);
        videoDec = nullptr;
    }
    if (codecUserData) {
        delete codecUserData;
    }
}


bool HardwareDecoder::initDecoder(const VideoFormat& format) {
    videoDec = OH_VideoDecoder_CreateByMime(format.mimeType.c_str());
    if (videoDec == nullptr) {
        OH_LOG_ERROR(LOG_APP,"create hardware decoder failed!");
        return false;
    }
    
    codecUserData = new CodecUserData();
    OH_AVCodecCallback callback = {OH_AVCodecOnError, OH_AVCodecOnStreamChanged, OH_AVCodecOnNeedInputBuffer, 
        OH_AVCodecOnNewOutputBuffer};
    int ret = OH_VideoDecoder_RegisterCallback(videoDec, callback, codecUserData);
    if (ret != AV_ERR_OK) {
        OH_LOG_ERROR(LOG_APP,"hardware decoder register callback failed!, ret:%{public}d", ret);
        delete codecUserData;
        codecUserData = nullptr;
        return false;
    }
    
    OH_AVFormat *ohFormat = OH_AVFormat_Create();
    OH_AVFormat_SetIntValue(ohFormat, OH_MD_KEY_WIDTH, format.width);
    OH_AVFormat_SetIntValue(ohFormat, OH_MD_KEY_HEIGHT, format.height);
    OH_AVFormat_SetIntValue(ohFormat, OH_MD_KEY_PIXEL_FORMAT, AV_PIXEL_FORMAT_NV12);
    ret = OH_VideoDecoder_Configure(videoDec, ohFormat);
    OH_AVFormat_Destroy(ohFormat);
    if (ret != AV_ERR_OK) {
        OH_LOG_ERROR(LOG_APP,"config hardware decoder failed!, ret:%{public}d", ret);
        delete codecUserData;
        codecUserData = nullptr;
        return false;
    }
    ret = OH_VideoDecoder_Prepare(videoDec);
    if (ret != AV_ERR_OK) {
        OH_LOG_ERROR(LOG_APP,"hardware decoder prepare failed!, ret:%{public}d", ret);
        delete codecUserData;
        codecUserData = nullptr;
        return false;
    }
    
    videoFormat = format;
    if (!start()) {
         OH_LOG_ERROR(LOG_APP,"hardware decoder start failed!, ret:%{public}d", ret);
        delete codecUserData;
        codecUserData = nullptr;
        return false;
    }
    return true;
}

DecodingResult HardwareDecoder::onSendBytes(void *bytes, size_t length, int64_t time) {
    OH_LOG_INFO(LOG_APP, "------ onSendBytes --------length:%{public}ld", length);
    std::unique_lock<std::mutex> lock(codecUserData->inputMutex);
    codecUserData->inputCondition.wait(lock, [this](){
        return codecUserData->inputBufferInfoQueue.size() > 0 ;
    });
    CodecBufferInfo codecBufferInfo = codecUserData->inputBufferInfoQueue.front();
    lock.unlock();
    
    OH_AVCodecBufferAttr bufferAttr;
    bufferAttr.size = length;
    bufferAttr.offset = 0;
    bufferAttr.pts = time;
    bufferAttr.flags = length == 0 ? AVCODEC_BUFFER_FLAGS_EOS : AVCODEC_BUFFER_FLAGS_NONE;

    if (length > 0 && bytes != nullptr) {
        memcpy(OH_AVBuffer_GetAddr(codecBufferInfo.buffer), bytes, length);
    }
    int ret = OH_AVBuffer_SetBufferAttr(codecBufferInfo.buffer, &bufferAttr);
    if (ret == AV_ERR_OK) {
        OH_LOG_INFO(LOG_APP,"Set BufferAttr attr success!, ret:%{public}d", ret);
        ret = OH_VideoDecoder_PushInputBuffer(videoDec, codecBufferInfo.bufferIndex);
        if (ret != AV_ERR_OK) {
            OH_LOG_ERROR(LOG_APP,"OH_VideoDecoder_PushInputBuffer failed, ret:%{public}d", ret);
        }
    }
    OH_LOG_INFO(LOG_APP,"onSendBytes：, ret:%{public}d", ret);
    
    lock.lock();
    codecUserData->inputBufferInfoQueue.pop();
    lock.unlock();
    
    if (ret != AV_ERR_OK) {
        return DecodingResult::Error;
    }
    return DecodingResult::Success;
}

DecodingResult HardwareDecoder::onEndOfStream() {
    return onSendBytes(nullptr, 0, 0);
}

DecodingResult HardwareDecoder::onDecodeFrame() {
    std::unique_lock<std::mutex> lock(codecUserData->outputMutex);
    codecUserData->outputCondition.wait(lock,[this](){
        return codecUserData->outputBufferInfoQueue.size() > 0;
    });
    
    codecBufferInfo = codecUserData->outputBufferInfoQueue.front();
    codecUserData->outputBufferInfoQueue.pop();
    lock.unlock();
    OH_LOG_INFO(LOG_APP, "------ HardwareDecoder::onDecodeFrame() --------pts:%{public}d", codecBufferInfo.attr.pts);
    if (codecBufferInfo.attr.flags == AVCODEC_BUFFER_FLAGS_EOS) {
        return DecodingResult::EndOfStream;
    }
    return DecodingResult::Success;
}

void HardwareDecoder::onFlush() {
    OH_LOG_INFO(LOG_APP, "------ HardwareDecoder::onFlush --------");
    int ret = OH_VideoDecoder_Flush(videoDec);
    if (ret != AV_ERR_OK) {
         return;
    }
    codecUserData->clearQueue();
    start();
}

bool HardwareDecoder::start() {
    OH_LOG_INFO(LOG_APP, "------ HardwareDecoder::start --------");
    int ret = OH_VideoDecoder_Start(videoDec);
    if (ret != AV_ERR_OK) {
        OH_LOG_INFO(LOG_APP,"hardware decoder start failed!, ret:%d", ret);
        return false;
    }
    for (auto& header : videoFormat.headers) {
        onSendBytes(const_cast<void*>(header->data()), header->size(), 0);
    }
    OH_LOG_INFO(LOG_APP, "------ start finish --------");
    return true;
}

int64_t HardwareDecoder::presentationTime() {
    return codecBufferInfo.attr.pts;
}

std::shared_ptr<tgfx::ImageBuffer> HardwareDecoder::onRenderFrame() {
    // Get data
    
    OH_LOG_INFO(LOG_APP,"onRenderFrame, pts:%{public}d, size:%{public}d", codecBufferInfo.attr.pts, codecBufferInfo.attr.size);
    int ret = OH_VideoDecoder_FreeOutputBuffer(videoDec, codecBufferInfo.bufferIndex);
    if (ret != AV_ERR_OK) {
        OH_LOG_ERROR(LOG_APP,"OH_VideoDecoder_FreeOutputBuffer, ret:%{public}d", ret);
    }
    return nullptr;
}



}  // namespace pag