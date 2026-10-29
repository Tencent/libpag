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
//  license is distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "DisplacementMapLayerFilter.h"
#include <cmath>
#include <utility>
#include "rendering/filters/DisplacementMapFilter.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/ImageFilter.h"
#include "tgfx/core/PictureRecorder.h"
#include "tgfx/core/Point.h"
#include "tgfx/layers/Layer.h"

namespace pagx {

std::shared_ptr<DisplacementMapLayerFilter> DisplacementMapLayerFilter::Make(
    DisplacementMapSource useForHorizontalDisplacement, float maxHorizontalDisplacement,
    DisplacementMapSource useForVerticalDisplacement, float maxVerticalDisplacement,
    DisplacementMapBehavior displacementMapBehavior, bool edgeBehavior, bool expandOutput) {
  auto filter = std::shared_ptr<DisplacementMapLayerFilter>(new DisplacementMapLayerFilter());
  filter->_useForHorizontalDisplacement = useForHorizontalDisplacement;
  filter->_maxHorizontalDisplacement = maxHorizontalDisplacement;
  filter->_useForVerticalDisplacement = useForVerticalDisplacement;
  filter->_maxVerticalDisplacement = maxVerticalDisplacement;
  filter->_displacementMapBehavior = displacementMapBehavior;
  filter->_edgeBehavior = edgeBehavior;
  filter->_expandOutput = expandOutput;
  return filter;
}

void DisplacementMapLayerFilter::setTargetLayer(const std::weak_ptr<tgfx::Layer>& layer) {
  _targetLayer = layer;
}

void DisplacementMapLayerFilter::setMapLayer(const std::weak_ptr<tgfx::Layer>& layer) {
  _mapLayer = layer;
}

void DisplacementMapLayerFilter::setTargetSize(float width, float height) {
  _targetWidth = width;
  _targetHeight = height;
}

void DisplacementMapLayerFilter::setDisplacementSize(float width, float height) {
  _displacementWidth = width;
  _displacementHeight = height;
}

void DisplacementMapLayerFilter::setMaxHorizontalDisplacement(float value) {
  if (_maxHorizontalDisplacement == value) {
    return;
  }
  _maxHorizontalDisplacement = value;
  invalidateFilter();
}

void DisplacementMapLayerFilter::setMaxVerticalDisplacement(float value) {
  if (_maxVerticalDisplacement == value) {
    return;
  }
  _maxVerticalDisplacement = value;
  invalidateFilter();
}

void DisplacementMapLayerFilter::setEdgeBehavior(bool value) {
  if (_edgeBehavior == value) {
    return;
  }
  _edgeBehavior = value;
  invalidateFilter();
}

void DisplacementMapLayerFilter::setExpandOutput(bool value) {
  if (_expandOutput == value) {
    return;
  }
  _expandOutput = value;
  invalidateFilter();
}

void DisplacementMapLayerFilter::setDisplacementMapBehavior(DisplacementMapBehavior value) {
  if (_displacementMapBehavior == value) {
    return;
  }
  _displacementMapBehavior = value;
  invalidateFilter();
}

void DisplacementMapLayerFilter::setUseForHorizontalDisplacement(DisplacementMapSource value) {
  if (_useForHorizontalDisplacement == value) {
    return;
  }
  _useForHorizontalDisplacement = value;
  invalidateFilter();
}

void DisplacementMapLayerFilter::setUseForVerticalDisplacement(DisplacementMapSource value) {
  if (_useForVerticalDisplacement == value) {
    return;
  }
  _useForVerticalDisplacement = value;
  invalidateFilter();
}

void DisplacementMapLayerFilter::setEffectOpacity(float value) {
  if (_effectOpacity == value) {
    return;
  }
  _effectOpacity = value;
  invalidateFilter();
}

void DisplacementMapLayerFilter::setEnabled(bool value) {
  if (_enabled == value) {
    return;
  }
  _enabled = value;
  invalidateFilter();
}

std::shared_ptr<tgfx::Image> DisplacementMapLayerFilter::makeMapImage(float* displacementWidth,
                                                                      float* displacementHeight) {
  auto mapLayer = _mapLayer.lock();
  if (mapLayer == nullptr) {
    return nullptr;
  }
  auto bounds = mapLayer->getBounds();
  auto mapWidth = _displacementWidth > 0 ? _displacementWidth : bounds.width();
  auto mapHeight = _displacementHeight > 0 ? _displacementHeight : bounds.height();
  auto width = static_cast<int>(mapWidth);
  auto height = static_cast<int>(mapHeight);
  if (width <= 0 || height <= 0) {
    return nullptr;
  }
  auto mapPicture = mapLayer->makeContentPicture();
  if (mapPicture == nullptr) {
    return nullptr;
  }
  *displacementWidth = mapWidth;
  *displacementHeight = mapHeight;
  return tgfx::Image::MakeFrom(std::move(mapPicture), width, height);
}

std::shared_ptr<tgfx::Image> DisplacementMapLayerFilter::onFilterImage(
    std::shared_ptr<tgfx::Image> input, float scale, const tgfx::Rect&, tgfx::Point* offset,
    const tgfx::Point& inputOrigin) {
  auto targetLayer = _targetLayer.lock();
  auto mapLayer = _mapLayer.lock();
  if (input == nullptr || targetLayer == nullptr || mapLayer == nullptr || !_enabled) {
    return input;
  }
  // The legacy gates the effect off when both displacement selectors are constants
  // (DisplacementMapEffect::visibleAt): a constant source displaces everything uniformly, which
  // the legacy treats as a no-op rather than a uniform shift.
  auto isConstSource = [](DisplacementMapSource source) {
    return source == DisplacementMapSource::Full || source == DisplacementMapSource::Half ||
           source == DisplacementMapSource::Off;
  };
  if (isConstSource(_useForHorizontalDisplacement) && isConstSource(_useForVerticalDisplacement)) {
    return input;
  }

  // The legacy shader consumes an UNCLIPPED texture of the target's full content: displacement
  // sampling of a visible pixel routinely reads content outside the surface clip, and the map
  // placement math needs the content's true layer-local position. The framework input is
  // clipped to the visible region and anchored elsewhere, so the target's content is re-rendered
  // from its own layer subtree here, fresh each frame. Layer filters chained before this one are
  // not part of the re-render (legacy migration never chains displacement maps after other
  // effects on the same layer).
  auto picture = targetLayer->makeContentPicture();
  if (picture == nullptr) {
    return input;
  }
  auto contentBounds = picture->getBounds();
  auto pixelWidth = static_cast<int>(ceilf(contentBounds.width() * scale));
  auto pixelHeight = static_cast<int>(ceilf(contentBounds.height() * scale));
  if (pixelWidth <= 0 || pixelHeight <= 0) {
    return input;
  }
  auto rasterMatrix = tgfx::Matrix::MakeScale(scale, scale);
  rasterMatrix.preTranslate(-contentBounds.left, -contentBounds.top);
  auto contentImage = tgfx::Image::MakeFrom(picture, pixelWidth, pixelHeight, &rasterMatrix);
  if (contentImage == nullptr) {
    return input;
  }
  float displacementWidth = 0.0f;
  float displacementHeight = 0.0f;
  auto referenceImage = makeMapImage(&displacementWidth, &displacementHeight);
  if (referenceImage == nullptr) {
    return input;
  }
  // All shader inputs stay in layer-local units, exactly like the legacy renderer; the raster
  // scale above only sharpens the texture, so the UV math is scale-independent.
  auto size =
      tgfx::Size::Make(_targetWidth > 0 ? _targetWidth : targetLayer->getBounds().width(),
                       _targetHeight > 0 ? _targetHeight : targetLayer->getBounds().height());
  auto displacementSize = tgfx::Size::Make(displacementWidth, displacementHeight);
  auto layerMatrix = targetLayer->matrix();
  auto filter = std::make_shared<pag::DisplacementMapFilter>(
      nullptr, static_cast<pag::DisplacementMapSource>(_useForHorizontalDisplacement),
      _maxHorizontalDisplacement,
      static_cast<pag::DisplacementMapSource>(_useForVerticalDisplacement),
      _maxVerticalDisplacement, static_cast<pag::DisplacementMapBehavior>(_displacementMapBehavior),
      _edgeBehavior, _expandOutput, _effectOpacity, layerMatrix, size, displacementSize,
      contentBounds, std::move(referenceImage));
  tgfx::Point filterOffset = {};
  auto result =
      contentImage->makeWithFilter(tgfx::ImageFilter::Runtime(std::move(filter)), &filterOffset);
  if (result == nullptr) {
    return input;
  }
  // Anchor the re-rendered output back onto the framework input: makeWithFilter reports the
  // displacement expansion in texture pixels, and the framework draws the returned image at
  // (inputOrigin + offset / scale) in layer space, so the pixel-space difference between the
  // content origin and the input origin is scaled back up here.
  if (offset != nullptr) {
    offset->x = filterOffset.x + scale * (contentBounds.left - inputOrigin.x);
    offset->y = filterOffset.y + scale * (contentBounds.top - inputOrigin.y);
  }
  return result;
}

tgfx::Rect DisplacementMapLayerFilter::filterBounds(const tgfx::Rect& srcRect, float,
                                                    tgfx::MapDirection direction) {
  if (srcRect.isEmpty() || direction == tgfx::MapDirection::Reverse) {
    return srcRect;
  }
  // The legacy RuntimeFilter bounds expansion (expandOutput outsets by the max displacements
  // scaled to the content bounds) is self-contained given the params and a null map image.
  auto filter = std::make_shared<pag::DisplacementMapFilter>(
      nullptr, static_cast<pag::DisplacementMapSource>(_useForHorizontalDisplacement),
      _maxHorizontalDisplacement,
      static_cast<pag::DisplacementMapSource>(_useForVerticalDisplacement),
      _maxVerticalDisplacement, static_cast<pag::DisplacementMapBehavior>(_displacementMapBehavior),
      _edgeBehavior, _expandOutput, 1.0f, tgfx::Matrix::I(),
      tgfx::Size::Make(srcRect.width(), srcRect.height()),
      tgfx::Size::Make(srcRect.width(), srcRect.height()), srcRect, nullptr);
  return static_cast<const pag::RuntimeFilter*>(filter.get())
      ->filterBounds(srcRect, tgfx::MapDirection::Forward);
}

}  // namespace pagx
