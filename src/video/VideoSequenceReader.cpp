/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "VideoSequenceReader.h"
#include "VideoSequenceDemuxer.h"
#include "rendering/caches/RenderCache.h"

namespace pag {
std::shared_ptr<SequenceReader> SequenceReader::Make(std::shared_ptr<File> file,
        VideoSequence* sequence,
        DecodingPolicy policy) {
    return std::make_shared<VideoSequenceReader>(std::move(file), sequence, policy);
}

VideoSequenceReader::VideoSequenceReader(std::shared_ptr<File> file, VideoSequence* sequence,
        DecodingPolicy policy)
    : SequenceReader(std::move(file), sequence) {
    VideoConfig config = {};
    auto demuxer = std::make_unique<VideoSequenceDemuxer>(sequence);
    config.hasAlpha = sequence->alphaStartX + sequence->alphaStartY > 0;
    config.width = sequence->alphaStartX + sequence->width;
    if (config.width % 2 == 1) {
        config.width++;
    }
    config.height = sequence->alphaStartY + sequence->height;
    if (config.height % 2 == 1) {
        config.height++;
    }
    for (auto& header : sequence->headers) {
        auto bytes = ByteData::MakeWithoutCopy(header->data(), header->length());
        config.headers.push_back(std::move(bytes));
    }
    config.mimeType = "video/avc";
    config.colorSpace = YUVColorSpace::Rec601;
    config.frameRate = sequence->frameRate;
    reader = std::make_unique<VideoReader>(config, std::move(demuxer), policy);
}

void VideoSequenceReader::prepareAsync(Frame targetFrame) {
    if (!reader) {
        return;
    }
    auto targetTime = FrameToTime(targetFrame, sequence->frameRate);
    if (lastTask != nullptr) {
        if (lastFrame != -1) {
            pendingTime = targetTime;
        }
    } else {
        lastTask = VideoDecodingTask::MakeAndRun(reader.get(), targetTime);
    }
}

std::shared_ptr<Texture> VideoSequenceReader::readTexture(Frame targetFrame, RenderCache* cache) {
    if (!reader) {
        return nullptr;
    }
    if (lastFrame == targetFrame) {
        return lastTexture;
    }
    auto startTime = GetTimer();
    // setting task to nullptr triggers cancel(), in case the bitmap content changes before we
    // makeTexture().
    lastTask = nullptr;
    auto targetTime = FrameToTime(targetFrame, sequence->frameRate);
    auto buffer = reader->readSample(targetTime);
    auto decodingTime = GetTimer() - startTime;
    reader->recordPerformance(cache, decodingTime);
    lastTexture = nullptr;  // Release the last texture for reusing in context.
    lastFrame = -1;
    if (buffer) {
        startTime = GetTimer();
        lastTexture = buffer->makeTexture(cache->getContext());
        lastFrame = targetFrame;
        cache->textureUploadingTime += GetTimer() - startTime;
        if (!staticContent) {
            auto nextSampleTime = reader->getNextSampleTimeAt(targetTime);
            if (nextSampleTime == INT64_MAX && pendingTime >= 0) {
                // Add preparation for the first frame when reach to the end.
                nextSampleTime = pendingTime;
                pendingTime = -1;
            }
            if (nextSampleTime != INT64_MAX) {
                lastTask = VideoDecodingTask::MakeAndRun(reader.get(), nextSampleTime);
            }
        }
    }
    return lastTexture;
}
}  // namespace pag
