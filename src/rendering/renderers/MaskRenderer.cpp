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

#include "MaskRenderer.h"
#include "pag/file.h"
#include "rendering/utils/PathUtil.h"
#include "tgfx/core/PathEffect.h"

namespace pag {
tgfx::PathOp ToPathOp(Enum maskMode) {
  switch (maskMode) {
    case MaskMode::Subtract:
      return tgfx::PathOp::Difference;
    case MaskMode::Intersect:
      return tgfx::PathOp::Intersect;
    case MaskMode::Difference:
      return tgfx::PathOp::XOR;
    case MaskMode::Darken:  // without the opacity blending, haven't supported it
      return tgfx::PathOp::Intersect;
    default:
      return tgfx::PathOp::Union;
  }
}

static void ExpandPath(tgfx::Path* path, float expansion) {
  if (expansion == 0) {
    return;
  }
  auto strokePath = *path;
  auto effect = tgfx::PathEffect::MakeStroke(tgfx::Stroke(fabsf(expansion) * 2));
  if (effect) {
    effect->applyTo(&strokePath);
  }
  if (expansion < 0) {
    path->addPath(strokePath, tgfx::PathOp::Difference);
  } else {
    path->addPath(strokePath, tgfx::PathOp::Union);
  }
}

void RenderMasks(tgfx::Path* maskContent, const std::vector<MaskData*>& masks, Frame layerFrame) {
  bool isFirst = true;
  for (auto& mask : masks) {
    auto path = mask->maskPath->getValueAt(layerFrame);
    if (path == nullptr || !path->isClosed() || mask->maskMode == MaskMode::None) {
      continue;
    }
    auto maskPath = ToPath(*path);
    auto expansion = mask->maskExpansion->getValueAt(layerFrame);
    ExpandPath(&maskPath, expansion);
    auto inverted = mask->inverted;
    if (isFirst) {
      if (mask->maskMode == MaskMode::Subtract) {
        inverted = !inverted;
      }
    }
    if (inverted) {
      maskPath.toggleInverseFillType();
    }
    if (isFirst) {
      isFirst = false;
      *maskContent = maskPath;
    } else {
      maskContent->addPath(maskPath, ToPathOp(mask->maskMode));
    }
  }
}
}  // namespace pag
