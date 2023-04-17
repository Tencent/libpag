/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "base/utils/Log.h"
#include "base/utils/TGFXCast.h"
#include "base/utils/TimeUtil.h"
#include "pag/pag.h"
#include "rendering/caches/DiskCache.h"

namespace pag {
float PAGDecoder::GetFrameRate(std::shared_ptr<PAGComposition> pagComposition) {
  auto numChildren = pagComposition->numChildren();
  if (numChildren == 0) {
    auto composition = static_cast<PreComposeLayer*>(pagComposition->layer)->composition;
    if (composition->type() == CompositionType::Bitmap ||
        composition->type() == CompositionType::Video) {
      return composition->frameRate;
    }
  }
  if (numChildren == 1) {
    auto firstLayer = pagComposition->getLayerAt(0);
    if (firstLayer->layerType() == LayerType::PreCompose) {
      return GetFrameRate(std::static_pointer_cast<PAGComposition>(firstLayer));
    }
  }
  return pagComposition->frameRate();
}

std::shared_ptr<PAGDecoder> PAGDecoder::MakeFrom(std::shared_ptr<PAGComposition> composition,
                                                 float maxFrameRate, float scale) {
  if (composition == nullptr) {
    return nullptr;
  }
  auto width = roundf(static_cast<float>(composition->width()) * scale);
  auto height = roundf(static_cast<float>(composition->height()) * scale);
  auto compositionFrameRate = GetFrameRate(composition);
  auto frameRate = std::min(maxFrameRate, compositionFrameRate);
  auto duration = composition->duration();
  auto numFrames = static_cast<int>(round(static_cast<double>(duration) * frameRate / 1000000.0));
  return std::shared_ptr<PAGDecoder>(new PAGDecoder(std::move(composition), static_cast<int>(width),
                                                    static_cast<int>(height),
                                                    static_cast<int>(numFrames), frameRate));
}

PAGDecoder::PAGDecoder(std::shared_ptr<PAGComposition> composition, int width, int height,
                       int numFrames, float frameRate)
    : composition(std::move(composition)), _width(width), _height(height), _numFrames(numFrames),
      _frameRate(frameRate) {
}

bool PAGDecoder::readFrame(int index, void* pixels, size_t rowBytes, pag::ColorType colorType,
                           pag::AlphaType alphaType) {
  std::lock_guard<std::mutex> auoLock(locker);
  if (sequenceFile == nullptr && composition != nullptr) {
    auto key = generateCacheKey();
    auto info =
        tgfx::ImageInfo::Make(_width, _height, ToTGFX(colorType), ToTGFX(alphaType), rowBytes);
    sequenceFile = DiskCache::OpenSequence(key, info, _numFrames, _frameRate);
    if (sequenceFile == nullptr) {
      return false;
    }
    lastRowBytes = rowBytes;
    lastColorType = colorType;
    lastAlphaType = alphaType;
    lastContentVersion = composition->contentVersion;
    checkCacheComplete();
  }
  if (rowBytes != lastRowBytes || colorType != lastColorType || alphaType != lastAlphaType) {
    return false;
  }
  if (composition != nullptr && lastContentVersion != composition->contentVersion) {
    return false;
  }
  auto success = sequenceFile->readFrame(index, pixels);
  if (!success) {
    success = renderFrame(index, pixels, rowBytes, colorType, alphaType);
    if (success) {
      success = sequenceFile->writeFrame(index, pixels);
      if (success) {
        checkCacheComplete();
      } else {
        LOGE("Failed to write frame to SequenceFile!");
      }
    }
  }
  return success;
}

bool PAGDecoder::renderFrame(int index, void* pixels, size_t rowBytes, pag::ColorType colorType,
                             pag::AlphaType alphaType) {
  if (pagPlayer == nullptr) {
    if (composition == nullptr) {
      return false;
    }
    auto pagSurface = PAGSurface::MakeOffscreen(_width, _height);
    if (pagSurface == nullptr) {
      return false;
    }
    pagPlayer = std::make_unique<PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(composition);
  }
  auto progress = FrameToProgress(static_cast<Frame>(index), _numFrames);
  pagPlayer->setProgress(progress);
  pagPlayer->flush();
  auto pagSurface = pagPlayer->getSurface();
  return pagSurface->readPixels(colorType, alphaType, pixels, rowBytes);
}

void PAGDecoder::checkCacheComplete() {
  if (!sequenceFile->isComplete()) {
    return;
  }
  pagPlayer = nullptr;
  composition = nullptr;
}

std::string PAGDecoder::generateCacheKey() {
  if (composition == nullptr || !composition->isPAGFile() || composition->contentModified()) {
    return "";
  }
  auto filePath = static_cast<PAGFile*>(composition.get())->path();
  return filePath + "." + std::to_string(_width) + "x" + std::to_string(_height);
}
}  // namespace pag
