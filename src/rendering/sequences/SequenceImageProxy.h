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

#include "pag/file.h"
#include "rendering/graphics/ImageProxy.h"
#include "rendering/sequences/SequenceInfo.h"

namespace pag {
class SequenceImageProxy : public ImageProxy {
 public:
  SequenceImageProxy(std::shared_ptr<SequenceInfo> sequence, Frame targetFrame);

  int width() const override {
    return sequence->width();
  }

  int height() const override {
    return sequence->height();
  }

  bool isTemporary() const override {
    return !sequence->staticContent();
  }

  void prepareImage(RenderCache* cache) const override;

  std::shared_ptr<tgfx::Image> getImage(RenderCache* cache) const override;

 protected:
  std::shared_ptr<tgfx::Image> makeImage(RenderCache* cache) const override;

 private:
  std::shared_ptr<SequenceInfo> sequence = nullptr;
  Frame targetFrame = 0;
};
}  // namespace pag
