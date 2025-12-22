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

#pragma once

#include "pag/pag.h"
#include "rendering/drawables/BitmapDrawable.h"
#include "rendering/utils/BitmapBuffer.h"

namespace pag {
class CompositionReader {
 public:
  static std::shared_ptr<CompositionReader> Make(int width, int height,
                                                 void* sharedContext = nullptr);

  ~CompositionReader();

  int width() const {
    return drawable->width();
  }

  int height() const {
    return drawable->height();
  }

  std::shared_ptr<PAGComposition> getComposition();

  void setComposition(std::shared_ptr<PAGComposition> composition);

  bool readFrame(double progress, std::shared_ptr<BitmapBuffer> bitmap);

 private:
  std::mutex locker = {};
  PAGPlayer* pagPlayer = nullptr;
  std::shared_ptr<BitmapDrawable> drawable = nullptr;

  CompositionReader(std::shared_ptr<BitmapDrawable> bitmapDrawable);

  bool renderFrame(double progress);
};
}  // namespace pag
