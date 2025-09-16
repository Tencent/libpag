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

#include "Modifier.h"
#include "Graphic.h"
#include "base/utils/MatrixUtil.h"
#include "base/utils/TGFXCast.h"
#include "base/utils/UniqueID.h"
#include "rendering/utils/SurfaceUtil.h"
#include "tgfx/core/BlendMode.h"
#include "tgfx/core/Surface.h"

namespace pag {
class BlendModifier : public Modifier {
 public:
  BlendModifier(float alpha, tgfx::BlendMode blendMode) : alpha(alpha), blendMode(blendMode) {
  }

  ID type() const override {
    static const auto TypeID = UniqueID::Next();
    return TypeID;
  }

  bool isEmpty() const override {
    return alpha == 0.0f;
  }

  bool hitTest(RenderCache* cache, float x, float y) const override;

  void prepare(RenderCache*) const override {
  }

  void applyToBounds(tgfx::Rect*) const override{};

  bool applyToPath(tgfx::Path*) const override {
    return alpha == 1.0f;  // blendMode 只影响颜色，不改变形状或透明度，不影响 getPath 结果。
  }

  void applyToGraphic(Canvas* canvas, std::shared_ptr<Graphic> graphic) const override;

  std::shared_ptr<Modifier> mergeWith(const Modifier* modifier) const override;

 private:
  float alpha = 1.0f;
  tgfx::BlendMode blendMode = tgfx::BlendMode::SrcOver;
};

class ClipModifier : public Modifier {
 public:
  explicit ClipModifier(tgfx::Path clip) : clip(std::move(clip)) {
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

  void applyToBounds(tgfx::Rect* bounds) const override;

  bool applyToPath(tgfx::Path* path) const override;

  void applyToGraphic(Canvas* canvas, std::shared_ptr<Graphic> graphic) const override;

  std::shared_ptr<Modifier> mergeWith(const Modifier* modifier) const override;

 private:
  tgfx::Path clip = {};
};

class MaskModifier : public Modifier {
 public:
  MaskModifier(std::shared_ptr<Graphic> mask, bool inverted, bool useLuma)
      : mask(std::move(mask)), inverted(inverted), useLuma(useLuma) {
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

  void applyToBounds(tgfx::Rect* bounds) const override;

  bool applyToPath(tgfx::Path*) const override {
    return false;
  }

  void applyToGraphic(Canvas* canvas, std::shared_ptr<Graphic> graphic) const override;

  std::shared_ptr<Modifier> mergeWith(const Modifier*) const override {
    return nullptr;
  }

 private:
  // 可能是 nullptr
  std::shared_ptr<Graphic> mask = nullptr;
  bool inverted = false;
  bool useLuma = false;
};

//================================================================================

std::shared_ptr<Modifier> Modifier::MakeBlend(float alpha, tgfx::BlendMode blendMode) {
  if (alpha == 1.0f && blendMode == tgfx::BlendMode::SrcOver) {
    return nullptr;
  }
  return std::make_shared<BlendModifier>(alpha, blendMode);
}

std::shared_ptr<Modifier> Modifier::MakeClip(const tgfx::Path& clip) {
  if (clip.isEmpty() && clip.isInverseFillType()) {
    // is full.
    return nullptr;
  }
  return std::make_shared<ClipModifier>(clip);
}

std::shared_ptr<Modifier> Modifier::MakeMask(std::shared_ptr<Graphic> graphic, bool inverted,
                                             bool useLuma) {
  if (graphic == nullptr && inverted) {
    // 返回空，表示保留目标对象的全部内容。
    return nullptr;
  }
  tgfx::Path clipPath = {};
  if (!useLuma && graphic && graphic->getPath(&clipPath)) {
    if (inverted) {
      clipPath.toggleInverseFillType();
    }
    return Modifier::MakeClip(clipPath);
  }
  return std::make_shared<MaskModifier>(graphic, inverted, useLuma);
}

//================================================================================

bool BlendModifier::hitTest(RenderCache*, float, float) const {
  return alpha != 0.0f;
}

void BlendModifier::applyToGraphic(Canvas* canvas, std::shared_ptr<Graphic> graphic) const {
  canvas->save();
  auto newAlpha = canvas->getAlpha() * alpha;
  canvas->setAlpha(newAlpha);
  if (blendMode != tgfx::BlendMode::SrcOver) {
    canvas->setBlendMode(blendMode);
  }
  graphic->draw(canvas);
  canvas->restore();
}

std::shared_ptr<Modifier> BlendModifier::mergeWith(const Modifier* modifier) const {
  auto target = static_cast<const BlendModifier*>(modifier);
  if (target->blendMode != tgfx::BlendMode::SrcOver && blendMode != tgfx::BlendMode::SrcOver) {
    return nullptr;
  }
  auto newBlendMode = blendMode != tgfx::BlendMode::SrcOver ? blendMode : target->blendMode;
  auto newAlpha = alpha * target->alpha;
  return std::make_shared<BlendModifier>(newAlpha, newBlendMode);
}

void ClipModifier::applyToBounds(tgfx::Rect* bounds) const {
  tgfx::Path boundsPath = {};
  boundsPath.addRect(*bounds);
  boundsPath.addPath(clip, tgfx::PathOp::Intersect);
  *bounds = boundsPath.getBounds();
}

bool ClipModifier::applyToPath(tgfx::Path* path) const {
  path->addPath(clip, tgfx::PathOp::Intersect);
  return true;
}

void ClipModifier::applyToGraphic(Canvas* canvas, std::shared_ptr<Graphic> graphic) const {
  canvas->save();
  canvas->clipPath(clip);
  graphic->draw(canvas);
  canvas->restore();
}

std::shared_ptr<Modifier> ClipModifier::mergeWith(const Modifier* modifier) const {
  auto target = static_cast<const ClipModifier*>(modifier);
  auto newClip = clip;
  newClip.addPath(target->clip, tgfx::PathOp::Intersect);
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

void MaskModifier::applyToBounds(tgfx::Rect* bounds) const {
  if (mask == nullptr) {
    bounds->setEmpty();
    return;
  }
  if (!inverted) {
    tgfx::Rect clipBounds = tgfx::Rect::MakeEmpty();
    mask->measureBounds(&clipBounds);
    if (!bounds->intersect(clipBounds)) {
      bounds->setEmpty();
    }
  }
}

void MaskModifier::applyToGraphic(Canvas* canvas, std::shared_ptr<Graphic> graphic) const {
  if (mask == nullptr) {
    return;
  }
  tgfx::Rect bounds = tgfx::Rect::MakeEmpty();
  graphic->measureBounds(&bounds);
  auto originBounds = bounds;
  applyToBounds(&bounds);
  if (bounds.isEmpty()) {
    // 与遮罩不相交，直接跳过绘制。
    return;
  }
  if (originBounds != bounds) {
    // roundOut() prevents resampling when the graphic's content needs to be cropped.
    bounds.roundOut();
  }
  auto contentSurface = SurfaceUtil::MakeContentSurface(canvas, bounds, FLT_MAX);
  if (contentSurface == nullptr) {
    return;
  }
  Canvas contentCanvas(contentSurface.get(), canvas->getCache());
  auto contentMatrix = contentCanvas.getMatrix();
  graphic->draw(&contentCanvas);
  auto maskSurface = tgfx::Surface::Make(contentSurface->getContext(), contentSurface->width(),
                                         contentSurface->height(), !useLuma);
  if (maskSurface == nullptr) {
    maskSurface = tgfx::Surface::Make(contentSurface->getContext(), contentSurface->width(),
                                      contentSurface->height());
  }
  if (maskSurface == nullptr) {
    return;
  }
  Canvas maskCanvas(maskSurface.get(), canvas->getCache());
  maskCanvas.setMatrix(contentMatrix);
  mask->draw(&maskCanvas);
  auto shader = tgfx::Shader::MakeImageShader(maskSurface->makeImageSnapshot());
  if (shader == nullptr) {
    return;
  }
  auto image = contentSurface->makeImageSnapshot();
  auto scaleFactor = GetMaxScaleFactor(contentMatrix);
  auto matrix = tgfx::Matrix::MakeScale(1.0f / scaleFactor);
  matrix.postTranslate(bounds.x(), bounds.y());
  canvas->save();
  canvas->concat(matrix);
  tgfx::Paint paint;
  if (useLuma) {
    auto lumaFilter = tgfx::ColorFilter::Matrix(
        {0,      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.21260000000000001, 0.71519999999999995,
         0.0722, 0, 0});
    shader = shader->makeWithColorFilter(lumaFilter);
  }
  paint.setMaskFilter(tgfx::MaskFilter::MakeShader(std::move(shader), inverted));
  canvas->drawImage(image, &paint);
  canvas->restore();
}
}  // namespace pag
