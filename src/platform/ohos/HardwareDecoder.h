//
// Created on 2024/7/9.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#pragma once

#include <multimedia/player_framework/native_avcodec_videodecoder.h>
#include <queue>

#include "rendering/video/DecodingResult.h"
#include "rendering/video/VideoDecoder.h"
#include "tgfx/platform/ohos/SurfaceTextureReader.h"

namespace pag {
struct CodecBufferInfo {
    uint32_t bufferIndex = 0;
    OH_AVBuffer *buffer = nullptr;
    OH_AVCodecBufferAttr attr = {0, 0, 0, AVCODEC_BUFFER_FLAGS_NONE};
    
    CodecBufferInfo(uint32_t argBufferIndex, OH_AVBuffer *argBuffer)
        : bufferIndex(argBufferIndex), buffer(argBuffer) {
        OH_AVBuffer_GetBufferAttr(argBuffer, &attr);
    };
};

class CodecUserData {
public:
    std::mutex inputMutex;
    std::condition_variable inputCondition;
    std::queue<CodecBufferInfo> inputBufferInfoQueue;

    std::mutex outputMutex;
    std::condition_variable outputCondition;
    std::queue<CodecBufferInfo> outputBufferInfoQueue;

    void clearQueue() {
        {
            std::unique_lock<std::mutex> lock(inputMutex);
            auto emptyQueue = std::queue<CodecBufferInfo>();
            inputBufferInfoQueue.swap(emptyQueue);
        }
        {
            std::unique_lock<std::mutex> lock(outputMutex);
            auto emptyQueue = std::queue<CodecBufferInfo>();
            outputBufferInfoQueue.swap(emptyQueue);
        }
    }
};

class HardwareDecoder : public VideoDecoder {
    public:
    
    ~HardwareDecoder() override;
    
    DecodingResult onSendBytes(void *bytes, size_t length, int64_t time) override;
    
    DecodingResult onEndOfStream() override;
    
    DecodingResult onDecodeFrame() override;
    
    void onFlush() override;
    
    int64_t presentationTime() override;
    
    std::shared_ptr<tgfx::ImageBuffer> onRenderFrame() override;
    
    private:
    bool isValid = false;
    OH_AVCodec *videoDec = nullptr;
    CodecUserData *codecUserData = nullptr;
    CodecBufferInfo codecBufferInfo = {0, nullptr};
    VideoFormat videoFormat{};
    std::shared_ptr<tgfx::SurfaceTextureReader> imageReader = nullptr;
    explicit HardwareDecoder(const VideoFormat& format);
    bool initDecoder(const VideoFormat& format);
    bool start();
    
    friend class HardwareDecoderFactory;
    

};
}  // namespace pag
