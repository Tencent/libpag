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

#include "base/utils/Verify.h"
#include "pag/file.h"

namespace pag {
DisplacementMapEffect::~DisplacementMapEffect() {
    delete useForHorizontalDisplacement;
    delete maxHorizontalDisplacement;
    delete useForVerticalDisplacement;
    delete maxVerticalDisplacement;
    delete displacementMapBehavior;
    delete edgeBehavior;
    delete expandOutput;
}

bool DisplacementMapEffect::visibleAt(Frame layerFrame) const {
    if (displacementMapLayer == nullptr) {
        return false;
    }
    // DisplacementMap特效只支持视频序列帧或者图片序列帧。
    if (displacementMapLayer->type() == LayerType::PreCompose) {
        auto preComposeLayer = static_cast<PreComposeLayer*>(displacementMapLayer);
        auto composition = preComposeLayer->composition;
        if (composition->type() == CompositionType::Video ||
                composition->type() == CompositionType::Bitmap) {
            auto mapContentFrame = layerFrame - displacementMapLayer->startTime;
            if (mapContentFrame < 0 || mapContentFrame >= displacementMapLayer->duration) {
                return false;
            }
            return maxHorizontalDisplacement->getValueAt(layerFrame) != 0 ||
                   maxVerticalDisplacement->getValueAt(layerFrame) != 0;
        }
    }
    return false;
}

void DisplacementMapEffect::transformBounds(Rect*, const Point&, Frame) const {
}

void DisplacementMapEffect::excludeVaryingRanges(std::vector<pag::TimeRange>* timeRanges) const {
    Effect::excludeVaryingRanges(timeRanges);
    useForHorizontalDisplacement->excludeVaryingRanges(timeRanges);
    maxHorizontalDisplacement->excludeVaryingRanges(timeRanges);
    useForVerticalDisplacement->excludeVaryingRanges(timeRanges);
    maxVerticalDisplacement->excludeVaryingRanges(timeRanges);
    displacementMapBehavior->excludeVaryingRanges(timeRanges);
    edgeBehavior->excludeVaryingRanges(timeRanges);
    expandOutput->excludeVaryingRanges(timeRanges);
}

bool DisplacementMapEffect::verify() const {
    if (!Effect::verify()) {
        VerifyFailed();
        return false;
    }
    VerifyAndReturn(useForHorizontalDisplacement != nullptr && maxHorizontalDisplacement != nullptr &&
                    useForVerticalDisplacement != nullptr && maxVerticalDisplacement != nullptr &&
                    displacementMapBehavior != nullptr && edgeBehavior != nullptr &&
                    expandOutput != nullptr);
}
}  // namespace pag
