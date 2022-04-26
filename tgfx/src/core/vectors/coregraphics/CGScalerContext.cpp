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

#include "core/vectors/coregraphics/CGScalerContext.h"
#include "core/utils/Log.h"
#include "core/utils/MathExtra.h"
#include "platform/apple/BitmapContextUtil.h"
#include "tgfx/core/PathEffect.h"

namespace tgfx {
inline float Interpolate(const float& a, const float& b, const float& t) {
  return a + (b - a) * t;
}

/**
 * Interpolate along the function described by (keys[length], values[length])
 * for the passed searchKey. SearchKeys outside the range keys[0]-keys[Length]
 * clamp to the min or max value. This function assumes the number of pairs
 * (length) will be small and a linear search is used.
 *
 * Repeated keys are allowed for discontinuous functions (so long as keys is
 * monotonically increasing). If key is the value of a repeated scalar in
 * keys the first one will be used.
 */
float FloatInterpFunc(float searchKey, const float keys[], const float values[], int length) {
  int right = 0;
  while (right < length && keys[right] < searchKey) {
    ++right;
  }
  // Could use sentinel values to eliminate conditionals, but since the
  // tables are taken as input, a simpler format is better.
  if (right == length) {
    return values[length - 1];
  }
  if (right == 0) {
    return values[0];
  }
  // Otherwise, interpolate between right - 1 and right.
  float leftKey = keys[right - 1];
  float rightKey = keys[right];
  float t = (searchKey - leftKey) / (rightKey - leftKey);
  return Interpolate(values[right - 1], values[right], t);
}

static CGAffineTransform MatrixToCGAffineTransform(const Matrix& matrix) {
  return CGAffineTransformMake(
      static_cast<CGFloat>(matrix.getScaleX()), -static_cast<CGFloat>(matrix.getSkewY()),
      -static_cast<CGFloat>(matrix.getSkewX()), static_cast<CGFloat>(matrix.getScaleY()),
      static_cast<CGFloat>(matrix.getTranslateX()), static_cast<CGFloat>(matrix.getTranslateY()));
}

static constexpr float ITALIC_SKEW = -0.20f;
static const float kStdFakeBoldInterpKeys[] = {
    9.f,
    36.f,
};
static const float kStdFakeBoldInterpValues[] = {
    1.f / 24.f,
    1.f / 32.f,
};

std::unique_ptr<CGScalerContext> CGScalerContext::Make(std::shared_ptr<Typeface> typeface,
                                                       float size, bool fauxBold, bool fauxItalic,
                                                       bool verticalText) {
  if (typeface == nullptr) {
    return nullptr;
  }
  CGScalerContextRec rec;
  rec.textSize = size;
  if (fauxBold) {
    auto fauxBoldScale = FloatInterpFunc(size, kStdFakeBoldInterpKeys, kStdFakeBoldInterpValues, 2);
    rec.fauxBoldSize = size * fauxBoldScale;
  }
  rec.skewX = fauxItalic ? ITALIC_SKEW : 0;
  rec.verticalText = verticalText;
  return std::unique_ptr<CGScalerContext>(new CGScalerContext(std::move(typeface), rec));
}

CGScalerContext::CGScalerContext(std::shared_ptr<Typeface> typeface, CGScalerContextRec rec)
    : _typeface(std::move(typeface)), rec(rec) {
  CTFontRef font = std::static_pointer_cast<CGTypeface>(_typeface)->ctFont;
  auto matrix = Matrix::I();
  matrix.postSkew(rec.skewX, 0.f);
  transform = MatrixToCGAffineTransform(matrix);
  ctFont =
      CTFontCreateCopyWithAttributes(font, static_cast<CGFloat>(rec.textSize), nullptr, nullptr);
}

CGScalerContext::~CGScalerContext() {
  if (ctFont) {
    CFRelease(ctFont);
  }
}

FontMetrics CGScalerContext::generateFontMetrics() {
  FontMetrics metrics;
  auto theBounds = CTFontGetBoundingBox(ctFont);
  metrics.top = static_cast<float>(-CGRectGetMaxY(theBounds));
  metrics.ascent = static_cast<float>(-CTFontGetAscent(ctFont));
  metrics.descent = static_cast<float>(CTFontGetDescent(ctFont));
  metrics.bottom = static_cast<float>(-CGRectGetMinY(theBounds));
  metrics.leading = static_cast<float>(CTFontGetLeading(ctFont));
  metrics.xMin = static_cast<float>(CGRectGetMinX(theBounds));
  metrics.xMax = static_cast<float>(CGRectGetMaxX(theBounds));
  metrics.xHeight = static_cast<float>(CTFontGetXHeight(ctFont));
  metrics.capHeight = static_cast<float>(CTFontGetCapHeight(ctFont));
  metrics.underlineThickness = static_cast<float>(CTFontGetUnderlineThickness(ctFont));
  metrics.underlinePosition = -static_cast<float>(CTFontGetUnderlinePosition(ctFont));
  return metrics;
}

GlyphMetrics CGScalerContext::generateGlyphMetrics(GlyphID glyphID) {
  GlyphMetrics glyph;
  const auto cgGlyph = static_cast<CGGlyph>(glyphID);
  // The following block produces cgAdvance in CG units (pixels, y up).
  CGSize cgAdvance;
  if (rec.verticalText) {
    CTFontGetAdvancesForGlyphs(ctFont, kCTFontOrientationVertical, &cgGlyph, &cgAdvance, 1);
    // Vertical advances are returned as widths instead of heights.
    std::swap(cgAdvance.width, cgAdvance.height);
  } else {
    CTFontGetAdvancesForGlyphs(ctFont, kCTFontOrientationHorizontal, &cgGlyph, &cgAdvance, 1);
  }
  cgAdvance = CGSizeApplyAffineTransform(cgAdvance, transform);
  glyph.advanceX = static_cast<float>(cgAdvance.width);
  glyph.advanceY = static_cast<float>(cgAdvance.height);
  // Glyphs are always drawn from the horizontal origin. The caller must manually use the result
  // of CTFontGetVerticalTranslationsForGlyphs to calculate where to draw the glyph for vertical
  // glyphs. As a result, always get the horizontal bounds of a glyph and translate it if the
  // glyph is vertical. This avoids any disagreement between the various means of retrieving
  // vertical metrics.
  // CTFontGetBoundingRectsForGlyphs produces cgBounds in CG units (pixels, y up).
  CGRect cgBounds;
  CTFontGetBoundingRectsForGlyphs(ctFont, kCTFontOrientationHorizontal, &cgGlyph, &cgBounds, 1);
  cgBounds = CGRectApplyAffineTransform(cgBounds, transform);
  if (CGRectIsEmpty(cgBounds)) {
    return glyph;
  }
  // Convert cgBounds to Glyph units (pixels, y down).
  auto rect = Rect::MakeXYWH(static_cast<float>(cgBounds.origin.x),
                             static_cast<float>(-cgBounds.origin.y - cgBounds.size.height),
                             static_cast<float>(cgBounds.size.width),
                             static_cast<float>(cgBounds.size.height));

  // We're trying to pack left and top into int16_t,
  // and width and height into uint16_t
  if (!Rect::MakeXYWH(-32767, -32767, 65535, 65535).contains(rect)) {
    return glyph;
  }
  if (rec.fauxBoldSize != 0) {
    rect.outset(rec.fauxBoldSize, rec.fauxBoldSize);
  }
  rect.roundOut();
  glyph.left = rect.left;
  glyph.top = rect.top;
  glyph.width = rect.width();
  glyph.height = rect.height();
  return glyph;
}

Point CGScalerContext::getGlyphVerticalOffset(GlyphID glyphID) const {
  // CTFontGetVerticalTranslationsForGlyphs produces cgVertOffset in CG units (pixels, y up).
  CGSize cgVertOffset;
  CTFontGetVerticalTranslationsForGlyphs(ctFont, &glyphID, &cgVertOffset, 1);
  cgVertOffset = CGSizeApplyAffineTransform(cgVertOffset, transform);
  Point vertOffset = {static_cast<float>(cgVertOffset.width),
                      static_cast<float>(cgVertOffset.height)};
  // From CG units (pixels, y up) to Glyph units (pixels, y down).
  vertOffset.y = -vertOffset.y;
  return vertOffset;
}

class CTPathGeometrySink {
 public:
  Path path() {
    return _path;
  }

  static void ApplyElement(void* ctx, const CGPathElement* element) {
    CTPathGeometrySink& self = *static_cast<CTPathGeometrySink*>(ctx);
    CGPoint* points = element->points;
    switch (element->type) {
      case kCGPathElementMoveToPoint:
        self.started = false;
        self.current = points[0];
        break;
      case kCGPathElementAddLineToPoint:
        if (self.currentIsNot(points[0])) {
          self.goingTo(points[0]);
          self._path.lineTo(static_cast<float>(points[0].x), -static_cast<float>(points[0].y));
        }
        break;
      case kCGPathElementAddQuadCurveToPoint:
        if (self.currentIsNot(points[0]) || self.currentIsNot(points[1])) {
          self.goingTo(points[1]);
          self._path.quadTo(static_cast<float>(points[0].x), -static_cast<float>(points[0].y),
                            static_cast<float>(points[1].x), -static_cast<float>(points[1].y));
        }
        break;
      case kCGPathElementAddCurveToPoint:
        if (self.currentIsNot(points[0]) || self.currentIsNot(points[1]) ||
            self.currentIsNot(points[2])) {
          self.goingTo(points[2]);
          self._path.cubicTo(static_cast<float>(points[0].x), -static_cast<float>(points[0].y),
                             static_cast<float>(points[1].x), -static_cast<float>(points[1].y),
                             static_cast<float>(points[2].x), -static_cast<float>(points[2].y));
        }
        break;
      case kCGPathElementCloseSubpath:
        if (self.started) {
          self._path.close();
        }
        break;
      default:
        LOGE("Unknown path element!");
        break;
    }
  }

 private:
  void goingTo(const CGPoint pt) {
    if (!started) {
      started = true;
      _path.moveTo(static_cast<float>(current.x), -static_cast<float>(current.y));
    }
    current = pt;
  }

  bool currentIsNot(const CGPoint pt) const {
    return current.x != pt.x || current.y != pt.y;
  }

  Path _path = {};
  bool started = false;
  CGPoint current = {0, 0};
};

bool CGScalerContext::generatePath(GlyphID glyphID, Path* path) {
  auto fontFormat = CTFontCopyAttribute(ctFont, kCTFontFormatAttribute);
  if (!fontFormat) {
    return false;
  }
  SInt16 format;
  CFNumberGetValue(static_cast<CFNumberRef>(fontFormat), kCFNumberSInt16Type, &format);
  if (format == kCTFontFormatUnrecognized || format == kCTFontFormatBitmap) {
    return false;
  }
  CGAffineTransform xform = transform;
  auto cgGlyph = static_cast<CGGlyph>(glyphID);
  auto cgPath = CTFontCreatePathForGlyph(ctFont, cgGlyph, &xform);
  if (cgPath) {
    CTPathGeometrySink sink;
    CGPathApply(cgPath, &sink, CTPathGeometrySink::ApplyElement);
    *path = sink.path();
    CFRelease(cgPath);
    if (rec.fauxBoldSize != 0) {
      auto strokePath = *path;
      auto pathEffect = PathEffect::MakeStroke(Stroke(rec.fauxBoldSize));
      pathEffect->applyTo(&strokePath);
      path->addPath(strokePath, PathOp::Union);
    }
  } else {
    path->reset();
  }
  return true;
}

std::shared_ptr<TextureBuffer> CGScalerContext::generateImage(GlyphID glyphID, Matrix* matrix) {
  CGRect cgBounds;
  CTFontGetBoundingRectsForGlyphs(ctFont, kCTFontOrientationHorizontal, &glyphID, &cgBounds, 1);
  cgBounds = CGRectApplyAffineTransform(cgBounds, transform);
  CTFontSymbolicTraits traits = CTFontGetSymbolicTraits(ctFont);
  auto hasColor = static_cast<bool>(traits & kCTFontTraitColorGlyphs);
  auto pixelBuffer = PixelBuffer::Make(static_cast<int>(cgBounds.size.width),
                                       static_cast<int>(cgBounds.size.height), !hasColor);
  if (pixelBuffer == nullptr) {
    return nullptr;
  }
  auto pixels = pixelBuffer->lockPixels();
  auto cgContext = CreateBitmapContext(pixelBuffer->info(), pixels);
  if (cgContext == nullptr) {
    pixelBuffer->unlockPixels();
    return nullptr;
  }
  CGContextClearRect(cgContext, CGRectMake(0, 0, cgBounds.size.width, cgBounds.size.height));
  CGContextSetTextMatrix(cgContext, transform);
  CGContextSetBlendMode(cgContext, kCGBlendModeCopy);
  CGContextSetTextDrawingMode(cgContext, kCGTextFill);
  CGContextSetShouldAntialias(cgContext, true);
  CGContextSetShouldSmoothFonts(cgContext, true);
  auto point = CGPointMake(-cgBounds.origin.x, -cgBounds.origin.y);
  CTFontDrawGlyphs(ctFont, &glyphID, &point, 1, cgContext);
  CGContextRelease(cgContext);
  pixelBuffer->unlockPixels();
  if (matrix) {
    matrix->setTranslate(static_cast<float>(cgBounds.origin.x),
                         static_cast<float>(-cgBounds.origin.y - cgBounds.size.height));
  }
  return pixelBuffer;
}
}  // namespace tgfx
