/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include <platform/Platform.h>
#include "base/utils/Log.h"
#include "base/utils/TGFXCast.h"
#include "base/utils/TimeUtil.h"
#include "pag/pag.h"
#include "rendering/CompositionReader.h"
#include "rendering/caches/DiskCache.h"
#include "rendering/layers/ContentVersion.h"
#include "rendering/utils/BitmapBuffer.h"
#include "rendering/utils/LockGuard.h"
#include "tgfx/gpu/opengl/GLDevice.h"

namespace pag {

static std::string DefaultCacheKeyGeneratorFunc(PAGDecoder* decoder,
                                                std::shared_ptr<PAGComposition> composition) {
  if (!composition->isPAGFile() || pag::ContentVersion::Get(composition) > 0) {
    return "";
  }
  auto filePath = static_cast<PAGFile*>(composition.get())->path();
  filePath = Platform::Current()->getSandboxPath(filePath);
  if (filePath.empty()) {
    return "";
  }
  return filePath + "." + std::to_string(decoder->width()) + "x" +
         std::to_string(decoder->height());
}

Composition* PAGDecoder::GetSingleComposition(std::shared_ptr<PAGComposition> pagComposition) {
  auto numChildren = pagComposition->numChildren();
  if (numChildren == 0) {
    auto composition = static_cast<PreComposeLayer*>(pagComposition->layer)->composition;
    if (composition->type() == CompositionType::Bitmap ||
        composition->type() == CompositionType::Video) {
      return composition;
    }
  }
  if (numChildren == 1) {
    auto firstLayer = pagComposition->getLayerAt(0);
    if (firstLayer->layerType() == LayerType::PreCompose) {
      return GetSingleComposition(std::static_pointer_cast<PAGComposition>(firstLayer));
    }
  }
  return nullptr;
}

std::pair<int, float> PAGDecoder::GetFrameCountAndRate(
    std::shared_ptr<PAGComposition> pagComposition, float maxFrameRate) {
  auto composition = GetSingleComposition(pagComposition);
  auto compositionFrameRate =
      composition != nullptr ? composition->frameRate : pagComposition->frameRate();
  auto frameRate = std::min(maxFrameRate, compositionFrameRate);
  auto duration = pagComposition->duration();
  auto numFrames = static_cast<int>(round(static_cast<double>(duration) * frameRate / 1000000.0));
  return {numFrames, frameRate};
}

std::vector<TimeRange> PAGDecoder::GetStaticTimeRange(std::shared_ptr<PAGComposition> composition,
                                                      int numFrames) {
  LockGuard autoLock(composition->rootLocker);
  std::vector<TimeRange> timeRanges = {};
  auto startTime = composition->startTimeInternal();
  auto duration = composition->durationInternal();
  auto oldLayerTime = composition->currentTimeInternal();
  composition->gotoTime(startTime);
  TimeRange timeRange = {0, 0};
  for (int i = 1; i < numFrames; i++) {
    auto progress = FrameToProgress(static_cast<Frame>(i), numFrames);
    auto layerTime = startTime + ProgressToTime(progress, duration);
    if (!composition->gotoTime(layerTime)) {
      timeRange.end++;
      continue;
    }
    if (timeRange.duration() > 1) {
      timeRanges.push_back(timeRange);
    }
    timeRange = {i, i};
  }
  if (timeRange.duration() > 1) {
    timeRanges.push_back(timeRange);
  }
  composition->gotoTime(oldLayerTime);
  return timeRanges;
}

std::shared_ptr<PAGDecoder> PAGDecoder::MakeFrom(std::shared_ptr<PAGComposition> composition,
                                                 float maxFrameRate, float scale) {
  auto sharedContext = tgfx::GLDevice::CurrentNativeHandle();
  if (composition == nullptr || maxFrameRate <= 0 || scale <= 0) {
    return nullptr;
  }
  auto width = roundf(static_cast<float>(composition->width()) * scale);
  auto height = roundf(static_cast<float>(composition->height()) * scale);
  auto result = GetFrameCountAndRate(composition, maxFrameRate);
  return std::shared_ptr<PAGDecoder>(new PAGDecoder(std::move(composition), static_cast<int>(width),
                                                    static_cast<int>(height), result.first,
                                                    result.second, maxFrameRate, sharedContext));
}

PAGDecoder::PAGDecoder(std::shared_ptr<PAGComposition> composition, int width, int height,
                       int numFrames, float frameRate, float maxFrameRate, void* sharedContext)
    : _width(width), _height(height), _numFrames(numFrames), _frameRate(frameRate),
      maxFrameRate(maxFrameRate), sharedContext(sharedContext) {
  container = PAGComposition::Make(width, height);
  container->addLayer(composition);
  staticTimeRanges = GetStaticTimeRange(composition, _numFrames);
  lastImageInfo = new tgfx::ImageInfo();
}

PAGDecoder::~PAGDecoder() {
  delete lastImageInfo;
}

int PAGDecoder::numFrames() {
  std::lock_guard<std::mutex> auoLock(locker);
  checkCompositionChange(getComposition());
  return _numFrames;
}

float PAGDecoder::frameRate() {
  std::lock_guard<std::mutex> auoLock(locker);
  checkCompositionChange(getComposition());
  return _frameRate;
}

bool PAGDecoder::checkFrameChanged(int index) {
  if (index < 0 || index >= _numFrames) {
    LOGE("PAGDecoder::readFrame() The index is out of range!");
    return false;
  }
  checkCompositionChange(getComposition());
  if (index == lastReadIndex) {
    return false;
  }
  auto timeRange = GetTimeRangeContains(staticTimeRanges, index);
  return !timeRange.contains(lastReadIndex);
}

bool PAGDecoder::readFrame(int index, void* pixels, size_t rowBytes, ColorType colorType,
                           AlphaType alphaType) {
  std::lock_guard<std::mutex> auoLock(locker);
  auto info =
      tgfx::ImageInfo::Make(_width, _height, ToTGFX(colorType), ToTGFX(alphaType), rowBytes);
  auto bitmap = BitmapBuffer::Wrap(info, pixels);
  return readFrameInternal(index, bitmap);
}

bool PAGDecoder::readFrame(int index, HardwareBufferRef hardwareBuffer) {
  std::lock_guard<std::mutex> auoLock(locker);
  auto bitmap = BitmapBuffer::Wrap(hardwareBuffer);
  return readFrameInternal(index, bitmap);
}

bool PAGDecoder::readFrameInternal(int index, std::shared_ptr<BitmapBuffer> bitmap) {
  if (bitmap == nullptr) {
    LOGE("PAGDecoder::readFrame() The specified bitmap buffer is invalid!");
    return false;
  }
  auto composition = getComposition();
  checkCompositionChange(composition);
  if (index < 0 || index >= _numFrames) {
    LOGE("PAGDecoder::readFrame() The index is out of range!");
    return false;
  }
  if (!checkSequenceFile(composition, bitmap->info())) {
    return false;
  }
  auto success = sequenceFile->readFrame(index, bitmap);
  if (!success) {
    success = renderFrame(composition, index, bitmap);
    if (success) {
      success = sequenceFile->writeFrame(index, bitmap);
      if (!success) {
        LOGE("PAGDecoder::readFrame() Failed to write frame to SequenceFile!");
      }
    }
  }
  if (sequenceFile->isComplete() && composition != nullptr) {
    if (reader != nullptr) {
      reader = nullptr;
      if (composition.use_count() != 1) {
        container->addLayer(composition);
      }
    } else if (composition.use_count() <= 2) {
      container->removeAllLayers();
    }
  }
  if (success) {
    lastReadIndex = index;
  }
  return success;
}

bool PAGDecoder::renderFrame(std::shared_ptr<PAGComposition> composition, int index,
                             std::shared_ptr<BitmapBuffer> bitmap) {
  if (composition == nullptr) {
    reader = nullptr;
    LOGE(
        "PAGDecoder: Failed to get PAGComposition! the associated PAGComposition "
        "may be added to another parent after the PAGDecoder was created.");
    return false;
  }
  if (reader == nullptr) {
    reader = CompositionReader::Make(_width, _height, sharedContext);
    if (reader == nullptr) {
      LOGE("PAGDecoder::renderFrame() Failed to create a CompositionReader!");
      return false;
    }
    reader->setComposition(composition);
  }
  auto progress = FrameToProgress(static_cast<Frame>(index), _numFrames);
  return reader->readFrame(progress, bitmap);
}

bool PAGDecoder::checkSequenceFile(std::shared_ptr<PAGComposition> composition,
                                   const tgfx::ImageInfo& info) {
  if (sequenceFile != nullptr) {
    if (*lastImageInfo != info) {
      LOGE(
          "PAGDecoder::readFrame() The rowBytes, colorType or alphaType is not the same as the "
          "previous call!");
      return false;
    }
    return true;
  }
  if (composition == nullptr) {
    LOGE(
        "PAGDecoder: Failed to get PAGComposition! the associated PAGComposition "
        "may be added to another parent after the PAGDecoder was created.");
    return false;
  }
  auto key = generateCacheKey(composition);
  sequenceFile = DiskCache::OpenSequence(key, info, _numFrames, _frameRate, staticTimeRanges);
  if (sequenceFile == nullptr) {
    LOGE("PAGDecoder: Failed to open SequenceFile!");
    return false;
  }
  *lastImageInfo = info;
  return true;
}

void PAGDecoder::checkCompositionChange(std::shared_ptr<PAGComposition> composition) {
  if (composition == nullptr) {
    return;
  }
  auto contentVersion = ContentVersion::Get(composition);
  if (contentVersion == lastContentVersion) {
    return;
  }
  sequenceFile = nullptr;
  lastContentVersion = contentVersion;
  lastReadIndex = -1;
  auto result = GetFrameCountAndRate(composition, maxFrameRate);
  _numFrames = result.first;
  _frameRate = result.second;
  staticTimeRanges = GetStaticTimeRange(composition, _numFrames);
}

std::string PAGDecoder::generateCacheKey(std::shared_ptr<PAGComposition> composition) {
  return cacheKeyGeneratorFun == nullptr ? DefaultCacheKeyGeneratorFunc(this, composition)
                                         : cacheKeyGeneratorFun(this, composition);
}

std::shared_ptr<PAGComposition> PAGDecoder::getComposition() {
  if (container->numChildren() == 1) {
    return std::static_pointer_cast<PAGComposition>(container->getLayerAt(0));
  }
  if (reader != nullptr) {
    return reader->getComposition();
  }
  return nullptr;
}

void PAGDecoder::setCacheKeyGeneratorFun(
    std::function<std::string(PAGDecoder*, std::shared_ptr<PAGComposition> composition)> fun) {
  cacheKeyGeneratorFun = fun;
}

}  // namespace pag
