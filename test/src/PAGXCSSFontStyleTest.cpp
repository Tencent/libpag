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
#include <fstream>
#include "base/PAGTest.h"
#include "nlohmann/json.hpp"
#include "pagx/LayoutContext.h"
#include "pagx/SystemFonts.h"
#include "pagx/utils/CSSFontStyle.h"
#include "tgfx/core/Typeface.h"
#include "utils/ProjectPath.h"

namespace pag {

CLI_TEST(PAGXCSSFontStyleTest, InvalidNumericDescriptorsUseCSSDefaults) {
  for (const auto* weight : {"0", "-1", "1001", "nan", "inf", "550px", "500 600"}) {
    SCOPED_TRACE(weight);
    EXPECT_DOUBLE_EQ(pagx::ParseCSSFontStyle(weight, "normal").weight, 400);
  }
  for (const auto* width : {"0%", "-1%", "nan%", "inf%", "80px", "80%junk"}) {
    SCOPED_TRACE(width);
    EXPECT_DOUBLE_EQ(pagx::ParseCSSFontStyle("400", "normal", width).width, 100);
  }
  auto style = pagx::ParseCSSFontStyle(" 550.5 ", " OBLIQUE 12deg ", " 87.25% ");
  EXPECT_DOUBLE_EQ(style.weight, 550.5);
  EXPECT_DOUBLE_EQ(style.width, 87.25);
  EXPECT_EQ(style.slant, pagx::CSSFontSlant::Oblique);
  EXPECT_DOUBLE_EQ(pagx::ParseCSSFontStyle("lighter", "normal").weight, 300);
  EXPECT_DOUBLE_EQ(pagx::ParseCSSFontStyle("bolder", "normal").weight, 700);
  for (int invalid : {-1, 0, 10}) EXPECT_DOUBLE_EQ(pagx::CSSFontWidthFromClass(invalid), 100);
}

CLI_TEST(PAGXCSSFontStyleTest, EscapedFamilyNamesRetainCommasQuotesAndBackslashes) {
  auto families = pagx::ParseCSSFontFamilyList(
      R"CSS( Vendor\, Name , 'Single\'Quote', "Double\"Quote", Back\\Slash, sans-serif )CSS");
  EXPECT_EQ(families, (std::vector<std::string>{"Vendor, Name", "Single'Quote", "Double\"Quote",
                                                "Back\\Slash", "sans-serif"}));
}

CLI_TEST(PAGXCSSFontStyleTest, EqualMetadataUsesStableFaceNameTieBreak) {
  std::vector<pagx::CSSFontFace> faces = {{"Zeta", {550.5, 87.25}},
                                          {"Alpha", {550.5, 87.25}},
                                          {"Near", {551, 87.25}}};
  for (size_t i = 0; i < faces.size(); ++i) {
    auto index = pagx::MatchCSSFontFace(faces, {550.5, 87.25});
    ASSERT_GE(index, 0);
    EXPECT_EQ(faces[index].name, "Alpha");
    std::rotate(faces.begin(), faces.begin() + 1, faces.end());
  }
  EXPECT_EQ(pagx::CompareCSSFontStyles({}, {400, 100}, {400, 100}), 0);
}

CLI_TEST(PAGXCSSFontStyleTest, FontMetadataDistinguishesStaticAndVariableFaces) {
  auto face =
      tgfx::Typeface::MakeFromPath(ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"));
  ASSERT_NE(face, nullptr);
  auto actual = pagx::SystemFonts::TypefaceStyle(face.get(), {900, 50});
  EXPECT_DOUBLE_EQ(actual.weight, 400);
  EXPECT_DOUBLE_EQ(actual.width, 100);
  auto variable = tgfx::Typeface::MakeFromPath(
      ProjectPath::Absolute("resources/font/NotoSansHebrew-Regular.ttf"));
  ASSERT_NE(variable, nullptr);
  ASSERT_NE(variable->copyTableData(0x66766172), nullptr);  // fvar
  // OS/2 describes only the default instance; keep the caller's instantiated native traits.
  auto instance = pagx::SystemFonts::TypefaceStyle(variable.get(), {550.5, 87.25});
  EXPECT_DOUBLE_EQ(instance.weight, 550.5);
  EXPECT_DOUBLE_EQ(instance.width, 87.25);
  auto fallback =
      pagx::SystemFonts::TypefaceStyle(nullptr, {550.5, 87.25, pagx::CSSFontSlant::Oblique});
  EXPECT_DOUBLE_EQ(fallback.weight, 550.5);
  EXPECT_DOUBLE_EQ(fallback.width, 87.25);
  EXPECT_EQ(fallback.slant, pagx::CSSFontSlant::Oblique);
  EXPECT_DOUBLE_EQ(pagx::SystemFonts::TypefaceStyle(nullptr).weight, 400);
}

CLI_TEST(PAGXCSSFontStyleTest, DocumentFontMatchingRetainsRegistrationKeys) {
  pagx::FontConfig fonts;
  auto path = ProjectPath::Absolute("resources/font/NotoSansHebrew-Regular.ttf");
  fonts.registerFont(path, 0, "DocumentAlias", "VendorFace");
  fonts.registerFont(path, 0, "OtherAlias", "OtherFace");
  fonts.addFallbackFont(path, 0, "FallbackAlias", "FallbackFace");
  pagx::LayoutContext context(&fonts);
  std::string selected;
  auto face = context.matchTypeface("DocumentAlias", {900, 75}, &selected);
  ASSERT_NE(face, nullptr);
  EXPECT_EQ(selected, "VendorFace");
  EXPECT_EQ(context.findTypeface("DocumentAlias", selected), face);
  EXPECT_EQ(context.matchTypeface("DocumentAlias", {}), face);
  auto fallback = context.matchTypeface("FallbackAlias", {}, &selected);
  ASSERT_NE(fallback, nullptr);
  EXPECT_EQ(selected, "FallbackFace");
  selected = "unchanged";
  EXPECT_EQ(context.matchTypeface("PAGX Missing Font 5d481d18", {}, &selected), nullptr);
  EXPECT_EQ(selected, "unchanged");
}

CLI_TEST(PAGXCSSFontStyleTest, MissingFamiliesDoNotAcceptPlatformSubstitutions) {
  for (const auto* family : {"", "PAGX Missing Font 5d481d18"}) {
    EXPECT_EQ(pagx::SystemFonts::MatchTypeface(family, {}), nullptr);
    EXPECT_EQ(pagx::SystemFonts::ResolveTypeface(family, ""), nullptr);
    EXPECT_EQ(pagx::SystemFonts::ResolveTypeface(family, "Bold Italic"), nullptr);
  }
  EXPECT_EQ(pagx::SystemFonts::NormalizeFontName(" \tNoto  Sans\n Hebrew  "), "noto sans hebrew");
  EXPECT_TRUE(pagx::SystemFonts::FontNamesMatch(" Noto\tSans ", "noto sans"));
  EXPECT_FALSE(pagx::SystemFonts::FontNamesMatch("NotoSans", "Noto Sans"));
  EXPECT_FALSE(pagx::SystemFonts::FontNamesMatch("", ""));
}

CLI_TEST(PAGXCSSFontStyleTest, SharedPropertyMatchingCases) {
  std::ifstream input(ProjectPath::Absolute("test/fixtures/font-matching-cases.json"));
  ASSERT_TRUE(input.good());
  auto cases = nlohmann::json::parse(input);
  for (const auto& row : cases) {
    SCOPED_TRACE(row["name"].get<std::string>());
    auto props = [](const nlohmann::json& values, size_t offset) {
      return pagx::ParseCSSFontStyle(values[offset].dump(), values[offset + 2].get<std::string>(),
                                     values[offset + 1].dump() + "%");
    };
    auto request = props(row["request"], 0);
    std::vector<pagx::CSSFontFace> faces;
    for (const auto& face : row["faces"])
      faces.push_back({face[0].get<std::string>(), props(face, 1)});
    for (size_t i = 0; i < faces.size(); ++i) {
      auto index = pagx::MatchCSSFontFace(faces, request);
      ASSERT_GE(index, 0);
      EXPECT_EQ(faces[index].name, row["expected"].get<std::string>());
      std::rotate(faces.begin(), faces.begin() + 1, faces.end());
    }
    std::reverse(faces.begin(), faces.end());
    EXPECT_EQ(faces[pagx::MatchCSSFontFace(faces, request)].name,
              row["expected"].get<std::string>());
  }
}

CLI_TEST(PAGXCSSFontStyleTest, NumericRequestsAndWidthNames) {
  for (auto width : {50.0, 62.5, 75.0, 87.5, 100.0, 112.5, 125.0, 150.0, 200.0}) {
    auto request = pagx::ParseCSSFontStyle("550.5", "oblique", std::to_string(width) + "%");
    auto restored = pagx::FontStyleFromName(pagx::FontStyleRequestName(request));
    EXPECT_DOUBLE_EQ(restored.width, width);
    EXPECT_DOUBLE_EQ(restored.weight, 550.5);
    EXPECT_EQ(restored.slant, pagx::CSSFontSlant::Oblique);
  }
  EXPECT_DOUBLE_EQ(pagx::FontStyleFromName("Semi Condensed Bold").width, 87.5);
  EXPECT_DOUBLE_EQ(pagx::FontStyleFromName("Extra-Expanded Light").width, 150);
  EXPECT_DOUBLE_EQ(pagx::FontStyleFromName("UltraLight").width, 100);
  EXPECT_EQ(pagx::MatchCSSFontFace({}, {}), -1);
}

CLI_TEST(PAGXCSSFontStyleTest, NativeMetadataAndExactNameAreDistinct) {
#ifdef __APPLE__
  auto normal = pagx::SystemFonts::MatchTypeface("Helvetica Neue", {800, 100});
  auto condensed = pagx::SystemFonts::MatchTypeface("Helvetica Neue", {800, 75});
  auto exact = pagx::SystemFonts::ResolveTypeface("Helvetica Neue", "Condensed Black");
  ASSERT_NE(normal, nullptr);
  ASSERT_NE(condensed, nullptr);
  ASSERT_NE(exact, nullptr);
  EXPECT_EQ(normal->fontStyle(), "Bold");
  EXPECT_EQ(condensed->fontStyle(), "Condensed Black");
  EXPECT_EQ(exact->fontStyle(), "Condensed Black");
  auto thin = pagx::SystemFonts::MatchTypeface("Helvetica Neue", {100, 100});
  ASSERT_NE(thin, nullptr);
  EXPECT_EQ(thin->fontStyle(), "UltraLight");
  EXPECT_DOUBLE_EQ(pagx::SystemFonts::TypefaceStyle(normal.get()).width, 100);
  EXPECT_DOUBLE_EQ(pagx::SystemFonts::TypefaceStyle(condensed.get()).width, 75);
#endif
}

CLI_TEST(PAGXCSSFontStyleTest, MatchAvailableWeightAndSlant) {
  EXPECT_EQ(pagx::MatchCSSFontStyle({"Regular", "Medium", "Semibold"}, "Bold"), "Semibold");
  EXPECT_EQ(pagx::MatchCSSFontStyle({"Light", "Medium", "Semibold"}, "Regular"), "Medium");
  EXPECT_EQ(pagx::MatchCSSFontStyle({"Light", "Semibold"}, "Regular"), "Light");
  EXPECT_EQ(pagx::MatchCSSFontStyle({"Bold", "Regular Italic"}, "Bold Italic"), "Regular Italic");
  EXPECT_EQ(pagx::MatchCSSFontStyle({"Regular", "Bold"}, "MissingStyle"), "Regular");
}


// ResolveFontStyleName: maps a CSS font-weight + font-style pair onto a PAGX `fontStyle` label.
// Regular (400) collapses to an empty weight portion; italic/oblique fold onto the result.

CLI_TEST(PAGXCSSFontStyleTest, ResolveName_RegularEmpty) {
  EXPECT_EQ(pagx::ResolveFontStyleName("400", ""), "");
}

CLI_TEST(PAGXCSSFontStyleTest, ResolveName_NormalKeywordEmpty) {
  EXPECT_EQ(pagx::ResolveFontStyleName("normal", ""), "");
}

CLI_TEST(PAGXCSSFontStyleTest, ResolveName_EmptyWeightEmpty) {
  EXPECT_EQ(pagx::ResolveFontStyleName("", ""), "");
}

CLI_TEST(PAGXCSSFontStyleTest, ResolveName_Bold) {
  EXPECT_EQ(pagx::ResolveFontStyleName("bold", ""), "Bold");
}

CLI_TEST(PAGXCSSFontStyleTest, ResolveName_Bolder) {
  EXPECT_EQ(pagx::ResolveFontStyleName("bolder", ""), "Bold");
}

CLI_TEST(PAGXCSSFontStyleTest, ResolveName_Lighter) {
  EXPECT_EQ(pagx::ResolveFontStyleName("lighter", ""), "Light");
}

CLI_TEST(PAGXCSSFontStyleTest, ResolveName_BlackItalic) {
  EXPECT_EQ(pagx::ResolveFontStyleName("900", "italic"), "Black Italic");
}

CLI_TEST(PAGXCSSFontStyleTest, ResolveName_MediumItalic) {
  EXPECT_EQ(pagx::ResolveFontStyleName("500", "italic"), "Medium Italic");
}

CLI_TEST(PAGXCSSFontStyleTest, ResolveName_ObliqueOnlyIsItalic) {
  EXPECT_EQ(pagx::ResolveFontStyleName("normal", "oblique"), "Italic");
}

CLI_TEST(PAGXCSSFontStyleTest, ResolveName_RoundsToNearestHundred) {
  // 550 rounds up to 600 -> SemiBold.
  EXPECT_EQ(pagx::ResolveFontStyleName("550", ""), "SemiBold");
}

CLI_TEST(PAGXCSSFontStyleTest, ResolveName_ThinFloor) {
  // 10 clamps up to the 100 keyword (Thin) rather than collapsing to Regular.
  EXPECT_EQ(pagx::ResolveFontStyleName("10", ""), "Thin");
}

CLI_TEST(PAGXCSSFontStyleTest, ResolveName_AboveMaxClampsToBlack) {
  EXPECT_EQ(pagx::ResolveFontStyleName("5000", ""), "Black");
}

CLI_TEST(PAGXCSSFontStyleTest, ResolveName_UnparseableFallsBackToRegular) {
  EXPECT_EQ(pagx::ResolveFontStyleName("heavy-ish", ""), "");
}

CLI_TEST(PAGXCSSFontStyleTest, ResolveName_WhitespaceAndCaseNormalised) {
  EXPECT_EQ(pagx::ResolveFontStyleName("  BOLD  ", "  ITALIC "), "Bold Italic");
}

// ResolveFontStyleSynthesis: splits the request into a real-face label plus a faux italic axis.
// Both the weight and slant axes are real-face keywords (never faux-bold); fauxItalic stays true
// only as a synthesis fallback for when the styled italic face is unavailable at layout time.

CLI_TEST(PAGXCSSFontStyleTest, Synthesis_BlackItalicKeepsRealFaceFauxItalic) {
  auto out = pagx::ResolveFontStyleSynthesis("900", "italic");
  EXPECT_EQ(out.fontStyleName, "Black Italic");
  EXPECT_FALSE(out.fauxBold);
  EXPECT_TRUE(out.fauxItalic);
}

CLI_TEST(PAGXCSSFontStyleTest, Synthesis_BoldKeepsRealWeight) {
  auto out = pagx::ResolveFontStyleSynthesis("700", "");
  EXPECT_EQ(out.fontStyleName, "Bold");
  EXPECT_FALSE(out.fauxBold);
  EXPECT_FALSE(out.fauxItalic);
}

CLI_TEST(PAGXCSSFontStyleTest, Synthesis_SemiBoldItalicKeepsRealFace) {
  auto out = pagx::ResolveFontStyleSynthesis("600", "italic");
  EXPECT_EQ(out.fontStyleName, "SemiBold Italic");
  EXPECT_FALSE(out.fauxBold);
  EXPECT_TRUE(out.fauxItalic);
}

CLI_TEST(PAGXCSSFontStyleTest, Synthesis_MediumItalicKeepsRealFace) {
  auto out = pagx::ResolveFontStyleSynthesis("500", "italic");
  EXPECT_EQ(out.fontStyleName, "Medium Italic");
  EXPECT_FALSE(out.fauxBold);
  EXPECT_TRUE(out.fauxItalic);
}

CLI_TEST(PAGXCSSFontStyleTest, Synthesis_LightKeepsRealFaceNoFaux) {
  auto out = pagx::ResolveFontStyleSynthesis("300", "");
  EXPECT_EQ(out.fontStyleName, "Light");
  EXPECT_FALSE(out.fauxBold);
  EXPECT_FALSE(out.fauxItalic);
}

CLI_TEST(PAGXCSSFontStyleTest, Synthesis_RegularItalicOnly) {
  auto out = pagx::ResolveFontStyleSynthesis("400", "italic");
  EXPECT_EQ(out.fontStyleName, "Italic");
  EXPECT_FALSE(out.fauxBold);
  EXPECT_TRUE(out.fauxItalic);
}

CLI_TEST(PAGXCSSFontStyleTest, Synthesis_RegularPlain) {
  auto out = pagx::ResolveFontStyleSynthesis("400", "");
  EXPECT_EQ(out.fontStyleName, "");
  EXPECT_FALSE(out.fauxBold);
  EXPECT_FALSE(out.fauxItalic);
}

// ParseFontStyleName: parses a PAGX/CSS font-style label back into a numeric weight + italic flag.

CLI_TEST(PAGXCSSFontStyleTest, Parse_EmptyIsRegular) {
  auto out = pagx::ParseFontStyleName("");
  EXPECT_EQ(out.weight, 400);
  EXPECT_FALSE(out.italic);
}

CLI_TEST(PAGXCSSFontStyleTest, Parse_BoldItalic) {
  auto out = pagx::ParseFontStyleName("Bold Italic");
  EXPECT_EQ(out.weight, 700);
  EXPECT_TRUE(out.italic);
}

CLI_TEST(PAGXCSSFontStyleTest, Parse_BlackKeyword) {
  auto out = pagx::ParseFontStyleName("Black");
  EXPECT_EQ(out.weight, 900);
  EXPECT_FALSE(out.italic);
}

CLI_TEST(PAGXCSSFontStyleTest, Parse_ItalicOnlyStaysRegularWeight) {
  auto out = pagx::ParseFontStyleName("Italic");
  EXPECT_EQ(out.weight, 400);
  EXPECT_TRUE(out.italic);
}

CLI_TEST(PAGXCSSFontStyleTest, Parse_VendorAliasDemi) {
  auto out = pagx::ParseFontStyleName("Demi");
  EXPECT_EQ(out.weight, 600);
}

CLI_TEST(PAGXCSSFontStyleTest, Parse_VendorAliasHeavy) {
  auto out = pagx::ParseFontStyleName("Heavy");
  EXPECT_EQ(out.weight, 900);
}

CLI_TEST(PAGXCSSFontStyleTest, Parse_NumericWeight) {
  auto out = pagx::ParseFontStyleName("700");
  EXPECT_EQ(out.weight, 700);
}

CLI_TEST(PAGXCSSFontStyleTest, Parse_NumericClampsLow) {
  auto out = pagx::ParseFontStyleName("50");
  EXPECT_EQ(out.weight, 100);
}

CLI_TEST(PAGXCSSFontStyleTest, Parse_NumericClampsHigh) {
  auto out = pagx::ParseFontStyleName("1200");
  EXPECT_EQ(out.weight, 900);
}

CLI_TEST(PAGXCSSFontStyleTest, Parse_UnknownTokensIgnored) {
  // "W4" and "Pro" are macOS-style compound tokens with no weight meaning; the recognised
  // "SemiBold" token still wins and italic is detected.
  auto out = pagx::ParseFontStyleName("Pro SemiBold W4 Oblique");
  EXPECT_EQ(out.weight, 600);
  EXPECT_TRUE(out.italic);
}

CLI_TEST(PAGXCSSFontStyleTest, Parse_OnlyUnknownTokensDefaultRegular) {
  auto out = pagx::ParseFontStyleName("Pro Condensed");
  EXPECT_EQ(out.weight, 400);
  EXPECT_FALSE(out.italic);
}

CLI_TEST(PAGXCSSFontStyleTest, Parse_CaseInsensitiveKeywords) {
  auto out = pagx::ParseFontStyleName("extrabold");
  EXPECT_EQ(out.weight, 800);
}

}  // namespace pag
