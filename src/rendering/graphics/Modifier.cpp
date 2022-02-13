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

#include "Modifier.h"
#include "Graphic.h"
#include "base/utils/MatrixUtil.h"
#include "base/utils/UniqueID.h"
#include "core/Blend.h"
#include "gpu/Surface.h"
#include "rendering/utils/SurfaceUtil.h"

namespace pag {
static constexpr std::pair<Enum, Blend> kBlendModeMap[] = {
    {BlendMode::Normal, Blend::SrcOver},
    {BlendMode::Multiply, Blend::Multiply},
    {BlendMode::Screen, Blend::Screen},
    {BlendMode::Overlay, Blend::Overlay},
    {BlendMode::Darken, Blend::Darken},
    {BlendMode::Lighten, Blend::Lighten},
    {BlendMode::ColorDodge, Blend::ColorDodge},
    {BlendMode::ColorBurn, Blend::ColorBurn},
    {BlendMode::HardLight, Blend::HardLight},
    {BlendMode::SoftLight, Blend::SoftLight},
    {BlendMode::Difference, Blend::Difference},
    {BlendMode::Exclusion, Blend::Exclusion},
    {BlendMode::Hue, Blend::Hue},
    {BlendMode::Saturation, Blend::Saturation},
    {BlendMode::Color, Blend::Color},
    {BlendMode::Luminosity, Blend::Luminosity},
    {BlendMode::Add, Blend::Plus}};

Blend ToBlend(Enum blendMode) {
  for (const auto& pair : kBlendModeMap) {
    if (pair.first == blendMode) {
      return pair.second;
    }
  }
  return Blend::SrcOver;
}

class BlendModifier : public Modifier {
 public:
  BlendModifier(Opacity alpha, Enum blendMode) : alpha(alpha), blendMode(blendMode) {
  }

  ID type() const override {
    static const auto TypeID = UniqueID::Next();
    return TypeID;
  }

  bool isEmpty() const override {
    return alpha == Transparent;
  }

  bool hitTest(RenderCache* cache, float x, float y) const override;

  void prepare(RenderCache*) const override {
  }

  void applyToBounds(Rect*) const override{};

  bool applyToPath(Path*) const override {
    return alpha == Opaque;  // blendMode 只影响颜色，不改变形状或透明度，不影响 getPath 结果。
  }

  void applyToGraphic(Canvas* canvas, RenderCache* cache,
                      std::shared_ptr<Graphic> graphic) const override;

  std::shared_ptr<Modifier> mergeWith(const Modifier* modifier) const override;

 private:
  Opacity alpha = Opaque;
  Enum blendMode = BlendMode::Normal;
};

class ClipModifier : public Modifier {
 public:
  explicit ClipModifier(Path clip) : clip(std::move(clip)) {
  }

  ID type() const override {
    static const auto TypeID = UniqueID::Next();
    return TypeID;
  }

  bool isEmpty() const override {
    return clip.isEmpty();
  }

  bool hitTest(RenderCache*, float x, float y) const override {
    return clip.contains(x, y);
  }

  void prepare(RenderCache*) const override {
  }

  void applyToBounds(Rect* bounds) const override;

  bool applyToPath(Path* path) const override;

  void applyToGraphic(Canvas* canvas, RenderCache* cache,
                      std::shared_ptr<Graphic> graphic) const override;

  std::shared_ptr<Modifier> mergeWith(const Modifier* modifier) const override;

 private:
  Path clip = {};
};

class MaskModifier : public Modifier {
 public:
  MaskModifier(std::shared_ptr<Graphic> mask, bool inverted)
      : mask(std::move(mask)), inverted(inverted) {
  }

  ID type() const override {
    static const auto TypeID = UniqueID::Next();
    return TypeID;
  }

  bool isEmpty() const override {
    return mask == nullptr;
  }

  bool hitTest(RenderCache* cache, float x, float y) const override;

  void prepare(RenderCache* cache) const override;

  void applyToBounds(Rect* bounds) const override;

  bool applyToPath(Path*) const override {
    return false;
  }

  void applyToGraphic(Canvas* canvas, RenderCache* cache,
                      std::shared_ptr<Graphic> graphic) const override;

  std::shared_ptr<Modifier> mergeWith(const Modifier*) const override {
    return nullptr;
  }

 private:
  // 可能是 nullptr
  std::shared_ptr<Graphic> mask = nullptr;
  bool inverted = false;
};

//================================================================================

std::shared_ptr<Modifier> Modifier::MakeBlend(Opacity alpha, Enum blendMode) {
  if (alpha == Opaque && blendMode == BlendMode::Normal) {
    return nullptr;
  }
  return std::make_shared<BlendModifier>(alpha, blendMode);
}

std::shared_ptr<Modifier> Modifier::MakeClip(const Path& clip) {
  if (clip.isEmpty() && clip.isInverseFillType()) {
    // is full.
    return nullptr;
  }
  return std::make_shared<ClipModifier>(clip);
}

std::shared_ptr<Modifier> Modifier::MakeMask(std::shared_ptr<Graphic> graphic, bool inverted) {
  if (graphic == nullptr && inverted) {
    // 返回空，表示保留目标对象的全部内容。
    return nullptr;
  }
  Path clipPath = {};
  if (graphic && graphic->getPath(&clipPath)) {
    if (inverted) {
      clipPath.toggleInverseFillType();
    }
    return Modifier::MakeClip(clipPath);
  }
  return std::make_shared<MaskModifier>(graphic, inverted);
}

//================================================================================

bool BlendModifier::hitTest(RenderCache*, float, float) const {
  return alpha != Transparent;
}

void BlendModifier::applyToGraphic(Canvas* canvas, RenderCache* cache,
                                   std::shared_ptr<Graphic> graphic) const {
  canvas->save();
  auto newAlpha = OpacityConcat(canvas->getAlpha(), alpha);
  canvas->setAlpha(newAlpha);
  if (blendMode != BlendMode::Normal) {
    canvas->setBlendMode(ToBlend(blendMode));
  }
  graphic->draw(canvas, cache);
  canvas->restore();
}

std::shared_ptr<Modifier> BlendModifier::mergeWith(const Modifier* modifier) const {
  auto target = static_cast<const BlendModifier*>(modifier);
  if (target->blendMode != BlendMode::Normal && blendMode != BlendMode::Normal) {
    return nullptr;
  }
  auto newBlendMode = blendMode != BlendMode::Normal ? blendMode : target->blendMode;
  auto newAlpha = OpacityConcat(alpha, target->alpha);
  return std::make_shared<BlendModifier>(newAlpha, newBlendMode);
}

void ClipModifier::applyToBounds(Rect* bounds) const {
  Path boundsPath = {};
  boundsPath.addRect(*bounds);
  boundsPath.addPath(clip, PathOp::Intersect);
  *bounds = boundsPath.getBounds();
}

bool ClipModifier::applyToPath(Path* path) const {
  path->addPath(clip, PathOp::Intersect);
  return true;
}

void ClipModifier::applyToGraphic(Canvas* canvas, RenderCache* cache,
                                  std::shared_ptr<Graphic> graphic) const {
  canvas->save();
  canvas->clipPath(clip);
  graphic->draw(canvas, cache);
  canvas->restore();
}

std::shared_ptr<Modifier> ClipModifier::mergeWith(const Modifier* modifier) const {
  auto target = static_cast<const ClipModifier*>(modifier);
  auto newClip = clip;
  newClip.addPath(target->clip, PathOp::Intersect);
  return std::make_shared<ClipModifier>(newClip);
}

bool MaskModifier::hitTest(RenderCache* cache, float x, float y) const {
  if (mask == nullptr) {
    return false;
  }
  auto result = mask->hitTest(cache, x, y);
  return result != inverted;
}

void MaskModifier::prepare(RenderCache* cache) const {
  if (mask) {
    mask->prepare(cache);
  }
}

void MaskModifier::applyToBounds(Rect* bounds) const {
  if (mask == nullptr) {
    bounds->setEmpty();
    return;
  }
  if (!inverted) {
    Rect clipBounds = Rect::MakeEmpty();
    mask->measureBounds(&clipBounds);
    if (!bounds->intersect(clipBounds)) {
      bounds->setEmpty();
    }
  }
}

void MaskModifier::applyToGraphic(Canvas* canvas, RenderCache* cache,
                                  std::shared_ptr<Graphic> graphic) const {
  if (mask == nullptr) {
    return;
  }
  Rect bounds = Rect::MakeEmpty();
  graphic->measureBounds(&bounds);
  applyToBounds(&bounds);
  if (bounds.isEmpty()) {
    // 与遮罩不相交，直接跳过绘制。
    return;
  }
  auto contentSurface = SurfaceUtil::MakeContentSurface(canvas, bounds, FLT_MAX);
  if (contentSurface == nullptr) {
    return;
  }
  auto contentCanvas = contentSurface->getCanvas();
  auto contentMatrix = contentCanvas->getMatrix();
  graphic->draw(contentCanvas, cache);
  auto maskSurface = Surface::Make(contentSurface->getContext(), contentSurface->width(),
                                   contentSurface->height(), true);
  if (maskSurface == nullptr) {
    maskSurface = Surface::Make(contentSurface->getContext(), contentSurface->width(),
                                contentSurface->height());
  }
  if (maskSurface == nullptr) {
    return;
  }
  auto maskCanvas = maskSurface->getCanvas();
  maskCanvas->setMatrix(contentMatrix);
  mask->draw(maskCanvas, cache);
  auto maskTexture = maskSurface->getTexture();
  auto texture = contentSurface->getTexture();
  auto scaleFactor = GetMaxScaleFactor(contentMatrix);
  auto matrix = Matrix::MakeScale(1.0f / scaleFactor);
  matrix.postTranslate(bounds.x(), bounds.y());
  canvas->save();
  canvas->concat(matrix);
  canvas->drawTexture(texture.get(), maskTexture.get(), inverted);
  canvas->restore();
}
}  // namespace pag
