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
//  Unless required by applicable law or agreed to in writing, software distributed under the
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
  EXPECT_NE(result.find(".ps0{color:red}"), std::string::npos);
  EXPECT_NE(result.find("class=\"ps0\""), std::string::npos);
  EXPECT_EQ(result.find("style=\"color:red\""), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, DuplicateStyles) {
  std::string input =
      R"(<html><head></head><body><div style="color:red">a</div><span style="color:red">b</span></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find(".ps0{color:red}"), std::string::npos);
  // Only one rule.
  EXPECT_EQ(result.find(".ps1{"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, ExistingClassMerged) {
  std::string input =
      R"(<html><head></head><body><div class="foo" style="color:red">hello</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("class=\"foo ps0\""), std::string::npos);
  EXPECT_EQ(result.find("style=\"color:red\""), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, EmptyExistingClass) {
  std::string input =
      R"(<html><head></head><body><div class="" style="color:red">hello</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("class=\"ps0\""), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, EntityApostrophe) {
  std::string input =
      R"(<html><head></head><body><div style="font-family:&#39;Arial&#39;">hello</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find(".ps0{font-family:'Arial'}"), std::string::npos);
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
  EXPECT_EQ(result.find(".ps0"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, CommentSkipped) {
  std::string input =
      R"(<html><head></head><body><!-- <div style="color:red"> --><div style="color:blue">hello</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find(".ps0{color:blue}"), std::string::npos);
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
  EXPECT_NE(result.find("class=\"ps0\""), std::string::npos);
  EXPECT_NE(result.find("/>"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, ClassInsertedAfterTagName) {
  std::string input =
      R"(<html><head></head><body><div id="x" style="color:red">hello</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("class=\"ps0\""), std::string::npos);
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
  EXPECT_NE(result.find(".ps0{color:red}"), std::string::npos);
  EXPECT_NE(result.find(".ps1{color:blue}"), std::string::npos);
}

}  // namespace pag
