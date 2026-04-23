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
//  license is distributed on "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the details.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <cmath>
#include "base/PAGTest.h"
#include "pagx/html/FontHoist.h"

namespace pag {

// FontSignature::equals tests

CLI_TEST(FontHoistTest, SignatureEqualityBasic) {
  pagx::FontSignature sig1 = {};
  sig1.fontFamily = "Arial";
  sig1.renderFontSize = 16.0f;
  sig1.bold = false;
  sig1.italic = false;
  pagx::FontSignature sig2 = {};
  sig2.fontFamily = "Arial";
  sig2.renderFontSize = 16.0f;
  sig2.bold = false;
  sig2.italic = false;
  EXPECT_TRUE(sig1.equals(sig2));
}

CLI_TEST(FontHoistTest, SignatureEqualityDiffFamily) {
  pagx::FontSignature sig1 = {};
  sig1.fontFamily = "Arial";
  sig1.renderFontSize = 16.0f;
  pagx::FontSignature sig2 = {};
  sig2.fontFamily = "Helvetica";
  sig2.renderFontSize = 16.0f;
  EXPECT_FALSE(sig1.equals(sig2));
}

CLI_TEST(FontHoistTest, SignatureEqualityDiffSize) {
  pagx::FontSignature sig1 = {};
  sig1.fontFamily = "Arial";
  sig1.renderFontSize = 16.0f;
  pagx::FontSignature sig2 = {};
  sig2.fontFamily = "Arial";
  sig2.renderFontSize = 24.0f;
  EXPECT_FALSE(sig1.equals(sig2));
}

CLI_TEST(FontHoistTest, SignatureEqualityDiffBold) {
  pagx::FontSignature sig1 = {};
  sig1.fontFamily = "Arial";
  sig1.renderFontSize = 16.0f;
  sig1.bold = true;
  pagx::FontSignature sig2 = {};
  sig2.fontFamily = "Arial";
  sig2.renderFontSize = 16.0f;
  sig2.bold = false;
  EXPECT_FALSE(sig1.equals(sig2));
}

CLI_TEST(FontHoistTest, SignatureEqualityNaN) {
  pagx::FontSignature sig1 = {};
  sig1.fontFamily = "Arial";
  sig1.renderFontSize = 16.0f;
  sig1.letterSpacing = std::nanf("");
  sig1.lineHeight = std::nanf("");
  pagx::FontSignature sig2 = {};
  sig2.fontFamily = "Arial";
  sig2.renderFontSize = 16.0f;
  sig2.letterSpacing = std::nanf("");
  sig2.lineHeight = std::nanf("");
  EXPECT_TRUE(sig1.equals(sig2));
  // NaN vs non-NaN should not be equal.
  sig2.letterSpacing = 0.0f;
  EXPECT_FALSE(sig1.equals(sig2));
}

// FontSignatureToCss tests

CLI_TEST(FontHoistTest, CssOutputFamilyAndSize) {
  pagx::FontSignature sig = {};
  sig.fontFamily = "Arial";
  sig.renderFontSize = 16.0f;
  auto css = pagx::FontSignatureToCss(sig);
  EXPECT_NE(css.find("font-family:'Arial'"), std::string::npos);
  EXPECT_NE(css.find("font-size:16px"), std::string::npos);
  // No bold/italic/letter-spacing → those properties absent.
  EXPECT_EQ(css.find("font-weight:"), std::string::npos);
  EXPECT_EQ(css.find("font-style:"), std::string::npos);
  EXPECT_EQ(css.find("letter-spacing:"), std::string::npos);
}

CLI_TEST(FontHoistTest, CssOutputBoldItalic) {
  pagx::FontSignature sig = {};
  sig.fontFamily = "Arial";
  sig.renderFontSize = 16.0f;
  sig.bold = true;
  sig.italic = true;
  auto css = pagx::FontSignatureToCss(sig);
  EXPECT_NE(css.find("font-weight:bold"), std::string::npos);
  EXPECT_NE(css.find("font-style:italic"), std::string::npos);
}

CLI_TEST(FontHoistTest, CssOutputLetterSpacing) {
  pagx::FontSignature sig = {};
  sig.fontFamily = "Arial";
  sig.renderFontSize = 16.0f;
  sig.letterSpacing = 2.0f;
  auto css = pagx::FontSignatureToCss(sig);
  EXPECT_NE(css.find("letter-spacing:2px"), std::string::npos);
}

CLI_TEST(FontHoistTest, CssOutputLineHeight) {
  pagx::FontSignature sig = {};
  sig.fontFamily = "Arial";
  sig.renderFontSize = 16.0f;
  sig.lineHeight = 18.4f;
  auto css = pagx::FontSignatureToCss(sig);
  EXPECT_NE(css.find("line-height:18.4px"), std::string::npos);
}

CLI_TEST(FontHoistTest, CssOutputEmptySignature) {
  pagx::FontSignature sig = {};
  auto css = pagx::FontSignatureToCss(sig);
  // Empty signature with fontFamily="" and renderFontSize=0 produces no CSS.
  EXPECT_TRUE(css.empty());
}

}  // namespace pag
