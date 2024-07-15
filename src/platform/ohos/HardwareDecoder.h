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
namespace pag {
struct CodecBufferInfo {
    uint32_t bufferIndex = 0;
    uintptr_t *buffer = nullptr;
    uint8_t *bufferAddr = nullptr;
    OH_AVCodecBufferAttr attr = {0, 0, 0, AVCODEC_BUFFER_FLAGS_NONE};

    CodecBufferInfo(uint8_t *addr) : bufferAddr(addr){};
    CodecBufferInfo(uint8_t *addr, int32_t bufferSize)
        : bufferAddr(addr), attr({0, bufferSize, 0, AVCODEC_BUFFER_FLAGS_NONE}){};
    CodecBufferInfo(uint32_t argBufferIndex, OH_AVMemory *argBuffer, OH_AVCodecBufferAttr argAttr)
        : bufferIndex(argBufferIndex), buffer(reinterpret_cast<uintptr_t *>(argBuffer)), attr(argAttr){};
    CodecBufferInfo(uint32_t argBufferIndex, OH_AVMemory *argBuffer)
        : bufferIndex(argBufferIndex), buffer(reinterpret_cast<uintptr_t *>(argBuffer)){};
    CodecBufferInfo(uint32_t argBufferIndex, OH_AVBuffer *argBuffer)
        : bufferIndex(argBufferIndex), buffer(reinterpret_cast<uintptr_t *>(argBuffer)) {
        OH_AVBuffer_GetBufferAttr(argBuffer, &attr);
    };
};

class CodecUserData {
public:
    uint32_t inputFrameCount_ = 0;
    std::mutex inputMutex_;
    std::condition_variable inputCond_;
    std::queue<CodecBufferInfo> inputBufferInfoQueue_;

    uint32_t outputFrameCount_ = 0;
    std::mutex outputMutex_;
    std::condition_variable outputCond_;
    std::queue<CodecBufferInfo> outputBufferInfoQueue_;

    void ClearQueue() {
        {
            std::unique_lock<std::mutex> lock(inputMutex_);
            auto emptyQueue = std::queue<CodecBufferInfo>();
            inputBufferInfoQueue_.swap(emptyQueue);
        }
        {
            std::unique_lock<std::mutex> lock(outputMutex_);
            auto emptyQueue = std::queue<CodecBufferInfo>();
            outputBufferInfoQueue_.swap(emptyQueue);
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
    CodecBufferInfo codecBufferInfo = {nullptr};
    VideoFormat videoFormat{};
    explicit HardwareDecoder(const VideoFormat& format);
    bool initDecoder(const VideoFormat& format);
    bool start();
    
    friend class HardwareDecoderFactory;
    

};
}  // namespace pag
