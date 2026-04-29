/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#include "pagx/utils/VerifyUtils.h"
#include <cmath>

namespace pagx {

bool HasLayerOnlyFeatures(const Layer* layer) {
  if (!layer->id.empty() || !layer->name.empty()) {
    return true;
  }
  if (!layer->visible) {
    return true;
  }
  if (layer->alpha != 1.0f) {
    return true;
  }
  if (layer->blendMode != BlendMode::Normal) {
    return true;
  }
  if (!layer->matrix3D.isIdentity()) {
    return true;
  }
  if (layer->preserve3D) {
    return true;
  }
  if (!layer->antiAlias) {
    return true;
  }
  if (!layer->groupOpacity) {
    return true;
  }
  if (!layer->passThroughBackground) {
    return true;
  }
  if (layer->hasScrollRect) {
    return true;
  }
  if (layer->clipToBounds) {
    return true;
  }
  if (layer->mask != nullptr) {
    return true;
  }
  if (layer->maskType != MaskType::Alpha) {
    return true;
  }
  if (layer->composition != nullptr) {
    return true;
  }
  if (!layer->styles.empty()) {
    return true;
  }
  if (!layer->filters.empty()) {
    return true;
  }
  if (layer->layout != LayoutMode::None) {
    return true;
  }
  if (layer->gap != 0.0f) {
    return true;
  }
  if (layer->flex != 0.0f) {
    return true;
  }
  if (layer->alignment != Alignment::Stretch) {
    return true;
  }
  if (layer->arrangement != Arrangement::Start) {
    return true;
  }
  if (!layer->includeInLayout) {
    return true;
  }
  return false;
}

bool IsLayerShell(const Layer* layer) {
  if (HasLayerOnlyFeatures(layer)) {
    return false;
  }
  if (layer->x != 0.0f || layer->y != 0.0f) {
    return false;
  }
  if (!layer->matrix.isIdentity()) {
    return false;
  }
  if (!std::isnan(layer->width) || !std::isnan(layer->height)) {
    return false;
  }
  if (!std::isnan(layer->percentWidth) || !std::isnan(layer->percentHeight)) {
    return false;
  }
  if (!layer->padding.isZero()) {
    return false;
  }
  if (!std::isnan(layer->left) || !std::isnan(layer->right) || !std::isnan(layer->top) ||
      !std::isnan(layer->bottom) || !std::isnan(layer->centerX) || !std::isnan(layer->centerY)) {
    return false;
  }
  return true;
}

bool HasPainter(const std::vector<Element*>& elements) {
  for (auto* el : elements) {
    if (IsPainter(el->nodeType())) {
      return true;
    }
  }
  return false;
}

}  // namespace pagx
