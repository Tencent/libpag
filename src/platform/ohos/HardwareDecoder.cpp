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
#include <native_buffer/native_buffer.h>
#include "hilog/log.h"
#include <mutex>

#include "base/utils/Log.h"
#include "pag/pag.h"

#define LOG_TAG "PAG" 

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
    std::unique_lock<std::mutex> lock(codecUserData->inputMutex_);
    codecUserData->inputBufferInfoQueue_.emplace(index, buffer);
    codecUserData->inputCond_.notify_all();
}

void OH_AVCodecOnNewOutputBuffer(OH_AVCodec *codec, uint32_t index, OH_AVBuffer *buffer, void *userData) {
    if (userData == nullptr) {
        return;
    }
    (void)codec;
    OH_LOG_INFO(LOG_APP, "------OH_AVCodecOnNewOutputBuffer---------");
    CodecUserData *codecUserData = static_cast<CodecUserData*>(userData);
    std::unique_lock<std::mutex> lock(codecUserData->outputMutex_);
    codecUserData->outputBufferInfoQueue_.emplace(index, buffer);
    codecUserData->outputCond_.notify_all();
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
//     if (!start()) {
//          OH_LOG_ERROR(LOG_APP,"hardware decoder start failed!, ret:%{public}d", ret);
//         delete codecUserData;
//         codecUserData = nullptr;
//         return false;
//     }
    return true;
}

DecodingResult HardwareDecoder::onSendBytes(void *bytes, size_t length, int64_t time) {
    OH_LOG_INFO(LOG_APP, "------ onSendBytes --------length:%{public}ld", length);
    std::unique_lock<std::mutex> lock(codecUserData->inputMutex_);
    codecUserData->inputCond_.wait(lock, [this](){
        return codecUserData->inputBufferInfoQueue_.size() > 0 ;
    });
    CodecBufferInfo codecBufferInfo = codecUserData->inputBufferInfoQueue_.front();
    int index = codecBufferInfo.bufferIndex;
    lock.unlock();
    
    OH_AVCodecBufferAttr bufferAttr;
    bufferAttr.size = length;
    bufferAttr.offset = 0;
    bufferAttr.pts = time;
    bufferAttr.flags = length == 0 ? AVCODEC_BUFFER_FLAGS_EOS : AVCODEC_BUFFER_FLAGS_NONE;
    if (length > 100) {
         OH_LOG_INFO(LOG_APP, "------ onSendBytes -----data----");
        bufferAttr.flags = AVCODEC_BUFFER_FLAGS_SYNC_FRAME;
    }
    
    OH_AVBuffer *buffer = nullptr;
    if (length > 0 && bytes != nullptr) {
        buffer = OH_AVBuffer_Create(length);
        if (buffer == nullptr) {
            OH_LOG_ERROR(LOG_APP,"OH_AVBuffer_Create failed!");
        }
        memcpy(OH_AVBuffer_GetAddr(buffer), bytes, length);
    }
    
    int ret = OH_AVBuffer_SetBufferAttr(buffer, &bufferAttr);
    if (ret == AV_ERR_OK) {
        OH_LOG_INFO(LOG_APP,"Set BufferAttr attr success!, ret:%{public}d", ret);
        ret = OH_VideoDecoder_PushInputBuffer(videoDec, index);
        if (ret != AV_ERR_OK) {
            OH_LOG_ERROR(LOG_APP,"OH_VideoDecoder_PushInputBuffer failed, ret:%{public}d", ret);
        }
    }
    OH_LOG_INFO(LOG_APP,"onSendBytes：, ret:%{public}d", ret);
    if (buffer) {
        OH_AVBuffer_Destroy(buffer);
    }
    lock.lock();
    codecUserData->inputBufferInfoQueue_.pop();
    lock.unlock();
    
    if (ret != AV_ERR_OK) {
        return DecodingResult::Error;
    }
    return DecodingResult::Success;
}

DecodingResult HardwareDecoder::onEndOfStream() {
    OH_AVCodecBufferAttr bufferAttr;
    bufferAttr.size = 0;
    bufferAttr.offset = 0;
    bufferAttr.pts = 0;
    bufferAttr.flags = AVCODEC_BUFFER_FLAGS_NONE;
    int ret = OH_AVBuffer_SetBufferAttr(nullptr, &bufferAttr);
    if (ret != AV_ERR_OK) {
        OH_LOG_ERROR(LOG_APP,"Set BufferAttr attr failed!, ret:%{public}d", ret);
        return DecodingResult::Error;
    }
    ret = OH_VideoDecoder_PushInputBuffer(videoDec, 0);
    if (ret != AV_ERR_OK) {
        OH_LOG_ERROR(LOG_APP,"OH_VideoDecoder_PushInputBuffer failed, ret:%{public}d", ret);
    }
    OH_LOG_INFO(LOG_APP,"onSendBytes：, ret:%{public}d", ret);
    return DecodingResult::Success;
}

DecodingResult HardwareDecoder::onDecodeFrame() {
    std::unique_lock<std::mutex> lock(codecUserData->outputMutex_);
    codecUserData->outputCond_.wait(lock,[this](){
        return codecUserData->outputBufferInfoQueue_.size() > 0;
    });
    
    codecBufferInfo = codecUserData->outputBufferInfoQueue_.front();
    codecUserData->outputBufferInfoQueue_.pop();
    lock.unlock();
    
    
    return DecodingResult::Success;
}

void HardwareDecoder::onFlush() {
    OH_LOG_INFO(LOG_APP, "------ HardwareDecoder::onFlush --------");
//     int ret = OH_VideoDecoder_Flush(videoDec);
//     if (ret != AV_ERR_OK) {
//          return;
//     }
//     ret = OH_VideoDecoder_Stop(videoDec);
//     if (ret != AV_ERR_OK) {
//         return;
//     }
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
    return codecBufferInfo.bufferIndex;
}

std::shared_ptr<tgfx::ImageBuffer> HardwareDecoder::onRenderFrame() {
    auto yuvData = codecBufferInfo.bufferAddr;
    
    int ret = OH_VideoDecoder_FreeOutputBuffer(videoDec, codecBufferInfo.bufferIndex);
    if (ret != AV_ERR_OK) {
        OH_LOG_ERROR(LOG_APP,"OH_VideoDecoder_FreeOutputBuffer, ret:%{public}d", ret);
         return nullptr;
    }
    return nullptr;
}



}  // namespace pag