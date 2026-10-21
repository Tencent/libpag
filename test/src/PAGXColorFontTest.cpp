/////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Tencent. All rights reserved.
// Licensed under the Apache License, Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0).
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <array>
#include <cmath>
#include "base/PAGTest.h"
#include "pagx/LayoutContext.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/TextLayout.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/SolidColor.h"
#include "renderer/FontEmbedder.h"
#include "renderer/GlyphRunRenderer.h"
#include "renderer/TextShaper.h"
#include "tgfx/core/CustomTypeface.h"
#include "tgfx/core/GlyphRun.h"
#include "utils/TestUtils.h"

namespace pag {
namespace {
std::string Fixture(const std::string& name = "ColorPaints.otf") {
  return ProjectPath::Absolute("resources/font/test/" + name);
}

struct Raster {
  int width = 0;
  int height = 0;
  tgfx::Matrix matrix;
  std::vector<uint8_t> pixels;

  std::array<int, 4> at(int x, int y) const {
    if (x < 0 || y < 0 || x >= width || y >= height) return {};
    auto p = pixels.data() + (y * width + x) * 4;
    return {p[0], p[1], p[2], p[3]};
  }
};

Raster Rasterize(const tgfx::Font& font, int cp, const tgfx::GlyphImageOptions& options = {}) {
  Raster result;
  auto codec = font.getImage(font.getGlyphID(cp), nullptr, &result.matrix, options);
  if (!codec) return result;
  result.width = codec->width();
  result.height = codec->height();
  auto info = tgfx::ImageInfo::Make(result.width, result.height, tgfx::ColorType::RGBA_8888,
                                    tgfx::AlphaType::Premultiplied);
  result.pixels.resize(info.byteSize());
  if (!codec->readPixels(info, result.pixels.data())) return {};
  return result;
}

pagx::Text* AddText(const std::shared_ptr<pagx::PAGXDocument>& doc, const std::string& value,
                    float size, pagx::Color color = {1, 0, 0, 1}) {
  auto* layer = doc->makeNode<pagx::Layer>();
  layer->width = doc->width;
  layer->height = doc->height;
  doc->layers.push_back(layer);
  auto* text = doc->makeNode<pagx::Text>();
  text->text = value;
  text->fontFamily = "Fixture";
  text->fontSize = size;
  text->baseline = pagx::TextBaseline::Alphabetic;
  text->position = {20, size + 20};
  auto* fill = doc->makeNode<pagx::Fill>();
  auto* solid = doc->makeNode<pagx::SolidColor>();
  solid->color = color;
  fill->color = solid;
  layer->contents = {text, fill};
  return text;
}

void Layout(const std::shared_ptr<pagx::PAGXDocument>& doc,
            const std::string& file = "ColorPaints.otf") {
  pagx::FontConfig fonts;
  fonts.registerFont(Fixture(file), 0, "Fixture", "Regular");
  doc->applyLayout(&fonts);
}
}  // namespace

CLI_TEST(PAGXColorFontTest, ColorFamilyKeepsOrdinaryEmojiBases) {
  for (auto file : {"ColorPaints.otf", "ColorLayers.otf"}) {
    pagx::FontConfig fonts;
    fonts.registerFont(Fixture(file), 0, "Fixture", "Regular");
    fonts.addFallbackFont(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"), 0);
    pagx::LayoutContext context(&fonts);
    tgfx::Font font(context.findTypeface("Fixture", "Regular"), 22);
    auto shaped = pagx::TextShaper::Shape("A 0#* B", font, context, false, false,
                                          pagx::TextOrientation::Mixed, "Fixture", "Regular");
    ASSERT_EQ(shaped.size(), 7u);
    for (const auto& g : shaped) EXPECT_EQ(g.font.getTypeface(), font.getTypeface());
  }
}

CLI_TEST(PAGXColorFontTest, RepresentationIsPerGlyphAndCOLRPrecedesSVG) {
  tgfx::Font font(tgfx::Typeface::MakeFromPath(Fixture()), 100);
  ASSERT_NE(font.getTypeface(), nullptr);
  EXPECT_EQ(font.getGlyphRepresentation(font.getGlyphID('A')).content, tgfx::GlyphContent::Color);
  EXPECT_EQ(font.getGlyphRepresentation(font.getGlyphID('B')).content, tgfx::GlyphContent::Outline);
  EXPECT_EQ(font.getGlyphRepresentation(font.getGlyphID(' ')).content, tgfx::GlyphContent::Empty);
  EXPECT_TRUE(font.getGlyphRepresentation(font.getGlyphID('C')).usesForeground);
  tgfx::Path path;
  EXPECT_FALSE(font.getPath(font.getGlyphID('A'), &path));
  EXPECT_TRUE(font.getPath(font.getGlyphID('B'), &path));
  auto image = Rasterize(font, 'A');
  ASSERT_EQ(image.width, 100);
  ASSERT_EQ(image.height, 100);
  // CPAL specifies interpolation in linear-light RGB: a midpoint is approximately 188,188,0.
  auto middle = image.at(49, 50);
  EXPECT_NEAR(middle[0], 188, 2);
  EXPECT_NEAR(middle[1], 188, 2);
  EXPECT_EQ(middle[2], 0);
  EXPECT_EQ(middle[3], 255);
  EXPECT_EQ(Rasterize(font, 'J').pixels, image.pixels);  // PaintColrGlyph reference.
  // COLR can paint outside the head/glyf bounding box. TextBlob culling must use color bounds.
  auto outsideID = font.getGlyphID('O');
  tgfx::Point origin = {};
  auto blob = tgfx::TextBlob::MakeFrom(&outsideID, &origin, 1, font);
  ASSERT_NE(blob, nullptr);
  EXPECT_NEAR(blob->getBounds().left, 120, 1);
  EXPECT_NEAR(blob->getBounds().right, 220, 1);
}

CLI_TEST(PAGXColorFontTest, GradientsTransformsClipsLayersAndComposite) {
  tgfx::Font font(tgfx::Typeface::MakeFromPath(Fixture()), 100);
  auto radial = Rasterize(font, 'D');
  ASSERT_EQ(radial.width, 100);
  EXPECT_GT(radial.at(49, 49)[0], 250);
  EXPECT_LT(radial.at(49, 49)[1], 40);
  EXPECT_EQ(radial.at(0, 0)[1], 255);
  auto sweep = Rasterize(font, 'E');
  ASSERT_EQ(sweep.width, 100);
  EXPECT_NEAR(sweep.at(50, 10)[0], 188, 3);
  EXPECT_NEAR(sweep.at(50, 10)[1], 188, 3);
  EXPECT_EQ(sweep.at(50, 90)[1], 255);
  auto translated = Rasterize(font, 'F');
  EXPECT_EQ(translated.width, 50);
  EXPECT_EQ(translated.height, 50);
  EXPECT_FLOAT_EQ(translated.matrix.getTranslateX(), 20);
  EXPECT_FLOAT_EQ(translated.matrix.getTranslateY(), -60);
  auto rotated = Rasterize(font, 'G');
  EXPECT_EQ(rotated.width, 50);
  EXPECT_EQ(rotated.height, 50);
  EXPECT_NEAR(rotated.matrix.getTranslateX(), 10, 1);
  auto affine = Rasterize(font, 'H');
  EXPECT_EQ(affine.width, 75);
  EXPECT_EQ(affine.height, 50);
  EXPECT_EQ(affine.at(0, 0)[3], 0);
  EXPECT_EQ(affine.at(40, 25)[2], 255);
  auto skew = Rasterize(font, 'I');
  EXPECT_EQ(skew.width, 50);
  EXPECT_EQ(skew.height, 50);  // ClipBox removes negative x.
  auto repeat = Rasterize(font, 'K'), reflect = Rasterize(font, 'L');
  EXPECT_EQ(repeat.at(10, 50), repeat.at(35, 50));
  EXPECT_NE(reflect.at(10, 50), reflect.at(35, 50));
  auto layers = Rasterize(font, 'M');
  EXPECT_EQ(layers.at(10, 10), (std::array<int, 4>{0, 255, 0, 255}));
  EXPECT_EQ(layers.at(50, 50), (std::array<int, 4>{255, 0, 0, 255}));
  auto over = Rasterize(font, 'N');
  EXPECT_NEAR(over.at(50, 50)[0], 188, 1);
  EXPECT_NEAR(over.at(50, 50)[2], 188, 1);
  // Exercise every composite mode with known alpha, including non-separable HSL modes.
  const std::array<int, 28> alphas = {0,   128, 128, 191, 191, 64,  64,  64,  64,  128,
                                      128, 128, 255, 191, 191, 191, 191, 191, 191, 191,
                                      191, 191, 191, 191, 191, 191, 191, 191};
  for (int mode = 1; mode < 28; ++mode) {
    auto blend = Rasterize(font, 0xe000 + mode);
    ASSERT_EQ(blend.width, 100) << mode;
    EXPECT_NEAR(blend.at(50, 50)[3], alphas[mode], 1) << mode;
  }
  EXPECT_EQ(font.getGlyphRepresentation(font.getGlyphID(0xe000)).content,
            tgfx::GlyphContent::Empty);
}

CLI_TEST(PAGXColorFontTest, PaletteAndForegroundArePerRasterization) {
  tgfx::Font font(tgfx::Typeface::MakeFromPath(Fixture()), 100);
  auto red = Rasterize(font, 'C', {tgfx::Color::Red(), 0});
  auto blue = Rasterize(font, 'C', {tgfx::Color::Blue(), 0});
  EXPECT_EQ(red.at(50, 50), (std::array<int, 4>{255, 0, 0, 255}));
  EXPECT_EQ(blue.at(50, 50), (std::array<int, 4>{0, 0, 255, 255}));
  EXPECT_NE(Rasterize(font, 'A').pixels, Rasterize(font, 'A', {tgfx::Color::Black(), 1}).pixels);
  tgfx::Font v0(tgfx::Typeface::MakeFromPath(Fixture("ColorLayers.otf")), 100);
  EXPECT_EQ(Rasterize(v0, 'A', {tgfx::Color::Green(), 0}).at(50, 50),
            (std::array<int, 4>{0, 255, 0, 255}));
}

CLI_TEST(PAGXColorFontTest, EmbeddingKeepsSizesOrderSpacesAndForeground) {
  auto doc = pagx::PAGXDocument::Make(800, 250);
  auto* mixed = AddText(doc, " A B A", 22);
  auto* large = AddText(doc, "D", 110);
  auto* red = AddText(doc, "C", 50, {1, 0, 0, 1});
  auto* blue = AddText(doc, "C", 50, {0, 0, 1, 1});
  Layout(doc);
  pagx::FontEmbedder embedder;
  ASSERT_TRUE(embedder.embed(doc.get())) << embedder.error();
  ASSERT_EQ(mixed->glyphRuns.size(), 3u);
  EXPECT_EQ(mixed->glyphRuns[0]->glyphs.size(), 3u);
  EXPECT_EQ(mixed->glyphRuns[1]->glyphs.size(), 1u);
  EXPECT_EQ(mixed->glyphRuns[2]->glyphs.size(), 2u);
  auto* smallFont = mixed->glyphRuns[0]->font;
  EXPECT_EQ(smallFont->unitsPerEm, 22);
  EXPECT_EQ(large->glyphRuns[0]->font->unitsPerEm, 110);
  EXPECT_NE(red->glyphRuns[0]->font, blue->glyphRuns[0]->font);
  EXPECT_NE(red->glyphRuns[0]->font->glyphs[0]->image->data,
            blue->glyphRuns[0]->font->glyphs[0]->image->data);
  auto copy = pagx::PAGXImporter::FromXML(pagx::PAGXExporter::ToXML(*doc));
  ASSERT_NE(copy, nullptr);
  copy->applyLayout();
  auto* text = static_cast<pagx::Text*>(copy->layers[0]->contents[0]);
  pagx::GlyphRunRenderer::BuildTextBlob(text, tgfx::Matrix::I());
  ASSERT_NE(text->glyphData->textBlob, nullptr);
  auto& run = *text->glyphData->textBlob->begin();
  ASSERT_EQ(run.glyphCount, 3u);
  EXPECT_TRUE(run.font.getBounds(run.glyphs[0]).isEmpty());
  EXPECT_FALSE(run.font.getBounds(run.glyphs[1]).isEmpty());
  EXPECT_TRUE(run.font.getBounds(run.glyphs[2]).isEmpty());
}

class FailedGlyphCodec : public tgfx::ImageCodec {
 public:
  FailedGlyphCodec() : ImageCodec(10, 10) {
  }

 protected:
  bool onReadPixels(tgfx::ColorType, tgfx::AlphaType, size_t, std::shared_ptr<tgfx::ColorSpace>,
                    void*) const override {
    return false;
  }
};

CLI_TEST(PAGXColorFontTest, UnsupportedAndDecodeFailuresAreAtomic) {
  auto doc = pagx::PAGXDocument::Make(200, 150);
  auto* text = AddText(doc, "BA", 50);
  Layout(doc, "UnsupportedSVG.otf");
  auto originalCount = doc->nodes.size();
  pagx::FontEmbedder embedder;
  EXPECT_FALSE(embedder.embed(doc.get()));
  EXPECT_NE(embedder.error().find("unsupported glyph"), std::string::npos);
  EXPECT_EQ(doc->nodes.size(), originalCount);
  EXPECT_TRUE(text->glyphRuns.empty());

  auto valid = pagx::PAGXDocument::Make(200, 150);
  auto* t = AddText(valid, "B", 50);
  Layout(valid);
  ASSERT_TRUE(embedder.embed(valid.get()));
  auto originalRuns = t->glyphRuns;
  originalCount = valid->nodes.size();
  tgfx::ImageTypefaceBuilder builder(10);
  auto id = builder.addGlyph(std::make_shared<FailedGlyphCodec>(), {}, 10);
  pagx::TextLayoutGlyphRun run;
  run.font = tgfx::Font(builder.detach(), 50);
  run.glyphs = {id};
  run.positions = {{50, 0}};
  t->glyphData->layoutRuns.push_back(run);
  EXPECT_FALSE(embedder.embed(valid.get()));
  EXPECT_EQ(valid->nodes.size(), originalCount);
  EXPECT_EQ(t->glyphRuns, originalRuns);
}

CLI_TEST(PAGXColorFontTest, BitmapEmojiKeepNativeStrikeMetricsAndEmptyGlyphs) {
  auto doc = pagx::PAGXDocument::Make(600, 250);
  auto* small = AddText(doc, "😀 😀", 22);
  auto* large = AddText(doc, "😀", 110);
  pagx::FontConfig fonts;
  fonts.registerFont(ProjectPath::Absolute("resources/font/NotoColorEmoji.ttf"), 0, "Fixture",
                     "Regular");
  doc->applyLayout(&fonts);
  const auto& original = large->glyphData->layoutRuns.front();
  auto originalBounds = original.font.getBounds(original.glyphs.front());
  pagx::FontEmbedder embedder;
  ASSERT_TRUE(embedder.embed(doc.get())) << embedder.error();
  ASSERT_EQ(small->glyphRuns.size(), 1u);
  ASSERT_EQ(small->glyphRuns.front()->glyphs.size(), 3u);
  pagx::GlyphRunRenderer::BuildTextBlob(small, tgfx::Matrix::I());
  ASSERT_NE(small->glyphData->textBlob, nullptr);
  const auto& smallRun = *small->glyphData->textBlob->begin();
  EXPECT_FALSE(smallRun.font.getBounds(smallRun.glyphs[0]).isEmpty());
  EXPECT_TRUE(smallRun.font.getBounds(smallRun.glyphs[1]).isEmpty());
  EXPECT_FALSE(smallRun.font.getBounds(smallRun.glyphs[2]).isEmpty());
  pagx::GlyphRunRenderer::BuildTextBlob(large, tgfx::Matrix::I());
  ASSERT_NE(large->glyphData->textBlob, nullptr);
  const auto& largeRun = *large->glyphData->textBlob->begin();
  auto embeddedBounds = largeRun.font.getBounds(largeRun.glyphs[0]);
  // FreeType rounds getBounds outward, whereas an image typeface retains the exact scale.
  EXPECT_NEAR(embeddedBounds.width(), originalBounds.width(), 2);
  EXPECT_NEAR(embeddedBounds.height(), originalBounds.height(), 2);
}

CLI_TEST(PAGXColorFontTest, ConflictingForegroundPaintersAreRejected) {
  auto doc = pagx::PAGXDocument::Make(200, 150);
  auto* text = AddText(doc, "C", 50);
  auto* secondFill = doc->makeNode<pagx::Fill>();
  auto* blue = doc->makeNode<pagx::SolidColor>();
  blue->color = {0, 0, 1, 1};
  secondFill->color = blue;
  doc->layers[0]->contents.push_back(secondFill);
  Layout(doc);
  auto originalCount = doc->nodes.size();
  pagx::FontEmbedder embedder;
  EXPECT_FALSE(embedder.embed(doc.get()));
  EXPECT_NE(embedder.error().find("currentColor"), std::string::npos);
  EXPECT_EQ(doc->nodes.size(), originalCount);
  EXPECT_TRUE(text->glyphRuns.empty());
}

CLI_TEST(PAGXColorFontTest, RuntimeAndReloadRenderTheSameColors) {
  auto doc = pagx::PAGXDocument::Make(400, 200);
  AddText(doc, "AB C", 50, {0, 0, 1, 1});
  AddText(doc, "C", 50, {1, 0, 0, 1})->fauxItalic = true;
  doc->layers[1]->x = 200;
  doc->layers[1]->y = 60;
  const float angle = 17 * 3.14159265358979323846f / 180;
  doc->layers[1]->matrix = {
      std::cos(angle), std::sin(angle), -std::sin(angle), std::cos(angle), 0, 0};
  doc->layers[1]->alpha = .6f;
  Layout(doc);
  auto surface = pagx::PAGSurface::MakeOffscreen(400, 200);
  ASSERT_NE(surface, nullptr);
  auto scene = pagx::PAGScene::Make(doc);
  ASSERT_TRUE(scene->draw(surface));
  std::vector<uint8_t> before(400 * 200 * 4), after(before.size());
  ASSERT_TRUE(surface->readPixels(before.data(), 400 * 4));
  pagx::FontEmbedder embedder;
  ASSERT_TRUE(embedder.embed(doc.get())) << embedder.error();
  auto copy = pagx::PAGXImporter::FromXML(pagx::PAGXExporter::ToXML(*doc));
  ASSERT_NE(copy, nullptr);
  scene = pagx::PAGScene::Make(copy);
  ASSERT_TRUE(scene->draw(surface));
  ASSERT_TRUE(surface->readPixels(after.data(), 400 * 4));
  size_t total = 0;
  for (size_t i = 0; i < before.size(); ++i) total += std::abs(before[i] - after[i]);
  EXPECT_LT(static_cast<double>(total) / before.size(), .5);
  EXPECT_GT(before[(45 * 400 + 170) * 4 + 2], 240);  // CurrentColor is blue in the first layer.
  EXPECT_GT(before[(45 * 400 + 95) * 4 + 2], 240);   // Ordinary outline in the same color font.

  auto outside = pagx::PAGXDocument::Make(50, 50);
  auto* text = AddText(outside, "O", 50);
  text->position = {-60, 50};  // Outline bounds are wholly offscreen; the color paint is visible.
  Layout(outside);
  auto smallSurface = pagx::PAGSurface::MakeOffscreen(50, 50);
  ASSERT_NE(smallSurface, nullptr);
  scene = pagx::PAGScene::Make(outside);
  ASSERT_TRUE(scene->draw(smallSurface));
  std::vector<uint8_t> smallPixels(50 * 50 * 4);
  ASSERT_TRUE(smallSurface->readPixels(smallPixels.data(), 50 * 4));
  EXPECT_EQ(smallPixels[(25 * 50 + 25) * 4], 255);
  EXPECT_EQ(smallPixels[(25 * 50 + 25) * 4 + 3], 255);
}

}  // namespace pag
