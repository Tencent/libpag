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

#include "TextAtlas.h"
#include "RenderCache.h"
#include "tgfx/core/Mask.h"
#include "tgfx/gpu/Canvas.h"
#include "tgfx/gpu/Surface.h"
#include "tgfx/gpu/Texture.h"

namespace pag {
class Atlas {
 public:
  static std::unique_ptr<Atlas> Make(tgfx::Context* context, float scale,
                                     const std::vector<GlyphHandle>& glyphs, int maxTextureSize,
                                     bool alphaOnly = true);

  bool getLocator(const tgfx::BytesKey& bytesKey, AtlasLocator* locator) const;

  size_t memoryUsage() const;

 private:
  Atlas(std::vector<std::shared_ptr<tgfx::Texture>> textures,
        std::unordered_map<tgfx::BytesKey, AtlasLocator, tgfx::BytesHasher> glyphLocators)
      : textures(std::move(textures)), glyphLocators(std::move(glyphLocators)) {
  }

  std::vector<std::shared_ptr<tgfx::Texture>> textures;
  std::unordered_map<tgfx::BytesKey, AtlasLocator, tgfx::BytesHasher> glyphLocators;

  friend class TextAtlas;
};

static constexpr int DefaultPadding = 3;

class RectanglePack {
 public:
  explicit RectanglePack(int padding = DefaultPadding) : padding(padding) {
    reset();
  }

  int width() const {
    return _width;
  }

  int height() const {
    return _height;
  }

  Point addRect(int w, int h) {
    w += padding;
    h += padding;
    auto area = (_width - x) * (_height - y);
    if ((x + w - _width) * y > area || (y + h - _height) * x > area) {
      if (_width <= _height) {
        x = _width;
        y = padding;
        _width += w;
      } else {
        x = padding;
        y = _height;
        _height += h;
      }
    }
    auto point = Point::Make(static_cast<float>(x), static_cast<float>(y));
    if (x + w - _width < y + h - _height) {
      x += w;
      _height = std::max(_height, y + h);
    } else {
      y += h;
      _width = std::max(_width, x + w);
    }
    return point;
  }

  void reset() {
    _width = padding;
    _height = padding;
    x = padding;
    y = padding;
  }

 private:
  int padding = DefaultPadding;
  int _width = 0;
  int _height = 0;
  int x = 0;
  int y = 0;
};

size_t Atlas::memoryUsage() const {
  if (textures.empty()) {
    return 0;
  }
  size_t usage = 0;
  for (auto& texture : textures) {
    usage += texture->width() * texture->height();
  }
  auto bytesPerPixels = textures[0]->getSampler()->format == tgfx::PixelFormat::ALPHA_8 ? 1 : 4;
  return usage * bytesPerPixels;
}

static tgfx::PaintStyle ToTGFX(TextStyle style) {
  switch (style) {
    case TextStyle::StrokeAndFill:
    case TextStyle::Fill:
      return tgfx::PaintStyle::Fill;
    case TextStyle::Stroke:
      return tgfx::PaintStyle::Stroke;
  }
}

struct AtlasTextRun {
  tgfx::Paint paint;
  tgfx::Font textFont = {};
  std::vector<tgfx::GlyphID> glyphIDs;
  std::vector<tgfx::Point> positions;
};

static AtlasTextRun CreateTextRun(const GlyphHandle& glyph) {
  AtlasTextRun textRun;
  textRun.textFont = glyph->getFont();
  textRun.paint.setStyle(ToTGFX(glyph->getStyle()));
  if (glyph->getStyle() == TextStyle::Stroke) {
    textRun.paint.setStrokeWidth(glyph->getStrokeWidth());
  }
  return textRun;
}

static void ComputeStyleKey(tgfx::BytesKey* styleKey, const GlyphHandle& glyph) {
  styleKey->write(static_cast<uint32_t>(glyph->getStyle()));
  styleKey->write(glyph->getStrokeWidth());
  styleKey->write(glyph->getFont().getTypeface()->uniqueID());
}

struct Page {
  std::vector<AtlasTextRun> textRuns;
  int width = 0;
  int height = 0;
  std::unordered_map<tgfx::BytesKey, AtlasLocator, tgfx::BytesHasher> locators;
};

static std::vector<Page> CreatePages(const std::vector<GlyphHandle>& glyphs, float scale,
                                     int maxTextureSize) {
  std::vector<Page> pages = {};
  std::vector<tgfx::BytesKey> styleKeys = {};
  std::vector<AtlasTextRun> textRuns = {};
  auto maxPageSize = static_cast<int>(std::floor(static_cast<float>(maxTextureSize) / scale));
  int padding = static_cast<int>(ceil(static_cast<float>(DefaultPadding) / scale));
  RectanglePack pack(padding);
  Page page;
  for (auto& glyph : glyphs) {
    if (glyph->getName() == "\n" || glyph->getName() == " ") {
      continue;
    }
    tgfx::BytesKey styleKey = {};
    ComputeStyleKey(&styleKey, glyph);
    auto iter = std::find(styleKeys.begin(), styleKeys.end(), styleKey);
    AtlasTextRun* textRun;
    if (iter == styleKeys.end()) {
      styleKeys.push_back(styleKey);
      textRuns.push_back(CreateTextRun(glyph));
      textRun = &textRuns.back();
    } else {
      auto index = iter - styleKeys.begin();
      textRun = &textRuns[index];
    }
    auto glyphWidth = static_cast<int>(glyph->getBounds().width());
    auto glyphHeight = static_cast<int>(glyph->getBounds().height());
    int strokeWidth = 0;
    if (glyph->getStyle() == TextStyle::Stroke) {
      strokeWidth = static_cast<int>(ceil(glyph->getStrokeWidth()));
    }
    auto x = glyph->getBounds().x() - static_cast<float>(strokeWidth);
    auto y = glyph->getBounds().y() - static_cast<float>(strokeWidth);
    auto width = glyphWidth + strokeWidth * 2;
    auto height = glyphHeight + strokeWidth * 2;
    auto packWidth = pack.width();
    auto packHeight = pack.height();
    auto point = pack.addRect(width, height);
    if (pack.width() > maxPageSize || pack.height() > maxPageSize) {
      page.textRuns = std::move(textRuns);
      page.width = static_cast<int>(ceil(static_cast<float>(packWidth) * scale));
      page.height = static_cast<int>(ceil(static_cast<float>(packHeight) * scale));
      pages.push_back(std::move(page));
      styleKeys.clear();
      textRuns = {};
      page = {};
      pack.reset();
      point = pack.addRect(width, height);
    }
    textRun->glyphIDs.push_back(glyph->getGlyphID());
    textRun->positions.push_back({-x + point.x, -y + point.y});
    AtlasLocator locator;
    locator.textureIndex = pages.size();
    locator.location = tgfx::Rect::MakeXYWH(point.x, point.y, static_cast<float>(width),
                                            static_cast<float>(height));
    locator.location.scale(scale, scale);
    tgfx::BytesKey bytesKey;
    glyph->computeAtlasKey(&bytesKey, glyph->getStyle());
    page.locators[bytesKey] = locator;
  }
  page.textRuns = std::move(textRuns);
  page.width = static_cast<int>(ceil(static_cast<float>(pack.width()) * scale));
  page.height = static_cast<int>(ceil(static_cast<float>(pack.height()) * scale));
  pages.push_back(std::move(page));
  return pages;
}

std::shared_ptr<tgfx::Texture> DrawMask(tgfx::Context* context, const Page& page, float scale) {
  auto mask = tgfx::Mask::Make(page.width, page.height);
  if (mask == nullptr) {
    LOGE("Atlas: create mask failed.");
    return nullptr;
  }
  mask->setMatrix(tgfx::Matrix::MakeScale(scale));
  for (auto& textRun : page.textRuns) {
    auto blob = tgfx::TextBlob::MakeFrom(&textRun.glyphIDs[0], &textRun.positions[0],
                                         textRun.glyphIDs.size(), textRun.textFont);
    if (textRun.paint.getStyle() == tgfx::PaintStyle::Fill) {
      mask->fillText(blob.get());
    } else {
      mask->strokeText(blob.get(), *textRun.paint.getStroke());
    }
  }
  return mask->makeTexture(context);
}

std::shared_ptr<tgfx::Texture> DrawColor(tgfx::Context* context, const Page& page, float scale) {
  auto surface = tgfx::Surface::Make(context, page.width, page.height);
  if (surface == nullptr) {
    return nullptr;
  }
  auto canvas = surface->getCanvas();
  auto totalMatrix = canvas->getMatrix();
  for (auto& textRun : page.textRuns) {
    canvas->setMatrix(totalMatrix);
    canvas->concat(tgfx::Matrix::MakeScale(scale));
    auto glyphs = &textRun.glyphIDs[0];
    auto positions = &textRun.positions[0];
    canvas->drawGlyphs(glyphs, positions, textRun.glyphIDs.size(), textRun.textFont, textRun.paint);
  }
  canvas->setMatrix(totalMatrix);
  return surface->getTexture();
}

static std::vector<std::shared_ptr<tgfx::Texture>> DrawPages(tgfx::Context* context,
                                                             std::vector<Page>* pages, float scale,
                                                             bool alphaOnly) {
  std::vector<std::shared_ptr<tgfx::Texture>> textures;
  for (auto& page : *pages) {
    auto texture = alphaOnly ? DrawMask(context, page, scale) : DrawColor(context, page, scale);
    if (texture) {
      textures.push_back(texture);
    }
  }
  return textures;
}

std::unique_ptr<Atlas> Atlas::Make(tgfx::Context* context, float scale,
                                   const std::vector<GlyphHandle>& glyphs, int maxTextureSize,
                                   bool alphaOnly) {
  if (glyphs.empty()) {
    return nullptr;
  }
  auto pages = CreatePages(glyphs, scale, maxTextureSize);
  if (pages.empty()) {
    return nullptr;
  }
  std::unordered_map<tgfx::BytesKey, AtlasLocator, tgfx::BytesHasher> glyphLocators;
  for (const auto& page : pages) {
    glyphLocators.insert(page.locators.begin(), page.locators.end());
  }
  auto textures = DrawPages(context, &pages, scale, alphaOnly);
  return std::unique_ptr<Atlas>(new Atlas(std::move(textures), std::move(glyphLocators)));
}

bool Atlas::getLocator(const tgfx::BytesKey& bytesKey, AtlasLocator* locator) const {
  auto iter = glyphLocators.find(bytesKey);
  if (iter == glyphLocators.end()) {
    return false;
  }
  if (locator) {
    *locator = iter->second;
  }
  return true;
}

static constexpr float MaxAtlasFontSize = 256.f;

std::unique_ptr<TextAtlas> TextAtlas::Make(const TextGlyphs* textGlyphs, RenderCache* renderCache,
                                           float scale) {
  auto context = renderCache->getContext();
  auto maxTextureSize = context->caps()->maxTextureSize;
  auto maxScale = scale * textGlyphs->maxScale();
  const auto& maskGlyphs = textGlyphs->maskAtlasGlyphs();
  if (maskGlyphs.empty() || maskGlyphs[0]->getFont().getSize() * maxScale > MaxAtlasFontSize) {
    return nullptr;
  }
  const auto& colorGlyphs = textGlyphs->colorAtlasGlyphs();
  if (!colorGlyphs.empty() && colorGlyphs[0]->getFont().getSize() * maxScale > MaxAtlasFontSize) {
    return nullptr;
  }
  auto maskAtlas = Atlas::Make(context, maxScale, maskGlyphs, maxTextureSize).release();
  if (maskAtlas == nullptr) {
    return nullptr;
  }
  auto colorAtlas = Atlas::Make(context, maxScale, colorGlyphs, maxTextureSize, false).release();
  return std::unique_ptr<TextAtlas>(new TextAtlas(textGlyphs->id(), maskAtlas, colorAtlas, scale));
}

TextAtlas::~TextAtlas() {
  delete maskAtlas;
  delete colorAtlas;
}

bool TextAtlas::getLocator(const tgfx::BytesKey& bytesKey, AtlasLocator* locator) const {
  if (colorAtlas && colorAtlas->getLocator(bytesKey, locator)) {
    locator->textureIndex += maskAtlas->textures.size();
    return true;
  }
  return maskAtlas->getLocator(bytesKey, locator);
}

std::shared_ptr<tgfx::Texture> TextAtlas::getAtlasTexture(size_t textureIndex) const {
  if (0 <= textureIndex && textureIndex < maskAtlas->textures.size()) {
    return maskAtlas->textures[textureIndex];
  }
  textureIndex = textureIndex - maskAtlas->textures.size();
  if (colorAtlas && 0 <= textureIndex && textureIndex < colorAtlas->textures.size()) {
    return colorAtlas->textures[textureIndex];
  }
  return nullptr;
}

size_t TextAtlas::memoryUsage() const {
  size_t usage = 0;
  if (maskAtlas) {
    usage += maskAtlas->memoryUsage();
  }
  if (colorAtlas) {
    usage += colorAtlas->memoryUsage();
  }
  return usage;
}
}  // namespace pag
