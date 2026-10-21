/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "pagx/PAGXDocument.h"
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/html/importer/HTMLDiagnosticSink.h"
#include "pagx/html/importer/HTMLLayerBuilder.h"
#include "pagx/html/importer/HTMLValueParser.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Gradient.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Rectangle.h"

namespace pagx {
using namespace html;
namespace {
bool FourValues(const std::string& raw, float* result, const float* bases, const float* multipliers,
                const float* autoValues, HTMLValueParser& parser, bool allowLength) {
  auto tokens = SplitTopLevelWhitespace(raw);
  if (tokens.empty() || tokens.size() > 4) return false;
  int index[] = {0, tokens.size() > 1 ? 1 : 0, tokens.size() > 2 ? 2 : 0,
                 tokens.size() > 3 ? 3 : (tokens.size() > 1 ? 1 : 0)};
  for (int i = 0; i < 4; ++i) {
    const auto& token = tokens[index[i]];
    float fraction = 0;
    if (token == "auto" && autoValues) result[i] = autoValues[i];
    else if (ParseCssPercentage(token, fraction) && bases)
      result[i] = bases[i] * fraction;
    else {
      char* end = nullptr;
      float number = std::strtof(token.c_str(), &end);
      if (end == token.c_str()) return false;
      if (*end == 0) result[i] = number * (multipliers ? multipliers[i] : 1);
      else if (allowLength)
        result[i] = parser.parseAbsoluteLengthPx(token);
      else
        return false;
    }
    if (!std::isfinite(result[i]) || result[i] < 0) return false;
  }
  return true;
}
struct TileAxis {
  float first = 0, size = 0, step = 0;
  int count = 0;
};
constexpr int MaxBorderTiles = 4096;
TileAxis Tiles(float start, float extent, float natural, const std::string& mode) {
  if (extent <= 0 || natural <= 0) return {};
  if (mode == "stretch") return {start, extent, extent, 1};
  const float ratio = extent / natural;
  if (!std::isfinite(ratio) || ratio > MaxBorderTiles)
    return {start, natural, natural, MaxBorderTiles + 1};
  if (mode == "round") {
    int n = std::max(1, static_cast<int>(std::round(ratio)));
    return {start, extent / n, extent / n, n};
  }
  if (mode == "space") {
    int n = static_cast<int>(std::floor(ratio));
    if (n < 1) return {};
    float gap = (extent - n * natural) / (n + 1);
    return {start + gap, natural, natural + gap, n};
  }
  // repeat centers a tile, not the entire set (whose parity may vary).
  int n = 1 + 2 * static_cast<int>(std::ceil(std::max(0.0f, (ratio - 1) / 2)));
  return {start + (extent - n * natural) / 2, natural, natural, n};
}
}  // namespace

bool HTMLLayerBuilder::applyBorderImage(Layer* layer, const HTMLBoxAttributes& box) {
  const auto& value = box.borderImageSource;
  if (value.empty() || ToLower(value) == "none") return false;
  if (!std::isfinite(box.widthPx) || !std::isfinite(box.heightPx)) {
    _diagnostics.warn("html: border-image requires resolved box dimensions");
    return false;
  }
  HTMLBackgroundImage source;
  if (ToLower(value).rfind("url(", 0) == 0 && _backgroundImageResolver)
    source = _backgroundImageResolver(value);
  const auto& b = box.borderWidths;
  float borders[] = {b.top, b.right, b.bottom, b.left};
  float outset[4];
  if (!FourValues(box.borderImageOutset, outset, nullptr, borders, nullptr, _valueParser, true)) {
    _diagnostics.warn("html: invalid border-image-outset");
    return false;
  }
  float aw = box.widthPx + outset[1] + outset[3];
  float ah = box.heightPx + outset[0] + outset[2];
  float sw = std::isfinite(source.intrinsicWidth) ? source.intrinsicWidth : aw;
  float sh = std::isfinite(source.intrinsicHeight) ? source.intrinsicHeight : ah;
  if (sw <= 0 || sh <= 0 || aw <= 0 || ah <= 0) return false;
  if (!source.color && !source.vectorTile) source.color = parseGradientByValue(value, sw, sh);
  if (!source.color && !source.vectorTile) {
    _diagnostics.warn("html: unavailable border-image-source; using ordinary border");
    return false;
  }
  // Paint slices directly in source coordinates. Per-tile rectangular clips can
  // leave seams when fractional edges are rounded independently by the renderer.
  if (source.color && source.color->nodeType() == NodeType::ImagePattern) {
    auto* pattern = static_cast<ImagePattern*>(source.color);
    pattern->scaleMode = ScaleMode::None;
  } else if (source.color && (source.color->nodeType() == NodeType::LinearGradient ||
                              source.color->nodeType() == NodeType::RadialGradient ||
                              source.color->nodeType() == NodeType::ConicGradient)) {
    auto* gradient = static_cast<Gradient*>(source.color);
    if (gradient->fitsToGeometry) {
      gradient->matrix = Matrix::Scale(sw, sh) * gradient->matrix;
      gradient->fitsToGeometry = false;
    }
  }
  auto sliceTokens = SplitTopLevelWhitespace(box.borderImageSlice);
  bool fill = false;
  std::string rawSlice;
  for (const auto& token : sliceTokens) {
    if (ToLower(token) == "fill") {
      fill = true;
      continue;
    }
    if (!rawSlice.empty()) rawSlice += " ";
    rawSlice += token;
  }
  float slice[4], width[4];
  float sourceBases[] = {sh, sw, sh, sw}, areaBases[] = {ah, aw, ah, aw};
  if (!FourValues(rawSlice, slice, sourceBases, nullptr, nullptr, _valueParser, false) ||
      !FourValues(box.borderImageWidth, width, areaBases, borders, slice, _valueParser, true)) {
    _diagnostics.warn("html: invalid border-image slice or width");
    return false;
  }
  for (int i = 0; i < 4; ++i) slice[i] = std::min(slice[i], sourceBases[i]);
  float factor = 1;
  if (width[1] + width[3] > 0) factor = std::min(factor, aw / (width[1] + width[3]));
  if (width[0] + width[2] > 0) factor = std::min(factor, ah / (width[0] + width[2]));
  for (auto& v : width) v *= factor;
  auto repeat = SplitTopLevelWhitespace(ToLower(box.borderImageRepeat));
  if (repeat.empty()) repeat.push_back("stretch");
  if (repeat.size() == 1) repeat.push_back(repeat[0]);
  if (repeat.size() != 2 || std::any_of(repeat.begin(), repeat.end(), [](const auto& s) {
        return s != "stretch" && s != "repeat" && s != "round" && s != "space";
      })) {
    _diagnostics.warn("html: invalid border-image-repeat");
    return false;
  }
  float sx[] = {0, slice[3], sw - slice[1], sw};
  float sy[] = {0, slice[0], sh - slice[2], sh};
  float dx[] = {-outset[3], -outset[3] + width[3], box.widthPx + outset[1] - width[1],
                box.widthPx + outset[1]};
  float dy[] = {-outset[0], -outset[0] + width[0], box.heightPx + outset[2] - width[2],
                box.heightPx + outset[2]};
  struct Patch {
    int x, y;
    TileAxis tx, ty;
  };
  std::vector<Patch> patches;
  int64_t count = 0;
  for (int y = 0; y < 3; ++y)
    for (int x = 0; x < 3; ++x) {
      if (x == 1 && y == 1 && !fill) continue;
      float iw = sx[x + 1] - sx[x], ih = sy[y + 1] - sy[y];
      float ow = dx[x + 1] - dx[x], oh = dy[y + 1] - dy[y];
      if (iw <= 0 || ih <= 0 || ow <= 0 || oh <= 0) continue;
      float nx = iw * oh / ih, ny = ih * ow / iw;
      if (x == 1 && y == 1) {
        nx = iw * (slice[0] > 0 && width[0] > 0   ? width[0] / slice[0]
                   : slice[2] > 0 && width[2] > 0 ? width[2] / slice[2]
                                                  : 1);
        ny = ih * (slice[3] > 0 && width[3] > 0   ? width[3] / slice[3]
                   : slice[1] > 0 && width[1] > 0 ? width[1] / slice[1]
                                                  : 1);
      }
      auto tx = Tiles(dx[x], ow, nx, x == 1 ? repeat[0] : "stretch");
      auto ty = Tiles(dy[y], oh, ny, y == 1 ? repeat[1] : "stretch");
      count += static_cast<int64_t>(tx.count) * ty.count;
      if (count > MaxBorderTiles || tx.count > MaxBorderTiles || ty.count > MaxBorderTiles) {
        _diagnostics.warn("html: border-image tile budget exceeded; using ordinary border");
        return false;
      }
      patches.push_back({x, y, tx, ty});
    }
  // Nine independent source slices, with the source coordinate system preserved.
  // Border radius deliberately does not clip border images (CSS Backgrounds 3).
  for (const auto& p : patches) {
    int x = p.x, y = p.y;
    auto* patch = _document->makeNode<Layer>();
    // Constraint layout snaps left/top to pixels. Image tiles must keep their
    // fractional phase, so translations belong in the affine transform instead.
    patch->left = patch->top = 0;
    patch->matrix = Matrix::Translate(dx[x], dy[y]);
    patch->width = dx[x + 1] - dx[x];
    patch->height = dy[y + 1] - dy[y];
    patch->includeInLayout = false;
    patch->clipToBounds = true;
    patch->antiAlias = false;
    layer->children.push_back(patch);
    for (int j = 0; j < p.ty.count; ++j)
      for (int i = 0; i < p.tx.count; ++i) {
        auto* tile = _document->makeNode<Layer>();
        tile->left = tile->top = 0;
        tile->matrix = Matrix::Translate(p.tx.first + i * p.tx.step - dx[x],
                                         p.ty.first + j * p.ty.step - dy[y]);
        tile->width = p.tx.size;
        tile->height = p.ty.size;
        tile->clipToBounds = static_cast<bool>(source.vectorTile);
        tile->includeInLayout = false;
        tile->antiAlias = false;
        patch->children.push_back(tile);
        auto* image = source.vectorTile ? source.vectorTile(sw, sh) : _document->makeNode<Layer>();
        image->width = sw;
        image->height = sh;
        image->includeInLayout = false;
        image->antiAlias = false;
        float scaleX = p.tx.size / (sx[x + 1] - sx[x]), scaleY = p.ty.size / (sy[y + 1] - sy[y]);
        image->matrix = Matrix::Scale(scaleX, scaleY) * Matrix::Translate(-sx[x], -sy[y]);
        if (!source.vectorTile) {
          auto* rect = _document->makeNode<Rectangle>();
          rect->width = sx[x + 1] - sx[x];
          rect->height = sy[y + 1] - sy[y];
          rect->left = sx[x];
          rect->top = sy[y];
          auto* paint = _document->makeNode<Fill>();
          paint->color = source.color;
          image->contents = {rect, paint};
        }
        tile->children.push_back(image);
      }
  }
  return true;
}
}  // namespace pagx
