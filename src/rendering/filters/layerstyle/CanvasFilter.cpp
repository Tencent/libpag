/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "CanvasFilter.h"
#include <tgfx/core/Recorder.h>

namespace pag {

bool CanvasFilter::draw(Frame layerFrame, std::shared_ptr<tgfx::Image> source,
                        const tgfx::Point& filterScale, const tgfx::Matrix& matrix,
                        tgfx::Canvas* target) {
  return onDraw(layerFrame, source, filterScale, matrix, target);
}

std::shared_ptr<tgfx::Image> CanvasFilter::applyFilterEffect(Frame layerFrame,
                                                             std::shared_ptr<tgfx::Image> source,
                                                             const tgfx::Point& filterScale,
                                                             tgfx::Point* offset) {
  tgfx::Recorder recorder;
  auto canvas = recorder.beginRecording();
  if (!draw(layerFrame, source, filterScale, tgfx::Matrix::I(), canvas)) {
    return nullptr;
  }
  auto picture = recorder.finishRecordingAsPicture();
  auto bounds = picture->getBounds();
  auto pictureMatrix = tgfx::Matrix::MakeTrans(-bounds.x(), -bounds.y());
  auto image = tgfx::Image::MakeFrom(picture, source->width(), source->height(), &pictureMatrix);
  if (image == nullptr) {
    return nullptr;
  }
  if (offset) {
    offset->x = 2 * bounds.x();
    offset->y = 2 * bounds.y();
  }
  return image;
}
}  // namespace pag