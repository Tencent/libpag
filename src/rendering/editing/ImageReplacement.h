/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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
#include "rendering/caches/GraphicContent.h"

namespace pag {
class ImageReplacement : public Content {
 public:
  ImageReplacement(std::shared_ptr<PAGImage> pagImage, PAGScaleMode scaleMode,
                   ImageBytes* imageBytes);

  void measureBounds(tgfx::Rect* bounds) override;
  void draw(Recorder* recorder) override;
  tgfx::Point getScaleFactor() const;
  std::shared_ptr<PAGImage> getImage();
  bool setContentTime(int64_t time);
  std::shared_ptr<Graphic> getGraphic();

 private:
  std::shared_ptr<PAGImage> pagImage;
  PAGScaleMode defaultScaleMode = PAGScaleMode::LetterBox;
  int contentWidth = 0;
  int contentHeight = 0;
  Frame contentFrame = -1;
};
}  // namespace pag
