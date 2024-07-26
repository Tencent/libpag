/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 THL A29 Limited, a Tencent company. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//

#pragma once

#include <multimedia/player_framework/native_avcodec_videodecoder.h>
#include <cstdint>
#include <list>
#include <queue>

#include "rendering/video/DecodingResult.h"
#include "rendering/video/VideoDecoder.h"

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
    OH_AVCodec *videoCodec = nullptr;
    CodecUserData *codecUserData = nullptr;
    CodecBufferInfo codecBufferInfo = {0, nullptr};
    VideoFormat videoFormat{};
    std::list<int64_t> pendingFrames{};
    explicit HardwareDecoder(const VideoFormat& format);
    bool initDecoder(const VideoFormat& format);
    bool start();
    
    friend class HardwareDecoderFactory;
    

};
}  // namespace pag
