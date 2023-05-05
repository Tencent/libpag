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

#include "CompositionReader.h"

namespace pag {
std::shared_ptr<CompositionReader> CompositionReader::Make(int width, int height) {
  if (width <= 0 || height <= 0) {
    return nullptr;
  }
  return std::shared_ptr<CompositionReader>(new CompositionReader(width, height));
}

CompositionReader::CompositionReader(int width, int height) {
  pagPlayer = new PAGPlayer();
  drawable = RasterDrawable::Make(width, height);
  auto pagSurface = PAGSurface::MakeFrom(drawable);
  pagPlayer->setSurface(pagSurface);
}

CompositionReader::~CompositionReader() {
  delete pagPlayer;
}

std::shared_ptr<PAGComposition> CompositionReader::getComposition() {
  std::lock_guard<std::mutex> autoLock(locker);
  return pagPlayer->getComposition();
}

void CompositionReader::setComposition(std::shared_ptr<PAGComposition> composition) {
  std::lock_guard<std::mutex> autoLock(locker);
  pagPlayer->setComposition(std::move(composition));
}

bool CompositionReader::readFrame(double progress, const tgfx::ImageInfo& info, void* pixels) {
  std::lock_guard<std::mutex> autoLock(locker);
  drawable->setPixelBuffer(info, pixels);
  return renderFrame(progress);
}

bool CompositionReader::readFrame(double progress, tgfx::HardwareBufferRef hardwareBuffer) {
  std::lock_guard<std::mutex> autoLock(locker);
  drawable->setHardwareBuffer(hardwareBuffer);
  return renderFrame(progress);
}

bool CompositionReader::renderFrame(double progress) {
  pagPlayer->setProgress(progress);
  pagPlayer->flush();
  return drawable->isPixelCopied();
}
}  // namespace pag
