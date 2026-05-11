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

std::string GetImageSrc(const Image* image) {
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
  // Relative file paths would otherwise resolve against the emitted HTML's directory, which
  // rarely matches the PAGX source tree (complete_example's `spec/samples/pag_logo.png`
  // symptom: the HTML sits in test/out/html-comparison/cli/spec/ so the browser hits a
  // non-existent spec/samples/ next to it). When the path has no scheme and points to a
  // readable file on disk, inline it as a data URI so the HTML stays self-contained.
  size_t colon = image->filePath.find(':');
  bool hasScheme = (colon != std::string::npos && colon > 0);
  if (!hasScheme) {
    auto codec = tgfx::ImageCodec::MakeFrom(image->filePath);
    if (codec) {
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

Color SampleLinearGradient(const std::vector<ColorStop*>& stops, float t) {
  if (stops.empty()) {
    return {};
  }
  // Clamp t to [0, 1] then find the surrounding stops.
  t = std::max(0.0f, std::min(1.0f, t));
  if (t <= stops.front()->offset) {
    return stops.front()->color;
  }
  if (t >= stops.back()->offset) {
    return stops.back()->color;
  }
  for (size_t i = 1; i < stops.size(); i++) {
    if (t <= stops[i]->offset) {
      float range = stops[i]->offset - stops[i - 1]->offset;
      float local = (range > 1e-6f) ? (t - stops[i - 1]->offset) / range : 0.0f;
      return LerpColor(stops[i - 1]->color, stops[i]->color, local);
    }
  }
  return stops.back()->color;
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

std::string RewriteLineBreakHints(const std::string& text) {
  // ZWSP (ZERO WIDTH SPACE) and SHY (SOFT HYPHEN) UTF-8 byte sequences. ZWSP is U+200B
  // (E2 80 8B); SHY is U+00AD (C2 AD).
  static constexpr const char ZWSP[] = "\xE2\x80\x8B";
  std::string out;
  out.reserve(text.size() + text.size() / 8);
  for (size_t i = 0; i < text.size();) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    if (c == 0xC2 && i + 1 < text.size() && static_cast<unsigned char>(text[i + 1]) == 0xAD) {
      // Replace SHY with ZWSP — same break-allowed semantic, no visible hyphen at line end.
      out.append(ZWSP, 3);
      i += 2;
      continue;
    }
    if (c == '/') {
      // Inject ZWSP after every '/' so Chromium can break at the same point tgfx's UAX-14
      // BA classification allows. Without this, "path/to/some/file" stays as one atomic
      // token under `word-wrap:break-word` and is split mid-character ("some/fil" + "e")
      // when the box is too narrow.
      out += '/';
      out.append(ZWSP, 3);
      i += 1;
      continue;
    }
    out += text[i];
    i += 1;
  }
  return out;
}

Matrix BuildGroupMatrix(const Group* group) {
  bool hasAnchor = !FloatNearlyZero(group->anchor.x) || !FloatNearlyZero(group->anchor.y);
  // renderPosition() returns the layout-resolved top-left (incorporates constraint layout such as
  // centerX/centerY/left/right). When the group has no layout constraints, renderPosition equals
  // (position.x, position.y), so this subsumes the explicit position property.
  auto renderPos = group->renderPosition();
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

namespace {

// Consumes one UTF-8 codepoint starting at utf8Text[pos]. Advances pos past the codepoint.
// Returns {codepoint, byteLength}; on malformed input, returns {replacement, 1}.
struct Codepoint {
  uint32_t cp;
  size_t byteLen;
};
Codepoint NextCodepoint(const std::string& utf8Text, size_t pos) {
  const unsigned char* p = reinterpret_cast<const unsigned char*>(utf8Text.c_str()) + pos;
  size_t rem = utf8Text.size() - pos;
  if (rem == 0) return {0, 0};
  if (*p < 0x80) return {*p, 1};
  if ((*p & 0xE0) == 0xC0 && rem >= 2) {
    return {static_cast<uint32_t>(((*p & 0x1F) << 6) | (p[1] & 0x3F)), 2};
  }
  if ((*p & 0xF0) == 0xE0 && rem >= 3) {
    return {static_cast<uint32_t>(((*p & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F)), 3};
  }
  if ((*p & 0xF8) == 0xF0 && rem >= 4) {
    return {static_cast<uint32_t>(((*p & 0x07) << 18) | ((p[1] & 0x3F) << 12) |
                                  ((p[2] & 0x3F) << 6) | (p[3] & 0x3F)),
            4};
  }
  return {static_cast<uint32_t>(*p), 1};
}

bool IsCJKCodepoint(uint32_t cp) {
  // CJK Unified Ideographs, CJK Extension A, CJK Symbols and Punctuation, Hiragana, Katakana,
  // Hangul, Fullwidth Forms. Matches characters that tgfx treats as `Upright` in vertical mode
  // (per-char advance = font vertical advance, can break before).
  return (cp >= 0x3000 && cp <= 0x303F) ||  // CJK punctuation
         (cp >= 0x3040 && cp <= 0x309F) ||  // Hiragana
         (cp >= 0x30A0 && cp <= 0x30FF) ||  // Katakana
         (cp >= 0x3400 && cp <= 0x4DBF) ||  // CJK Ext A
         (cp >= 0x4E00 && cp <= 0x9FFF) ||  // CJK Unified
         (cp >= 0xAC00 && cp <= 0xD7AF) ||  // Hangul
         (cp >= 0xF900 && cp <= 0xFAFF) ||  // CJK Compat
         (cp >= 0xFF00 && cp <= 0xFFEF);    // Fullwidth forms
}

std::string EscapeHtmlChar(uint32_t cp, const std::string& srcBytes) {
  if (cp == '&') return "&amp;";
  if (cp == '<') return "&lt;";
  if (cp == '>') return "&gt;";
  if (cp == '"') return "&quot;";
  return srcBytes;
}

}  // namespace

std::string HTMLWriter::rewriteVerticalColumnBreaks(const Text* text) {
  if (text == nullptr || text->glyphData == nullptr) return text ? text->text : std::string{};
  const auto& runs = text->glyphData->layoutRuns;
  std::vector<tgfx::Point> glyphPositions;
  for (const auto& run : runs) {
    for (const auto& pos : run.positions) {
      glyphPositions.push_back(pos);
    }
  }
  if (glyphPositions.size() < 2) return text->text;
  size_t visibleCount = 0;
  for (size_t pos = 0; pos < text->text.size();) {
    auto c = NextCodepoint(text->text, pos);
    if (c.byteLen == 0) break;
    if (c.cp != '\n') ++visibleCount;
    pos += c.byteLen;
  }
  if (visibleCount != glyphPositions.size()) return text->text;
  // A real column break shows up as `|x_i - x_{i-1}| >= fontSize/2`. Latin/digit glyphs in a
  // CJK vertical column sit ~1.54 px off the CJK baseline X (per HarfBuzz vertical metrics for
  // Noto Sans SC), which would otherwise be misread as a column transition and inject one <br>
  // per CJK<->Latin handoff (broke vertical.pagx Box A `2024年12月31日` and friends in the
  // 2026-05-09 attempt). The real per-column inline width is the natural CJK advance, i.e.
  // the rendered fontSize, so half of that is a comfortable threshold separating the two.
  float threshold = text->renderFontSize() * 0.5f;
  if (threshold <= 0) threshold = 12.0f;
  std::vector<bool> startsNewColumn(glyphPositions.size(), false);
  for (size_t i = 1; i < glyphPositions.size(); ++i) {
    if (std::fabs(glyphPositions[i].x - glyphPositions[i - 1].x) >= threshold) {
      startsNewColumn[i] = true;
    }
  }
  bool hasBreak = false;
  for (bool b : startsNewColumn) {
    if (b) {
      hasBreak = true;
      break;
    }
  }
  if (!hasBreak) return text->text;
  // Walk source text and glyphs in lock-step, inserting '\n' before any glyph that started a
  // new column. Skip the insertion when the source already has a '\n' immediately before this
  // codepoint (avoid emitting two consecutive <br>s).
  std::string out;
  out.reserve(text->text.size() + 8);
  size_t gi = 0;
  bool prevWasNewline = false;
  for (size_t pos = 0; pos < text->text.size();) {
    auto c = NextCodepoint(text->text, pos);
    if (c.byteLen == 0) break;
    std::string srcBytes = text->text.substr(pos, c.byteLen);
    pos += c.byteLen;
    if (c.cp == '\n') {
      out += srcBytes;
      prevWasNewline = true;
      continue;
    }
    if (gi < startsNewColumn.size() && startsNewColumn[gi] && !prevWasNewline) {
      out += '\n';
    }
    out += srcBytes;
    prevWasNewline = false;
    ++gi;
  }
  return out;
}

std::string HTMLWriter::buildVerticalJustifyContent(const Text* text, float boxHeight) {
  if (text == nullptr) return {};
  const auto& runs = text->glyphData->layoutRuns;
  if (runs.empty()) return {};
  // Flatten glyph data across runs. Each glyph needs three derived attributes that tgfx's
  // vertical justify pass consumes:
  //   - position (for column-partitioning via x-coordinate gaps);
  //   - `advance` = vg.height, i.e. the per-glyph inline-axis advance tgfx steps by. We
  //     recompute it here from `font.getAdvance(glyphID, vertical=true/false)` rather than
  //     plumb a new field through TextLayoutGlyphRun — the font is already stored per run,
  //     the vertical/horizontal choice is encoded by whether the run used RSXform (rotated
  //     Latin → horizontal advance) or not (upright CJK → vertical advance), and those are
  //     the same two branches tgfx's layout pass uses (TextLayout.cpp:1127 / :1131). Note
  //     this does not pick up author-specified letterSpacing; samples using letterSpacing
  //     will be off by that amount but still produce a reasonable justify distribution.
  //   - `canBreakBefore`, the UAX-14 flag tgfx consults before inserting justifyGap. We
  //     recompute it by walking the source text's codepoints in lockstep with glyphs — the
  //     mapping is 1:1 because a non-matching glyph/char count already triggers the early
  //     fallback below.
  std::vector<tgfx::Point> glyphPositions;
  std::vector<float> glyphAdvances;
  for (const auto& run : runs) {
    bool rotated = !run.xforms.empty();
    for (size_t i = 0; i < run.positions.size(); ++i) {
      glyphPositions.push_back(run.positions[i]);
      float adv = run.font.getAdvance(run.glyphs[i], /*verticalText=*/!rotated);
      glyphAdvances.push_back(adv);
    }
  }
  if (glyphPositions.empty()) return {};

  // Count visible (non-newline) codepoints in the source text and gather the codepoints in
  // text order so we can compute `canBreakBefore` per glyph.
  std::vector<int32_t> visibleCodepoints;
  visibleCodepoints.reserve(text->text.size());
  for (size_t pos = 0; pos < text->text.size();) {
    auto c = NextCodepoint(text->text, pos);
    if (c.byteLen == 0) break;
    if (c.cp != '\n') visibleCodepoints.push_back(static_cast<int32_t>(c.cp));
    pos += c.byteLen;
  }
  // If the layout produced a different number of glyphs than visible chars, fall back (the
  // mapping would be ambiguous — for instance a cluster/ligature collapsed multiple source
  // codepoints into one glyph). Upstream caller should emit plain text instead.
  if (visibleCodepoints.size() != glyphPositions.size()) return {};

  std::vector<bool> glyphBreakBefore(glyphPositions.size(), false);
  for (size_t i = 1; i < visibleCodepoints.size(); ++i) {
    glyphBreakBefore[i] =
        LineBreaker::CanBreakBetween(visibleCodepoints[i - 1], visibleCodepoints[i]);
  }

  // Partition glyphs into columns using the same threshold as rewriteVerticalColumnBreaks:
  // Latin glyphs sit ~1.54px off the CJK baseline X inside a shared column (HarfBuzz vertical
  // metrics), so only `|dx| >= renderFontSize/2` counts as a real column transition.
  float columnThreshold = text->renderFontSize() * 0.5f;
  if (columnThreshold <= 0) columnThreshold = 12.0f;
  std::vector<size_t> columnStart;  // index of first glyph in each column
  columnStart.push_back(0);
  for (size_t i = 1; i < glyphPositions.size(); ++i) {
    if (std::fabs(glyphPositions[i].x - glyphPositions[i - 1].x) >= columnThreshold) {
      columnStart.push_back(i);
    }
  }
  columnStart.push_back(glyphPositions.size());  // sentinel

  // Replay tgfx's justify pass: for every non-last column with extra space and at least one
  // `canBreakBefore`, distribute `(boxHeight - sum(advances))` evenly across break points.
  // Glyphs preceded by a break point emit a leading spacer of `justifyGap`.
  std::vector<float> leadingGap(glyphPositions.size(), 0.0f);
  size_t columnCount = columnStart.size() - 1;
  for (size_t c = 0; c < columnCount; ++c) {
    size_t start = columnStart[c];
    size_t end = columnStart[c + 1];
    if (c + 1 == columnCount) continue;  // last column: no justify (TextAlign::Start semantics)
    if (boxHeight <= 0) continue;
    float naturalHeight = 0;
    size_t breakCount = 0;
    for (size_t i = start; i < end; ++i) {
      naturalHeight += glyphAdvances[i];
      if (i > start && glyphBreakBefore[i]) ++breakCount;
    }
    if (breakCount == 0) continue;
    float extra = boxHeight - naturalHeight;
    if (extra <= 0) continue;
    float justifyGap = extra / static_cast<float>(breakCount);
    for (size_t i = start + 1; i < end; ++i) {
      if (glyphBreakBefore[i]) leadingGap[i] = justifyGap;
    }
  }

  // Emit one inline-block wrapper per glyph. Wrapper height pins the tgfx-computed advance
  // so Chromium's vertical inline flow reproduces tgfx's per-glyph spacing verbatim;
  // a leading empty inline-block spacer of `justifyGap` height inserts the extra space tgfx
  // distributed at break points. CJK glyphs centre horizontally inside the wrapper to match
  // tgfx's column-centred placement; Latin/digit/space glyphs rely on `white-space:nowrap`
  // to stay cluster-tight.
  std::string out;
  out.reserve(text->text.size() * 24);
  size_t gi = 0;
  for (size_t pos = 0; pos < text->text.size();) {
    auto c = NextCodepoint(text->text, pos);
    if (c.byteLen == 0) break;
    std::string srcBytes = text->text.substr(pos, c.byteLen);
    pos += c.byteLen;
    if (c.cp == '\n') {
      out += "<br>";
      continue;
    }
    if (gi >= glyphPositions.size()) break;
    float advance = glyphAdvances[gi];
    float gap = leadingGap[gi];
    bool isCjk = IsCJKCodepoint(c.cp);
    // Emit the justifyGap spacer *before* the glyph that triggered the break (matching
    // tgfx's `currentY += justifyGap` which runs before writing glyph i+1). Using a spacer
    // inline-block instead of margin-inline-start sidesteps Chromium's margin-collapsing
    // rules between consecutive inline-blocks in vertical writing modes.
    if (gap > 0) {
      out += "<span style=\"display:inline-block;height:";
      out += CssFloatToString(gap);
      out += "px;width:0\"></span>";
    }
    out += "<span style=\"display:inline-block;white-space:nowrap;line-height:";
    out += CssFloatToString(advance > 0 ? advance : text->renderFontSize());
    out += "px;height:";
    out += CssFloatToString(advance > 0 ? advance : text->renderFontSize());
    out += "px";
    if (isCjk) out += ";text-align:center";
    out += "\">";
    out += EscapeHtmlChar(c.cp, srcBytes);
    out += "</span>";
    ++gi;
  }
  return out;
}

}  // namespace pagx
