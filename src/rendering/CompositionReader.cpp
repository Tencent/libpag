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

#include "CompositionReader.h"

namespace pag {
std::shared_ptr<CompositionReader> CompositionReader::Make(int width, int height, void* sharedContext) {
  if (width <= 0 || height <= 0) {
    return nullptr;
  }
  auto drawable = BitmapDrawable::Make(width, height, sharedContext);
  if (drawable == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<CompositionReader>(new CompositionReader(drawable));
}

CompositionReader::CompositionReader(std::shared_ptr<BitmapDrawable> bitmapDrawable)
    : drawable(std::move(bitmapDrawable)) {
  pagPlayer = new PAGPlayer();
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

bool CompositionReader::readFrame(double progress, std::shared_ptr<BitmapBuffer> bitmap) {
  std::lock_guard<std::mutex> autoLock(locker);
  drawable->setBitmap(std::move(bitmap));
  return renderFrame(progress);
}

bool CompositionReader::renderFrame(double progress) {
  pagPlayer->setProgress(progress);
  pagPlayer->flush();
  return drawable->isPixelCopied();
}
}  // namespace pag
