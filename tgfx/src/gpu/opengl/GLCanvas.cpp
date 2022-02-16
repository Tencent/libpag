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
#include "core/Mask.h"
#include "core/PathEffect.h"
#include "core/TextBlob.h"
#include "core/utils/MathExtra.h"
#include "gpu/AlphaFragmentProcessor.h"
#include "gpu/ColorShader.h"
#include "gpu/TextureFragmentProcessor.h"
#include "gpu/TextureMaskFragmentProcessor.h"

namespace tgfx {
GLCanvas::GLCanvas(Surface* surface) : Canvas(surface) {
}

void GLCanvas::clear() {
  static_cast<GLSurface*>(surface)->getRenderTarget()->clear(GLContext::Unwrap(getContext()));
}

void GLCanvas::drawTexture(const Texture* texture, const Texture* mask, bool inverted) {
  drawTexture(texture, nullptr, mask, inverted);
}

Surface* GLCanvas::getClipSurface() {
  if (_clipSurface == nullptr) {
    _clipSurface = Surface::Make(getContext(), surface->width(), surface->height(), true);
    if (_clipSurface == nullptr) {
      _clipSurface = Surface::Make(getContext(), surface->width(), surface->height());
    }
  }
  return _clipSurface.get();
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

std::unique_ptr<FragmentProcessor> GLCanvas::getClipMask(const Rect& deviceQuad,
                                                         Rect* scissorRect) {
  auto& clipPath = globalPaint.clip;
  if (!clipPath.contains(deviceQuad)) {
    auto rect = Rect::MakeEmpty();
    if (clipPath.asRect(&rect) && IsPixelAligned(rect)) {
      if (scissorRect) {
        *scissorRect = rect;
        scissorRect->round();
      }
    } else {
      auto clipSurface = getClipSurface();
      auto clipCanvas = clipSurface->getCanvas();
      clipCanvas->clear();
      Paint paint = {};
      paint.setColor(Color::Black());
      clipCanvas->drawPath(globalPaint.clip, paint);
      return TextureMaskFragmentProcessor::MakeUseDeviceCoord(clipSurface->getTexture().get(),
                                                              surface->origin());
    }
  }
  return nullptr;
}

Rect GLCanvas::clipLocalQuad(Rect localQuad, Rect* outClippedDeviceQuad) {
  auto deviceQuad = globalPaint.matrix.mapRect(localQuad);
  auto surfaceBounds =
      Rect::MakeWH(static_cast<float>(surface->width()), static_cast<float>(surface->height()));
  auto clippedDeviceQuad = deviceQuad;
  if (!clippedDeviceQuad.intersect(surfaceBounds)) {
    return Rect::MakeEmpty();
  }
  auto clippedLocalQuad = localQuad;
  if (globalPaint.matrix.getSkewX() == 0 && globalPaint.matrix.getSkewY() == 0 &&
      clippedDeviceQuad != deviceQuad) {
    Matrix inverse = Matrix::I();
    globalPaint.matrix.invert(&inverse);
    clippedLocalQuad = inverse.mapRect(clippedDeviceQuad);
  }
  if (outClippedDeviceQuad) {
    *outClippedDeviceQuad = clippedDeviceQuad;
  }
  return clippedLocalQuad;
}

void GLCanvas::drawTexture(const Texture* texture, const RGBAAALayout* layout, const Texture* mask,
                           bool inverted) {
  if (texture == nullptr) {
    return;
  }
  auto width = static_cast<float>(layout ? layout->width : texture->width());
  auto height = static_cast<float>(layout ? layout->height : texture->height());
  auto clippedDeviceQuad = Rect::MakeEmpty();
  auto clippedLocalQuad = clipLocalQuad(Rect::MakeWH(width, height), &clippedDeviceQuad);
  if (clippedLocalQuad.isEmpty()) {
    return;
  }
  auto localMatrix = Matrix::I();
  auto scale = texture->getTextureCoord(clippedLocalQuad.width(), clippedLocalQuad.height()) -
               texture->getTextureCoord(0, 0);
  localMatrix.postScale(scale.x, scale.y);
  auto translate = texture->getTextureCoord(clippedLocalQuad.x(), clippedLocalQuad.y());
  localMatrix.postTranslate(translate.x, translate.y);
  auto processor = TextureFragmentProcessor::Make(texture, layout, localMatrix);
  if (processor == nullptr) {
    return;
  }
  draw(clippedLocalQuad, clippedDeviceQuad, GLFillRectOp::Make(), std::move(processor),
       TextureMaskFragmentProcessor::MakeUseLocalCoord(mask, Matrix::I(), inverted), true);
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

static std::unique_ptr<GLDrawOp> MakeSimplePathOp(const Path& path) {
  if (path.asRect(nullptr)) {
    return GLFillRectOp::Make();
  }
  RRect rRect;
  if (path.asRRect(&rRect)) {
    return GLRRectOp::Make(rRect);
  }
  return nullptr;
}

void GLCanvas::fillPath(const Path& path, const Shader* shader) {
  if (path.isEmpty()) {
    return;
  }
  auto bounds = path.getBounds();
  auto clippedLocalQuad = clipLocalQuad(bounds, nullptr);
  if (clippedLocalQuad.isEmpty()) {
    return;
  }
  auto op = MakeSimplePathOp(path);
  if (op) {
    auto localMatrix = Matrix::MakeScale(bounds.width(), bounds.height());
    localMatrix.postTranslate(bounds.x(), bounds.y());
    auto args = FPArgs(getContext(), localMatrix);
    draw(bounds, bounds, std::move(op), shader->asFragmentProcessor(args));
    return;
  }
  auto quad = globalPaint.matrix.mapRect(clippedLocalQuad);
  auto width = ceilf(quad.width());
  auto height = ceilf(quad.height());
  auto mask = Mask::Make(static_cast<int>(width), static_cast<int>(height));
  if (!mask) {
    return;
  }
  auto totalMatrix = globalPaint.matrix;
  auto matrix = Matrix::MakeTrans(-quad.x(), -quad.y());
  matrix.postScale(width / quad.width(), height / quad.height());
  totalMatrix.postConcat(matrix);
  mask->setMatrix(totalMatrix);
  mask->fillPath(path);
  auto maskTexture = mask->makeTexture(getContext());
  drawMask(quad, maskTexture.get(), shader);
}

void GLCanvas::drawMask(Rect quad, const Texture* mask, const Shader* shader) {
  if (mask == nullptr || shader == nullptr) {
    return;
  }
  auto scale =
      mask->getTextureCoord(static_cast<float>(mask->width()), static_cast<float>(mask->height()));
  auto localMatrix = Matrix::I();
  localMatrix.postScale(quad.width(), quad.height());
  localMatrix.postTranslate(quad.x(), quad.y());
  auto invert = Matrix::I();
  globalPaint.matrix.invert(&invert);
  localMatrix.postConcat(invert);
  auto args = FPArgs(getContext(), localMatrix);
  save();
  resetMatrix();
  draw(quad, quad, GLFillRectOp::Make(), shader->asFragmentProcessor(args),
       TextureMaskFragmentProcessor::MakeUseLocalCoord(mask, Matrix::MakeScale(scale.x, scale.y)));
  restore();
}

void GLCanvas::drawGlyphs(const GlyphID glyphIDs[], const Point positions[], size_t glyphCount,
                          const Font& font, const Paint& paint) {
  auto textBlob = TextBlob::MakeFrom(glyphIDs, positions, glyphCount, font);
  if (textBlob == nullptr) {
    return;
  }
  if (font.getTypeface()->hasColor()) {
    drawColorGlyphs(glyphIDs, positions, glyphCount, font, paint);
    return;
  }
  Path path = {};
  auto stroke = paint.getStyle() == PaintStyle::Stroke ? paint.getStroke() : nullptr;
  if (textBlob->getPath(&path, stroke)) {
    auto shader = Shader::MakeColorShader(paint.getColor());
    fillPath(path, shader.get());
    return;
  }
  drawMaskGlyphs(textBlob.get(), paint);
}

void GLCanvas::drawColorGlyphs(const GlyphID glyphIDs[], const Point positions[], size_t glyphCount,
                               const Font& font, const Paint& paint) {
  auto scaleX = globalPaint.matrix.getScaleX();
  auto skewY = globalPaint.matrix.getSkewY();
  auto scale = std::sqrt(scaleX * scaleX + skewY * skewY);
  auto scaleFont = font.makeWithSize(font.getSize() * scale);
  for (size_t i = 0; i < glyphCount; ++i) {
    const auto& glyphID = glyphIDs[i];
    const auto& position = positions[i];

    auto glyphMatrix = Matrix::I();
    auto glyphBuffer = scaleFont.getGlyphImage(glyphID, &glyphMatrix);
    if (glyphBuffer == nullptr) {
      continue;
    }
    glyphMatrix.postScale(1.f / scale, 1.f / scale);
    glyphMatrix.postTranslate(position.x, position.y);
    save();
    concat(glyphMatrix);
    globalPaint.alpha *= paint.getAlpha();
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
  auto bounds = textBlob->getBounds(stroke);
  auto clippedDeviceQuad = Rect::MakeEmpty();
  auto clippedLocalQuad = clipLocalQuad(bounds, &clippedDeviceQuad);
  if (clippedLocalQuad.isEmpty()) {
    return;
  }
  auto width = ceilf(clippedDeviceQuad.width());
  auto height = ceilf(clippedDeviceQuad.height());
  auto mask = Mask::Make(static_cast<int>(width), static_cast<int>(height));
  if (mask == nullptr) {
    return;
  }
  auto totalMatrix = globalPaint.matrix;
  auto matrix = Matrix::I();
  matrix.postTranslate(-clippedDeviceQuad.x(), -clippedDeviceQuad.y());
  matrix.postScale(width / clippedDeviceQuad.width(), height / clippedDeviceQuad.height());
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
  drawMask(clippedDeviceQuad, texture.get(), shader.get());
}

GLDrawer* GLCanvas::getDrawer() {
  if (_drawer == nullptr) {
    _drawer = GLDrawer::Make(getContext());
  }
  return _drawer.get();
}

Matrix GLCanvas::getViewMatrix() {
  auto matrix = globalPaint.matrix;
  if (surface->origin() == ImageOrigin::BottomLeft) {
    // Flip Y
    matrix.postScale(1, -1);
    matrix.postTranslate(0, static_cast<float>(surface->height()));
  }
  return matrix;
}

void GLCanvas::draw(const Rect& localQuad, const Rect& deviceQuad, std::unique_ptr<GLDrawOp> op,
                    std::unique_ptr<FragmentProcessor> color,
                    std::unique_ptr<FragmentProcessor> mask, bool aa) {
  auto* drawer = getDrawer();
  if (drawer == nullptr) {
    return;
  }
  auto renderTarget = static_cast<GLSurface*>(surface)->getRenderTarget();
  auto aaType = AAType::None;
  if (renderTarget->usesMSAA()) {
    aaType = AAType::MSAA;
  } else if (aa && !IsPixelAligned(deviceQuad)) {
    aaType = AAType::Coverage;
  } else {
    auto& matrix = globalPaint.matrix;
    auto rotation = std::round(RadiansToDegrees(atan2f(matrix.getSkewX(), matrix.getScaleX())));
    if (static_cast<int>(rotation) % 90 != 0) {
      aaType = AAType::Coverage;
    }
  }
  DrawArgs args;
  if (color) {
    args.colors.push_back(std::move(color));
  }
  if (globalPaint.alpha != 1.0) {
    args.colors.push_back(AlphaFragmentProcessor::Make(globalPaint.alpha));
  }
  if (mask) {
    args.masks.push_back(std::move(mask));
  }
  auto clipMask = getClipMask(deviceQuad, &args.scissorRect);
  if (clipMask) {
    args.masks.push_back(std::move(clipMask));
  }
  args.context = surface->getContext();
  args.blendMode = globalPaint.blendMode;
  args.viewMatrix = getViewMatrix();
  args.renderTarget = renderTarget.get();
  args.renderTargetTexture = surface->getTexture();
  args.aa = aaType;
  args.rectToDraw = localQuad;
  drawer->draw(std::move(args), std::move(op));
}
}  // namespace tgfx
