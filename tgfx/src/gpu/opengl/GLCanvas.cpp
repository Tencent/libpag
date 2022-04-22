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

#include "GLCanvas.h"
#include "GLFillRectOp.h"
#include "GLRRectOp.h"
#include "GLSurface.h"
#include "core/utils/MathExtra.h"
#include "gpu/AARectEffect.h"
#include "gpu/AlphaFragmentProcessor.h"
#include "gpu/ColorShader.h"
#include "gpu/TextureFragmentProcessor.h"
#include "gpu/TextureMaskFragmentProcessor.h"
#include "gpu/opengl/GLTriangulatingPathOp.h"
#include "tgfx/core/Mask.h"
#include "tgfx/core/PathEffect.h"
#include "tgfx/core/TextBlob.h"

namespace tgfx {
GLCanvas::GLCanvas(Surface* surface) : Canvas(surface) {
}

void GLCanvas::clear() {
  auto renderTarget = std::static_pointer_cast<GLRenderTarget>(surface->getRenderTarget());
  renderTarget->clear();
}

void GLCanvas::drawTexture(const Texture* texture, const Texture* mask, bool inverted) {
  drawTexture(texture, nullptr, mask, inverted);
}

Texture* GLCanvas::getClipTexture() {
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
  return _clipSurface->getTexture().get();
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

void GLCanvas::drawTexture(const Texture* texture, const RGBAAALayout* layout) {
  drawTexture(texture, layout, nullptr, false);
}

std::unique_ptr<FragmentProcessor> GLCanvas::getClipMask(const Rect& deviceBounds,
                                                         Rect* scissorRect) {
  const auto& clipPath = state->clip;
  if (clipPath.contains(deviceBounds)) {
    return nullptr;
  }
  auto rect = Rect::MakeEmpty();
  if (clipPath.asRect(&rect)) {
    if (surface->origin() == ImageOrigin::BottomLeft) {
      auto height = rect.height();
      rect.top = static_cast<float>(surface->height()) - rect.bottom;
      rect.bottom = rect.top + height;
    }
    if (IsPixelAligned(rect) && scissorRect) {
      *scissorRect = rect;
      scissorRect->round();
      return nullptr;
    } else {
      return AARectEffect::Make(rect);
    }
  } else {
    return TextureMaskFragmentProcessor::MakeUseDeviceCoord(getClipTexture(), surface->origin());
  }
}

Rect GLCanvas::clipLocalBounds(Rect localBounds) {
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

void GLCanvas::drawTexture(const Texture* texture, const RGBAAALayout* layout, const Texture* mask,
                           bool inverted) {
  if (texture == nullptr) {
    return;
  }
  auto width = static_cast<float>(layout ? layout->width : texture->width());
  auto height = static_cast<float>(layout ? layout->height : texture->height());
  auto localBounds = clipLocalBounds(Rect::MakeWH(width, height));
  if (localBounds.isEmpty()) {
    return;
  }
  auto localMatrix = Matrix::I();
  auto scale = texture->getTextureCoord(localBounds.width(), localBounds.height()) -
               texture->getTextureCoord(0, 0);
  localMatrix.postScale(scale.x, scale.y);
  auto translate = texture->getTextureCoord(localBounds.x(), localBounds.y());
  localMatrix.postTranslate(translate.x, translate.y);
  auto processor = TextureFragmentProcessor::Make(texture, layout, localMatrix);
  if (processor == nullptr) {
    return;
  }
  draw(GLFillRectOp::Make(localBounds, getViewMatrix()), std::move(processor),
       TextureMaskFragmentProcessor::MakeUseLocalCoord(mask, localMatrix, inverted), true);
}

void GLCanvas::drawPath(const Path& path, const Paint& paint) {
  auto shader = paint.getShader();
  if (shader == nullptr) {
    shader = Shader::MakeColorShader(paint.getColor());
  }
  if (paint.getStyle() == PaintStyle::Fill) {
    fillPath(path, shader.get());
    return;
  }
  auto strokePath = path;
  auto strokeEffect = PathEffect::MakeStroke(*paint.getStroke());
  if (strokeEffect) {
    strokeEffect->applyTo(&strokePath);
  }
  fillPath(strokePath, shader.get());
}

static std::unique_ptr<GLDrawOp> MakeSimplePathOp(const Path& path, const Matrix& viewMatrix) {
  auto rect = Rect::MakeEmpty();
  if (path.asRect(&rect)) {
    return GLFillRectOp::Make(rect, viewMatrix);
  }
  RRect rRect;
  if (path.asRRect(&rRect)) {
    return GLRRectOp::Make(rRect, viewMatrix);
  }
  return nullptr;
}

void GLCanvas::fillPath(const Path& path, const Shader* shader) {
  if (path.isEmpty()) {
    return;
  }
  auto bounds = path.getBounds();
  auto localBounds = clipLocalBounds(bounds);
  if (localBounds.isEmpty()) {
    return;
  }
  auto viewMatrix = getViewMatrix();
  auto op = MakeSimplePathOp(path, viewMatrix);
  if (op) {
    auto localMatrix = Matrix::MakeScale(bounds.width(), bounds.height());
    localMatrix.postTranslate(bounds.x(), bounds.y());
    auto args = FPArgs(getContext(), localMatrix);
    draw(std::move(op), shader->asFragmentProcessor(args));
    return;
  }
  auto tempPath = path;
  tempPath.transform(viewMatrix);
  op = GLTriangulatingPathOp::Make(tempPath, state->clip.getBounds());
  if (op) {
    auto localMatrix = Matrix::I();
    viewMatrix.invert(&localMatrix);
    save();
    resetMatrix();
    auto args = FPArgs(getContext(), localMatrix);
    draw(std::move(op), shader->asFragmentProcessor(args));
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
  auto maskTexture = mask->makeTexture(getContext());
  drawMask(deviceBounds, maskTexture.get(), shader);
}

void GLCanvas::drawMask(const Rect& bounds, const Texture* mask, const Shader* shader) {
  if (mask == nullptr || shader == nullptr) {
    return;
  }
  auto scale =
      mask->getTextureCoord(static_cast<float>(mask->width()), static_cast<float>(mask->height()));
  auto localMatrix = Matrix::I();
  localMatrix.postScale(bounds.width(), bounds.height());
  localMatrix.postTranslate(bounds.x(), bounds.y());
  auto invert = Matrix::I();
  state->matrix.invert(&invert);
  localMatrix.postConcat(invert);
  auto args = FPArgs(getContext(), localMatrix);
  save();
  resetMatrix();
  draw(GLFillRectOp::Make(bounds, getViewMatrix()), shader->asFragmentProcessor(args),
       TextureMaskFragmentProcessor::MakeUseLocalCoord(mask, Matrix::MakeScale(scale.x, scale.y)));
  restore();
}

void GLCanvas::drawGlyphs(const GlyphID glyphIDs[], const Point positions[], size_t glyphCount,
                          const Font& font, const Paint& paint) {
  auto scaleX = state->matrix.getScaleX();
  auto skewY = state->matrix.getSkewY();
  auto scale = std::sqrt(scaleX * scaleX + skewY * skewY);
  auto scaledFont = font.makeWithSize(font.getSize() * scale);
  std::vector<Point> scaledPositions;
  for (size_t i = 0; i < glyphCount; ++i) {
    scaledPositions.push_back(Point::Make(positions[i].x * scale, positions[i].y * scale));
  }
  save();
  concat(Matrix::MakeScale(1.f / scale));
  if (scaledFont.getTypeface()->hasColor()) {
    drawColorGlyphs(glyphIDs, &scaledPositions[0], glyphCount, scaledFont, paint);
    restore();
    return;
  }
  auto textBlob = TextBlob::MakeFrom(glyphIDs, &scaledPositions[0], glyphCount, scaledFont);
  if (textBlob == nullptr) {
    restore();
    return;
  }
  Path path = {};
  auto stroke = paint.getStyle() == PaintStyle::Stroke ? paint.getStroke() : nullptr;
  if (textBlob->getPath(&path, stroke)) {
    auto shader = Shader::MakeColorShader(paint.getColor());
    fillPath(path, shader.get());
    restore();
    return;
  }
  drawMaskGlyphs(textBlob.get(), paint);
  restore();
}

void GLCanvas::drawColorGlyphs(const GlyphID glyphIDs[], const Point positions[], size_t glyphCount,
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
    state->alpha *= paint.getAlpha();
    auto texture = glyphBuffer->makeTexture(getContext());
    drawTexture(texture.get(), nullptr, false);
    restore();
  }
}

void GLCanvas::drawMaskGlyphs(TextBlob* textBlob, const Paint& paint) {
  if (textBlob == nullptr) {
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
  auto texture = mask->makeTexture(getContext());
  auto shader = Shader::MakeColorShader(paint.getColor());
  drawMask(deviceBounds, texture.get(), shader.get());
}

void GLCanvas::drawAtlas(const Texture* atlas, const Matrix matrix[], const Rect tex[],
                         const Color colors[], size_t count) {
  if (atlas == nullptr || count == 0) {
    return;
  }
  auto totalMatrix = getMatrix();
  std::vector<Rect> rects;
  std::vector<Matrix> matrices;
  std::vector<Matrix> localMatrices;
  std::vector<Color> colorVector;
  for (size_t i = 0; i < count; ++i) {
    concat(matrix[i]);
    auto width = static_cast<float>(tex[i].width());
    auto height = static_cast<float>(tex[i].height());
    auto localBounds = clipLocalBounds(Rect::MakeWH(width, height));
    if (localBounds.isEmpty()) {
      setMatrix(totalMatrix);
      continue;
    }
    rects.push_back(localBounds);
    matrices.push_back(getViewMatrix());
    auto localMatrix = Matrix::I();
    auto scale = atlas->getTextureCoord(localBounds.width(), localBounds.height());
    localMatrix.postScale(scale.x, scale.y);
    auto translate =
        atlas->getTextureCoord(tex[i].x() + localBounds.x(), tex[i].y() + localBounds.y());
    localMatrix.postTranslate(translate.x, translate.y);
    localMatrices.push_back(localMatrix);
    if (colors) {
      colorVector.push_back(colors[i]);
    }
    setMatrix(totalMatrix);
  }
  if (rects.empty()) {
    return;
  }
  std::unique_ptr<FragmentProcessor> colorFP;
  std::unique_ptr<FragmentProcessor> maskFP;
  if (colors) {
    maskFP = TextureMaskFragmentProcessor::MakeUseLocalCoord(atlas, Matrix::I(), false);
  } else {
    colorFP = TextureFragmentProcessor::Make(atlas, nullptr, Matrix::I());
  }
  draw(GLFillRectOp::Make(rects, matrices, localMatrices, colorVector), std::move(colorFP),
       std::move(maskFP), false);
}

GLDrawer* GLCanvas::getDrawer() {
  if (_drawer == nullptr) {
    _drawer = GLDrawer::Make(getContext());
  }
  return _drawer.get();
}

Matrix GLCanvas::getViewMatrix() {
  auto matrix = state->matrix;
  if (surface->origin() == ImageOrigin::BottomLeft) {
    // Flip Y
    matrix.postScale(1, -1);
    matrix.postTranslate(0, static_cast<float>(surface->height()));
  }
  return matrix;
}

void GLCanvas::draw(std::unique_ptr<GLDrawOp> op, std::unique_ptr<FragmentProcessor> color,
                    std::unique_ptr<FragmentProcessor> mask, bool aa) {
  auto* drawer = getDrawer();
  if (drawer == nullptr) {
    return;
  }
  auto renderTarget = surface->getRenderTarget();
  auto aaType = AAType::None;
  if (renderTarget->sampleCount() > 1) {
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
  DrawArgs args;
  if (color) {
    args.colors.push_back(std::move(color));
  }
  if (state->alpha != 1.0) {
    args.colors.push_back(AlphaFragmentProcessor::Make(state->alpha));
  }
  if (mask) {
    args.masks.push_back(std::move(mask));
  }
  auto clipMask = getClipMask(op->bounds(), &args.scissorRect);
  if (clipMask) {
    args.masks.push_back(std::move(clipMask));
  }
  args.context = surface->getContext();
  args.blendMode = state->blendMode;
  args.renderTarget = renderTarget.get();
  args.renderTargetTexture = surface->getTexture();
  args.aa = aaType;
  drawer->draw(std::move(args), std::move(op));
}
}  // namespace tgfx
