/////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Tencent. All rights reserved.
// Licensed under the Apache License, Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0).
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <fstream>
#include <sstream>
#include "base/PAGTest.h"
#include "pagx/LayoutContext.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/SystemFonts.h"
#include "pagx/TextLayout.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/utils/TextUtils.h"
#include "renderer/FontEmbedder.h"
#include "renderer/TextShaper.h"
#include "renderer/TextUnicode.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/UTF.h"
#include "utils/TestUtils.h"
#ifdef __APPLE__
#include "tgfx/platform/apple/CTTypeface.h"
#endif

namespace pag {

static std::shared_ptr<pagx::PAGXDocument> CapturedParagraphDocument() {
  auto doc = pagx::PAGXDocument::Make(150, 120);
  auto* layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  auto* box = doc->makeNode<pagx::TextBox>("paragraph");
  layer->contents.push_back(box);
  box->width = 120;
  box->height = 90;
  box->lineHeight = 25;
  box->textAlign = pagx::TextAlign::Justify;
  auto* text = doc->makeNode<pagx::Text>("text");
  box->elements.push_back(text);
  text->text = "AB CD\nEF";
  text->fontFamily = "CaptureFixture";
  text->fontSize = 20;
  box->layoutFragments = {{0, 0, 2, {4.375f, 20.25f}, {4.375f, 0.25f, 35, 25}, {0, 16}},
                          {0, 3, 2, {7.125f, 47.5f}, {7.125f, 27.5f, 38, 25}, {0, 19}},
                          {0, 6, 2, {8.25f, 82.125f}, {8.25f, 62.125f, 36, 25}, {0, 17}}};
  box->layoutKey = pagx::TextLayout::CaptureKey(pagx::TextLayout::MakeElements({text}),
                                                pagx::MakeTextBoxParams(box));
  return doc;
}

static void RegisterCaptureFont(pagx::FontConfig& config) {
  config.registerFont(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"), 0,
                      "CaptureFixture", "Regular");
}

CLI_TEST(PAGXCapturedParagraphTest, PreservesLineOriginsWithoutRejustifying) {
  auto doc = CapturedParagraphDocument();
  auto* box = doc->findNode<pagx::TextBox>("paragraph");
  auto* text = doc->findNode<pagx::Text>("text");
  pagx::FontConfig config;
  RegisterCaptureFont(config);
  pagx::LayoutContext context(&config);
  auto elements = pagx::TextLayout::MakeElements({text});
  auto params = pagx::MakeTextBoxParams(box);
  ASSERT_TRUE(pagx::TextLayout::ValidateCapture(elements, params));
  auto result = pagx::TextLayout::Layout(elements, params, &context, false);
  auto runs = result.extractLayoutRuns(text);
  ASSERT_EQ(runs.size(), 3u);
  for (size_t i = 0; i < runs.size(); ++i) {
    ASSERT_EQ(runs[i].positions.size(), 2u);
    EXPECT_NEAR(runs[i].positions[0].x, box->layoutFragments[i].position.x, .001f);
    EXPECT_NEAR(runs[i].positions[0].y, box->layoutFragments[i].position.y, .001f);
    EXPECT_NEAR(runs[i].positions[1].x - runs[i].positions[0].x,
                box->layoutFragments[i].inlinePositions[1], .001f);
  }
  ASSERT_NE(result.getTextLines(text), nullptr);
  EXPECT_EQ(result.getTextLines(text)->size(), 3u);
  EXPECT_EQ(text->text, "AB CD\nEF");
}

CLI_TEST(PAGXCapturedParagraphTest, EmbeddingAndSerializationKeepSemanticStylesAndLayout) {
  auto doc = CapturedParagraphDocument();
  auto* box = doc->findNode<pagx::TextBox>("paragraph");
  auto* text = doc->findNode<pagx::Text>("text");
  text->fauxItalic = true;
  box->layoutKey = pagx::TextLayout::CaptureKey(pagx::TextLayout::MakeElements({text}),
                                                pagx::MakeTextBoxParams(box));
  const auto key = box->layoutKey;
  pagx::FontConfig config;
  RegisterCaptureFont(config);
  doc->applyLayout(&config);
  ASSERT_TRUE(pagx::FontEmbedder().embed(doc.get()));
  ASSERT_EQ(text->glyphRuns.size(), 3u);
  EXPECT_TRUE(text->fauxItalic);
  EXPECT_EQ(text->glyphRuns.front()->fauxItalic, false);
  auto copy = pagx::PAGXImporter::FromXML(pagx::PAGXExporter::ToXML(*doc));
  ASSERT_NE(copy, nullptr);
  ASSERT_TRUE(copy->errors.empty());
  copy->applyLayout(&config);
  auto* restoredBox = copy->findNode<pagx::TextBox>("paragraph");
  auto* restoredText = copy->findNode<pagx::Text>("text");
  EXPECT_EQ(restoredBox->layoutKey, key);
  EXPECT_EQ(restoredText->text, "AB CD\nEF");
  EXPECT_TRUE(restoredText->fauxItalic);
  EXPECT_EQ(restoredText->glyphRuns.front()->fauxItalic, false);
  // Re-embedding starts from semantic text, with its synthetic style still intact.
  pagx::FontEmbedder::ClearEmbeddedGlyphRuns(copy.get());
  copy->applyLayout(&config);
  EXPECT_EQ(restoredBox->layoutKey, key);
  ASSERT_TRUE(pagx::FontEmbedder().embed(copy.get()));
  EXPECT_EQ(restoredText->glyphRuns.size(), 3u);
}

CLI_TEST(PAGXCapturedParagraphTest, EditsInvalidateTheWholeParagraphAndEmbeddedGlyphs) {
  for (int edit = 0; edit < 8; ++edit) {
    auto doc = CapturedParagraphDocument();
    auto* box = doc->findNode<pagx::TextBox>("paragraph");
    auto* text = doc->findNode<pagx::Text>("text");
    pagx::FontConfig config;
    RegisterCaptureFont(config);
    doc->applyLayout(&config);
    ASSERT_TRUE(pagx::FontEmbedder().embed(doc.get()));
    ASSERT_FALSE(text->glyphRuns.empty());
    switch (edit) {
      case 0:
        text->text = "New content";
        break;
      case 1:
        text->fontSize = 22;
        break;
      case 2:
        text->letterSpacing = 2;
        break;
      case 3:
        box->width = 80;
        break;
      case 4:
        box->lineHeight = 30;
        break;
      case 5:
        text->fontFamily = "Another family";
        break;
      case 6:
        text->fauxItalic = true;
        break;
      case 7:
        box->layoutFragments[0].position.x += 3;
        break;
    }
    // applyLayout() resets measured sizes on an already laid-out document, as notifyChange does.
    doc->applyLayout(&config);
    EXPECT_TRUE(box->layoutKey.empty()) << edit;
    EXPECT_TRUE(box->layoutFragments.empty()) << edit;
    EXPECT_TRUE(text->glyphRuns.empty()) << edit;
  }
}

CLI_TEST(PAGXCapturedParagraphTest, RuntimeOverrideCannotReuseEmbeddedContent) {
  auto doc = CapturedParagraphDocument();
  auto* box = doc->findNode<pagx::TextBox>("paragraph");
  auto* text = doc->findNode<pagx::Text>("text");
  pagx::FontConfig config;
  RegisterCaptureFont(config);
  doc->applyLayout(&config);
  ASSERT_TRUE(pagx::FontEmbedder().embed(doc.get()));
  auto elements = pagx::TextLayout::MakeElements({text});
  elements.front().glyph.text = "Q";
  pagx::LayoutContext context(&config);
  auto result = pagx::TextLayout::Layout(elements, pagx::MakeTextBoxParams(box), &context);
  auto runs = result.extractLayoutRuns(text);
  ASSERT_EQ(runs.size(), 1u);
  EXPECT_EQ(runs.front().glyphs.size(), 1u);
  EXPECT_EQ(text->text, "AB CD\nEF");
  EXPECT_FALSE(box->layoutKey.empty());
}

CLI_TEST(PAGXCapturedParagraphTest, InvalidRangesFallBackAtomically) {
  for (int invalid = 0; invalid < 5; ++invalid) {
    auto doc = CapturedParagraphDocument();
    auto* box = doc->findNode<pagx::TextBox>("paragraph");
    auto* text = doc->findNode<pagx::Text>("text");
    switch (invalid) {
      case 0:
        box->layoutFragments[1].start = 1;
        break;  // Overlap.
      case 1:
        box->layoutFragments[0].length = 100;
        break;
      case 2:
        box->layoutFragments[0].inlinePositions.pop_back();
        break;
      case 3:
        box->layoutFragments[0].position.x = NAN;
        break;
      case 4:
        box->layoutFragments.erase(box->layoutFragments.begin());
        break;  // Lost letters.
    }
    const auto elements = pagx::TextLayout::MakeElements({text});
    const auto params = pagx::MakeTextBoxParams(box);
    box->layoutKey = pagx::TextLayout::CaptureKey(elements, params);
    EXPECT_FALSE(pagx::TextLayout::ValidateCapture(elements, params)) << invalid;
  }
}

CLI_TEST(PAGXCapturedParagraphTest, FragmentBoundariesCannotSplitGraphemes) {
  pagx::TextElement element;
  element.glyph.text = "Á😀";
  std::vector<pagx::TextLayoutFragment> fragments = {{0, 0, 1, {0, 20}, {0, 0, 20, 25}, {0}},
                                                     {0, 1, 2, {0, 50}, {0, 30, 20, 25}, {0, 0}}};
  EXPECT_FALSE(pagx::TextLayout::ValidateFragments({element}, fragments));
  fragments[0].length = 2;
  fragments[0].inlinePositions = {0, 0};
  fragments[1].start = 2;
  fragments[1].length = 1;
  fragments[1].inlinePositions = {0};
  EXPECT_TRUE(pagx::TextLayout::ValidateFragments({element}, fragments));
}

CLI_TEST(PAGXCapturedParagraphTest, EmbeddingSeparatesSyntheticStylesOfTheSameGlyph) {
  auto doc = pagx::PAGXDocument::Make(200, 80);
  auto* layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  std::vector<pagx::Text*> texts;
  for (int style = 0; style < 3; ++style) {
    auto* text = doc->makeNode<pagx::Text>();
    text->text = "A";
    text->fontFamily = "CaptureFixture";
    text->fontSize = 20;
    text->fauxItalic = style == 1;
    text->fauxBold = style == 2;
    layer->contents.push_back(text);
    texts.push_back(text);
  }
  pagx::FontConfig config;
  RegisterCaptureFont(config);
  doc->applyLayout(&config);
  ASSERT_TRUE(pagx::FontEmbedder().embed(doc.get()));
  for (auto* text : texts) {
    ASSERT_EQ(text->glyphRuns.size(), 1u);
    EXPECT_EQ(text->glyphRuns.front()->fauxItalic, false);
    EXPECT_EQ(text->glyphRuns.front()->fauxBold, false);
  }
  EXPECT_TRUE(texts[1]->fauxItalic);
  EXPECT_TRUE(texts[2]->fauxBold);
  EXPECT_NE(texts[0]->glyphRuns[0]->glyphs, texts[1]->glyphRuns[0]->glyphs);
  EXPECT_NE(texts[0]->glyphRuns[0]->glyphs, texts[2]->glyphRuns[0]->glyphs);
}

CLI_TEST(PAGXTextFallbackTest, UnicodeGraphemeConformance) {
  std::ifstream file(ProjectPath::Absolute("test/data/unicode/GraphemeBreakTest.txt"));
  ASSERT_TRUE(file.good());
  std::string line;
  size_t cases = 0;
  while (std::getline(file, line)) {
    auto data = line.substr(0, line.find('#'));
    std::istringstream tokens(data);
    std::string token, text;
    std::vector<size_t> expected;
    while (tokens >> token) {
      if (token == "÷") expected.push_back(text.size());
      else if (token != "×")
        text += tgfx::UTF::ToUTF8(std::stoi(token, nullptr, 16));
    }
    if (text.empty()) continue;
    std::vector<size_t> actual;
    for (const auto& cluster : pagx::SegmentTextClusters(text)) actual.push_back(cluster.start);
    actual.push_back(text.size());
    EXPECT_EQ(actual, expected) << line;
    ++cases;
  }
  EXPECT_GT(cases, 1000u);
}

CLI_TEST(PAGXTextFallbackTest, EmojiSequencesAndSpacesUseIndependentFonts) {
  pagx::FontConfig fonts;
  fonts.registerFont(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"), 0, "Primary",
                     "Regular");
  fonts.addFallbackFont(ProjectPath::Absolute("resources/font/NotoColorEmoji.ttf"), 0);
  pagx::LayoutContext context(&fonts);
  tgfx::Font font(context.findTypeface("Primary", "Regular"), 44);
  for (const std::string emoji :
       {"😀", "❤️", "☀️", "⭐", "👍🏽", "👨‍👩‍👧‍👦", "🇨🇳", "1️⃣"}) {
    auto glyphs = pagx::TextShaper::Shape(emoji, font, context, false, false,
                                          pagx::TextOrientation::Mixed, "Primary", "Regular");
    ASSERT_FALSE(glyphs.empty()) << emoji;
    for (const auto& glyph : glyphs) {
      EXPECT_TRUE(glyph.font.getTypeface()->hasColor()) << emoji;
      EXPECT_EQ(glyph.cluster, 0u) << emoji;
    }
    EXPECT_EQ(glyphs.size(), 1u) << emoji;  // These sequences are ligatures in the fixture font.
  }
  auto glyphs = pagx::TextShaper::Shape("😀 😀", font, context, false, false,
                                        pagx::TextOrientation::Mixed, "Primary", "Regular");
  ASSERT_EQ(glyphs.size(), 3u);
  EXPECT_FALSE(glyphs[1].font.getTypeface()->hasColor());
  auto space = pagx::TextShaper::Shape(" ", font, context, false, false,
                                       pagx::TextOrientation::Mixed, "Primary", "Regular");
  ASSERT_EQ(space.size(), 1u);
  EXPECT_FLOAT_EQ(glyphs[1].xAdvance, space[0].xAdvance);
  auto textHeart = pagx::TextShaper::Shape("❤︎", font, context, false, false,
                                           pagx::TextOrientation::Mixed, "Primary", "Regular");
  ASSERT_FALSE(textHeart.empty());
  EXPECT_FALSE(textHeart.front().font.getTypeface()->hasColor());
}

CLI_TEST(PAGXTextFallbackTest, FamilyListsStayScopedAndPersist) {
  pagx::FontConfig fonts;
  fonts.registerFont(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"), 0, "Sans",
                     "Regular");
  fonts.registerFont(ProjectPath::Absolute("resources/font/NotoSerifSC-Regular.otf"), 0, "Serif",
                     "Regular");
  pagx::LayoutContext context(&fonts);
  tgfx::Font primary(context.findTypeface("Sans", "Regular"), 24);
  auto first =
      pagx::TextShaper::Shape("文字", primary, context, false, false, pagx::TextOrientation::Mixed,
                              "Sans", "Regular", "Missing, 'Serif', 'Sans'");
  auto second =
      pagx::TextShaper::Shape("文字", primary, context, false, false, pagx::TextOrientation::Mixed,
                              "Sans", "Regular", "Missing, 'Sans', 'Serif'");
  ASSERT_FALSE(first.empty());
  ASSERT_FALSE(second.empty());
  EXPECT_NE(first[0].font.getTypeface()->fontFamily(), second[0].font.getTypeface()->fontFamily());
  auto doc = pagx::PAGXDocument::Make(200, 100);
  auto layer = doc->makeNode<pagx::Layer>();
  doc->layers.push_back(layer);
  auto text = doc->makeNode<pagx::Text>();
  text->text = "文字❤️";
  text->fontFamilyList = "Missing, 'Serif', Sans";
  text->fontVariantEmoji = "unicode";
  layer->contents.push_back(text);
  auto restored = pagx::PAGXImporter::FromXML(pagx::PAGXExporter::ToXML(*doc));
  ASSERT_NE(restored, nullptr);
  ASSERT_TRUE(restored->errors.empty());
  auto copy = static_cast<pagx::Text*>(restored->layers.front()->contents.front());
  EXPECT_EQ(copy->fontFamilyList, text->fontFamilyList);
  EXPECT_EQ(copy->fontVariantEmoji, text->fontVariantEmoji);
  auto required = restored->getRequiredFonts();
  ASSERT_EQ(required.size(), 3u);
}

CLI_TEST(PAGXTextFallbackTest, EmojiPresentationPreservesSourceClustersAndExplicitSelectors) {
  pagx::FontConfig fonts;
  fonts.registerFont(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"), 0, "Primary",
                     "Regular");
  fonts.addFallbackFont(ProjectPath::Absolute("resources/font/NotoColorEmoji.ttf"), 0);
  pagx::LayoutContext context(&fonts);
  tgfx::Font primary(context.findTypeface("Primary", "Regular"), 24);
  auto shape = [&](const std::string& text, const std::string& style) {
    return pagx::TextShaper::Shape(text, primary, context, false, false,
                                   pagx::TextOrientation::Mixed, "Primary", "Regular", "", style);
  };
  auto implicit = shape("❤ A", "emoji");
  auto explicitEmoji = shape("❤️ A", "normal");
  ASSERT_EQ(implicit.size(), explicitEmoji.size());
  ASSERT_EQ(implicit.size(), 3u);
  for (size_t i = 0; i < implicit.size(); ++i) {
    EXPECT_EQ(implicit[i].glyphID, explicitEmoji[i].glyphID);
    EXPECT_EQ(implicit[i].font.getTypeface(), explicitEmoji[i].font.getTypeface());
  }
  EXPECT_EQ(implicit[0].cluster, 0u);
  EXPECT_EQ(implicit[1].cluster, 3u);
  EXPECT_EQ(implicit[2].cluster, 4u);
  EXPECT_TRUE(implicit[0].font.getTypeface()->hasColor());
  auto textOverride = shape("❤︎", "emoji");
  auto emojiOverride = shape("❤️", "text");
  ASSERT_FALSE(textOverride.empty());
  ASSERT_FALSE(emojiOverride.empty());
  EXPECT_FALSE(textOverride[0].font.getTypeface()->hasColor());
  EXPECT_TRUE(emojiOverride[0].font.getTypeface()->hasColor());
  auto implicitText = shape("❤", "text");
  ASSERT_FALSE(implicitText.empty());
  EXPECT_EQ(implicitText[0].glyphID, textOverride[0].glyphID);
  EXPECT_EQ(implicitText[0].font.getTypeface(), textOverride[0].font.getTypeface());
}

#ifdef __APPLE__
CLI_TEST(PAGXTextFallbackTest, NativeSystemFontMetricsFollowRequestedSize) {
  pagx::LayoutContext context(nullptr);
  for (float size : {20.0f, 22.0f, 44.0f, 64.0f}) {
    auto ui = context.findTypeface("system-ui", "Regular", -1, size);
    ASSERT_NE(ui, nullptr);
    ASSERT_NE(tgfx::CTTypeface::GetCTFont(ui.get()), nullptr);
    auto reference = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, size, nullptr);
    UniChar character = ' ';
    CGGlyph glyph = 0;
    CGSize advance = {};
    CTFontGetGlyphsForCharacters(reference, &character, &glyph, 1);
    CTFontGetAdvancesForGlyphs(reference, kCTFontOrientationHorizontal, &glyph, &advance, 1);
    tgfx::Font font(ui, size);
    auto space = pagx::TextShaper::Shape(" ", font, context, false, false,
                                         pagx::TextOrientation::Mixed, "system-ui", "Regular");
    ASSERT_EQ(space.size(), 1u);
    EXPECT_NEAR(space[0].xAdvance, advance.width, 1.0f / 64);
    CFRelease(reference);
    // Atlas rasterization changes the physical size, not the optical-size design selected
    // for this text. Scaling a native system face must retain the same glyph outline.
    auto letter = ui->getGlyphID('H');
    tgfx::Path originalPath, scaledPath;
    ASSERT_TRUE(font.getPath(letter, &originalPath));
    ASSERT_TRUE(tgfx::Font(ui, size * 4).getPath(letter, &scaledPath));
    scaledPath.transform(tgfx::Matrix::MakeScale(0.25f));
    auto originalBounds = originalPath.getBounds();
    auto scaledBounds = scaledPath.getBounds();
    EXPECT_NEAR(originalBounds.left, scaledBounds.left, 0.001f);
    EXPECT_NEAR(originalBounds.top, scaledBounds.top, 0.001f);
    EXPECT_NEAR(originalBounds.right, scaledBounds.right, 0.001f);
    EXPECT_NEAR(originalBounds.bottom, scaledBounds.bottom, 0.001f);
    auto emoji = pagx::TextShaper::Shape("😀", font, context);
    ASSERT_EQ(emoji.size(), 1u);
    auto emojiFont = emoji.front().font;
    auto native = tgfx::CTTypeface::GetCTFont(emojiFont.getTypeface().get());
    ASSERT_NE(native, nullptr);
    glyph = emoji.front().glyphID;
    // Apple emoji occupy one logical em, even when CoreText optically enlarges their bitmap.
    EXPECT_NEAR(emoji.front().xAdvance, size, 1.0f / 64);
    tgfx::Matrix matrix;
    ASSERT_NE(emojiFont.getImage(glyph, nullptr, &matrix), nullptr);
    auto bounds =
        CTFontGetBoundingRectsForGlyphs(native, kCTFontOrientationHorizontal, &glyph, nullptr, 1);
    EXPECT_NEAR(matrix.getTranslateY(), -CGRectGetMaxY(bounds), 1.01f);
  }
}
#endif
}  // namespace pag
