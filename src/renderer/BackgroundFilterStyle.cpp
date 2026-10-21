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

#include "renderer/BackgroundFilterStyle.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/MaskFilter.h"
#include "tgfx/core/Shader.h"

namespace pagx {
BackdropStyle::BackdropStyle(std::vector<std::shared_ptr<tgfx::LayerFilter>> filters,
                             tgfx::Path clip)
    : filters(std::move(filters)), clip(std::move(clip)) {
}
uint32_t BackdropStyle::extraSourceType() const {
  return static_cast<uint32_t>(tgfx::LayerStyleExtraSourceType::Background) |
         (clip.isEmpty() ? static_cast<uint32_t>(tgfx::LayerStyleExtraSourceType::Contour) : 0);
}
tgfx::Rect BackdropStyle::filterSourceBounds(const tgfx::Rect& bounds, float scale) {
  auto result = bounds;
  auto area = clip.getBounds();
  area.scale(scale, scale);
  result.join(area);
  return result;
}
tgfx::Rect BackdropStyle::filterBounds(const tgfx::Rect& bounds, float scale) {
  return filterSourceBounds(bounds, scale);
}
void BackdropStyle::attachToLayer(tgfx::Layer* layer) {
  for (auto& filter : filters) replaceChildProperty(filter.get(), nullptr);
  LayerStyle::attachToLayer(layer);
  for (auto& filter : filters) replaceChildProperty(nullptr, filter.get());
}
void BackdropStyle::detachFromLayer(tgfx::Layer* layer) {
  for (auto& filter : filters) replaceChildProperty(filter.get(), nullptr);
  LayerStyle::detachFromLayer(layer);
  for (auto& filter : filters) replaceChildProperty(nullptr, filter.get());
}
tgfx::Rect BackdropStyle::filterBackgroundSharp(const tgfx::Rect& bounds, float scale) {
  auto result = bounds;
  for (auto it = filters.rbegin(); it != filters.rend(); ++it)
    result = (*it)->filterBounds(result, scale, tgfx::MapDirection::Reverse);
  // TGFX schedules background capture from nonzero sampling outsets. Colour-only
  // chains still depend on the backdrop; reserve one pixel for capture and AA.
  result.outset(1, 1);
  return result;
}
void BackdropStyle::onDraw(tgfx::Canvas* canvas, const tgfx::LayerStyleInput& input, float,
                           tgfx::BlendMode mode) {
  auto* background = input.findExtraSource(tgfx::StyleInputSource::Type::Base);
  auto* contour = input.findExtraSource(tgfx::StyleInputSource::Type::Contour);
  if (!background || !background->image()) return;
  if (clip.isEmpty() && (!contour || !contour->image())) return;
  auto image = background->image();
  auto offset = background->imageOffset();
  for (auto& filter : filters) {
    tgfx::Point delta = {};
    auto bounds = tgfx::Rect::MakeWH(image->width(), image->height());
    image = filter->filterImage(image, input.contentScale, bounds, &delta);
    if (!image) return;
    offset.x += delta.x;
    offset.y += delta.y;
  }
  tgfx::Paint paint;
  tgfx::AutoCanvasRestore restore(canvas);
  if (!clip.isEmpty()) {
    auto path = clip;
    path.transform(tgfx::Matrix::MakeScale(input.contentScale));
    path.transform(tgfx::Matrix::MakeTrans(-input.contentOffset.x, -input.contentOffset.y));
    canvas->clipPath(path);
  } else {
    auto shader = tgfx::Shader::MakeImageShader(contour->image(), tgfx::TileMode::Decal,
                                                tgfx::TileMode::Decal);
    auto origin = contour->imageOffset();
    shader = shader->makeWithMatrix(tgfx::Matrix::MakeTrans(origin.x, origin.y));
    paint.setMaskFilter(tgfx::MaskFilter::MakeShader(shader, false));
  }
  // Composite the filtered backdrop before the owning layer's normal foreground.
  // Source-over preserves the unfiltered backdrop under opacity() in the chain.
  paint.setBlendMode(mode);
  canvas->drawImage(image, offset.x, offset.y, &paint);
}
}  // namespace pagx
