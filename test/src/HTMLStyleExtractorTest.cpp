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

#include "base/PAGTest.h"
#include "pagx/html/HTMLStyleExtractor.h"

namespace pag {

CLI_TEST(HTMLStyleExtractorTest, EmptyInput) {
  EXPECT_EQ(pagx::HTMLStyleExtractor::Extract(""), "");
}

CLI_TEST(HTMLStyleExtractorTest, NoStyleAttributes) {
  std::string input = "<html><body><div></div></body></html>";
  EXPECT_EQ(pagx::HTMLStyleExtractor::Extract(input), input);
}

CLI_TEST(HTMLStyleExtractorTest, SingleStyle) {
  std::string input =
      R"(<html><head></head><body><div style="color:red">hello</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find(".div0{color:red}"), std::string::npos);
  EXPECT_NE(result.find("class=\"div0\""), std::string::npos);
  EXPECT_EQ(result.find("style=\"color:red\""), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, DuplicateStyles) {
  std::string input =
      R"(<html><head></head><body><div style="color:red">a</div><span style="color:red">b</span></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find(".div0{color:red}"), std::string::npos);
  // Only one rule — the span reuses the div's class via exact-string dedup.
  EXPECT_EQ(result.find(".div1{"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, ExistingClassMerged) {
  std::string input =
      R"(<html><head></head><body><div class="foo" style="color:red">hello</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("class=\"foo div0\""), std::string::npos);
  EXPECT_EQ(result.find("style=\"color:red\""), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, EmptyExistingClass) {
  std::string input =
      R"(<html><head></head><body><div class="" style="color:red">hello</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("class=\"div0\""), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, EntityApostrophe) {
  std::string input =
      R"(<html><head></head><body><div style="font-family:&#39;Arial&#39;">hello</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find(".div0{font-family:'Arial'}"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, EntityAll5Named) {
  std::string input =
      R"(<html><head></head><body><div style="a:&amp;b:&lt;c:&gt;d:&quot;e:&apos;">hello</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("a:&b:<c:>d:\"e:'"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, EntityNumericDec) {
  std::string input = R"(<html><head></head><body><div style="x:&#65;">hello</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("x:A"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, EntityNumericHex) {
  std::string input = R"(<html><head></head><body><div style="x:&#x41;">hello</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("x:A"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, BodyStylePreserved) {
  std::string input =
      R"(<html><head></head><body style="margin:0"><div style="color:red">hello</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("body style=\"margin:0\""), std::string::npos);
  EXPECT_EQ(result.find("body class="), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, EmptyStyleAttr) {
  std::string input = R"(<html><head></head><body><div style="">hello</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  // Empty style should not generate a class.
  EXPECT_EQ(result.find(".div0"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, CommentSkipped) {
  std::string input =
      R"(<html><head></head><body><!-- <div style="color:red"> --><div style="color:blue">hello</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find(".div0{color:blue}"), std::string::npos);
  // The comment text is preserved verbatim; only the real div's style is extracted.
  EXPECT_NE(result.find("<!-- <div style=\"color:red\"> -->"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, StyleValueWithDataUri) {
  std::string input =
      R"xyz(<html><head></head><body><div style="background:url(data:image/png;base64,abc;def)">hello</div></body></html>)xyz";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("url(data:image/png;base64,abc;def)"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, NoHeadInsertBeforeBody) {
  std::string input = R"(<html><body><div style="color:red">hello</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("<style>"), std::string::npos);
  // Style block should be before <body.
  auto stylePos = result.find("<style>");
  auto bodyPos = result.find("<body");
  EXPECT_LT(stylePos, bodyPos);
}

CLI_TEST(HTMLStyleExtractorTest, SelfClosingTag) {
  std::string input =
      R"(<html><head></head><body><img src="x.png" style="width:100px" /></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("class=\"div0\""), std::string::npos);
  EXPECT_NE(result.find("/>"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, ClassInsertedAfterTagName) {
  std::string input =
      R"(<html><head></head><body><div id="x" style="color:red">hello</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("class=\"div0\""), std::string::npos);
  EXPECT_NE(result.find("id=\"x\""), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, StyleInsertedBeforeHeadClose) {
  std::string input =
      R"(<html><head><meta charset="utf-8"></head><body><div style="color:red">hello</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  auto stylePos = result.find("<style>");
  auto headClosePos = result.find("</head>");
  EXPECT_LT(stylePos, headClosePos);
}

CLI_TEST(HTMLStyleExtractorTest, MultipleUniqueStyles) {
  std::string input =
      R"(<html><head></head><body><div style="color:red">a</div><div style="color:blue">b</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find(".div0{color:red}"), std::string::npos);
  EXPECT_NE(result.find(".div1{color:blue}"), std::string::npos);
}

// =============================================================================
// Base+modifier split tests
// =============================================================================

CLI_TEST(HTMLStyleExtractorTest, BaseModifierBlend) {
  std::string input =
      R"(<html><head></head><body>)"
      R"(<div class="pagx-layer" style="position:absolute;width:48px;height:30px;mix-blend-mode:multiply">a</div>)"
      R"(<div class="pagx-layer" style="position:absolute;width:48px;height:30px;mix-blend-mode:screen">b</div>)"
      R"(<div class="pagx-layer" style="position:absolute;width:48px;height:30px;mix-blend-mode:overlay">c</div>)"
      R"(</body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  // Base class with shared properties.
  EXPECT_NE(result.find(".blend0{position:absolute;width:48px;height:30px}"), std::string::npos);
  // Modifier classes with individual blend modes.
  EXPECT_NE(result.find(".blend1{mix-blend-mode:multiply}"), std::string::npos);
  EXPECT_NE(result.find(".blend2{mix-blend-mode:screen}"), std::string::npos);
  EXPECT_NE(result.find(".blend3{mix-blend-mode:overlay}"), std::string::npos);
  // Elements should have base+modifier class pair.
  EXPECT_NE(result.find("class=\"pagx-layer blend0 blend1\""), std::string::npos);
  EXPECT_NE(result.find("class=\"pagx-layer blend0 blend2\""), std::string::npos);
  EXPECT_NE(result.find("class=\"pagx-layer blend0 blend3\""), std::string::npos);
  // No inline style should remain.
  EXPECT_EQ(result.find("style=\"position:absolute"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, BaseModifierBgColor) {
  std::string input =
      R"(<html><head></head><body>)"
      R"(<div class="pagx-layer" style="position:absolute;width:50px;height:40px;background-color:#FF0000">a</div>)"
      R"(<div class="pagx-layer" style="position:absolute;width:50px;height:40px;background-color:#00FF00">b</div>)"
      R"(</body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  // Base class with shared properties.
  EXPECT_NE(result.find(".bg0{position:absolute;width:50px;height:40px}"), std::string::npos);
  // Modifier classes with individual background colors.
  EXPECT_NE(result.find(".bg1{background-color:#FF0000}"), std::string::npos);
  EXPECT_NE(result.find(".bg2{background-color:#00FF00}"), std::string::npos);
  // Elements should have base+modifier class pair.
  EXPECT_NE(result.find("class=\"pagx-layer bg0 bg1\""), std::string::npos);
  EXPECT_NE(result.find("class=\"pagx-layer bg0 bg2\""), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, BaseModifierText) {
  std::string input = R"(<html><head></head><body>)"
                      R"(<span style="position:absolute;top:0;color:#FFF;width:27px">a</span>)"
                      R"(<span style="position:absolute;top:0;color:#FFF;width:35px">b</span>)"
                      R"(</body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  // Base class with shared properties.
  EXPECT_NE(result.find(".text0{position:absolute;top:0;color:#FFF}"), std::string::npos);
  // Modifier classes with individual widths.
  EXPECT_NE(result.find(".text1{width:27px}"), std::string::npos);
  EXPECT_NE(result.find(".text2{width:35px}"), std::string::npos);
  // Elements should have base+modifier class pair.
  EXPECT_NE(result.find("class=\"text0 text1\""), std::string::npos);
  EXPECT_NE(result.find("class=\"text0 text2\""), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, FallbackNoSplit) {
  // 3+ varying properties → should not split, fallback to standalone classes.
  std::string input = R"(<html><head></head><body>)"
                      R"(<div style="color:red;font-size:12px;margin:10px">a</div>)"
                      R"(<div style="color:blue;font-size:14px;margin:20px">b</div>)"
                      R"(</body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  // All 3 properties differ → 3 varying > 2, so no split.
  EXPECT_NE(result.find(".div0{color:red;font-size:12px;margin:10px}"), std::string::npos);
  EXPECT_NE(result.find(".div1{color:blue;font-size:14px;margin:20px}"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, SharedModifierClass) {
  // Two elements with the same varying value should share a modifier class.
  std::string input =
      R"(<html><head></head><body>)"
      R"(<div class="pagx-layer" style="position:absolute;width:48px;height:30px;mix-blend-mode:multiply">a</div>)"
      R"(<div class="pagx-layer" style="position:absolute;width:48px;height:30px;mix-blend-mode:screen">b</div>)"
      R"(<div class="pagx-layer" style="position:absolute;width:48px;height:30px;mix-blend-mode:multiply">c</div>)"
      R"(</body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  // Base class with shared properties.
  EXPECT_NE(result.find(".blend0{position:absolute;width:48px;height:30px}"), std::string::npos);
  // Multiply modifier should appear only once (shared by elements a and c).
  EXPECT_NE(result.find(".blend1{mix-blend-mode:multiply}"), std::string::npos);
  EXPECT_NE(result.find(".blend2{mix-blend-mode:screen}"), std::string::npos);
  // No third modifier should exist.
  EXPECT_EQ(result.find(".blend3{"), std::string::npos);
  // Two elements share blend1 modifier.
  int count = 0;
  size_t pos = 0;
  while ((pos = result.find("blend0 blend1", pos)) != std::string::npos) {
    count++;
    pos++;
  }
  EXPECT_EQ(count, 2);
}

CLI_TEST(HTMLStyleExtractorTest, GradientParenthesesParse) {
  // Semicolons and colons inside parentheses must not cause splits.
  std::string input =
      R"(<html><head></head><body>)"
      R"(<div style="position:absolute;background:linear-gradient(135deg,rgba(255,0,0,0.5) 50%,#00FF00 100%);color:#FFF">a</div>)"
      R"(<div style="position:absolute;background:linear-gradient(135deg,rgba(255,0,0,0.5) 50%,#00FF00 100%);color:#000">b</div>)"
      R"(</body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  // Three properties: position, background, color. Shared: position, background (2). Varying: color (1).
  // Should split into base+modifier since shared >= 2 and varying <= 2.
  EXPECT_NE(result.find("linear-gradient(135deg,rgba(255,0,0,0.5) 50%,#00FF00 100%)"),
            std::string::npos);
  // The gradient value must be intact (not split at internal ; or :).
  EXPECT_NE(result.find(".div0{position:absolute;background:linear-gradient(135deg,rgba(255,0,0,0.5) 50%,#00FF00 100%)}"),
            std::string::npos);
  EXPECT_NE(result.find(".div1{color:#FFF}"), std::string::npos);
  EXPECT_NE(result.find(".div2{color:#000}"), std::string::npos);
}

}  // namespace pag
