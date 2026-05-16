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

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "base/utils/MathUtil.h"
#include "pagx/TextLayout.h"
#include "pagx/html/HTMLWriter.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/svg/SVGBlendMode.h"
#include "pagx/types/Alignment.h"
#include "pagx/types/Arrangement.h"
#include "pagx/types/Padding.h"
#include "pagx/utils/Base64.h"
#include "pagx/utils/StringParser.h"
#include "renderer/LineBreaker.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/ImageCodec.h"

namespace pagx {

using pag::DegreesToRadians;
using pag::FloatNearlyZero;

void ColorToRGB(const Color& color, int& r, int& g, int& b) {
  r = std::clamp(static_cast<int>(std::round(color.red * 255.0f)), 0, 255);
  g = std::clamp(static_cast<int>(std::round(color.green * 255.0f)), 0, 255);
  b = std::clamp(static_cast<int>(std::round(color.blue * 255.0f)), 0, 255);
}

std::string ColorToHex(const Color& color) {
  if (color.colorSpace == ColorSpace::DisplayP3) {
    return "color(display-p3 " + CssFloatToString(color.red) + " " + CssFloatToString(color.green) +
           " " + CssFloatToString(color.blue) + ")";
  }
  int r = 0, g = 0, b = 0;
  ColorToRGB(color, r, g, b);
  char buf[8] = {};
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
  return buf;
}

// SVG attributes (stop-color, flood-color, fill, stroke) do not support the CSS color() function.
// For DisplayP3 colors, approximate by treating the P3 channel values as sRGB.
std::string ColorToSVGHex(const Color& color) {
  int r = 0, g = 0, b = 0;
  ColorToRGB(color, r, g, b);
  char buf[8] = {};
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
  return buf;
}

std::string ColorToRGBA(const Color& color, float extra) {
  float a = color.alpha * extra;
  if (color.colorSpace == ColorSpace::DisplayP3) {
    return "color(display-p3 " + CssFloatToString(color.red) + " " + CssFloatToString(color.green) +
           " " + CssFloatToString(color.blue) + " / " + CssFloatToString(a) + ")";
  }
  if (a >= 1.0f) {
    return ColorToHex(color);
  }
  int r = 0, g = 0, b = 0;
  ColorToRGB(color, r, g, b);
  return "rgba(" + std::to_string(r) + "," + std::to_string(g) + "," + std::to_string(b) + "," +
         CssFloatToString(a) + ")";
}

const char* DetectImageMime(const uint8_t* bytes, size_t size) {
  if (size >= 8 && bytes[0] == 0x89 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G') {
    return "image/png";
  }
  if (size >= 3 && bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF) {
    return "image/jpeg";
  }
  if (size >= 4 && bytes[0] == 'R' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == 'F') {
    return "image/webp";
  }
  if (size >= 4 && bytes[0] == 'G' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == '8') {
    return "image/gif";
  }
  return nullptr;
}

std::string EscapeCSSUrl(const std::string& url) {
  std::string r;
  r.reserve(url.size());
  for (char c : url) {
    if (c == ')' || c == '(' || c == '\'' || c == '"' || c == ' ' || c == '\\') {
      r += '\\';
    }
    r += c;
  }
  return r;
}

// Returns true when url has no scheme (relative path), or has a scheme on the safe allow-list
// (http, https, data). Everything else (file://, javascript:, vbscript:, etc.) is rejected.
// Scheme detection follows RFC 3986: a leading run of [a-zA-Z][a-zA-Z0-9+.-]* followed by ':'.
static bool IsSafeImageUrl(const std::string& url) {
  size_t colon = url.find(':');
  if (colon == std::string::npos || colon == 0) {
    return true;
  }
  char first = url[0];
  bool isAlpha = (first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z');
  if (!isAlpha) {
    return true;
  }
  for (size_t i = 1; i < colon; i++) {
    char c = url[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '+' || c == '.' || c == '-';
    if (!ok) {
      return true;
    }
  }
  std::string scheme;
  scheme.reserve(colon);
  for (size_t i = 0; i < colon; i++) {
    char c = url[i];
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
    scheme += c;
  }
  return scheme == "http" || scheme == "https" || scheme == "data";
}

// Returns the basename of `path` restricted to filesystem-safe ASCII ([A-Za-z0-9_.-]). Any
// other byte (path separators, whitespace, non-ASCII) is replaced with '_'. Used when copying
// external image files into staticImgDir; combined with the same-source dedup cache, this
// prevents a crafted filePath from escaping the destination directory through path-traversal
// sequences.
static std::string SanitizeBasename(const std::string& path) {
  size_t slash = path.find_last_of("/\\");
  std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
  std::string out;
  out.reserve(base.size());
  for (char c : base) {
    auto uc = static_cast<unsigned char>(c);
    bool allowed = (uc >= 'A' && uc <= 'Z') || (uc >= 'a' && uc <= 'z') ||
                   (uc >= '0' && uc <= '9') || c == '_' || c == '.' || c == '-';
    out += allowed ? c : '_';
  }
  if (out.empty() || out == "." || out == "..") {
    out = "img";
  }
  return out;
}

// Splits a filename into (stem, extension-including-dot). For "logo.png" returns ("logo",
// ".png"); for "no_dot" returns ("no_dot", ""); for ".hidden" returns (".hidden", "").
static std::pair<std::string, std::string> SplitStemExt(const std::string& filename) {
  size_t dot = filename.find_last_of('.');
  if (dot == std::string::npos || dot == 0) {
    return {filename, std::string()};
  }
  return {filename.substr(0, dot), filename.substr(dot)};
}

// Copies the external image file referenced by `srcPath` into the writer's staticImgDir, and
// returns the relative URL the HTML should reference. Same-source paths are copied at most
// once per document via ctx->externalImageCopies; distinct sources sharing a basename receive
// disambiguated names (e.g. "logo.png", "logo_1.png", ...) tracked in
// ctx->externalImageClaimedNames. Returns an empty string when the destination cannot be
// prepared or the file cannot be copied — the caller falls back to inlining.
static std::string CopyExternalImageToStaticDir(const std::string& srcPath,
                                                HTMLWriterContext* ctx) {
  if (!ctx || ctx->staticImgDir.empty()) {
    return {};
  }
  std::error_code ec;
  auto absSrc = std::filesystem::weakly_canonical(srcPath, ec);
  std::string keyPath = ec ? srcPath : absSrc.string();

  auto cached = ctx->externalImageCopies.find(keyPath);
  if (cached != ctx->externalImageCopies.end()) {
    return ctx->staticImgUrlPrefix + cached->second;
  }

  std::filesystem::create_directories(ctx->staticImgDir, ec);
  if (ec) {
    return {};
  }

  std::string baseName = SanitizeBasename(srcPath);
  auto [stem, ext] = SplitStemExt(baseName);
  std::string candidate = baseName;
  for (int suffix = 1; ctx->externalImageClaimedNames.count(candidate) > 0; suffix++) {
    candidate = stem + "_" + std::to_string(suffix) + ext;
  }

  std::filesystem::path dst = std::filesystem::path(ctx->staticImgDir) / candidate;
  std::filesystem::copy_file(srcPath, dst, std::filesystem::copy_options::overwrite_existing, ec);
  if (ec) {
    return {};
  }
  ctx->externalImageCopies[keyPath] = candidate;
  ctx->externalImageClaimedNames.insert(candidate);
  return ctx->staticImgUrlPrefix + candidate;
}

std::string GetImageSrc(const Image* image, HTMLWriterContext* ctx) {
  if (image->data) {
    auto mime = DetectImageMime(image->data->bytes(), image->data->size());
    if (!mime) {
      return {};
    }
    return "data:" + std::string(mime) + ";base64," +
           Base64Encode(image->data->bytes(), image->data->size());
  }
  if (!IsSafeImageUrl(image->filePath)) {
    return {};
  }
  size_t colon = image->filePath.find(':');
  bool hasScheme = (colon != std::string::npos && colon > 0);
  if (!hasScheme) {
    auto codec = tgfx::ImageCodec::MakeFrom(image->filePath);
    if (codec) {
      // Preferred path: copy the file alongside the HTML output (under staticImgDir) and
      // emit a relative URL. Keeps the HTML small and lets browsers cache the image
      // separately. Falls through to base64 inlining when staticImgDir is unset or the
      // copy fails, so callers that omit staticImgDir still get a self-contained HTML.
      auto copiedUrl = CopyExternalImageToStaticDir(image->filePath, ctx);
      if (!copiedUrl.empty()) {
        return copiedUrl;
      }
      // Re-read the file bytes directly — ImageCodec decodes lazily, so we cannot pull the raw
      // encoded bytes out of it. Use a plain std::ifstream to slurp the file.
      std::ifstream f(image->filePath, std::ios::binary | std::ios::ate);
      if (f.good()) {
        auto sz = f.tellg();
        f.seekg(0, std::ios::beg);
        std::vector<uint8_t> buf(static_cast<size_t>(sz));
        if (f.read(reinterpret_cast<char*>(buf.data()), sz)) {
          auto mime = DetectImageMime(buf.data(), buf.size());
          if (mime) {
            return "data:" + std::string(mime) + ";base64," + Base64Encode(buf.data(), buf.size());
          }
        }
      }
    }
  }
  return EscapeCSSUrl(image->filePath);
}

std::pair<int, int> GetImageNativeSize(const Image* image) {
  if (!image) {
    return {0, 0};
  }
  std::shared_ptr<tgfx::ImageCodec> codec;
  if (image->data && image->data->size() > 0) {
    auto td = tgfx::Data::MakeWithCopy(image->data->bytes(), image->data->size());
    codec = tgfx::ImageCodec::MakeFrom(std::move(td));
  } else if (!image->filePath.empty()) {
    codec = tgfx::ImageCodec::MakeFrom(image->filePath);
  }
  if (!codec) {
    return {0, 0};
  }
  return {codec->width(), codec->height()};
}

std::string CSSStops(const std::vector<ColorStop*>& stops) {
  if (stops.empty()) {
    return "transparent,transparent";
  }
  // Skip any null elements — malformed PAGX inputs can leave entries as nullptr.
  const ColorStop* firstValid = nullptr;
  for (auto* s : stops) {
    if (s) {
      firstValid = s;
      break;
    }
  }
  if (!firstValid) {
    return "transparent,transparent";
  }
  if (stops.size() == 1) {
    auto c = ColorToRGBA(firstValid->color);
    return c + "," + c;
  }
  std::string r;
  r.reserve(stops.size() * 32);
  bool first = true;
  for (size_t i = 0; i < stops.size(); i++) {
    if (!stops[i]) {
      continue;
    }
    if (!first) {
      r += ',';
    }
    first = false;
    r += ColorToRGBA(stops[i]->color);
    r += ' ';
    r += CssFloatToString(stops[i]->offset * 100.0f);
    r += '%';
  }
  return r;
}

std::string MatrixToCSS(const Matrix& m) {
  std::string r = "matrix(";
  r += CssFloatToString(m.a) + ',' + CssFloatToString(m.b) + ',' + CssFloatToString(m.c) + ',' +
       CssFloatToString(m.d) + ',' + CssFloatToString(m.tx) + ',' + CssFloatToString(m.ty) + ')';
  return r;
}

std::string MatrixTransformToCSS(const Matrix& m) {
  // Decompose into simpler CSS transform functions when possible.
  bool hasTranslate = !FloatNearlyZero(m.tx) || !FloatNearlyZero(m.ty);

  // Pure translation: translate(tx, ty)
  bool isPureTranslation = FloatNearlyZero(m.a - 1.0f) && FloatNearlyZero(m.b) &&
                           FloatNearlyZero(m.c) && FloatNearlyZero(m.d - 1.0f);
  if (isPureTranslation) {
    if (!hasTranslate) {
      return {};
    }
    return "translate(" + CssFloatToString(m.tx) + "px," + CssFloatToString(m.ty) + "px)";
  }

  // Uniform scale + rotation: matrix has the form [a, b, -b, a] (i.e. a==d, b==-c).
  // This covers pure scale (b==0) and pure rotation (a²+b²==1) as special cases.
  bool isUniformScaleRotation = FloatNearlyZero(m.a - m.d) && FloatNearlyZero(m.b + m.c);
  if (isUniformScaleRotation) {
    float s = std::sqrt(m.a * m.a + m.b * m.b);
    float angle = std::atan2(m.b, m.a) * 180.0f / static_cast<float>(M_PI);

    std::string r;
    if (hasTranslate) {
      r += "translate(" + CssFloatToString(m.tx) + "px," + CssFloatToString(m.ty) + "px)";
    }
    if (!FloatNearlyZero(angle)) {
      if (!r.empty()) {
        r += ' ';
      }
      r += "rotate(" + CssFloatToString(angle) + "deg)";
    }
    if (!FloatNearlyZero(s - 1.0f)) {
      if (!r.empty()) {
        r += ' ';
      }
      r += "scale(" + CssFloatToString(s) + ")";
    }
    if (!r.empty()) {
      return r;
    }
  }

  // Non-uniform scale (b==0, c==0 but a!=d): scale(sx, sy)
  bool isNonUniformScale = FloatNearlyZero(m.b) && FloatNearlyZero(m.c);
  if (isNonUniformScale) {
    std::string r;
    if (hasTranslate) {
      r += "translate(" + CssFloatToString(m.tx) + "px," + CssFloatToString(m.ty) + "px)";
    }
    bool sxOne = FloatNearlyZero(m.a - 1.0f);
    bool syOne = FloatNearlyZero(m.d - 1.0f);
    if (!sxOne || !syOne) {
      if (!r.empty()) {
        r += ' ';
      }
      if (sxOne && !syOne) {
        r += "scaleY(" + CssFloatToString(m.d) + ")";
      } else if (!sxOne && syOne) {
        r += "scaleX(" + CssFloatToString(m.a) + ")";
      } else if (FloatNearlyZero(m.a - m.d)) {
        r += "scale(" + CssFloatToString(m.a) + ")";
      } else {
        r += "scale(" + CssFloatToString(m.a) + "," + CssFloatToString(m.d) + ")";
      }
    }
    if (!r.empty()) {
      return r;
    }
  }

  // Fallback: full matrix().
  return "matrix(" + CssFloatToString(m.a) + ',' + CssFloatToString(m.b) + ',' +
         CssFloatToString(m.c) + ',' + CssFloatToString(m.d) + ',' + CssFloatToString(m.tx) + ',' +
         CssFloatToString(m.ty) + ')';
}

std::string Matrix3DToCSS(const Matrix3D& m) {
  std::string r = "matrix3d(";
  for (int i = 0; i < 16; i++) {
    if (i > 0) {
      r += ',';
    }
    r += CssFloatToString(m.values[i]);
  }
  r += ')';
  return r;
}

const char* BlendModeToMixBlendMode(BlendMode mode) {
  auto svgStr = BlendModeToSVGString(mode);
  if (svgStr) {
    return svgStr;
  }
  if (mode == BlendMode::PlusLighter) {
    return "plus-lighter";
  }
  if (mode == BlendMode::PlusDarker) {
    // CSS has no direct equivalent for PlusDarker; approximate with darken.
    return "darken";
  }
  return nullptr;
}

Color LerpColor(const Color& a, const Color& b, float t) {
  Color result = {};
  result.red = a.red + (b.red - a.red) * t;
  result.green = a.green + (b.green - a.green) * t;
  result.blue = a.blue + (b.blue - a.blue) * t;
  result.alpha = a.alpha + (b.alpha - a.alpha) * t;
  result.colorSpace = a.colorSpace;
  return result;
}

std::string LayerTransformCSS(const Layer* layer) {
  if (!layer->matrix3D.isIdentity()) {
    return Matrix3DToCSS(layer->matrix3D);
  }
  // Layer position (x/y) is emitted as left/top by the caller. The matrix is applied on top of
  // that position, following the tgfx rendering formula: Translate(renderPosition) * matrix.
  Matrix m = layer->matrix;
  if (m.isIdentity()) {
    return {};
  }
  return MatrixTransformToCSS(m);
}

// HTML-local skew sign fix. Mirrors pagx::BuildGroupMatrix line-for-line except for one shear:
// the shear coefficient uses `tan(-group->skew)` so the result agrees with tgfx native rendering
// (VectorGroup::ApplySkew passes `DegreesToRadians(-skew)` into the shear). The shared
// pagx::BuildGroupMatrix uses the +skew sign because main's PAGXSVGTest.SVGExport_GroupSkew
// pinned that convention for SVG output, and the SVG/PPT exporters depend on it. Rather than
// flipping the shared helper (which would break those exporters and the pinned test) we keep
// the HTML exporter's path bake aligned with native by routing every BuildGroupMatrix call
// through this wrapper. Any change to the rest of BuildGroupMatrix must be mirrored here.
Matrix BuildGroupMatrixForHTML(const Group* group) {
  auto renderPos = group->renderPosition();
  bool hasAnchor = !FloatNearlyZero(group->anchor.x) || !FloatNearlyZero(group->anchor.y);
  bool hasPosition = !FloatNearlyZero(renderPos.x) || !FloatNearlyZero(renderPos.y);
  bool hasRotation = !FloatNearlyZero(group->rotation);
  bool hasScale =
      !FloatNearlyZero(group->scale.x - 1.0f) || !FloatNearlyZero(group->scale.y - 1.0f);
  bool hasSkew = !FloatNearlyZero(group->skew);

  if (!hasAnchor && !hasPosition && !hasRotation && !hasScale && !hasSkew) {
    return {};
  }

  Matrix m = {};
  if (hasAnchor) {
    m = Matrix::Translate(-group->anchor.x, -group->anchor.y);
  }
  if (hasScale) {
    m = Matrix::Scale(group->scale.x, group->scale.y) * m;
  }
  if (hasSkew) {
    m = Matrix::Rotate(group->skewAxis) * m;
    Matrix shear = {};
    // Sign deliberately negated relative to pagx::BuildGroupMatrix; see function comment.
    shear.c = std::tan(DegreesToRadians(-group->skew));
    m = shear * m;
    m = Matrix::Rotate(-group->skewAxis) * m;
  }
  if (hasRotation) {
    m = Matrix::Rotate(group->rotation) * m;
  }
  if (hasPosition) {
    m = Matrix::Translate(renderPos.x, renderPos.y) * m;
  }

  return m;
}

CSSFontProps ParseFontStyleToCSS(const std::string& fontStyle) {
  CSSFontProps props;
  if (fontStyle.empty()) {
    return props;
  }
  // Weight keywords ordered to avoid substring false positives (e.g. "SemiBold" matching "Bold").
  if (fontStyle.find("Thin") != std::string::npos ||
      fontStyle.find("Hairline") != std::string::npos) {
    props.weight = 100;
  } else if (fontStyle.find("ExtraLight") != std::string::npos ||
             fontStyle.find("UltraLight") != std::string::npos) {
    props.weight = 200;
  } else if (fontStyle.find("Light") != std::string::npos) {
    props.weight = 300;
  } else if (fontStyle.find("Medium") != std::string::npos) {
    props.weight = 500;
  } else if (fontStyle.find("SemiBold") != std::string::npos ||
             fontStyle.find("DemiBold") != std::string::npos ||
             fontStyle.find("Semibold") != std::string::npos) {
    props.weight = 600;
  } else if (fontStyle.find("ExtraBold") != std::string::npos ||
             fontStyle.find("UltraBold") != std::string::npos) {
    props.weight = 800;
  } else if (fontStyle.find("Black") != std::string::npos ||
             fontStyle.find("Heavy") != std::string::npos) {
    props.weight = 900;
  } else if (fontStyle.find("Bold") != std::string::npos) {
    props.weight = 700;
  }
  if (fontStyle.find("Italic") != std::string::npos) {
    props.italic = true;
  } else if (fontStyle.find("Oblique") != std::string::npos) {
    props.oblique = true;
  }
  return props;
}

const char* AlignmentToCSS(Alignment alignment) {
  switch (alignment) {
    case Alignment::Start:
      return "flex-start";
    case Alignment::Center:
      return "center";
    case Alignment::End:
      return "flex-end";
    case Alignment::Stretch:
      return "stretch";
  }
  return "stretch";
}

const char* ArrangementToCSS(Arrangement arrangement) {
  switch (arrangement) {
    case Arrangement::Start:
      return "flex-start";
    case Arrangement::Center:
      return "center";
    case Arrangement::End:
      return "flex-end";
    case Arrangement::SpaceBetween:
      return "space-between";
    case Arrangement::SpaceEvenly:
      return "space-evenly";
    case Arrangement::SpaceAround:
      return "space-around";
  }
  return "flex-start";
}

std::string PaddingToCSS(const Padding& padding) {
  if (FloatNearlyZero(padding.right - padding.left) &&
      FloatNearlyZero(padding.bottom - padding.top) &&
      FloatNearlyZero(padding.top - padding.left)) {
    return CssFloatToString(padding.top) + "px";
  }
  if (FloatNearlyZero(padding.top - padding.bottom) &&
      FloatNearlyZero(padding.left - padding.right)) {
    return CssFloatToString(padding.top) + "px " + CssFloatToString(padding.right) + "px";
  }
  return CssFloatToString(padding.top) + "px " + CssFloatToString(padding.right) + "px " +
         CssFloatToString(padding.bottom) + "px " + CssFloatToString(padding.left) + "px";
}

bool TextStartsWithRTL(const std::string& utf8Text) {
  // Walk the UTF-8 string and return true if the first Unicode codepoint with a strong BiDi
  // directional type is RTL (Hebrew, Arabic, or other RTL scripts). This matches the UAX#9
  // P2 paragraph-direction algorithm's "first strong character" rule so the HTML
  // `direction:rtl` CSS property is added on exactly the same TextBox containers that tgfx
  // marks as RTL paragraphs.
  const unsigned char* p = reinterpret_cast<const unsigned char*>(utf8Text.c_str());
  const unsigned char* end = p + utf8Text.size();
  while (p < end) {
    uint32_t cp = 0;
    if (*p < 0x80) {
      cp = *p++;
    } else if ((*p & 0xE0) == 0xC0 && p + 1 < end) {
      cp = ((*p & 0x1F) << 6) | (p[1] & 0x3F);
      p += 2;
    } else if ((*p & 0xF0) == 0xE0 && p + 2 < end) {
      cp = ((*p & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
      p += 3;
    } else if ((*p & 0xF8) == 0xF0 && p + 3 < end) {
      cp = ((*p & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
      p += 4;
    } else {
      p++;
      continue;
    }
    // Skip neutral/weak characters (spaces, digits, punctuation) that don't determine direction.
    // Hebrew block: U+0590–U+05FF
    // Arabic block: U+0600–U+06FF, U+0750–U+077F, U+08A0–U+08FF
    // Arabic Supplement/Extended: U+0600–U+08FF covers most cases
    // Arabic Presentation Forms: U+FB1D–U+FDFF, U+FE70–U+FEFF
    if ((cp >= 0x0590 && cp <= 0x05FF) || (cp >= 0x0600 && cp <= 0x08FF) ||
        (cp >= 0xFB1D && cp <= 0xFDFF) || (cp >= 0xFE70 && cp <= 0xFEFF)) {
      return true;
    }
    // Latin letters and CJK are strongly LTR — stop scanning on the first strong LTR character.
    if ((cp >= 0x0041 && cp <= 0x007A) || (cp >= 0x00C0 && cp <= 0x02AF) ||
        (cp >= 0x0400 && cp <= 0x052F) || (cp >= 0x4E00 && cp <= 0x9FFF) ||
        (cp >= 0xAC00 && cp <= 0xD7AF)) {
      return false;
    }
    // Digits, spaces, and other neutrals: keep scanning.
  }
  return false;
}

}  // namespace pagx
