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

#include "FTScalerContext.h"
#include <cmath>
#include "ft2build.h"
#include FT_BITMAP_H
#include FT_OUTLINE_H
#include FT_SIZES_H
#include FT_TRUETYPE_TABLES_H
#include "FTUtil.h"
#include "core/utils/Log.h"
#include "core/utils/MathExtra.h"
#include "skcms.h"
#include "tgfx/core/Bitmap.h"

namespace tgfx {
static float FTFixedToFloat(FT_Fixed x) {
  return static_cast<float>(x) * 1.52587890625e-5f;
}

static FT_Fixed FloatToFTFixed(float x) {
  static constexpr float MaxS32FitsInFloat = 2147483520.f;
  static constexpr float MinS32FitsInFloat = -MaxS32FitsInFloat;
  x = x < MaxS32FitsInFloat ? x : MaxS32FitsInFloat;
  x = x > MinS32FitsInFloat ? x : MinS32FitsInFloat;
  return static_cast<FT_Fixed>(x * (1 << 16));
}

static void ComputeGivensRotation(const Point& h, Matrix* G) {
  const auto& a = h.x;
  const auto& b = h.y;
  float c, s;
  if (0 == b) {
    c = copysignf(1.f, a);
    s = 0;
  } else if (0 == a) {
    c = 0;
    s = -copysignf(1.f, b);
  } else if (fabsf(b) > fabsf(a)) {
    auto t = a / b;
    auto u = copysignf(sqrtf(1.f + t * t), b);
    s = -1.f / u;
    c = -s * t;
  } else {
    auto t = b / a;
    auto u = copysignf(sqrtf(1.f + t * t), a);
    c = 1.f / u;
    s = -c * t;
  }

  G->setSinCos(s, c);
}

bool FTScalerContextRec::computeMatrices(Point* s, Matrix* sA) const {
  // A is the 'total' matrix.
  auto A = Matrix::MakeScale(textSize, textSize);
  if (skewX != 0) {
    A.postSkew(skewX, 0);
  }

  // GA is the matrix A with rotation removed.
  Matrix GA;
  bool skewedOrFlipped =
      A.getSkewX() != 0 || A.getSkewY() != 0 || A.getScaleX() < 0 || A.getScaleY() < 0;
  if (skewedOrFlipped) {
    // QR by Givens rotations. G is Q^T and GA is R. G is rotational (no reflections).
    // h is where A maps the horizontal baseline.
    Point h = Point::Make(1.f, 0);
    A.mapPoints(&h, 1);

    // G is the Givens Matrix for A (rotational matrix where GA[0][1] == 0).
    Matrix G;
    ComputeGivensRotation(h, &G);

    GA = G;
    GA.preConcat(A);
  } else {
    GA = A;
  }

  // If the 'total' matrix is singular, set the 'scale' to something finite and zero the matrices.
  // All underlying ports have issues with zero text size, so use the matricies to zero.
  // If one of the scale factors is less than 1/256 then an EM filling square will
  // never affect any pixels.
  // If there are any nonfinite numbers in the matrix, bail out and set the matrices to zero.
  if (fabsf(GA.getScaleX()) <= FLOAT_NEARLY_ZERO || fabsf(GA.getScaleY()) <= FLOAT_NEARLY_ZERO ||
      !GA.isFinite()) {
    s->x = 1.f;
    s->y = 1.f;
    sA->setScale(0, 0);
    return false;
  }

  // At this point, given GA, create s.
  s->x = fabsf(GA.getScaleX());
  s->y = fabsf(GA.getScaleY());

  // The 'remaining' matrix sA is the total matrix A without the scale.
  if (!skewedOrFlipped) {
    sA->reset();
  } else {
    *sA = A;
    sA->preScale(1.f / s->x, 1.f / s->y);
  }
  return true;
}

static constexpr float ITALIC_SKEW = -0.20f;

std::unique_ptr<FTScalerContext> FTScalerContext::Make(std::shared_ptr<Typeface> typeface,
                                                       float size, bool fauxBold, bool fauxItalic,
                                                       bool verticalText) {
  if (typeface == nullptr) {
    return nullptr;
  }
  FTScalerContextRec rec;
  rec.textSize = size;
  rec.embolden = fauxBold;
  rec.skewX = fauxItalic ? ITALIC_SKEW : 0;
  rec.verticalText = verticalText;
  auto scalerContext =
      std::unique_ptr<FTScalerContext>(new FTScalerContext(std::move(typeface), rec));
  if (!scalerContext->valid()) {
    return nullptr;
  }
  return scalerContext;
}

/**
 * Returns the bitmap strike equal to or just larger than the requested size.
 */
static FT_Int ChooseBitmapStrike(FT_Face face, FT_F26Dot6 scaleY) {
  if (face == nullptr) {
    return -1;
  }

  FT_Pos requestedPPEM = scaleY;  // FT_Bitmap_Size::y_ppem is in 26.6 format.
  FT_Int chosenStrikeIndex = -1;
  FT_Pos chosenPPEM = 0;
  for (FT_Int strikeIndex = 0; strikeIndex < face->num_fixed_sizes; ++strikeIndex) {
    FT_Pos strikePPEM = face->available_sizes[strikeIndex].y_ppem;
    if (strikePPEM == requestedPPEM) {
      // exact match - our search stops here
      return strikeIndex;
    } else if (chosenPPEM < requestedPPEM) {
      // attempt to increase chosenPPEM
      if (chosenPPEM < strikePPEM) {
        chosenPPEM = strikePPEM;
        chosenStrikeIndex = strikeIndex;
      }
    } else {
      // attempt to decrease chosenPPEM, but not below requestedPPEM
      if (requestedPPEM < strikePPEM && strikePPEM < chosenPPEM) {
        chosenPPEM = strikePPEM;
        chosenStrikeIndex = strikeIndex;
      }
    }
  }
  return chosenStrikeIndex;
}

FTScalerContext::FTScalerContext(std::shared_ptr<Typeface> typeFace, FTScalerContextRec rec)
    : typeface(std::move(typeFace)), rec(rec) {
  std::lock_guard<std::mutex> lockGuard(FTMutex());
  _face = static_cast<FTTypeface*>(typeface.get())->_face.get();
  loadGlyphFlags |= FT_LOAD_NO_BITMAP;
  // Always using FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH to get correct
  // advances, as fontconfig and cairo do.
  loadGlyphFlags |= FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH;
  loadGlyphFlags |= FT_LOAD_TARGET_NORMAL;
  if (FT_HAS_COLOR(_face->face)) {
    loadGlyphFlags |= FT_LOAD_COLOR;
  }
  if (rec.verticalText) {
    loadGlyphFlags |= FT_LOAD_VERTICAL_LAYOUT;
  }

  auto err = FT_New_Size(_face->face, &ftSize);
  if (err != FT_Err_Ok) {
    LOGE("FT_New_Size(%s) failed.", _face->face->family_name);
    return;
  }
  err = FT_Activate_Size(ftSize);
  if (err != FT_Err_Ok) {
    LOGE("FT_Activate_Size(%s) failed.", _face->face->family_name);
    return;
  }
  rec.computeMatrices(&scale, &matrix22Scalar);
  auto scaleX = FloatToFDot6(scale.x);
  auto scaleY = FloatToFDot6(scale.y);
  if (FT_IS_SCALABLE(_face->face)) {
    err = FT_Set_Char_Size(_face->face, scaleX, scaleY, 72, 72);
    if (err != FT_Err_Ok) {
      LOGE("FT_Set_CharSize(%s, %f, %f) failed.", _face->face->family_name, scaleX, scaleY);
      return;
    }
    // Adjust the matrix to reflect the actually chosen scale.
    // FreeType currently does not allow requesting sizes less than 1, this allows for scaling.
    // Don't do this at all sizes as that will interfere with hinting.
    if (scale.x < 1 || scale.y < 1) {
      auto unitsPerEm = static_cast<float>(_face->face->units_per_EM);
      const auto& metrics = _face->face->size->metrics;
      auto xPpem = unitsPerEm * FTFixedToFloat(metrics.x_scale) / 64.0f;
      auto yPpem = unitsPerEm * FTFixedToFloat(metrics.y_scale) / 64.0f;
      matrix22Scalar.preScale(scale.x / xPpem, scale.y / yPpem);
    }
  } else if (FT_HAS_FIXED_SIZES(_face->face)) {
    strikeIndex = ChooseBitmapStrike(_face->face, scaleY);
    if (strikeIndex == -1) {
      LOGE("No glyphs for font \"%s\" size %f.\n", _face->face->family_name, rec.textSize);
      return;
    }

    err = FT_Select_Size(_face->face, strikeIndex);
    if (err != 0) {
      LOGE("FT_Select_Size(%s, %d) failed.", _face->face->family_name, strikeIndex);
      strikeIndex = -1;
      return;
    }

    // Adjust the matrix to reflect the actually chosen scale.
    // It is likely that the ppem chosen was not the one requested, this allows for scaling.
    matrix22Scalar.preScale(scale.x / static_cast<float>(_face->face->size->metrics.x_ppem),
                            scale.y / static_cast<float>(_face->face->size->metrics.y_ppem));

    // FreeType documentation says:
    // FT_LOAD_NO_BITMAP -- Ignore bitmap strikes when loading.
    // Bitmap-only fonts ignore this flag.
    //
    // However, in FreeType 2.5.1 color bitmap only fonts do not ignore this flag.
    // Force this flag off for bitmap only fonts.
    loadGlyphFlags &= ~FT_LOAD_NO_BITMAP;
  } else {
    return;
  }
  matrix22.xx = FloatToFTFixed(matrix22Scalar.getScaleX());
  matrix22.xy = FloatToFTFixed(-matrix22Scalar.getSkewX());
  matrix22.yx = FloatToFTFixed(-matrix22Scalar.getSkewY());
  matrix22.yy = FloatToFTFixed(matrix22Scalar.getScaleY());
  FT_Set_Transform(_face->face, &matrix22, nullptr);
}

FTScalerContext::~FTScalerContext() {
  if (ftSize) {
    std::lock_guard<std::mutex> lockGuard(FTMutex());
    FT_Done_Size(ftSize);
  }
}

int FTScalerContext::setupSize() {
  FT_Error err = FT_Activate_Size(ftSize);
  if (err != 0) {
    return err;
  }
  FT_Set_Transform(_face->face, &matrix22, nullptr);
  return 0;
}

FontMetrics FTScalerContext::generateFontMetrics() {
  std::lock_guard<std::mutex> lockGuard(FTMutex());
  FontMetrics metrics;
  if (setupSize()) {
    return metrics;
  }
  auto face = _face->face;
  auto upem = static_cast<float>(FTTypeface::GetUnitsPerEm(face));

  // use the os/2 table as a source of reasonable defaults.
  auto xHeight = 0.0f;
  auto capHeight = 0.0f;
  auto* os2 = static_cast<TT_OS2*>(FT_Get_Sfnt_Table(face, FT_SFNT_OS2));
  if (os2) {
    xHeight = static_cast<float>(os2->sxHeight) / upem * scale.y;
    if (os2->version != 0xFFFF && os2->version >= 2) {
      capHeight = static_cast<float>(os2->sCapHeight) / upem * scale.y;
    }
  }

  // pull from format-specific metrics as needed
  float ascent;
  float descent;
  float leading;
  float xmin;
  float xmax;
  float ymin;
  float ymax;
  float underlineThickness;
  float underlinePosition;
  if (face->face_flags & FT_FACE_FLAG_SCALABLE) {  // scalable outline font
    // FreeType will always use HHEA metrics if they're not zero.
    // It completely ignores the OS/2 fsSelection::UseTypoMetrics bit.
    // It also ignores the VDMX tables, which are also of interest here
    // (and override everything else when they apply).
    static const int kUseTypoMetricsMask = (1 << 7);
    if (os2 && os2->version != 0xFFFF && (os2->fsSelection & kUseTypoMetricsMask)) {
      ascent = -static_cast<float>(os2->sTypoAscender) / upem;
      descent = -static_cast<float>(os2->sTypoDescender) / upem;
      leading = static_cast<float>(os2->sTypoLineGap) / upem;
    } else {
      ascent = -static_cast<float>(face->ascender) / upem;
      descent = -static_cast<float>(face->descender) / upem;
      leading = static_cast<float>(face->height + (face->descender - face->ascender)) / upem;
    }
    xmin = static_cast<float>(face->bbox.xMin) / upem;
    xmax = static_cast<float>(face->bbox.xMax) / upem;
    ymin = -static_cast<float>(face->bbox.yMin) / upem;
    ymax = -static_cast<float>(face->bbox.yMax) / upem;
    underlineThickness = static_cast<float>(face->underline_thickness) / upem;
    underlinePosition = -(static_cast<float>(face->underline_position) +
                          static_cast<float>(face->underline_thickness) / 2) /
                        upem;

    // we may be able to synthesize x_height and cap_height from outline
    if (xHeight == 0.f) {
      FT_BBox bbox;
      if (getCBoxForLetter('x', &bbox)) {
        xHeight = static_cast<float>(bbox.yMax) / 64.0f;
      }
    }
    if (capHeight == 0.f) {
      FT_BBox bbox;
      if (getCBoxForLetter('H', &bbox)) {
        capHeight = static_cast<float>(bbox.yMax) / 64.0f;
      }
    }
  } else if (strikeIndex != -1) {  // bitmap strike metrics
    auto xppem = static_cast<float>(face->size->metrics.x_ppem);
    auto yppem = static_cast<float>(face->size->metrics.y_ppem);
    ascent = -static_cast<float>(face->size->metrics.ascender) / (yppem * 64.0f);
    descent = -static_cast<float>(face->size->metrics.descender) / (yppem * 64.0f);
    leading = (static_cast<float>(face->size->metrics.height) / (yppem * 64.0f)) + ascent - descent;

    xmin = 0.0f;
    xmax = static_cast<float>(face->available_sizes[strikeIndex].width) / xppem;
    ymin = descent;
    ymax = ascent;

    underlineThickness = 0;
    underlinePosition = 0;

    auto* post = static_cast<TT_Postscript*>(FT_Get_Sfnt_Table(face, FT_SFNT_POST));
    if (post) {
      underlineThickness = static_cast<float>(post->underlineThickness) / upem;
      underlinePosition = -static_cast<float>(post->underlinePosition) / upem;
    }
  } else {
    return metrics;
  }

  // synthesize elements that were not provided by the os/2 table or format-specific metrics
  if (xHeight == 0.f) {
    xHeight = -ascent * scale.y;
  }
  if (capHeight == 0.f) {
    capHeight = -ascent * scale.y;
  }

  // disallow negative line spacing
  if (leading < 0.0f) {
    leading = 0.0f;
  }

  metrics.top = ymax * scale.y;
  metrics.ascent = ascent * scale.y;
  metrics.descent = descent * scale.y;
  metrics.bottom = ymin * scale.y;
  metrics.leading = leading * scale.y;
  metrics.xMin = xmin * scale.y;
  metrics.xMax = xmax * scale.y;
  metrics.xHeight = xHeight;
  metrics.capHeight = capHeight;
  metrics.underlineThickness = underlineThickness * scale.y;
  metrics.underlinePosition = underlinePosition * scale.y;
  return metrics;
}

bool FTScalerContext::getCBoxForLetter(char letter, FT_BBox* bbox) {
  auto face = _face->face;
  const auto glyph_id = FT_Get_Char_Index(face, letter);
  if (glyph_id == 0) {
    return false;
  }
  if (FT_Load_Glyph(face, glyph_id, loadGlyphFlags) != FT_Err_Ok) {
    return false;
  }
  emboldenIfNeeded(face, face->glyph, glyph_id);
  FT_Outline_Get_CBox(&face->glyph->outline, bbox);
  return true;
}

static constexpr int kOutlineEmboldenDivisor = 24;
// static constexpr int kOutlineEmboldenDivisorAndroid = 34;
//  See
//  http://freetype.sourceforge.net/freetype2/docs/reference/ft2-bitmap_handling.html#FT_Bitmap_Embolden
//  This value was chosen by eyeballing the result in Firefox and trying to match it.
static constexpr FT_Pos kBitmapEmboldenStrength = 1 << 6;
void FTScalerContext::emboldenIfNeeded(FT_Face face, FT_GlyphSlot glyph, GlyphID gid) const {
  if (!rec.embolden) {
    return;
  }
  switch (glyph->format) {
    case FT_GLYPH_FORMAT_OUTLINE:
      FT_Pos strength;
      strength =
          FT_MulFix(face->units_per_EM, face->size->metrics.y_scale) / kOutlineEmboldenDivisor;
      FT_Outline_Embolden(&glyph->outline, strength);
      break;
    case FT_GLYPH_FORMAT_BITMAP:
      if (!face->glyph->bitmap.buffer) {
        FT_Load_Glyph(face, gid, loadGlyphFlags);
      }
      FT_GlyphSlot_Own_Bitmap(glyph);
      FT_Bitmap_Embolden(glyph->library, &glyph->bitmap, kBitmapEmboldenStrength, 0);
      break;
    default:
      LOGE("unknown glyph format");
  }
}

class FTGeometrySink {
 public:
  explicit FTGeometrySink(Path* path) : path(path) {
  }

  static int Move(const FT_Vector* pt, void* ctx) {
    auto& self = *static_cast<FTGeometrySink*>(ctx);
    if (self.started) {
      self.path->close();
      self.started = false;
    }
    self.current = *pt;
    return 0;
  }

  static int Line(const FT_Vector* pt, void* ctx) {
    auto& self = *static_cast<FTGeometrySink*>(ctx);
    if (self.currentIsNot(pt)) {
      self.goingTo(pt);
      self.path->lineTo(FDot6ToFloat(pt->x), -FDot6ToFloat(pt->y));
    }
    return 0;
  }

  static int Conic(const FT_Vector* pt0, const FT_Vector* pt1, void* ctx) {
    auto& self = *static_cast<FTGeometrySink*>(ctx);
    if (self.currentIsNot(pt0) || self.currentIsNot(pt1)) {
      self.goingTo(pt1);
      self.path->quadTo(FDot6ToFloat(pt0->x), -FDot6ToFloat(pt0->y), FDot6ToFloat(pt1->x),
                        -FDot6ToFloat(pt1->y));
    }
    return 0;
  }

  static int Cubic(const FT_Vector* pt0, const FT_Vector* pt1, const FT_Vector* pt2, void* ctx) {
    auto& self = *static_cast<FTGeometrySink*>(ctx);
    if (self.currentIsNot(pt0) || self.currentIsNot(pt1) || self.currentIsNot(pt2)) {
      self.goingTo(pt2);
      self.path->cubicTo(FDot6ToFloat(pt0->x), -FDot6ToFloat(pt0->y), FDot6ToFloat(pt1->x),
                         -FDot6ToFloat(pt1->y), FDot6ToFloat(pt2->x), -FDot6ToFloat(pt2->y));
    }
    return 0;
  }

 private:
  bool currentIsNot(const FT_Vector* pt) const {
    return current.x != pt->x || current.y != pt->y;
  }

  void goingTo(const FT_Vector* pt) {
    if (!started) {
      started = true;
      path->moveTo(FDot6ToFloat(current.x), -FDot6ToFloat(current.y));
    }
    current = *pt;
  }

  Path* path;
  bool started = false;
  FT_Vector current = {0, 0};
};

static bool GenerateGlyphPath(FT_Face face, Path* path) {
  static constexpr FT_Outline_Funcs gFuncs{
      /*move_to =*/FTGeometrySink::Move,
      /*line_to =*/FTGeometrySink::Line,
      /*conic_to =*/FTGeometrySink::Conic,
      /*cubic_to =*/FTGeometrySink::Cubic,
      /*shift = */ 0,
      /*delta =*/0,
  };
  FTGeometrySink sink(path);
  auto err = FT_Outline_Decompose(&face->glyph->outline, &gFuncs, &sink);
  if (err != FT_Err_Ok) {
    path->reset();
    return false;
  }
  path->close();
  return true;
}

bool FTScalerContext::generatePath(GlyphID glyphID, Path* path) {
  std::lock_guard<std::mutex> lockGuard(FTMutex());
  auto face = _face->face;
  // FT_IS_SCALABLE is documented to mean the face contains outline glyphs.
  if (!FT_IS_SCALABLE(face) || this->setupSize()) {
    path->reset();
    return false;
  }
  auto flags = loadGlyphFlags;
  flags |= FT_LOAD_NO_BITMAP;  // ignore embedded bitmaps so we're sure to get the outline
  flags &= ~FT_LOAD_RENDER;    // don't scan convert (we just want the outline)

  auto err = FT_Load_Glyph(face, glyphID, flags);
  if (err != FT_Err_Ok || face->glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
    path->reset();
    return false;
  }
  emboldenIfNeeded(face, face->glyph, glyphID);

  if (!GenerateGlyphPath(face, path)) {
    path->reset();
    return false;
  }
  return true;
}

void FTScalerContext::getBBoxForCurrentGlyph(FT_BBox* bbox) {
  auto face = _face->face;
  FT_Outline_Get_CBox(&face->glyph->outline, bbox);

  // outset the box to integral boundaries
  bbox->xMin &= ~63;
  bbox->yMin &= ~63;
  bbox->xMax = (bbox->xMax + 63) & ~63;
  bbox->yMax = (bbox->yMax + 63) & ~63;
}

GlyphMetrics FTScalerContext::generateGlyphMetrics(GlyphID glyphID) {
  std::lock_guard<std::mutex> lockGuard(FTMutex());
  GlyphMetrics glyph;
  if (setupSize()) {
    return glyph;
  }

  auto face = _face->face;
  auto err = FT_Load_Glyph(face, glyphID,
                           loadGlyphFlags | static_cast<FT_Int32>(FT_LOAD_BITMAP_METRICS_ONLY));
  if (err != FT_Err_Ok) {
    return glyph;
  }
  emboldenIfNeeded(face, face->glyph, glyphID);
  if (face->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
    using FT_PosLimits = std::numeric_limits<FT_Pos>;
    FT_BBox bounds = {FT_PosLimits::max(), FT_PosLimits::max(), FT_PosLimits::min(),
                      FT_PosLimits::min()};
    if (0 < face->glyph->outline.n_contours) {
      getBBoxForCurrentGlyph(&bounds);
    } else {
      bounds = {0, 0, 0, 0};
    }
    // Round out, no longer dot6.
    bounds.xMin = FDot6Floor(bounds.xMin);
    bounds.yMin = FDot6Floor(bounds.yMin);
    bounds.xMax = FDot6Ceil(bounds.xMax);
    bounds.yMax = FDot6Ceil(bounds.yMax);

    FT_Pos width = bounds.xMax - bounds.xMin;
    FT_Pos height = bounds.yMax - bounds.yMin;
    FT_Pos top = -bounds.yMax;  // Freetype y-up, We y-down.
    FT_Pos left = bounds.xMin;

    glyph.width = static_cast<float>(width);
    glyph.height = static_cast<float>(height);
    glyph.top = static_cast<float>(top);
    glyph.left = static_cast<float>(left);
  } else if (face->glyph->format == FT_GLYPH_FORMAT_BITMAP) {
    auto rect = Rect::MakeXYWH(static_cast<float>(face->glyph->bitmap_left),
                               -static_cast<float>(face->glyph->bitmap_top),
                               static_cast<float>(face->glyph->bitmap.width),
                               static_cast<float>(face->glyph->bitmap.rows));
    matrix22Scalar.mapRect(&rect);
    rect.roundOut();
    glyph.width = static_cast<float>(rect.width());
    glyph.height = static_cast<float>(rect.height());
    glyph.top = static_cast<float>(rect.top);
    glyph.left = static_cast<float>(rect.left);
  } else {
    LOGE("unknown glyph format");
    return glyph;
  }

  glyph.advanceX = FDot6ToFloat(face->glyph->advance.x);
  glyph.advanceY = FDot6ToFloat(face->glyph->advance.y);
  return glyph;
}

static gfx::skcms_PixelFormat ToPixelFormat(ColorType colorType) {
  switch (colorType) {
    case ColorType::ALPHA_8:
      return gfx::skcms_PixelFormat_A_8;
    case ColorType::RGBA_8888:
      return gfx::skcms_PixelFormat_RGBA_8888;
    default:
      return gfx::skcms_PixelFormat_BGRA_8888;
  }
}

void CopyFTBitmap(const FT_Bitmap& bitmap, std::shared_ptr<PixelBuffer> pixelBuffer) {
  auto width = static_cast<int>(bitmap.width);
  auto height = static_cast<int>(bitmap.rows);
  auto src = reinterpret_cast<const uint8_t*>(bitmap.buffer);
  // FT_Bitmap::pitch is an int and allowed to be negative.
  auto srcRB = bitmap.pitch;
  auto srcFormat = bitmap.pixel_mode == FT_PIXEL_MODE_GRAY ? gfx::skcms_PixelFormat_A_8
                                                           : gfx::skcms_PixelFormat_BGRA_8888;
  Bitmap bm(pixelBuffer);
  auto dst = static_cast<uint8_t*>(bm.writablePixels());
  auto dstRB = bm.rowBytes();
  auto dstFormat = ToPixelFormat(bm.colorType());
  for (int i = 0; i < height; i++) {
    gfx::skcmsTransform(src, srcFormat, gfx::skcms_AlphaFormat_PremulAsEncoded, nullptr, dst,
                        dstFormat, gfx::skcms_AlphaFormat_PremulAsEncoded, nullptr, width);
    src += srcRB;
    dst += dstRB;
  }
}

std::shared_ptr<TextureBuffer> FTScalerContext::generateImage(GlyphID glyphId, Matrix* matrix) {
  std::lock_guard<std::mutex> lockGuard(FTMutex());
  if (setupSize()) {
    return nullptr;
  }
  auto face = _face->face;
  auto flags = loadGlyphFlags;
  flags |= FT_LOAD_RENDER;
  flags &= ~FT_LOAD_NO_BITMAP;
  auto err = FT_Load_Glyph(face, glyphId, flags);
  if (err != FT_Err_Ok || face->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
    return nullptr;
  }
  auto bitmap = face->glyph->bitmap;
  if (bitmap.pixel_mode != FT_PIXEL_MODE_BGRA && bitmap.pixel_mode != FT_PIXEL_MODE_GRAY) {
    return nullptr;
  }
  auto alphaOnly = bitmap.pixel_mode == FT_PIXEL_MODE_GRAY;
  auto pixelBuffer = PixelBuffer::Make(static_cast<int>(face->glyph->bitmap.width),
                                       static_cast<int>(face->glyph->bitmap.rows), alphaOnly);
  if (pixelBuffer == nullptr) {
    return nullptr;
  }
  CopyFTBitmap(face->glyph->bitmap, pixelBuffer);
  if (matrix) {
    matrix->setTranslate(static_cast<float>(face->glyph->bitmap_left),
                         -static_cast<float>(face->glyph->bitmap_top));
    matrix->postConcat(matrix22Scalar);
  }
  return pixelBuffer;
}
}  // namespace tgfx
