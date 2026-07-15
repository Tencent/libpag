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

#include "base/PAGTest.h"
#include "pagx/utils/CSSFontStyle.h"

namespace pag {

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

// ResolveFontStyleSynthesis: splits the request into a real-face label plus faux axes. The bold
// synthesis threshold is weight >= 600; weights below stay as keywords with no faux flag.

CLI_TEST(PAGXCSSFontStyleTest, Synthesis_BlackItalicIsAllFaux) {
  auto out = pagx::ResolveFontStyleSynthesis("900", "italic");
  EXPECT_EQ(out.fontStyleName, "");
  EXPECT_TRUE(out.fauxBold);
  EXPECT_TRUE(out.fauxItalic);
}

CLI_TEST(PAGXCSSFontStyleTest, Synthesis_BoldOnly) {
  auto out = pagx::ResolveFontStyleSynthesis("700", "");
  EXPECT_EQ(out.fontStyleName, "");
  EXPECT_TRUE(out.fauxBold);
  EXPECT_FALSE(out.fauxItalic);
}

CLI_TEST(PAGXCSSFontStyleTest, Synthesis_SemiBoldThresholdIsFauxBold) {
  auto out = pagx::ResolveFontStyleSynthesis("600", "italic");
  EXPECT_EQ(out.fontStyleName, "");
  EXPECT_TRUE(out.fauxBold);
  EXPECT_TRUE(out.fauxItalic);
}

CLI_TEST(PAGXCSSFontStyleTest, Synthesis_MediumKeepsRealFace) {
  auto out = pagx::ResolveFontStyleSynthesis("500", "italic");
  EXPECT_EQ(out.fontStyleName, "Medium");
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
  EXPECT_EQ(out.fontStyleName, "");
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
