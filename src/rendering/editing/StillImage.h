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

#pragma once

#include "pag/pag.h"
#include "rendering/graphics/Graphic.h"

namespace pag {

class StillImage : public PAGImage {
 public:
  static std::shared_ptr<StillImage> FromPixelBuffer(std::shared_ptr<PixelBuffer> pixelBuffer);
  static std::shared_ptr<StillImage> FromImage(std::shared_ptr<Image> image);

  void measureBounds(Rect* bounds) override;
  void draw(Recorder* recorder) override;

 protected:
  Rect getContentSize() const override;

  std::shared_ptr<Image> getImage() const override {
    return image;
  }

 private:
  int width = 0;
  int height = 0;
  std::shared_ptr<Image> image = nullptr;
  std::shared_ptr<Graphic> graphic = nullptr;

  void reset(std::shared_ptr<Graphic> graphic);

  friend class PAGImage;
};
}  // namespace pag
