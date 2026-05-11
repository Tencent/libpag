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
  // A single-use standalone style is inlined rather than hoisted into a class
  // (see InlineSingleUse* tests). For the class hoisting path, see DuplicateStyles.
  std::string input =
      R"(<html><head></head><body><div style="color:red">hello</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("style=\"color:red\""), std::string::npos);
  EXPECT_EQ(result.find(".div0{"), std::string::npos);
  EXPECT_EQ(result.find("<style>"), std::string::npos);
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
  // Pre-existing class must be preserved next to the extractor-assigned class.
  // Two tags share the style so it hoists to a class rather than inlining.
  std::string input = R"(<html><head></head><body><div class="foo" style="color:red">a</div>)"
                      R"(<div class="bar" style="color:red">b</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("class=\"foo div0\""), std::string::npos);
  EXPECT_NE(result.find("class=\"bar div0\""), std::string::npos);
  EXPECT_EQ(result.find("style=\"color:red\""), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, EmptyExistingClass) {
  // Empty class attribute must not leave a stray leading space in the merged value.
  // Two uses keep the class form active.
  std::string input = R"(<html><head></head><body>)"
                      R"(<div class="" style="color:red">a</div>)"
                      R"(<div class="" style="color:red">b</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("class=\"div0\""), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, EntityApostrophe) {
  // Two uses so the decoded style becomes a class (otherwise single-use inlining fires).
  std::string input = R"(<html><head></head><body><div style="font-family:&#39;Arial&#39;">a</div>)"
                      R"(<div style="font-family:&#39;Arial&#39;">b</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find(".div0{font-family:'Arial'}"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, EntityAll5Named) {
  std::string input = R"(<html><head></head><body>)"
                      R"(<div style="a:&amp;b:&lt;c:&gt;d:&quot;e:&apos;">a</div>)"
                      R"(<div style="a:&amp;b:&lt;c:&gt;d:&quot;e:&apos;">b</div>)"
                      R"(</body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("a:&b:<c:>d:\"e:'"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, EntityNumericDec) {
  std::string input = R"(<html><head></head><body>)"
                      R"(<div style="x:&#65;">a</div>)"
                      R"(<div style="x:&#65;">b</div>)"
                      R"(</body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("x:A"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, EntityNumericHex) {
  std::string input = R"(<html><head></head><body>)"
                      R"(<div style="x:&#x41;">a</div>)"
                      R"(<div style="x:&#x41;">b</div>)"
                      R"(</body></html>)";
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
  // Commented-out style="..." attributes must not be extracted. Two real divs share
  // the same style so it hoists to a class (class-form is what this test asserts).
  std::string input =
      R"(<html><head></head><body><!-- <div style="color:red"> -->)"
      R"(<div style="color:blue">a</div><div style="color:blue">b</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find(".div0{color:blue}"), std::string::npos);
  EXPECT_NE(result.find("<!-- <div style=\"color:red\"> -->"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, StyleValueWithDataUri) {
  std::string input =
      R"xyz(<html><head></head><body><div style="background:url(data:image/png;base64,abc;def)">hello</div></body></html>)xyz";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("url(data:image/png;base64,abc;def)"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, NoHeadInsertBeforeBody) {
  // Two uses so the extracted class persists; tests the placement of <style>.
  std::string input = R"(<html><body><div style="color:red">a</div>)"
                      R"(<div style="color:red">b</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("<style>"), std::string::npos);
  // Style block should be before <body.
  auto stylePos = result.find("<style>");
  auto bodyPos = result.find("<body");
  EXPECT_LT(stylePos, bodyPos);
}

CLI_TEST(HTMLStyleExtractorTest, SelfClosingTag) {
  // Two self-closing imgs share the style so it hoists to a class.
  std::string input = R"(<html><head></head><body>)"
                      R"(<img src="a.png" style="width:100px" />)"
                      R"(<img src="b.png" style="width:100px" /></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("class=\"div0\""), std::string::npos);
  EXPECT_NE(result.find("/>"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, ClassInsertedAfterTagName) {
  // Two uses so the style hoists to a class that sits next to an unrelated attribute.
  std::string input = R"(<html><head></head><body>)"
                      R"(<div id="x" style="color:red">a</div>)"
                      R"(<div id="y" style="color:red">b</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("class=\"div0\""), std::string::npos);
  EXPECT_NE(result.find("id=\"x\""), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, StyleInsertedBeforeHeadClose) {
  // Two uses so the <style> block gets emitted; tests placement relative to </head>.
  std::string input = R"(<html><head><meta charset="utf-8"></head><body>)"
                      R"(<div style="color:red">a</div>)"
                      R"(<div style="color:red">b</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  auto stylePos = result.find("<style>");
  auto headClosePos = result.find("</head>");
  EXPECT_LT(stylePos, headClosePos);
}

CLI_TEST(HTMLStyleExtractorTest, MultipleUniqueStyles) {
  // Each of the two styles is used twice so both hoist to classes.
  std::string input = R"(<html><head></head><body><div style="color:red">a1</div>)"
                      R"(<div style="color:red">a2</div>)"
                      R"(<div style="color:blue">b1</div>)"
                      R"(<div style="color:blue">b2</div></body></html>)";
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
      R"(<div style="position:absolute;width:48px;height:30px;mix-blend-mode:multiply">a</div>)"
      R"(<div style="position:absolute;width:48px;height:30px;mix-blend-mode:screen">b</div>)"
      R"(<div style="position:absolute;width:48px;height:30px;mix-blend-mode:overlay">c</div>)"
      R"(</body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  // Base class with shared properties.
  EXPECT_NE(result.find(".blend0{position:absolute;width:48px;height:30px}"), std::string::npos);
  // Modifier classes with individual blend modes.
  EXPECT_NE(result.find(".blend1{mix-blend-mode:multiply}"), std::string::npos);
  EXPECT_NE(result.find(".blend2{mix-blend-mode:screen}"), std::string::npos);
  EXPECT_NE(result.find(".blend3{mix-blend-mode:overlay}"), std::string::npos);
  // Elements should have base+modifier class pair (layer detected via mix-blend-mode).
  EXPECT_NE(result.find("class=\"blend0 blend1\""), std::string::npos);
  EXPECT_NE(result.find("class=\"blend0 blend2\""), std::string::npos);
  EXPECT_NE(result.find("class=\"blend0 blend3\""), std::string::npos);
  // No inline style should remain.
  EXPECT_EQ(result.find("style=\"position:absolute"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, BaseModifierBgColor) {
  std::string input =
      R"(<html><head></head><body>)"
      R"(<div style="position:absolute;width:50px;height:40px;background-color:#FF0000">a</div>)"
      R"(<div style="position:absolute;width:50px;height:40px;background-color:#00FF00">b</div>)"
      R"(</body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  // Base class with shared properties (layer detected via position:absolute + size, no inset).
  EXPECT_NE(result.find(".bg0{position:absolute;width:50px;height:40px}"), std::string::npos);
  // Modifier classes with individual background colors.
  EXPECT_NE(result.find(".bg1{background-color:#FF0000}"), std::string::npos);
  EXPECT_NE(result.find(".bg2{background-color:#00FF00}"), std::string::npos);
  // Elements should have base+modifier class pair.
  EXPECT_NE(result.find("class=\"bg0 bg1\""), std::string::npos);
  EXPECT_NE(result.find("class=\"bg0 bg2\""), std::string::npos);
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
  // Each style is used twice to keep Pass 1.5 from inlining single-use classes back.
  std::string input = R"(<html><head></head><body>)"
                      R"(<div style="color:red;font-size:12px;margin:10px">a1</div>)"
                      R"(<div style="color:red;font-size:12px;margin:10px">a2</div>)"
                      R"(<div style="color:blue;font-size:14px;margin:20px">b1</div>)"
                      R"(<div style="color:blue;font-size:14px;margin:20px">b2</div>)"
                      R"(</body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  // All 3 properties differ → 3 varying > 2, so no split. Declarations are emitted in
  // canonical order: margin (box, 38) → color (typography, 90) → font-size (92).
  EXPECT_NE(result.find(".div0{margin:10px;color:red;font-size:12px}"), std::string::npos);
  EXPECT_NE(result.find(".div1{margin:20px;color:blue;font-size:14px}"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, SharedModifierClass) {
  // Two elements with the same varying value should share a modifier class.
  std::string input =
      R"(<html><head></head><body>)"
      R"(<div style="position:absolute;width:48px;height:30px;mix-blend-mode:multiply">a</div>)"
      R"(<div style="position:absolute;width:48px;height:30px;mix-blend-mode:screen">b</div>)"
      R"(<div style="position:absolute;width:48px;height:30px;mix-blend-mode:multiply">c</div>)"
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

CLI_TEST(HTMLStyleExtractorTest, BaseModifierThreeVarying) {
  // Three glyph-like spans share color/display/font-size; top, left, and transform vary.
  // Three varying properties are still profitable to split because (N-1) * sharedDecls
  // dominates the modifier-class overhead.
  std::string input =
      R"X(<html><head></head><body>)X"
      R"X(<span style="color:#3B82F6;display:inline-block;font-size:14px;top:10px;left:20px;transform:rotate(5deg)">a</span>)X"
      R"X(<span style="color:#3B82F6;display:inline-block;font-size:14px;top:12px;left:30px;transform:rotate(10deg)">b</span>)X"
      R"X(<span style="color:#3B82F6;display:inline-block;font-size:14px;top:14px;left:40px;transform:rotate(15deg)">c</span>)X"
      R"X(</body></html>)X";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  // Base class with the 3 shared declarations (canonical order: display 20, color 90, font-size 92).
  EXPECT_NE(result.find(".text0{display:inline-block;color:#3B82F6;font-size:14px}"),
            std::string::npos);
  // Three modifier classes carrying only the varying triple (top 12, left 15, transform 40).
  EXPECT_NE(result.find(".text1{top:10px;left:20px;transform:rotate(5deg)}"), std::string::npos);
  EXPECT_NE(result.find(".text2{top:12px;left:30px;transform:rotate(10deg)}"), std::string::npos);
  EXPECT_NE(result.find(".text3{top:14px;left:40px;transform:rotate(15deg)}"), std::string::npos);
  // Each span should carry base+modifier pair.
  EXPECT_NE(result.find("class=\"text0 text1\""), std::string::npos);
  EXPECT_NE(result.find("class=\"text0 text2\""), std::string::npos);
  EXPECT_NE(result.find("class=\"text0 text3\""), std::string::npos);
  // No remaining inline styles.
  EXPECT_EQ(result.find("style=\"color"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, BaseModifierFourVaryingFallback) {
  // Four varying properties exceeds the upper bound (3); the group must fall back to
  // standalone classes rather than split.
  std::string input =
      R"(<html><head></head><body>)"
      R"(<div style="position:absolute;color:red;width:10px;top:0;left:0">a</div>)"
      R"(<div style="position:absolute;color:blue;width:20px;top:5px;left:5px">b</div>)"
      R"(</body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  // No base/modifier pair anywhere — each element should carry exactly one class.
  EXPECT_EQ(result.find("class=\"layer0 "), std::string::npos);
  EXPECT_EQ(result.find("class=\"div0 "), std::string::npos);
  // Each element gets a single standalone class containing all five declarations.
  EXPECT_NE(result.find("color:red"), std::string::npos);
  EXPECT_NE(result.find("color:blue"), std::string::npos);
  EXPECT_NE(result.find("width:10px"), std::string::npos);
  EXPECT_NE(result.find("width:20px"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, BaseModifierThreeVaryingZeroShared) {
  // Three properties all differ → shared = 0. The shared >= 2 guard must keep this in
  // fallback even though varying = 3 is now permitted.
  std::string input = R"(<html><head></head><body>)"
                      R"(<div style="color:red;width:10px;height:5px">a</div>)"
                      R"(<div style="color:blue;width:20px;height:8px">b</div>)"
                      R"(</body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  // Standalone classes; no base/modifier composition.
  EXPECT_EQ(result.find("class=\"div0 div"), std::string::npos);
  EXPECT_NE(result.find("color:red"), std::string::npos);
  EXPECT_NE(result.find("color:blue"), std::string::npos);
}

// =============================================================================
// Single-use inlining: classes referenced by exactly one tag (and not part of a
// base+modifier pair) are reverted to inline style="..." to save the class-header
// and class="name" overhead. See Pass 1.5 of Extract().
// =============================================================================

CLI_TEST(HTMLStyleExtractorTest, InlineSingleUseStandalone) {
  // A solo standalone class is cheaper as inline style.
  std::string input = R"(<html><head></head><body><span style="color:red">x</span></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("style=\"color:red\""), std::string::npos);
  EXPECT_EQ(result.find(".text0"), std::string::npos);
  EXPECT_EQ(result.find("class=\"text0\""), std::string::npos);
  EXPECT_EQ(result.find("<style>"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, InlineSingleUseKeepsRootClass) {
  // The document-root div is always kept as a class even when single-use so
  // downstream consumers can select it (`.root0`).
  std::string input =
      R"(<html><head></head><body>)"
      R"(<div style="position:relative;width:100px;height:100px;overflow:hidden">r</div>)"
      R"(</body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find(".root0"), std::string::npos);
  EXPECT_NE(result.find("class=\"root0\""), std::string::npos);
  EXPECT_EQ(result.find("style=\"position:relative"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, InlineSingleUseKeepsPairedModifier) {
  // Modifier classes in a base+modifier pair are often refCount=1 but must stay as
  // classes to preserve the shared base.
  std::string input =
      R"(<html><head></head><body>)"
      R"(<div style="position:absolute;width:50px;height:40px;background-color:#FF0000">a</div>)"
      R"(<div style="position:absolute;width:50px;height:40px;background-color:#00FF00">b</div>)"
      R"(</body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find(".bg0{position:absolute;width:50px;height:40px}"), std::string::npos);
  EXPECT_NE(result.find(".bg1{background-color:#FF0000}"), std::string::npos);
  EXPECT_NE(result.find(".bg2{background-color:#00FF00}"), std::string::npos);
  EXPECT_NE(result.find("class=\"bg0 bg1\""), std::string::npos);
  EXPECT_NE(result.find("class=\"bg0 bg2\""), std::string::npos);
  EXPECT_EQ(result.find("style=\"position:absolute"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, InlineSingleUsePreservesExistingClass) {
  // A pre-existing class attribute on the tag is preserved verbatim; the inlined style
  // is emitted alongside it as a separate style="..." attribute.
  std::string input = R"(<html><head></head><body>)"
                      R"(<div class="pagx-group" style="opacity:0.5">g</div></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("class=\"pagx-group\""), std::string::npos);
  EXPECT_NE(result.find("style=\"opacity:0.5\""), std::string::npos);
  EXPECT_EQ(result.find(".layer0"), std::string::npos);
  EXPECT_EQ(result.find(".div0"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, InlineSingleUseMultiUseStillClass) {
  // Two tags share the same style → refCount=2 → class form beats inline.
  std::string input =
      R"(<html><head></head><body>)"
      R"(<span style="color:red">a</span><span style="color:red">b</span></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find(".text0{color:red}"), std::string::npos);
  // Both spans reference the class; no inline style emission.
  EXPECT_EQ(result.find("style=\"color:red\""), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, InlineSingleUseOmitsStyleBlockWhenAllInlined) {
  // When every generated class gets inlined, Pass 3 must not leave an empty
  // <style></style> behind.
  std::string input = R"(<html><head></head><body>)"
                      R"(<span style="color:red">a</span>)"
                      R"(<span style="color:blue">b</span>)"
                      R"(<span style="color:green">c</span></body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("style=\"color:red\""), std::string::npos);
  EXPECT_NE(result.find("style=\"color:blue\""), std::string::npos);
  EXPECT_NE(result.find("style=\"color:green\""), std::string::npos);
  EXPECT_EQ(result.find("<style>"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, InlineSingleUseCanonicalOrder) {
  // Inline emission uses the same CSSPropertyOrder-sorted declarations as the class
  // form would have. Source order: color, width, position. Expected canonical order:
  // position (10) → width (31) → color (90).
  std::string input = R"(<html><head></head><body>)"
                      R"(<div style="color:red;width:10px;position:absolute">x</div>)"
                      R"(</body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("style=\"position:absolute;width:10px;color:red\""), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, InlineSingleUseEntityReencoding) {
  // A value containing & or " must be re-escaped when embedded into a double-quoted
  // style attribute. Source uses &quot;; decoded is "; re-encoded back to &quot;.
  std::string input = R"(<html><head></head><body>)"
                      R"(<div style="font-family:&quot;Arial&quot;,sans-serif">x</div>)"
                      R"(</body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input);
  EXPECT_NE(result.find("style=\"font-family:&quot;Arial&quot;,sans-serif\""), std::string::npos);
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
  EXPECT_NE(result.find(".div0{position:absolute;background:linear-gradient(135deg,rgba(255,0,0,0."
                        "5) 50%,#00FF00 100%)}"),
            std::string::npos);
  EXPECT_NE(result.find(".div1{color:#FFF}"), std::string::npos);
  EXPECT_NE(result.find(".div2{color:#000}"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, FormatPretty) {
  // Two uses keep the class form so we can inspect the Pretty <style> block shape.
  std::string input = R"(<html><head></head><body>)"
                      R"(<div style="color:red;font-size:12px">a</div>)"
                      R"(<div style="color:red;font-size:12px">b</div>)"
                      R"(</body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input, pagx::HTMLStyleExtractor::Format::Pretty);
  // Selector on its own line with opening brace separated by a space.
  EXPECT_NE(result.find(".div0 {\n"), std::string::npos);
  // Each declaration on its own line, indented by two spaces, terminated by a semicolon.
  EXPECT_NE(result.find("\n  color: red;\n"), std::string::npos);
  EXPECT_NE(result.find("\n  font-size: 12px;\n"), std::string::npos);
  // Closing brace on its own line.
  EXPECT_NE(result.find("\n}\n"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, FormatMinify) {
  // Use each style twice to keep the class form (single-use inlines, see InlineSingleUse*).
  std::string input = R"(<html><head></head><body>)"
                      R"(<div style="color:red">a1</div><div style="color:red">a2</div>)"
                      R"(<div style="color:blue">b1</div><div style="color:blue">b2</div>)"
                      R"(</body></html>)";
  auto result = pagx::HTMLStyleExtractor::Extract(input, pagx::HTMLStyleExtractor::Format::Minify);
  // Style block must be on a single line with no newlines anywhere inside it.
  auto styleStart = result.find("<style>");
  auto styleEnd = result.find("</style>");
  ASSERT_NE(styleStart, std::string::npos);
  ASSERT_NE(styleEnd, std::string::npos);
  auto styleBlock = result.substr(styleStart, styleEnd - styleStart + 8);
  EXPECT_EQ(styleBlock.find('\n'), std::string::npos);
  EXPECT_NE(styleBlock.find(".div0{color:red}"), std::string::npos);
  EXPECT_NE(styleBlock.find(".div1{color:blue}"), std::string::npos);
}

CLI_TEST(HTMLStyleExtractorTest, FormatCompactIsDefault) {
  // Calling without an explicit format argument must match explicit Compact.
  // Two uses keep the class hoisted so we can inspect the <style> block shape.
  std::string input = R"(<html><head></head><body>)"
                      R"(<div style="color:red">a</div>)"
                      R"(<div style="color:red">b</div>)"
                      R"(</body></html>)";
  auto defaulted = pagx::HTMLStyleExtractor::Extract(input);
  auto explicitCompact =
      pagx::HTMLStyleExtractor::Extract(input, pagx::HTMLStyleExtractor::Format::Compact);
  EXPECT_EQ(defaulted, explicitCompact);
  // Compact shape: "<style>\n.div0{color:red}\n</style>\n".
  EXPECT_NE(defaulted.find("<style>\n.div0{color:red}\n</style>\n"), std::string::npos);
}

}  // namespace pag
