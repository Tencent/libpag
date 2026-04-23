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
#include "pagx/PAGXDocument.h"
#include "pagx/html/FontHoist.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"

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

// CollectUniformSignature tests

// Helper: create a Text node with the given font properties.
static pagx::Text* MakeText(pagx::PAGXDocument* doc, const std::string& family, float size,
                            const std::string& style = "") {
  auto* t = doc->makeNode<pagx::Text>();
  t->fontFamily = family;
  t->fontSize = size;
  t->fontStyle = style;
  return t;
}

CLI_TEST(FontHoistTest, CollectEmptyContents) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  std::vector<pagx::Element*> contents = {};
  auto sig = pagx::CollectUniformSignature(contents);
  EXPECT_TRUE(sig.fontFamily.empty());
  EXPECT_EQ(sig.renderFontSize, 0.0f);
}

CLI_TEST(FontHoistTest, CollectSingleDirectText) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* t = MakeText(doc.get(), "Arial", 16);
  std::vector<pagx::Element*> contents = {t};
  auto sig = pagx::CollectUniformSignature(contents);
  EXPECT_EQ(sig.fontFamily, "Arial");
  EXPECT_EQ(sig.renderFontSize, 16.0f);
}

CLI_TEST(FontHoistTest, CollectUniformDirectTexts) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* t1 = MakeText(doc.get(), "Arial", 16);
  auto* t2 = MakeText(doc.get(), "Arial", 16);
  std::vector<pagx::Element*> contents = {t1, t2};
  auto sig = pagx::CollectUniformSignature(contents);
  EXPECT_EQ(sig.fontFamily, "Arial");
  EXPECT_EQ(sig.renderFontSize, 16.0f);
}

CLI_TEST(FontHoistTest, CollectMismatchedDirectTexts) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* t1 = MakeText(doc.get(), "Arial", 16);
  auto* t2 = MakeText(doc.get(), "Arial", 24);
  std::vector<pagx::Element*> contents = {t1, t2};
  auto sig = pagx::CollectUniformSignature(contents);
  EXPECT_TRUE(sig.fontFamily.empty());
}

CLI_TEST(FontHoistTest, CollectTextWithGlyphRunsReturnsEmpty) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* t = MakeText(doc.get(), "Arial", 16);
  auto* gr = doc->makeNode<pagx::GlyphRun>();
  gr->glyphs = {1};
  t->glyphRuns.push_back(gr);
  std::vector<pagx::Element*> contents = {t};
  auto sig = pagx::CollectUniformSignature(contents);
  EXPECT_TRUE(sig.fontFamily.empty());
}

CLI_TEST(FontHoistTest, CollectGroupTextsUniform) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* group = doc->makeNode<pagx::Group>();
  auto* t1 = MakeText(doc.get(), "Arial", 16);
  auto* t2 = MakeText(doc.get(), "Arial", 16);
  group->elements.push_back(t1);
  group->elements.push_back(t2);
  std::vector<pagx::Element*> contents = {group};
  auto sig = pagx::CollectUniformSignature(contents);
  EXPECT_EQ(sig.fontFamily, "Arial");
  EXPECT_EQ(sig.renderFontSize, 16.0f);
}

CLI_TEST(FontHoistTest, CollectGroupTextsMismatched) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* group = doc->makeNode<pagx::Group>();
  auto* t1 = MakeText(doc.get(), "Arial", 16);
  auto* t2 = MakeText(doc.get(), "Arial", 24);
  group->elements.push_back(t1);
  group->elements.push_back(t2);
  std::vector<pagx::Element*> contents = {group};
  auto sig = pagx::CollectUniformSignature(contents);
  EXPECT_TRUE(sig.fontFamily.empty());
}

CLI_TEST(FontHoistTest, CollectDirectTextAndGroupTextUniform) {
  // Direct Text + Group Text with same signature → uniform
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* direct = MakeText(doc.get(), "Arial", 16);
  auto* group = doc->makeNode<pagx::Group>();
  auto* groupText = MakeText(doc.get(), "Arial", 16);
  group->elements.push_back(groupText);
  std::vector<pagx::Element*> contents = {direct, group};
  auto sig = pagx::CollectUniformSignature(contents);
  EXPECT_EQ(sig.fontFamily, "Arial");
  EXPECT_EQ(sig.renderFontSize, 16.0f);
}

CLI_TEST(FontHoistTest, CollectDirectTextAndGroupTextMismatched) {
  // The exact scenario from root_document.pagx: direct Text fontSize=24, Group Text fontSize=12
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* direct = MakeText(doc.get(), "Arial", 24, "Bold");
  auto* group = doc->makeNode<pagx::Group>();
  auto* groupText = MakeText(doc.get(), "Arial", 12);
  group->elements.push_back(groupText);
  std::vector<pagx::Element*> contents = {direct, group};
  auto sig = pagx::CollectUniformSignature(contents);
  EXPECT_TRUE(sig.fontFamily.empty());
}

CLI_TEST(FontHoistTest, CollectGroupWithGlyphRunsTextReturnsEmpty) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* group = doc->makeNode<pagx::Group>();
  auto* t = MakeText(doc.get(), "Arial", 16);
  auto* gr = doc->makeNode<pagx::GlyphRun>();
  gr->glyphs = {1};
  t->glyphRuns.push_back(gr);
  group->elements.push_back(t);
  std::vector<pagx::Element*> contents = {group};
  auto sig = pagx::CollectUniformSignature(contents);
  EXPECT_TRUE(sig.fontFamily.empty());
}

CLI_TEST(FontHoistTest, CollectTextBoxTextsUniform) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* tb = doc->makeNode<pagx::TextBox>();
  auto* t1 = MakeText(doc.get(), "Arial", 16);
  auto* t2 = MakeText(doc.get(), "Arial", 16);
  tb->elements.push_back(t1);
  tb->elements.push_back(t2);
  std::vector<pagx::Element*> contents = {tb};
  auto sig = pagx::CollectUniformSignature(contents);
  EXPECT_EQ(sig.fontFamily, "Arial");
  EXPECT_EQ(sig.renderFontSize, 16.0f);
}

CLI_TEST(FontHoistTest, CollectTextBoxWithGroupTexts) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* tb = doc->makeNode<pagx::TextBox>();
  auto* directText = MakeText(doc.get(), "Arial", 16);
  auto* group = doc->makeNode<pagx::Group>();
  auto* groupText = MakeText(doc.get(), "Arial", 16);
  group->elements.push_back(groupText);
  tb->elements.push_back(directText);
  tb->elements.push_back(group);
  std::vector<pagx::Element*> contents = {tb};
  auto sig = pagx::CollectUniformSignature(contents);
  EXPECT_EQ(sig.fontFamily, "Arial");
  EXPECT_EQ(sig.renderFontSize, 16.0f);
}

CLI_TEST(FontHoistTest, CollectMixedDirectAndGroupAndTextBox) {
  // All three sources with uniform signature
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* direct = MakeText(doc.get(), "Arial", 16);
  auto* group = doc->makeNode<pagx::Group>();
  auto* groupText = MakeText(doc.get(), "Arial", 16);
  group->elements.push_back(groupText);
  auto* tb = doc->makeNode<pagx::TextBox>();
  auto* tbText = MakeText(doc.get(), "Arial", 16);
  tb->elements.push_back(tbText);
  std::vector<pagx::Element*> contents = {direct, group, tb};
  auto sig = pagx::CollectUniformSignature(contents);
  EXPECT_EQ(sig.fontFamily, "Arial");
  EXPECT_EQ(sig.renderFontSize, 16.0f);
}

CLI_TEST(FontHoistTest, CollectNonTextElementsIgnored) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* fill = doc->makeNode<pagx::Fill>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  std::vector<pagx::Element*> contents = {fill, rect};
  auto sig = pagx::CollectUniformSignature(contents);
  EXPECT_TRUE(sig.fontFamily.empty());
}

}  // namespace pag
