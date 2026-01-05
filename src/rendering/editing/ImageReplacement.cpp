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

#include "ImageReplacement.h"
#include "base/utils/MatrixUtil.h"
#include "base/utils/TGFXCast.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/graphics/Recorder.h"

namespace pag {
ImageReplacement::ImageReplacement(std::shared_ptr<PAGImage> pagImage, PAGScaleMode scaleMode,
                                   ImageBytes* imageBytes)
    : pagImage(std::move(pagImage)), defaultScaleMode(scaleMode), contentWidth(imageBytes->width),
      contentHeight(imageBytes->height) {
}

void ImageReplacement::measureBounds(tgfx::Rect* bounds) {
  tgfx::Rect contentBounds = {};
  auto graphic = pagImage->getGraphic(contentFrame);
  graphic->measureBounds(&contentBounds);
  auto contentMatrix = pagImage->getContentMatrix(defaultScaleMode, contentWidth, contentHeight);
  ToTGFX(contentMatrix).mapRect(&contentBounds);
  bounds->setXYWH(0, 0, static_cast<float>(contentWidth), static_cast<float>(contentHeight));
  if (!bounds->intersect(contentBounds)) {
    bounds->setEmpty();
  }
}

void ImageReplacement::draw(Recorder* recorder) {
  recorder->saveClip(0, 0, static_cast<float>(contentWidth), static_cast<float>(contentHeight));
  auto contentMatrix = pagImage->getContentMatrix(defaultScaleMode, contentWidth, contentHeight);
  recorder->concat(ToTGFX(contentMatrix));
  recorder->drawGraphic(pagImage->getGraphic(contentFrame));
  recorder->restore();
}

tgfx::Point ImageReplacement::getScaleFactor() const {
  // TODO((domrjchen):
  // 当PAGImage的适配模式或者matrix发生改变时，需要补充一个通知机制让上层重置scaleFactor。
  auto contentMatrix = pagImage->getContentMatrix(defaultScaleMode, contentWidth, contentHeight);
  return GetScaleFactor(ToTGFX(contentMatrix));
}

std::shared_ptr<PAGImage> ImageReplacement::getImage() {
  return pagImage;
}

bool ImageReplacement::setContentTime(int64_t time) {
  auto currentFrame = pagImage->getContentFrame(time);
  if (currentFrame == contentFrame) {
    return false;
  }
  contentFrame = currentFrame;
  return true;
}

std::shared_ptr<Graphic> ImageReplacement::getGraphic() {
  return pagImage->getGraphic(contentFrame);
}

}  // namespace pag
