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

#include "ImageReplacement.h"
#include "base/utils/MatrixUtil.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/editing/PAGImageHolder.h"
#include "rendering/graphics/Recorder.h"

namespace pag {
ImageReplacement::ImageReplacement(ImageLayer* imageLayer, PAGImageHolder* imageHolder,
                                   int editableIndex)
    : imageHolder(imageHolder), editableIndex(editableIndex) {
    auto imageFillRule = imageLayer->imageFillRule;
    defaultScaleMode = imageFillRule ? imageFillRule->scaleMode : PAGScaleMode::LetterBox;
    contentWidth = imageLayer->imageBytes->width;
    contentHeight = imageLayer->imageBytes->height;
}

void ImageReplacement::measureBounds(Rect* bounds) {
    Rect contentBounds = {};
    auto pagImage = imageHolder->getImage(editableIndex);
    pagImage->measureBounds(&contentBounds);
    auto contentMatrix = pagImage->getContentMatrix(defaultScaleMode, contentWidth, contentHeight);
    contentMatrix.mapRect(&contentBounds);
    bounds->setXYWH(0, 0, contentWidth, contentHeight);
    if (!bounds->intersect(contentBounds)) {
        bounds->setEmpty();
    }
}

void ImageReplacement::draw(Recorder* recorder) {
    recorder->saveClip(0, 0, static_cast<float>(contentWidth), static_cast<float>(contentHeight));
    auto pagImage = imageHolder->getImage(editableIndex);
    auto contentMatrix = pagImage->getContentMatrix(defaultScaleMode, contentWidth, contentHeight);
    recorder->concat(contentMatrix);
    pagImage->draw(recorder);
    recorder->restore();
}

Point ImageReplacement::getScaleFactor() const {
    // TODO((domrjchen):
    // 当PAGImage的适配模式或者matrix发生改变时，需要补充一个通知机制让上层重置scaleFactor。
    auto pagImage = imageHolder->getImage(editableIndex);
    auto contentMatrix = pagImage->getContentMatrix(defaultScaleMode, contentWidth, contentHeight);
    return GetScaleFactor(contentMatrix);
}
}  // namespace pag