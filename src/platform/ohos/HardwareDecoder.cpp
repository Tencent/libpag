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
        LOGE("create hardware decoder failed!");
        return false;
    }
    
    codecUserData = new CodecUserData();
    OH_AVCodecCallback callback = {OH_AVCodecOnError, OH_AVCodecOnStreamChanged, OH_AVCodecOnNeedInputBuffer, 
        OH_AVCodecOnNewOutputBuffer};
    int ret = OH_VideoDecoder_RegisterCallback(videoDec, callback, codecUserData);
    if (ret != AV_ERR_OK) {
        LOGE("hardware decoder register callback failed!, ret:%d", ret);
        return false;
    }
    
    OH_AVFormat *ohFormat = OH_AVFormat_Create();
    OH_AVFormat_SetIntValue(ohFormat, OH_MD_KEY_WIDTH, format.width);
    OH_AVFormat_SetIntValue(ohFormat, OH_MD_KEY_HEIGHT, format.height);
    OH_AVFormat_SetIntValue(ohFormat, OH_MD_KEY_PIXEL_FORMAT, AV_PIXEL_FORMAT_NV12);
    ret = OH_VideoDecoder_Configure(videoDec, ohFormat);
    OH_AVFormat_Destroy(ohFormat);
    if (ret != AV_ERR_OK) {
        LOGE("config hardware decoder failed!, ret:%d", ret);
        return false;
    }
    imageReader = tgfx::SurfaceTextureReader::Make(format.width, format.height);
    if (imageReader == nullptr) {
        LOGE("SurfaceTextureReader create failed!");
        return false;
    }
//     ret = OH_VideoDecoder_SetSurface(videoDec, imageReader->getInputSurface());
//     if (ret != AV_ERR_OK) {
//         LOGE("hardware decoder setSurface failed!, ret:%d", ret);
//         return false;
//     }
    ret = OH_VideoDecoder_Prepare(videoDec);
    if (ret != AV_ERR_OK) {
        LOGE("hardware decoder prepare failed!, ret:%d", ret);
        return false;
    }
    videoFormat = format;
//     if (!start()) {
//          LOGE("hardware decoder start failed!, ret:%d", ret);
//         return false;
//     }
    return true;
}

DecodingResult HardwareDecoder::onSendBytes(void *bytes, size_t length, int64_t time) {
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
        LOGI("Set BufferAttr attr success!, ret:%d", ret);
        ret = OH_VideoDecoder_PushInputBuffer(videoDec, codecBufferInfo.bufferIndex);
        if (ret != AV_ERR_OK) {
            LOGE("OH_VideoDecoder_PushInputBuffer failed, ret:%d", ret);
        }
    }
    LOGI("onSendBytes：, ret:%d", ret);
    
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
    if (codecBufferInfo.attr.flags == AVCODEC_BUFFER_FLAGS_EOS) {
        return DecodingResult::EndOfStream;
    }
    return DecodingResult::Success;
}

void HardwareDecoder::onFlush() {
//     int ret = OH_VideoDecoder_Flush(videoDec);
//     if (ret != AV_ERR_OK) {
//          return;
//     }
//     codecUserData->clearQueue();
    start();
}

bool HardwareDecoder::start() {
    int ret = OH_VideoDecoder_Start(videoDec);
    if (ret != AV_ERR_OK) {
        LOGE("hardware decoder start failed!, ret:%d", ret);
        return false;
    }
    for (auto& header : videoFormat.headers) {
        onSendBytes(const_cast<void*>(header->data()), header->size(), 0);
    }
    return true;
}

int64_t HardwareDecoder::presentationTime() {
    return codecBufferInfo.attr.pts;
}

std::shared_ptr<tgfx::ImageBuffer> HardwareDecoder::onRenderFrame() {
    auto imageBuffer = imageReader->acquireNextBuffer();
    LOGI("onRenderFrame, pts:%d, size:%d", codecBufferInfo.attr.pts, codecBufferInfo.attr.size);
    int ret = OH_VideoDecoder_FreeOutputBuffer(videoDec, codecBufferInfo.bufferIndex);
    if (ret != AV_ERR_OK) {
        LOGE("OH_VideoDecoder_FreeOutputBuffer, ret:%d", ret);
    }
    return imageBuffer;
}



}  // namespace pag