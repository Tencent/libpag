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

#ifdef FILE_MOVIE

#include "MovieReader.h"
#include "platform/Platform.h"
#include "rendering/caches/RenderCache.h"

namespace pag {
static std::mutex cacheLocker = {};
static std::unordered_map<ID, std::weak_ptr<VideoReader>> movieReaderCache = {};

static std::unique_ptr<VideoReader> MakeVideoReader(MovieInfo* movieInfo, DecodingPolicy policy) {
  auto demuxer = FFmpegDemuxer::Make(movieInfo->filePath(), Platform::Current()->naluType());
  if (demuxer == nullptr) {
    return nullptr;
  }
  demuxer->selectTrack(movieInfo->trackID());
  auto format = demuxer->getTrackFormat(movieInfo->trackID());
  if (format == nullptr) {
    return nullptr;
  }

  tgfx::YUVColorSpace colorSpace = tgfx::YUVColorSpace::Unknown;
  if (format->getString(KEY_COLOR_SPACE) == COLORSPACE_REC2020) {
    colorSpace = tgfx::YUVColorSpace::Rec2020;
  } else if (format->getString(KEY_COLOR_SPACE) == COLORSPACE_REC709) {
    colorSpace = tgfx::YUVColorSpace::Rec709;
  } else {
    colorSpace = tgfx::YUVColorSpace::Rec601;
  }

  tgfx::YUVColorRange colorRange = tgfx::YUVColorRange::Unknown;
  if (format->getString(KEY_COLOR_RANGE) == COLORRANGE_JPEG) {
    colorRange = tgfx::YUVColorRange::JPEG;
  } else {
    colorRange = tgfx::YUVColorRange::MPEG;
  }

  VideoConfig config = {};
  config.headers = format->headers();
  config.mimeType = format->getString(KEY_MIME);
  config.colorSpace = colorSpace;
  config.colorRange = colorRange;
  config.width = movieInfo->videoWidth();
  config.height = movieInfo->videoHeight();
  config.frameRate = format->getFloat(KEY_FRAME_RATE);
  return std::make_unique<VideoReader>(config, std::move(demuxer), policy);
}

MovieReader::MovieReader(MovieInfo* movieInfo, DecodingPolicy policy) {
  if (movieInfo->shareDecoder) {
    std::lock_guard<std::mutex> cacheLock(cacheLocker);
    auto result = movieReaderCache.find(movieInfo->uniqueID());
    if (result != movieReaderCache.end()) {
      reader = result->second.lock();
      if (reader) {
        return;
      }
      movieReaderCache.erase(result);
    }
    reader = MakeVideoReader(movieInfo, policy);
    if (reader) {
      movieReaderCache[movieInfo->uniqueID()] = reader;
    }
  } else {
    reader = MakeVideoReader(movieInfo, policy);
  }
}

void MovieReader::prepareAsync(int64_t targetTime) {
  if (!reader) {
    return;
  }
  if (lastTask != nullptr) {
    if (lastSampleTime != -1) {
      pendingTime = targetTime;
    }
  } else {
    lastTask = VideoDecodingTask::MakeAndRun(reader.get(), targetTime);
  }
}

std::shared_ptr<tgfx::Texture> MovieReader::readTexture(int64_t targetTime, RenderCache* cache) {
  if (!reader) {
    return nullptr;
  }
  auto sampleTime = reader->getSampleTimeAt(targetTime);
  if (sampleTime == lastSampleTime) {
    return lastTexture;
  }
  auto startTime = GetTimer();
  // 提前置空，析构会触发 cancel()，防止 makeTexture 之前内容被异步线程修改。
  lastTask = nullptr;
  auto buffer = reader->readSample(targetTime);
  auto decodingTime = GetTimer() - startTime;
  reader->recordPerformance(cache, decodingTime);
  lastTexture = nullptr;  // 先释放上一次的 Texture，允许在 Context 里复用。
  lastSampleTime = -1;
  if (buffer) {
    startTime = GetTimer();
    lastTexture = buffer->makeTexture(cache->getContext());
    lastSampleTime = sampleTime;
    cache->textureUploadingTime += GetTimer() - startTime;
    auto nextSampleTime = reader->getNextSampleTimeAt(targetTime);
    if (nextSampleTime == INT64_MAX && pendingTime >= 0) {
      nextSampleTime = pendingTime;
      pendingTime = -1;
    }
    if (nextSampleTime != INT64_MAX) {
      lastTask = VideoDecodingTask::MakeAndRun(reader.get(), nextSampleTime);
    }
  }
  return lastTexture;
}
}  // namespace pag

#endif
