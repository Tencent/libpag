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
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Text.h"
#include "renderer/TextShaper.h"
#include "renderer/TextUnicode.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/UTF.h"
#include "utils/TestUtils.h"
#ifdef __APPLE__
#include "tgfx/platform/apple/CTTypeface.h"
#endif

namespace pag {

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
