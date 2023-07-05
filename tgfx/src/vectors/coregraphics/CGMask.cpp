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

#include "CGMask.h"
#include "CGTypeface.h"
#include "CGUtil.h"
#include "core/SimpleTextBlob.h"
#include "platform/apple/BitmapContextUtil.h"
#include "tgfx/core/Mask.h"
#include "tgfx/core/Pixmap.h"

namespace tgfx {
static void Iterator(PathVerb verb, const Point points[4], void* info) {
  auto cgPath = reinterpret_cast<CGMutablePathRef>(info);
  switch (verb) {
    case PathVerb::Move:
      CGPathMoveToPoint(cgPath, nullptr, points[0].x, points[0].y);
      break;
    case PathVerb::Line:
      CGPathAddLineToPoint(cgPath, nullptr, points[1].x, points[1].y);
      break;
    case PathVerb::Quad:
      CGPathAddQuadCurveToPoint(cgPath, nullptr, points[1].x, points[1].y, points[2].x,
                                points[2].y);
      break;
    case PathVerb::Cubic:
      CGPathAddCurveToPoint(cgPath, nullptr, points[1].x, points[1].y, points[2].x, points[2].y,
                            points[3].x, points[3].y);
      break;
    case PathVerb::Close:
      CGPathCloseSubpath(cgPath);
      break;
  }
}

std::shared_ptr<Mask> Mask::Make(int width, int height, bool tryHardware) {
  auto pixelRef = PixelRef::Make(width, height, true, tryHardware);
  if (pixelRef == nullptr) {
    return nullptr;
  }
  pixelRef->clear();
  return std::make_shared<CGMask>(std::move(pixelRef));
}

void CGMask::onFillPath(const Path& path, const Matrix& matrix, bool needsGammaCorrection) {
  if (path.isEmpty()) {
    return;
  }
  auto pixels = pixelRef->lockWritablePixels();
  if (pixels == nullptr) {
    return;
  }
  const auto& info = pixelRef->info();
  auto cgContext = CreateBitmapContext(info, pixels);
  if (cgContext == nullptr) {
    pixelRef->unlockPixels();
    return;
  }

  auto finalPath = path;
  auto totalMatrix = matrix;
  totalMatrix.postScale(1, -1);
  totalMatrix.postTranslate(0, static_cast<float>(info.height()));
  finalPath.transform(totalMatrix);
  auto bounds = finalPath.getBounds();
  bounds.roundOut();
  markContentDirty(bounds, true);
  auto cgPath = CGPathCreateMutable();
  finalPath.decompose(Iterator, cgPath);

  CGContextSetShouldAntialias(cgContext, true);
  static const CGFloat white[] = {1.f, 1.f, 1.f, 1.f};
  if (finalPath.isInverseFillType()) {
    auto rect = CGRectMake(0.f, 0.f, info.width(), info.height());
    CGContextAddRect(cgContext, rect);
    CGContextSetFillColor(cgContext, white);
    CGContextFillPath(cgContext);
    CGContextAddPath(cgContext, cgPath);
    if (finalPath.getFillType() == PathFillType::InverseWinding) {
      CGContextClip(cgContext);
    } else {
      CGContextEOClip(cgContext);
    }
    CGContextClearRect(cgContext, rect);
  } else {
    CGContextAddPath(cgContext, cgPath);
    CGContextSetFillColor(cgContext, white);
    if (finalPath.getFillType() == PathFillType::Winding) {
      CGContextFillPath(cgContext);
    } else {
      CGContextEOFillPath(cgContext);
    }
  }
  CGContextRelease(cgContext);
  CGPathRelease(cgPath);
  pixelRef->unlockPixels();
  if (needsGammaCorrection) {
    applyGamma(bounds, true);
  }
}

bool CGMask::onFillText(const TextBlob* textBlob, const Stroke* stroke, const Matrix& matrix) {
  if (textBlob == nullptr || stroke) {
    return false;
  }
  auto blob = static_cast<const SimpleTextBlob*>(textBlob);
  if (blob->getFont().isFauxBold()) {
    return false;
  }
  auto pixels = pixelRef->lockWritablePixels();
  auto cgContext = CreateBitmapContext(pixelRef->info(), pixels);
  if (cgContext == nullptr) {
    pixelRef->unlockPixels();
    return false;
  }
  CGContextClearRect(cgContext, CGRectMake(0, 0, width(), height()));
  auto totalMatrix = matrix;
  totalMatrix.preScale(1.f, -1.f);
  totalMatrix.postScale(1.f, -1.f);
  totalMatrix.postTranslate(0, static_cast<float>(height()));
  const auto& font = blob->getFont();
  if (font.isFauxItalic()) {
    totalMatrix.postSkew(ITALIC_SKEW, 0);
  }
  auto transform = MatrixToCGAffineTransform(totalMatrix);
  CGContextSetTextMatrix(cgContext, transform);
  CGContextSetBlendMode(cgContext, kCGBlendModeCopy);
  CGContextSetTextDrawingMode(cgContext, kCGTextFill);
  CGContextSetGrayFillColor(cgContext, 1.0f, 1.0f);
  CGContextSetShouldAntialias(cgContext, true);
  CGContextSetShouldSmoothFonts(cgContext, true);
  CGContextSetAllowsFontSubpixelQuantization(cgContext, false);
  CGContextSetShouldSubpixelQuantizeFonts(cgContext, false);
  CGContextSetAllowsFontSubpixelPositioning(cgContext, true);
  CGContextSetShouldSubpixelPositionFonts(cgContext, true);
  CTFontRef ctFont = std::static_pointer_cast<CGTypeface>(font.getTypeface())->getCTFont();
  ctFont = CTFontCreateCopyWithAttributes(ctFont, static_cast<CGFloat>(font.getSize()), nullptr,
                                          nullptr);
  std::vector<CGPoint> points;
  auto count = blob->getGlyphIDs().size();
  for (size_t i = 0; i < count; ++i) {
    auto position = blob->getPositions()[i];
    points.emplace_back(CGPointMake(position.x, position.y));
  }
  CTFontDrawGlyphs(ctFont, static_cast<const CGGlyph*>(blob->getGlyphIDs().data()),
                   static_cast<const CGPoint*>(points.data()), count, cgContext);
  CFRelease(ctFont);
  CGContextRelease(cgContext);
  pixelRef->unlockPixels();
  return true;
}
}  // namespace tgfx
