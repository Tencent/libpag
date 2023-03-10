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
#include "tgfx/core/Canvas.h"
#include "tgfx/core/Mask.h"
#include "tgfx/gpu/Surface.h"

namespace pag {
class Atlas {
 public:
  static std::unique_ptr<Atlas> Make(tgfx::Context* context, const std::vector<GlyphHandle>& glyphs,
                                     int maxPageSize, bool alphaOnly = true);

  bool getLocator(const tgfx::BytesKey& bytesKey, AtlasLocator* locator) const;

  size_t memoryUsage() const;

 private:
  Atlas(std::vector<std::shared_ptr<tgfx::Image>> images,
        std::unordered_map<tgfx::BytesKey, AtlasLocator, tgfx::BytesHasher> glyphLocators)
      : images(std::move(images)), glyphLocators(std::move(glyphLocators)) {
  }

  std::vector<std::shared_ptr<tgfx::Image>> images;
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
    auto point = Point::Make(x, y);
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
  if (images.empty()) {
    return 0;
  }
  size_t usage = 0;
  for (auto& image : images) {
    usage += image->width() * image->height();
  }
  auto bytesPerPixels = images[0]->isAlphaOnly() ? 1 : 4;
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

static std::vector<Page> CreatePages(const std::vector<GlyphHandle>& glyphs, int maxPageSize) {
  std::vector<Page> pages = {};
  std::vector<tgfx::BytesKey> styleKeys = {};
  std::vector<AtlasTextRun> textRuns = {};
  int padding = DefaultPadding;
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
    float strokeWidth = 0;
    if (glyph->getStyle() == TextStyle::Stroke) {
      strokeWidth = glyph->getStrokeWidth();
    }
    auto bounds = glyph->getBounds();
    bounds.outset(strokeWidth, strokeWidth);
    bounds.roundOut();
    int width = static_cast<int>(bounds.width());
    int height = static_cast<int>(bounds.height());
    auto packWidth = pack.width();
    auto packHeight = pack.height();
    auto point = pack.addRect(width, height);
    if (pack.width() > maxPageSize || pack.height() > maxPageSize) {
      page.textRuns = std::move(textRuns);
      page.width = packWidth;
      page.height = packHeight;
      pages.push_back(std::move(page));
      styleKeys.clear();
      textRuns = {};
      page = {};
      pack.reset();
      point = pack.addRect(width, height);
    }
    textRun->glyphIDs.push_back(glyph->getGlyphID());
    textRun->positions.push_back({-bounds.x() + point.x, -bounds.y() + point.y});
    AtlasLocator locator;
    locator.imageIndex = pages.size();
    locator.location = tgfx::Rect::MakeXYWH(point.x, point.y, static_cast<float>(width),
                                            static_cast<float>(height));
    locator.glyphBounds = bounds;
    tgfx::BytesKey bytesKey;
    glyph->computeAtlasKey(&bytesKey, glyph->getStyle());
    page.locators[bytesKey] = locator;
  }
  page.textRuns = std::move(textRuns);
  page.width = pack.width();
  page.height = pack.height();
  pages.push_back(std::move(page));
  return pages;
}

std::shared_ptr<tgfx::Image> DrawMask(tgfx::Context* context, const Page& page) {
  auto mask = tgfx::Mask::Make(page.width, page.height);
  if (mask == nullptr) {
    LOGE("Atlas: create mask failed.");
    return nullptr;
  }
  for (auto& textRun : page.textRuns) {
    auto blob = tgfx::TextBlob::MakeFrom(&textRun.glyphIDs[0], &textRun.positions[0],
                                         textRun.glyphIDs.size(), textRun.textFont);
    if (textRun.paint.getStyle() == tgfx::PaintStyle::Fill) {
      mask->fillText(blob.get());
    } else {
      mask->strokeText(blob.get(), *textRun.paint.getStroke());
    }
  }
  return mask->makeImage(context);
}

std::shared_ptr<tgfx::Image> DrawColor(tgfx::Context* context, const Page& page) {
  auto surface = tgfx::Surface::Make(context, page.width, page.height);
  if (surface == nullptr) {
    return nullptr;
  }
  auto canvas = surface->getCanvas();
  auto totalMatrix = canvas->getMatrix();
  for (auto& textRun : page.textRuns) {
    canvas->setMatrix(totalMatrix);
    auto glyphs = &textRun.glyphIDs[0];
    auto positions = &textRun.positions[0];
    canvas->drawGlyphs(glyphs, positions, textRun.glyphIDs.size(), textRun.textFont, textRun.paint);
  }
  canvas->setMatrix(totalMatrix);
  return surface->makeImageSnapshot();
}

static std::vector<std::shared_ptr<tgfx::Image>> DrawPages(tgfx::Context* context,
                                                           std::vector<Page>* pages,
                                                           bool alphaOnly) {
  std::vector<std::shared_ptr<tgfx::Image>> images;
  for (auto& page : *pages) {
    auto image = alphaOnly ? DrawMask(context, page) : DrawColor(context, page);
    if (image) {
      images.push_back(image);
    }
  }
  return images;
}

std::unique_ptr<Atlas> Atlas::Make(tgfx::Context* context, const std::vector<GlyphHandle>& glyphs,
                                   int maxPageSize, bool alphaOnly) {
  if (glyphs.empty()) {
    return nullptr;
  }
  auto pages = CreatePages(glyphs, maxPageSize);
  if (pages.empty()) {
    return nullptr;
  }
  std::unordered_map<tgfx::BytesKey, AtlasLocator, tgfx::BytesHasher> glyphLocators;
  for (const auto& page : pages) {
    glyphLocators.insert(page.locators.begin(), page.locators.end());
  }
  auto images = DrawPages(context, &pages, alphaOnly);
  return std::unique_ptr<Atlas>(new Atlas(std::move(images), std::move(glyphLocators)));
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

std::unique_ptr<TextAtlas> TextAtlas::Make(const TextBlock* textBlock, RenderCache* renderCache,
                                           float scale) {
  auto context = renderCache->getContext();
  auto maxPageSize = context->caps()->maxTextureSize;
  auto maxScale = scale * textBlock->maxScale();
  auto maskGlyphs = textBlock->maskAtlasGlyphs(maxScale);
  if (maskGlyphs.empty() || maskGlyphs[0]->getFont().getSize() > MaxAtlasFontSize) {
    return nullptr;
  }
  auto colorGlyphs = textBlock->colorAtlasGlyphs(maxScale);
  if (!colorGlyphs.empty() && colorGlyphs[0]->getFont().getSize() > MaxAtlasFontSize) {
    return nullptr;
  }
  auto maskAtlas = Atlas::Make(context, maskGlyphs, maxPageSize).release();
  if (maskAtlas == nullptr) {
    return nullptr;
  }
  auto colorAtlas = Atlas::Make(context, colorGlyphs, maxPageSize, false).release();
  return std::unique_ptr<TextAtlas>(
      new TextAtlas(textBlock->id(), maskAtlas, colorAtlas, scale, maxScale));
}

TextAtlas::~TextAtlas() {
  delete maskAtlas;
  delete colorAtlas;
}

bool TextAtlas::getLocator(const tgfx::BytesKey& bytesKey, AtlasLocator* locator) const {
  if (colorAtlas && colorAtlas->getLocator(bytesKey, locator)) {
    locator->imageIndex += maskAtlas->images.size();
    return true;
  }
  return maskAtlas->getLocator(bytesKey, locator);
}

std::shared_ptr<tgfx::Image> TextAtlas::getAtlasImage(size_t imageIndex) const {
  if (imageIndex < maskAtlas->images.size()) {
    return maskAtlas->images[imageIndex];
  }
  imageIndex = imageIndex - maskAtlas->images.size();
  if (colorAtlas && imageIndex < colorAtlas->images.size()) {
    return colorAtlas->images[imageIndex];
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
