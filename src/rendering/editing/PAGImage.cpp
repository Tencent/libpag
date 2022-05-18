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

#include "base/utils/UniqueID.h"
#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/graphics/Recorder.h"
#include "rendering/utils/ApplyScaleMode.h"

namespace pag {
PAGImage::PAGImage(int width, int height)
    : _uniqueID(UniqueID::Next()), _width(width), _height(height) {
}

int PAGImage::scaleMode() const {
  std::lock_guard<std::mutex> autoLock(locker);
  return _scaleMode;
}

void PAGImage::setScaleMode(int mode) {
  std::lock_guard<std::mutex> autoLock(locker);
  _scaleMode = mode;
  _matrix.setIdentity();
  hasSetScaleMode = true;
}

Matrix PAGImage::matrix() const {
  std::lock_guard<std::mutex> autoLock(locker);
  return _matrix;
}

void PAGImage::setMatrix(const Matrix& matrix) {
  std::lock_guard<std::mutex> autoLock(locker);
  _scaleMode = PAGScaleMode::None;
  _matrix = matrix;
  hasSetScaleMode = true;
}

Matrix PAGImage::getContentMatrix(int defaultScaleMode, int contentWidth, int contentHeight) {
  auto scaleMode = hasSetScaleMode ? _scaleMode : defaultScaleMode;
  Matrix matrix = {};
  if (scaleMode != PAGScaleMode::None) {
    matrix = ApplyScaleMode(scaleMode, _width, _height, contentWidth, contentHeight);
  } else {
    matrix = _matrix;
  }
  return matrix;
}

void PAGImage::setOwner(PAGLayer* owner) {
  std::lock_guard<std::mutex> autoLock(locker);
  _owner = owner;
}

PAGLayer* PAGImage::getOwner() const {
  std::lock_guard<std::mutex> autoLock(locker);
  return _owner;
}
}  // namespace pag
