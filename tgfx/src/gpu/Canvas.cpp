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

#include "tgfx/gpu/Canvas.h"
#include <atomic>
#include "Blend.h"
#include "CanvasState.h"
#include "SurfaceDrawContext.h"
#include "core/utils/MathExtra.h"
#include "gpu/AARectEffect.h"
#include "gpu/ConstColorProcessor.h"
#include "gpu/DeviceSpaceTextureEffect.h"
#include "gpu/GpuPaint.h"
#include "gpu/PorterDuffXferProcessor.h"
#include "gpu/RGBAAATextureEffect.h"
#include "gpu/ops/ClearOp.h"
#include "gpu/ops/FillRectOp.h"
#include "gpu/ops/RRectOp.h"
#include "gpu/ops/TriangulatingPathOp.h"
#include "tgfx/core/BlendMode.h"
#include "tgfx/core/Mask.h"
#include "tgfx/core/PathEffect.h"
#include "tgfx/core/TextBlob.h"
#include "tgfx/gpu/Surface.h"

namespace tgfx {
static uint32_t NextClipID() {
  static const uint32_t kFirstUnreservedClipID = 1;
  static std::atomic_uint32_t nextID{kFirstUnreservedClipID};
  uint32_t id;
  do {
    id = nextID++;
  } while (id < kFirstUnreservedClipID);
  return id;
}

Canvas::Canvas(Surface* surface) : surface(surface), clipID(kDefaultClipID) {
  drawContext = new SurfaceDrawContext(surface);
  state = std::make_shared<CanvasState>();
  state->clip.addRect(0, 0, static_cast<float>(surface->width()),
                      static_cast<float>(surface->height()));
  state->clipID = NextClipID();
}

Canvas::~Canvas() {
  delete drawContext;
}

void Canvas::save() {
  auto canvasState = std::make_shared<CanvasState>();
  *canvasState = *state;
  savedStateList.push_back(canvasState);
}

void Canvas::restore() {
  if (savedStateList.empty()) {
    return;
  }
  state = savedStateList.back();
  savedStateList.pop_back();
}

Matrix Canvas::getMatrix() const {
  return state->matrix;
}

void Canvas::setMatrix(const Matrix& matrix) {
  state->matrix = matrix;
}

void Canvas::resetMatrix() {
  state->matrix.reset();
}

void Canvas::concat(const Matrix& matrix) {
  state->matrix.preConcat(matrix);
}

float Canvas::getAlpha() const {
  return state->alpha;
}

void Canvas::setAlpha(float newAlpha) {
  state->alpha = newAlpha;
}

BlendMode Canvas::getBlendMode() const {
  return state->blendMode;
}

void Canvas::setBlendMode(BlendMode blendMode) {
  state->blendMode = blendMode;
}

Path Canvas::getTotalClip() const {
  return state->clip;
}

void Canvas::clipPath(const Path& path) {
  auto clipPath = path;
  clipPath.transform(state->matrix);
  state->clip.addPath(clipPath, PathOp::Intersect);
  state->clipID = NextClipID();
}

void Canvas::clear(const Color& color) {
  auto oldBlend = getBlendMode();
  setBlendMode(BlendMode::Src);
  Paint paint;
  paint.setColor(color);
  auto rect =
      Rect::MakeWH(static_cast<float>(surface->width()), static_cast<float>(surface->height()));
  drawRect(rect, paint);
  setBlendMode(oldBlend);
}

void Canvas::drawRect(const Rect& rect, const Paint& paint) {
  Path path = {};
  path.addRect(rect);
  drawPath(path, paint);
}

void Canvas::drawTexture(std::shared_ptr<Texture> texture, const Matrix& matrix,
                         SamplingOptions sampling) {
  auto oldMatrix = getMatrix();
  concat(matrix);
  drawTexture(std::move(texture), sampling);
  setMatrix(oldMatrix);
}

void Canvas::drawTexture(std::shared_ptr<Texture> texture, SamplingOptions sampling,
                         const Paint* paint) {
  drawTexture(std::move(texture), nullptr, sampling, paint);
}

static Paint CleanPaintForDrawTexture(const Paint* paint) {
  Paint cleaned;
  if (paint) {
    cleaned = *paint;
    cleaned.setStyle(PaintStyle::Fill);
  }
  return cleaned;
}

void Canvas::drawTexture(std::shared_ptr<Texture> texture, const RGBAAALayout* layout,
                         SamplingOptions sampling, const Paint* paint) {
  auto realPaint = CleanPaintForDrawTexture(paint);
  std::shared_ptr<Texture> result;
  auto oldMatrix = getMatrix();
  if (realPaint.getImageFilter()) {
    auto clipBounds = state->clip.getBounds();
    Matrix inverted = Matrix::I();
    if (!oldMatrix.invert(&inverted)) {
      return;
    }
    inverted.mapRect(&clipBounds);
    ImageFilterContext context(getContext(), oldMatrix, clipBounds, texture);
    auto [image, offset] = realPaint.getImageFilter()->filterImage(context);
    texture = image;
    concat(Matrix::MakeTrans(offset.x, offset.y));
  }
  drawTexture(std::move(texture), layout, sampling, realPaint);
  setMatrix(oldMatrix);
}

void Canvas::flush() {
  surface->flush();
}

Context* Canvas::getContext() const {
  return surface->getContext();
}

Surface* Canvas::getSurface() const {
  return surface;
}

const SurfaceOptions* Canvas::surfaceOptions() const {
  return surface->options();
}

static bool PaintToGLPaint(Context* context, const Paint& paint, float alpha,
                           std::unique_ptr<FragmentProcessor> shaderProcessor, GpuPaint* glPaint) {
  FPArgs args(context);
  glPaint->context = context;
  glPaint->color = paint.getColor().makeOpaque();
  std::unique_ptr<FragmentProcessor> shaderFP;
  if (shaderProcessor) {
    shaderFP = std::move(shaderProcessor);
  } else if (auto shader = paint.getShader()) {
    shaderFP = shader->asFragmentProcessor(args);
    if (shaderFP == nullptr) {
      return false;
    }
  }
  alpha *= paint.getAlpha();
  if (shaderFP) {
    glPaint->colorFragmentProcessors.emplace_back(std::move(shaderFP));
    if (alpha != 1.f) {
      glPaint->colorFragmentProcessors.emplace_back(
          ConstColorProcessor::Make(Color{alpha, alpha, alpha, alpha}, InputMode::ModulateRGBA));
    }
  } else {
    glPaint->color.alpha = alpha;
    glPaint->color = glPaint->color.premultiply();
  }
  if (auto colorFilter = paint.getColorFilter()) {
    if (auto processor = colorFilter->asFragmentProcessor()) {
      glPaint->colorFragmentProcessors.emplace_back(std::move(processor));
    } else {
      return false;
    }
  }
  if (auto maskFilter = paint.getMaskFilter()) {
    if (auto processor = maskFilter->asFragmentProcessor(args)) {
      glPaint->coverageFragmentProcessors.emplace_back(std::move(processor));
    }
  }
  return true;
}

static bool PaintToGLPaintWithTexture(Context* context, const Paint& paint, float alpha,
                                      std::unique_ptr<FragmentProcessor> fp,
                                      bool textureIsAlphaOnly, GpuPaint* glPaint) {
  std::unique_ptr<FragmentProcessor> shaderFP;
  if (textureIsAlphaOnly) {
    if (auto shader = paint.getShader()) {
      shaderFP = shader->asFragmentProcessor(FPArgs(context));
      if (!shaderFP) {
        return false;
      }
      std::unique_ptr<FragmentProcessor> fpSeries[] = {std::move(shaderFP), std::move(fp)};
      shaderFP = FragmentProcessor::RunInSeries(fpSeries, 2);
    } else {
      shaderFP = std::move(fp);
    }
  } else {
    shaderFP = FragmentProcessor::MulChildByInputAlpha(std::move(fp));
  }
  return PaintToGLPaint(context, paint, alpha, std::move(shaderFP), glPaint);
}

std::shared_ptr<Texture> Canvas::getClipTexture() {
  if (clipID != state->clipID) {
    _clipSurface = nullptr;
  }
  if (_clipSurface == nullptr) {
    _clipSurface = Surface::Make(getContext(), surface->width(), surface->height(), true);
    if (_clipSurface == nullptr) {
      _clipSurface = Surface::Make(getContext(), surface->width(), surface->height());
    }
  }
  if (_clipSurface == nullptr) {
    return nullptr;
  }
  if (clipID != state->clipID) {
    auto clipCanvas = _clipSurface->getCanvas();
    clipCanvas->clear();
    Paint paint = {};
    paint.setColor(Color::Black());
    clipCanvas->drawPath(state->clip, paint);
    clipID = state->clipID;
  }
  return _clipSurface->getTexture();
}

static constexpr float BOUNDS_TO_LERANCE = 1e-3f;

/**
 * Returns true if the given rect counts as aligned with pixel boundaries.
 */
static bool IsPixelAligned(const Rect& rect) {
  return fabsf(roundf(rect.left) - rect.left) <= BOUNDS_TO_LERANCE &&
         fabsf(roundf(rect.top) - rect.top) <= BOUNDS_TO_LERANCE &&
         fabsf(roundf(rect.right) - rect.right) <= BOUNDS_TO_LERANCE &&
         fabsf(roundf(rect.bottom) - rect.bottom) <= BOUNDS_TO_LERANCE;
}

void FlipYIfNeeded(Rect* rect, const Surface* surface) {
  if (surface->origin() == ImageOrigin::BottomLeft) {
    auto height = rect->height();
    rect->top = static_cast<float>(surface->height()) - rect->bottom;
    rect->bottom = rect->top + height;
  }
}

std::pair<std::optional<Rect>, bool> Canvas::getClipRect() {
  auto rect = Rect::MakeEmpty();
  if (state->clip.asRect(&rect)) {
    FlipYIfNeeded(&rect, surface);
    if (IsPixelAligned(rect)) {
      rect.round();
      if (rect != Rect::MakeWH(static_cast<float>(surface->width()),
                               static_cast<float>(surface->height()))) {
        return {rect, true};
      } else {
        return {Rect::MakeEmpty(), false};
      }
    } else {
      return {rect, false};
    }
  }
  return {{}, false};
}

std::unique_ptr<FragmentProcessor> Canvas::getClipMask(const Rect& deviceBounds,
                                                       Rect* scissorRect) {
  const auto& clipPath = state->clip;
  if (clipPath.contains(deviceBounds)) {
    return nullptr;
  }
  auto [rect, useScissor] = getClipRect();
  if (rect.has_value()) {
    if (useScissor) {
      *scissorRect = *rect;
    } else if (!rect->isEmpty()) {
      return AARectEffect::Make(*rect);
    }
    return nullptr;
  } else {
    return FragmentProcessor::MulInputByChildAlpha(
        DeviceSpaceTextureEffect::Make(getClipTexture(), surface->origin()));
  }
}

Rect Canvas::clipLocalBounds(Rect localBounds) {
  auto deviceBounds = state->matrix.mapRect(localBounds);
  auto clipBounds = state->clip.getBounds();
  clipBounds.roundOut();
  auto clippedDeviceBounds = deviceBounds;
  if (!clippedDeviceBounds.intersect(clipBounds)) {
    return Rect::MakeEmpty();
  }
  auto clippedLocalBounds = localBounds;
  if (state->matrix.getSkewX() == 0 && state->matrix.getSkewY() == 0 &&
      clippedDeviceBounds != deviceBounds) {
    Matrix inverse = Matrix::I();
    state->matrix.invert(&inverse);
    clippedLocalBounds = inverse.mapRect(clippedDeviceBounds);
  }
  return clippedLocalBounds;
}

void Canvas::drawTexture(std::shared_ptr<Texture> texture, const RGBAAALayout* layout,
                         SamplingOptions sampling, const Paint& paint) {
  if (texture == nullptr) {
    return;
  }
  auto width = static_cast<float>(layout ? layout->width : texture->width());
  auto height = static_cast<float>(layout ? layout->height : texture->height());
  auto localBounds = clipLocalBounds(Rect::MakeWH(width, height));
  if (localBounds.isEmpty()) {
    return;
  }
  auto processor = RGBAAATextureEffect::Make(texture, sampling, Matrix::I(), layout);
  if (processor == nullptr) {
    return;
  }
  GpuPaint glPaint;
  if (!PaintToGLPaintWithTexture(getContext(), paint, state->alpha, std::move(processor),
                                 texture->getSampler()->format == PixelFormat::ALPHA_8, &glPaint)) {
    return;
  }
  auto op = FillRectOp::Make(glPaint.color, localBounds, state->matrix);
  draw(std::move(op), std::move(glPaint), true);
}

void Canvas::drawPath(const Path& path, const Paint& paint) {
  if (paint.getStyle() == PaintStyle::Fill) {
    fillPath(path, paint);
    return;
  }
  auto strokePath = path;
  auto strokeEffect = PathEffect::MakeStroke(*paint.getStroke());
  if (strokeEffect == nullptr) {
    return;
  }
  strokeEffect->applyTo(&strokePath);
  fillPath(strokePath, paint);
}

void Canvas::drawShape(std::shared_ptr<Shape> shape, const Paint& paint) {
  if (shape == nullptr) {
    return;
  }
  GpuPaint glPaint;
  if (!PaintToGLPaint(getContext(), paint, state->alpha, nullptr, &glPaint)) {
    return;
  }
  auto bounds = shape->getBounds();
  if (!state->matrix.isIdentity()) {
    state->matrix.mapRect(&bounds);
  }
  auto clipBounds = state->clip.getBounds();
  clipBounds.roundOut();
  if (!clipBounds.intersect(bounds)) {
    return;
  }
  auto op = shape->makeOp(&glPaint, state->matrix);
  if (op == nullptr) {
    return;
  }
  draw(std::move(op), std::move(glPaint));
}

static std::unique_ptr<DrawOp> MakeSimplePathOp(const Path& path, const GpuPaint& glPaint,
                                                const Matrix& viewMatrix) {
  auto rect = Rect::MakeEmpty();
  if (path.asRect(&rect)) {
    return FillRectOp::Make(glPaint.color, rect, viewMatrix);
  }
  RRect rRect;
  if (path.asRRect(&rRect)) {
    return RRectOp::Make(glPaint.color, rRect, viewMatrix);
  }
  return nullptr;
}

void Canvas::fillPath(const Path& path, const Paint& paint) {
  if (path.isEmpty()) {
    return;
  }
  GpuPaint glPaint;
  if (!PaintToGLPaint(getContext(), paint, state->alpha, nullptr, &glPaint)) {
    return;
  }
  auto bounds = path.getBounds();
  auto localBounds = clipLocalBounds(bounds);
  if (localBounds.isEmpty()) {
    return;
  }
  if (drawAsClear(path, glPaint)) {
    return;
  }
  auto op = MakeSimplePathOp(path, glPaint, state->matrix);
  if (op) {
    draw(std::move(op), std::move(glPaint));
    return;
  }
  auto localMatrix = Matrix::I();
  if (!state->matrix.invert(&localMatrix)) {
    return;
  }
  auto tempPath = path;
  tempPath.transform(state->matrix);
  op = TriangulatingPathOp::Make(glPaint.color, tempPath, state->clip.getBounds(), localMatrix);
  if (op) {
    save();
    resetMatrix();
    draw(std::move(op), std::move(glPaint));
    restore();
    return;
  }
  auto deviceBounds = state->matrix.mapRect(localBounds);
  auto width = ceilf(deviceBounds.width());
  auto height = ceilf(deviceBounds.height());
  auto mask = Mask::Make(static_cast<int>(width), static_cast<int>(height));
  if (!mask) {
    return;
  }
  auto totalMatrix = state->matrix;
  auto matrix = Matrix::MakeTrans(-deviceBounds.x(), -deviceBounds.y());
  matrix.postScale(width / deviceBounds.width(), height / deviceBounds.height());
  totalMatrix.postConcat(matrix);
  mask->setMatrix(totalMatrix);
  mask->fillPath(path);
  auto texture = mask->updateTexture(getContext());
  drawMask(deviceBounds, std::move(texture), std::move(glPaint));
}

void Canvas::drawMask(const Rect& bounds, std::shared_ptr<Texture> mask, GpuPaint glPaint) {
  if (mask == nullptr) {
    return;
  }
  auto localMatrix = Matrix::I();
  auto maskLocalMatrix = Matrix::I();
  auto invert = Matrix::I();
  if (!state->matrix.invert(&invert)) {
    return;
  }
  localMatrix.postConcat(invert);
  if (!localMatrix.invert(&invert)) {
    return;
  }
  maskLocalMatrix.postConcat(invert);
  maskLocalMatrix.postTranslate(-bounds.x(), -bounds.y());
  maskLocalMatrix.postScale(static_cast<float>(mask->width()) / bounds.width(),
                            static_cast<float>(mask->height()) / bounds.height());
  auto oldMatrix = state->matrix;
  resetMatrix();
  glPaint.coverageFragmentProcessors.emplace_back(FragmentProcessor::MulInputByChildAlpha(
      RGBAAATextureEffect::Make(std::move(mask), SamplingOptions(), maskLocalMatrix)));
  auto op = FillRectOp::Make(glPaint.color, bounds, state->matrix, localMatrix);
  draw(std::move(op), std::move(glPaint));
  setMatrix(oldMatrix);
}

void Canvas::drawGlyphs(const GlyphID glyphIDs[], const Point positions[], size_t glyphCount,
                        const Font& font, const Paint& paint) {
  auto scaleX = state->matrix.getScaleX();
  auto skewY = state->matrix.getSkewY();
  auto scale = std::sqrt(scaleX * scaleX + skewY * skewY);
  auto scaledFont = font.makeWithSize(font.getSize() * scale);
  auto scaledPaint = paint;
  scaledPaint.setStrokeWidth(paint.getStrokeWidth() * scale);
  std::vector<Point> scaledPositions;
  for (size_t i = 0; i < glyphCount; ++i) {
    scaledPositions.push_back(Point::Make(positions[i].x * scale, positions[i].y * scale));
  }
  save();
  concat(Matrix::MakeScale(1.f / scale));
  if (scaledFont.getTypeface()->hasColor()) {
    drawColorGlyphs(glyphIDs, &scaledPositions[0], glyphCount, scaledFont, scaledPaint);
    restore();
    return;
  }
  auto textBlob = TextBlob::MakeFrom(glyphIDs, &scaledPositions[0], glyphCount, scaledFont);
  if (textBlob == nullptr) {
    restore();
    return;
  }
  Path path = {};
  auto stroke = scaledPaint.getStyle() == PaintStyle::Stroke ? scaledPaint.getStroke() : nullptr;
  if (textBlob->getPath(&path, stroke)) {
    fillPath(path, scaledPaint);
    restore();
    return;
  }
  drawMaskGlyphs(textBlob.get(), scaledPaint);
  restore();
}

void Canvas::drawColorGlyphs(const GlyphID glyphIDs[], const Point positions[], size_t glyphCount,
                             const Font& font, const Paint& paint) {
  for (size_t i = 0; i < glyphCount; ++i) {
    const auto& glyphID = glyphIDs[i];
    const auto& position = positions[i];

    auto glyphMatrix = Matrix::I();
    auto glyphBuffer = font.getGlyphImage(glyphID, &glyphMatrix);
    if (glyphBuffer == nullptr) {
      continue;
    }
    glyphMatrix.postTranslate(position.x, position.y);
    save();
    concat(glyphMatrix);
    Canvas::drawTexture(glyphBuffer->makeTexture(getContext()), &paint);
    restore();
  }
}

void Canvas::drawMaskGlyphs(TextBlob* textBlob, const Paint& paint) {
  if (textBlob == nullptr) {
    return;
  }
  GpuPaint glPaint;
  if (!PaintToGLPaint(getContext(), paint, state->alpha, nullptr, &glPaint)) {
    return;
  }
  auto stroke = paint.getStyle() == PaintStyle::Stroke ? paint.getStroke() : nullptr;
  auto localBounds = clipLocalBounds(textBlob->getBounds(stroke));
  if (localBounds.isEmpty()) {
    return;
  }
  auto deviceBounds = state->matrix.mapRect(localBounds);
  auto width = ceilf(deviceBounds.width());
  auto height = ceilf(deviceBounds.height());
  auto mask = Mask::Make(static_cast<int>(width), static_cast<int>(height));
  if (mask == nullptr) {
    return;
  }
  auto totalMatrix = state->matrix;
  auto matrix = Matrix::I();
  matrix.postTranslate(-deviceBounds.x(), -deviceBounds.y());
  matrix.postScale(width / deviceBounds.width(), height / deviceBounds.height());
  totalMatrix.postConcat(matrix);
  mask->setMatrix(totalMatrix);
  if (paint.getStyle() == PaintStyle::Stroke) {
    if (stroke) {
      mask->strokeText(textBlob, *stroke);
    }
  } else {
    mask->fillText(textBlob);
  }
  auto texture = mask->updateTexture(getContext());
  drawMask(deviceBounds, std::move(texture), std::move(glPaint));
}

void Canvas::drawAtlas(std::shared_ptr<Texture> atlas, const Matrix matrix[], const Rect tex[],
                       const Color colors[], size_t count, SamplingOptions sampling) {
  if (atlas == nullptr || count == 0) {
    return;
  }
  auto totalMatrix = getMatrix();
  std::vector<std::unique_ptr<FillRectOp>> ops;
  FillRectOp* op = nullptr;
  for (size_t i = 0; i < count; ++i) {
    concat(matrix[i]);
    auto width = static_cast<float>(tex[i].width());
    auto height = static_cast<float>(tex[i].height());
    auto localBounds = clipLocalBounds(Rect::MakeWH(width, height));
    if (localBounds.isEmpty()) {
      setMatrix(totalMatrix);
      continue;
    }
    auto localMatrix = Matrix::MakeTrans(tex[i].x(), tex[i].y());
    auto color = colors ? std::optional<Color>(colors[i].premultiply()) : std::nullopt;
    if (op == nullptr) {
      ops.emplace_back(FillRectOp::Make(color, localBounds, state->matrix, localMatrix));
      op = ops.back().get();
    } else {
      if (!op->add(color, localBounds, state->matrix, localMatrix)) {
        ops.emplace_back(FillRectOp::Make(color, localBounds, state->matrix, localMatrix));
        op = ops.back().get();
      }
    }
    setMatrix(totalMatrix);
  }
  if (ops.empty()) {
    return;
  }
  for (auto& rectOp : ops) {
    GpuPaint glPaint;
    if (colors) {
      glPaint.coverageFragmentProcessors.emplace_back(
          FragmentProcessor::MulInputByChildAlpha(RGBAAATextureEffect::Make(atlas, sampling)));
    } else {
      glPaint.colorFragmentProcessors.emplace_back(RGBAAATextureEffect::Make(atlas, sampling));
    }
    draw(std::move(rectOp), std::move(glPaint), false);
  }
}

bool Canvas::drawAsClear(const Path& path, const GpuPaint& paint) {
  if (!paint.colorFragmentProcessors.empty() || !paint.coverageFragmentProcessors.empty() ||
      !state->matrix.rectStaysRect() || drawContext == nullptr) {
    return false;
  }
  auto bounds = Rect::MakeEmpty();
  if (!path.asRect(&bounds)) {
    return false;
  }
  state->matrix.mapRect(&bounds);
  if (!IsPixelAligned(bounds)) {
    return false;
  }
  auto [rect, useScissor] = getClipRect();
  if (rect.has_value()) {
    if (useScissor) {
      FlipYIfNeeded(&bounds, surface);
      rect->intersect(bounds);
      drawContext->addOp(ClearOp::Make(paint.color, *rect));
      return true;
    } else if (rect->isEmpty()) {
      FlipYIfNeeded(&bounds, surface);
      drawContext->addOp(ClearOp::Make(paint.color, bounds));
      return true;
    }
  }
  return false;
}

void Canvas::draw(std::unique_ptr<DrawOp> op, GpuPaint paint, bool aa) {
  if (drawContext == nullptr) {
    return;
  }
  auto aaType = AAType::None;
  if (surface->renderTarget->sampleCount() > 1) {
    aaType = AAType::MSAA;
  } else if (aa && !IsPixelAligned(op->bounds())) {
    aaType = AAType::Coverage;
  } else {
    const auto& matrix = state->matrix;
    auto rotation = std::round(RadiansToDegrees(atan2f(matrix.getSkewX(), matrix.getScaleX())));
    if (static_cast<int>(rotation) % 90 != 0) {
      aaType = AAType::Coverage;
    }
  }
  auto masks = std::move(paint.coverageFragmentProcessors);
  Rect scissorRect = Rect::MakeEmpty();
  auto clipMask = getClipMask(op->bounds(), &scissorRect);
  if (clipMask) {
    masks.push_back(std::move(clipMask));
  }
  op->setScissorRect(scissorRect);
  BlendModeCoeff first;
  BlendModeCoeff second;
  if (BlendModeAsCoeff(state->blendMode, &first, &second)) {
    op->setBlendFactors(std::make_pair(first, second));
  } else {
    op->setXferProcessor(PorterDuffXferProcessor::Make(state->blendMode));
    op->setRequireDstTexture(!getContext()->caps()->frameBufferFetchSupport);
  }
  op->setAA(aaType);
  op->setColors(std::move(paint.colorFragmentProcessors));
  op->setMasks(std::move(masks));
  drawContext->addOp(std::move(op));
}
}  // namespace tgfx
