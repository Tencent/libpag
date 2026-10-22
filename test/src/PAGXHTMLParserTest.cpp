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
#include <cmath>
#include <string>
#include <vector>
#include "base/PAGTest.h"
#include "pagx/HTMLImporter.h"
#include "pagx/PAGXDocument.h"
#include "pagx/html/importer/HTMLBackgroundLayout.h"
#include "pagx/html/importer/HTMLCSSPosition.h"
#include "pagx/html/importer/HTMLCssCascade.h"
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/html/importer/HTMLDiagnosticSink.h"
#include "pagx/html/importer/HTMLIdAllocator.h"
#include "pagx/html/importer/HTMLInlineSvgEmitter.h"
#include "pagx/html/importer/HTMLLayerBuilder.h"
#include "pagx/html/importer/HTMLObjectLayout.h"
#include "pagx/html/importer/HTMLStyleCascade.h"
#include "pagx/html/importer/HTMLSubsetTransformer.h"
#include "pagx/html/importer/HTMLTextFragmentBuilder.h"
#include "pagx/html/importer/HTMLValueParser.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/xml/XMLDOM.h"

namespace pag {
namespace {

struct ParserInput {
  float width = 100;
  float height = 80;
  std::shared_ptr<pagx::PAGXDocument> document = pagx::PAGXDocument::Make(width, height);
  pagx::HTMLDiagnosticSink diagnostics{false};
  pagx::HTMLValueParser values{diagnostics, width, height};
  pagx::HTMLIdAllocator ids;
  pagx::HTMLInlineSvgEmitter svg;
  pagx::HTMLLayerBuilder layers{diagnostics, values, svg, ids};
  pagx::HTMLStyleCascade styles{diagnostics, values};
  pagx::HTMLTextFragmentBuilder text{diagnostics, values, layers, styles, ids};

  ParserInput() {
    diagnostics.bindDocument(document.get());
    values.bindDocument(document.get());
    layers.bindDocument(document.get());
    text.bindDocument(document.get());
  }

  bool warned(const std::string& text) const {
    return std::any_of(document->errors.begin(), document->errors.end(),
                       [&](const auto& error) { return error.find(text) != std::string::npos; });
  }
};

std::shared_ptr<pagx::DOMNode> ParseNode(const std::string& source) {
  auto dom = pagx::XMLDOM::Make(reinterpret_cast<const uint8_t*>(source.data()), source.size());
  return dom == nullptr ? nullptr : dom->getRootNode();
}

std::string TextContent(const std::shared_ptr<pagx::PAGXDocument>& document) {
  std::string content;
  for (const auto& node : document->nodes) {
    if (node->nodeType() == pagx::NodeType::Text)
      content += static_cast<pagx::Text*>(node.get())->text;
  }
  return content;
}

}  // namespace

PAG_TEST(PAGXHTMLParserTest, DiagnosticsBufferAcrossUnbindingWithoutDuplication) {
  auto first = pagx::PAGXDocument::Make(1, 1);
  auto second = pagx::PAGXDocument::Make(1, 1);
  pagx::HTMLDiagnosticSink sink(false);
  sink.warn("before binding");
  sink.bindDocument(nullptr);
  sink.bindDocument(first.get());
  sink.warn("first document");
  sink.bindDocument(nullptr);
  sink.hardError("between documents");
  sink.bindDocument(second.get());
  sink.bindDocument(second.get());
  EXPECT_EQ(first->errors, (std::vector<std::string>{"before binding", "first document"}));
  EXPECT_EQ(second->errors, (std::vector<std::string>{"between documents"}));
  EXPECT_TRUE(sink.hadHardError());
  pagx::HTMLDiagnosticSink strict(true);
  strict.warn("strict warning");
  strict.bindDocument(second.get());
  EXPECT_TRUE(strict.hadHardError());
  EXPECT_EQ(second->errors.back(), "strict warning");
}

PAG_TEST(PAGXHTMLParserTest, KeyframeListsRecoverAfterMalformedSelectorsAndSerialize) {
  std::vector<std::string> dropped;
  std::vector<pagx::html::CssKeyframesRule> keyframes;
  auto rules = pagx::html::TokenizeStyleSheet(
      "@keyframes pulse { broken /* { comment } */ {opacity:9} "
      "from,25%,50%,to {opacity:1} 20oops% {opacity:8} } div {color:red}",
      dropped, keyframes);
  ASSERT_EQ(keyframes.size(), 1u);
  ASSERT_EQ(keyframes[0].stops.size(), 4u);
  EXPECT_FLOAT_EQ(keyframes[0].stops[0].percent, 0);
  EXPECT_FLOAT_EQ(keyframes[0].stops[1].percent, 25);
  EXPECT_FLOAT_EQ(keyframes[0].stops[2].percent, 50);
  EXPECT_FLOAT_EQ(keyframes[0].stops[3].percent, 100);
  ASSERT_EQ(rules.size(), 1u);
  EXPECT_EQ(rules[0].selectors, "div");
  EXPECT_TRUE(dropped.empty());
  auto serialized = pagx::html::SerializeKeyframes(keyframes);
  EXPECT_NE(serialized.find("opacity:1;"), std::string::npos);
  std::vector<pagx::html::CssKeyframesRule> reparsed;
  EXPECT_TRUE(pagx::html::TokenizeStyleSheet(serialized, dropped, reparsed).empty());
  ASSERT_EQ(reparsed.size(), 1u);
  ASSERT_EQ(reparsed[0].stops.size(), 4u);
  EXPECT_EQ(reparsed[0].stops[2].declarations, "opacity:1;");
  EXPECT_EQ(pagx::html::ClassifySelector("").kind, pagx::html::SelectorKind::Unsupported);
}

PAG_TEST(PAGXHTMLParserTest, AffineLengthsHandleScalarMultiplicationAndRejectInvalidDimensions) {
  ParserInput input;
  pagx::HTMLLengthPercentage length;
  ASSERT_TRUE(pagx::ParseHTMLLengthPercentage("calc(2 * (25% + 3px))", input.values, length));
  EXPECT_FLOAT_EQ(length.percent, .5f);
  EXPECT_FLOAT_EQ(length.pixels, 6);
  EXPECT_FLOAT_EQ(length.resolve(-20), -4);
  ASSERT_TRUE(pagx::ParseHTMLLengthPercentage("calc(10mm - 4q)", input.values, length));
  EXPECT_NEAR(length.pixels, 9 * 96.f / 25.4f, 1e-4f);
  for (const auto& invalid : {"1em", "10vh", "calc(2px * 3px)", "calc(1px / 0)", "calc(1px + 1)",
                              "1unknown", "1e999px", "calc(2 / 1px)", "10% trailing"}) {
    SCOPED_TRACE(invalid);
    EXPECT_FALSE(pagx::ParseHTMLLengthPercentage(invalid, input.values, length));
  }
  pagx::HTMLCSSPosition position;
  EXPECT_TRUE(position.isCenter());
  EXPECT_FALSE(pagx::ParseHTMLCSSPosition("center 2px bottom", input.values, position, true));
  EXPECT_FALSE(pagx::ParseHTMLCSSPosition("left 2px left 3px", input.values, position));
  EXPECT_FALSE(pagx::ParseHTMLCSSPosition("top right 1px 2px", input.values, position));
  ASSERT_TRUE(pagx::ParseHTMLCSSPosition("bottom 3px right 2px", input.values, position));
  EXPECT_FALSE(position.isCenter());
  EXPECT_FLOAT_EQ(position.x.resolve(-10), -12);
  EXPECT_FLOAT_EQ(position.y.resolve(20), 17);
}

PAG_TEST(PAGXHTMLParserTest, RoundBackgroundRescalesTheAutoAxisAndHonorsZeroTiles) {
  ParserInput input;
  pagx::HTMLBoxAttributes box;
  box.widthPx = 100;
  box.heightPx = 80;
  box.backgroundSize = "auto 30px";
  box.backgroundRepeat = "no-repeat round";
  pagx::HTMLBackgroundLayout result;
  ASSERT_TRUE(pagx::ResolveBackgroundLayout(box, 0, 40, 20, input.values, result));
  EXPECT_NEAR(result.y.size, 80.f / 3, 1e-5f);
  EXPECT_NEAR(result.x.size, 160.f / 3, 1e-5f);
  EXPECT_EQ(result.x.count, 1);
  EXPECT_EQ(result.y.count, 3);
  box.backgroundSize = "0px 10px";
  ASSERT_TRUE(pagx::ResolveBackgroundLayout(box, 0, 40, 20, input.values, result));
  EXPECT_EQ(result.x.count, 0);
  EXPECT_EQ(result.y.count, 0);
}

PAG_TEST(PAGXHTMLParserTest, ObjectSizingDerivesRatioAndRejectsUnknownFit) {
  pagx::HTMLImageIntrinsicSize intrinsic;
  intrinsic.width = 40;
  intrinsic.height = 20;
  float width = 0, height = 0;
  ASSERT_TRUE(pagx::ResolveHTMLObjectSize(intrinsic, "contain", 100, 80, width, height));
  EXPECT_FLOAT_EQ(width, 100);
  EXPECT_FLOAT_EQ(height, 50);
  EXPECT_FALSE(pagx::ResolveHTMLObjectSize(intrinsic, "invalid", 100, 80, width, height));
  pagx::HTMLCSSPosition position;
  position.x.percent = INFINITY;
  pagx::Rect object;
  EXPECT_FALSE(pagx::ResolveHTMLObjectRect(intrinsic, "none", position,
                                           pagx::Rect::MakeXYWH(0, 0, 100, 80), object));
}

PAG_TEST(PAGXHTMLParserTest, RepeatingGradientsWithoutBoxKeepOnePeriodAndWarn) {
  ParserInput input;
  auto* linear =
      input.layers.parseGradientByValue("repeating-linear-gradient(45deg,red 0%,blue 50%)");
  ASSERT_NE(linear, nullptr);
  EXPECT_EQ(linear->nodeType(), pagx::NodeType::LinearGradient);
  EXPECT_TRUE(input.warned("single non-repeating period"));
  input.document->errors.clear();
  auto* radial = input.layers.parseGradientByValue("repeating-radial-gradient(red 0%,blue 50%)");
  ASSERT_NE(radial, nullptr);
  EXPECT_EQ(radial->nodeType(), pagx::NodeType::RadialGradient);
  EXPECT_TRUE(input.warned("single non-repeating period"));
  EXPECT_FLOAT_EQ(input.values.parseColor("  ").alpha, 0);
}

PAG_TEST(PAGXHTMLParserTest, RadialDescriptorsRetainUsableAxesWhenOneLengthIsInvalid) {
  ParserInput input;
  auto* gradient = input.values.parseRadialGradient(
      "radial-gradient(ellipse 20px bad at left top,red,blue)", 100, 80);
  ASSERT_NE(gradient, nullptr);
  EXPECT_FLOAT_EQ(gradient->center.x, 0);
  EXPECT_FLOAT_EQ(gradient->center.y, 0);
  EXPECT_FLOAT_EQ(gradient->radius, .2f);
  EXPECT_TRUE(input.warned("using the x radius"));
  gradient = input.values.parseRadialGradient(
      "radial-gradient(ellipse bad at bad% center,red,blue)", 100, 80);
  ASSERT_NE(gradient, nullptr);
  EXPECT_TRUE(std::isfinite(gradient->radius));
  EXPECT_TRUE(input.warned("using box-filling radius"));
}

PAG_TEST(PAGXHTMLParserTest, ConicGradientRejectsMalformedDescriptorsAndEmptyStopLists) {
  ParserInput input;
  for (const auto& value :
       {"conic-gradient(from 12px,red,blue)", "conic-gradient(from 20deg extra,red,blue)",
        "conic-gradient(at left left,red,blue)", "conic-gradient(from 90deg)",
        "repeating-conic-gradient(from 0deg)"}) {
    SCOPED_TRACE(value);
    EXPECT_EQ(
        input.values.parseConicGradient(value, std::string(value).find("repeating") == 0, 100, 80),
        nullptr);
  }
  EXPECT_TRUE(input.warned("malformed conic-gradient angle"));
  EXPECT_TRUE(input.warned("unsupported conic-gradient descriptor"));
  EXPECT_TRUE(input.warned("unsupported conic-gradient position"));
}

PAG_TEST(PAGXHTMLParserTest, ConicGradientClipsStopsOutsideOneTurnWithInterpolatedColors) {
  ParserInput input;
  auto* gradient =
      input.values.parseConicGradient("conic-gradient(red -90deg,blue 450deg)", false, 100, 80);
  ASSERT_NE(gradient, nullptr);
  ASSERT_EQ(gradient->colorStops.size(), 2u);
  const auto* start = gradient->colorStops.front();
  const auto* end = gradient->colorStops.back();
  EXPECT_FLOAT_EQ(start->offset, 0);
  EXPECT_FLOAT_EQ(end->offset, 1);
  EXPECT_NEAR(start->color.red, 5.f / 6, 1e-5f);
  EXPECT_NEAR(start->color.blue, 1.f / 6, 1e-5f);
  EXPECT_NEAR(end->color.red, 1.f / 6, 1e-5f);
  EXPECT_NEAR(end->color.blue, 5.f / 6, 1e-5f);
}

PAG_TEST(PAGXHTMLParserTest, RepeatingConicGradientRejectsInsufficientOrExcessivelyDenseStops) {
  ParserInput input;
  EXPECT_EQ(input.values.parseConicGradient("repeating-conic-gradient(from 0deg,red)", true),
            nullptr);
  EXPECT_EQ(
      input.values.parseConicGradient("repeating-conic-gradient(red 0deg,blue .0001deg)", true),
      nullptr);
  EXPECT_TRUE(input.warned("repeating gradient exceeds the 4096-stop budget"));
}

PAG_TEST(PAGXHTMLParserTest, ConicGradientInfersMalformedAngularStopWithoutDroppingItsColor) {
  ParserInput input;
  auto* gradient = input.values.parseConicGradient("conic-gradient(red 12px,blue 360deg)");
  ASSERT_NE(gradient, nullptr);
  ASSERT_EQ(gradient->colorStops.size(), 2u);
  EXPECT_FLOAT_EQ(gradient->colorStops.front()->offset, 0);
  EXPECT_FLOAT_EQ(gradient->colorStops.front()->color.red, 1);
  EXPECT_FLOAT_EQ(gradient->colorStops.back()->offset, 1);
  EXPECT_FLOAT_EQ(gradient->colorStops.back()->color.blue, 1);
  EXPECT_TRUE(input.warned("malformed gradient stop offset '12px'"));
}

PAG_TEST(PAGXHTMLParserTest, InvalidTextMetadataPreservesCSSValuesAndOriginalText) {
  for (const auto& spacing : {"", "nan", "1px", "1e100", "1e-100"}) {
    SCOPED_TRACE(spacing);
    ParserInput input;
    auto node =
        ParseNode(std::string("<span data-pagx-text=\"AB\" data-pagx-inline-positions=\"bad\" ") +
                  "data-pagx-letter-spacing=\"" + spacing +
                  "\" data-pagx-faux-bold=\"true\" "
                  "data-pagx-faux-italic=\"no\">ignored glyphs</span>");
    ASSERT_NE(node, nullptr);
    pagx::HTMLInheritedStyle inherited;
    inherited.letterSpacing = "3px";
    auto* host = input.text.convertPAGXTextHost(node, inherited);
    ASSERT_NE(host, nullptr);
    ASSERT_FALSE(host->contents.empty());
    ASSERT_EQ(host->contents.front()->nodeType(), pagx::NodeType::Text);
    auto* text = static_cast<pagx::Text*>(host->contents.front());
    EXPECT_EQ(text->text, "AB");
    EXPECT_FLOAT_EQ(text->letterSpacing, 3);
    EXPECT_FALSE(text->fauxBold);
    EXPECT_FALSE(text->fauxItalic);
    EXPECT_TRUE(input.warned("invalid data-pagx-inline-positions"));
    EXPECT_TRUE(input.warned("invalid data-pagx-letter-spacing"));
    EXPECT_TRUE(input.warned("invalid data-pagx-faux-bold"));
    EXPECT_TRUE(input.warned("invalid data-pagx-faux-italic"));
  }
}

PAG_TEST(PAGXHTMLParserTest, EmptySemanticTextAndUnsupportedInlineNodesAreHandled) {
  ParserInput input;
  pagx::HTMLInheritedStyle inherited;
  pagx::HTMLBoxAttributes box;
  auto empty = ParseNode("<span data-pagx-text=\"\">ignored</span>");
  ASSERT_NE(empty, nullptr);
  EXPECT_EQ(input.text.convertTextLeaf(empty, box, inherited), nullptr);
  EXPECT_EQ(input.text.convertPAGXTextHost(empty, inherited), nullptr);
  auto missing = ParseNode("<div/>");
  EXPECT_EQ(input.text.convertPAGXTextHost(missing, inherited), nullptr);
  auto inlineNode = ParseNode("<p>A<unknown>discarded</unknown>B</p>");
  ASSERT_NE(input.text.convertTextLeaf(inlineNode, box, inherited), nullptr);
  EXPECT_EQ(TextContent(input.document), "AB");
  EXPECT_TRUE(input.warned("not supported inside text leaf"));
}

PAG_TEST(PAGXHTMLParserTest, MixedWhitespaceCompactsEmptyFragmentsWithoutLosingPreformattedRuns) {
  ParserInput input;
  auto node = ParseNode(
      "<p>  <span style=\"color:red\"> </span>"
      "<span style=\"white-space:pre\"> A\tB\n </span><span>C </span>"
      "<span style=\"color:blue\"> </span></p>");
  ASSERT_NE(node, nullptr);
  auto* layer = input.text.convertTextLeaf(node, {}, {});
  ASSERT_NE(layer, nullptr);
  EXPECT_EQ(TextContent(input.document), " A\tB\n C");
  ParserInput preLine;
  node = ParseNode("<p>A\tB\nC</p>");
  pagx::HTMLInheritedStyle inherited;
  inherited.whiteSpace = "pre-line";
  ASSERT_NE(preLine.text.convertTextLeaf(node, {}, inherited), nullptr);
  EXPECT_EQ(TextContent(preLine.document), "A B\nC");
}

PAG_TEST(PAGXHTMLParserTest, TextBoxKeepsEndAlignmentAndFirstRunStroke) {
  ParserInput input;
  auto node =
      ParseNode("<p style=\"width:100px;text-align:end;-webkit-text-stroke:2px red\">AB</p>");
  ASSERT_NE(node, nullptr);
  auto inherited = input.styles.resolveInheritedStyle(node, {});
  auto box = input.styles.computeBoxAttributes(node);
  auto* layer = input.text.convertTextLeaf(node, box, inherited);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->contents.size(), 1u);
  ASSERT_EQ(layer->contents.front()->nodeType(), pagx::NodeType::TextBox);
  auto* textBox = static_cast<pagx::TextBox*>(layer->contents.front());
  EXPECT_EQ(textBox->textAlign, pagx::TextAlign::End);
  ASSERT_EQ(textBox->elements.size(), 3u);
  ASSERT_EQ(textBox->elements.back()->nodeType(), pagx::NodeType::Stroke);
  auto* stroke = static_cast<pagx::Stroke*>(textBox->elements.back());
  EXPECT_FLOAT_EQ(stroke->width, 2);
  EXPECT_EQ(stroke->align, pagx::StrokeAlign::Center);
  ASSERT_EQ(stroke->color->nodeType(), pagx::NodeType::SolidColor);
  EXPECT_FLOAT_EQ(static_cast<pagx::SolidColor*>(stroke->color)->color.red, 1);
}

PAG_TEST(PAGXHTMLParserTest, TextGradientFailureFallsBackToResolvedColor) {
  ParserInput input;
  pagx::HTMLTextFragmentBuilder::TextFragment fragment;
  fragment.color = {0, 1, 0, 1, pagx::ColorSpace::SRGB};
  fragment.fillImage = "unsupported-gradient(red,blue)";
  auto* fill = input.text.buildTextFill(fragment);
  ASSERT_NE(fill, nullptr);
  ASSERT_NE(fill->color, nullptr);
  ASSERT_EQ(fill->color->nodeType(), pagx::NodeType::SolidColor);
  EXPECT_FLOAT_EQ(static_cast<pagx::SolidColor*>(fill->color)->color.green, 1);
}

PAG_TEST(PAGXHTMLParserTest, AbsoluteMarginsAdjustOnlyAuthoredAnchors) {
  ParserInput input;
  auto* layer = input.document->makeNode<pagx::Layer>();
  layer->left = 10;
  layer->right = 20;
  layer->top = 30;
  layer->bottom = 40;
  pagx::HTMLBoxAttributes box;
  box.absolute = true;
  box.marginLeftPx = 1;
  box.marginRightPx = 2;
  box.marginTopPx = 3;
  box.marginBottomPx = 4;
  EXPECT_EQ(input.layers.wrapForMargin(layer, box), layer);
  EXPECT_FLOAT_EQ(layer->left, 11);
  EXPECT_FLOAT_EQ(layer->right, 22);
  EXPECT_FLOAT_EQ(layer->top, 33);
  EXPECT_FLOAT_EQ(layer->bottom, 44);
  auto* unanchored = input.document->makeNode<pagx::Layer>();
  EXPECT_EQ(input.layers.wrapForMargin(unanchored, box), unanchored);
  EXPECT_TRUE(std::isnan(unanchored->left));
  EXPECT_TRUE(std::isnan(unanchored->bottom));
}

PAG_TEST(PAGXHTMLParserTest, TransformerKeepsKeyframesAfterExistingHeadMetadata) {
  auto root = ParseNode(
      "<html><head><title>Demo</title><meta charset=\"utf-8\"/>"
      "<style>@keyframes fade {from{opacity:0}to{opacity:1}}</style></head>"
      "<body style=\"width:100px;height:80px\"><div style=\"animation:fade 1s\"/></body></html>");
  ASSERT_NE(root, nullptr);
  auto result = pagx::HTMLSubsetTransformer::Transform(root);
  EXPECT_TRUE(result.ok);
  auto head = root->getFirstChild("head");
  ASSERT_NE(head, nullptr);
  auto title = head->getFirstChild("title");
  ASSERT_NE(title, nullptr);
  EXPECT_EQ(title->getFirstChild()->name, "Demo");
  auto style = head->getFirstChild("style");
  ASSERT_NE(style, nullptr);
  ASSERT_NE(style->getFirstChild(), nullptr);
  EXPECT_NE(style->getFirstChild()->name.find("@keyframes fade"), std::string::npos);
}

PAG_TEST(PAGXHTMLParserTest, RelativeFontWeightsResolveAgainstTheParentComputedWeight) {
  struct Case {
    double parent;
    double bolder;
    double lighter;
  };
  for (const auto& test : {Case{50, 400, 50}, Case{300, 400, 100}, Case{400, 700, 100},
                           Case{600, 900, 400}, Case{800, 900, 700}, Case{950, 950, 700}}) {
    SCOPED_TRACE(test.parent);
    ParserInput input;
    pagx::HTMLInheritedStyle parent;
    parent.fontRequest.weight = test.parent;
    auto bolder = ParseNode("<span style=\"font-weight:bolder\"/>");
    auto lighter = ParseNode("<span style=\"font-weight:lighter\"/>");
    EXPECT_DOUBLE_EQ(input.styles.resolveInheritedStyle(bolder, parent).fontRequest.weight,
                     test.bolder);
    EXPECT_DOUBLE_EQ(input.styles.resolveInheritedStyle(lighter, parent).fontRequest.weight,
                     test.lighter);
  }
}

PAG_TEST(PAGXHTMLParserTest, InvalidVisualPropertiesUseDocumentedFallbacks) {
  ParserInput input;
  auto node = ParseNode(
      "<div style=\"object-fit:unknown;object-position:left left;"
      "isolation:invalid;overflow:unknown\"/>");
  ASSERT_NE(node, nullptr);
  auto box = input.styles.computeBoxAttributes(node);
  EXPECT_TRUE(box.objectFit.empty());
  EXPECT_TRUE(box.objectPosition.isCenter());
  EXPECT_FALSE(box.clipOverflow);
  EXPECT_TRUE(input.warned("unsupported object-fit"));
  EXPECT_TRUE(input.warned("unsupported object-position"));
  EXPECT_TRUE(input.warned("unsupported isolation"));
  EXPECT_TRUE(input.warned("overflow: unknown"));
  node = ParseNode("<svg height=\"30\" viewBox=\"0 0 40 20\"/>");
  box = input.styles.computeBoxAttributes(node);
  EXPECT_FLOAT_EQ(box.widthPx, 60);
  EXPECT_FLOAT_EQ(box.heightPx, 30);
}

PAG_TEST(PAGXHTMLParserTest, RepeatingGradientPatternAcceptsAngularDirectionsAndPercentStops) {
  ParserInput input;
  for (const auto& angle : {"90deg", "0.25turn", "1.57079632679rad"}) {
    SCOPED_TRACE(angle);
    auto* pattern = input.values.parseRepeatingLinearGradientPattern(
        std::string("repeating-linear-gradient(") + angle + ",red 0%,blue 25%)", 100, 80);
    ASSERT_NE(pattern, nullptr);
    EXPECT_EQ(pattern->nodeType(), pagx::NodeType::ImagePattern);
  }
  EXPECT_EQ(input.values.parseConicGradient("conic-gradient()"), nullptr);
  EXPECT_EQ(input.values.parseConicGradient("repeating-conic-gradient()", true), nullptr);
  auto* gradient = input.values.parseConicGradient("conic-gradient(red bad%,blue)");
  ASSERT_NE(gradient, nullptr);
  ASSERT_EQ(gradient->colorStops.size(), 2u);
  EXPECT_FLOAT_EQ(gradient->colorStops.front()->offset, 0);
  EXPECT_FLOAT_EQ(gradient->colorStops.back()->offset, 1);
  EXPECT_TRUE(input.warned("malformed gradient stop offset"));
  pagx::HTMLBoxAttributes box;
  box.widthPx = 100;
  box.heightPx = 80;
  box.backgroundPosition = "left left";
  pagx::HTMLBackgroundLayout layout;
  EXPECT_FALSE(pagx::ResolveBackgroundLayout(box, 0, 20, 10, input.values, layout));
}

PAG_TEST(PAGXHTMLParserTest, DeepInlineRunsStopAtTheRecursionLimitAndKeepSiblings) {
  ParserInput input;
  auto root = ParseNode("<p>before<span/>after</p>");
  ASSERT_NE(root, nullptr);
  auto nested = root->getFirstChild("span");
  ASSERT_NE(nested, nullptr);
  for (int i = 0; i < pagx::MAX_HTML_RECURSION_DEPTH; i++) {
    auto child = std::make_shared<pagx::DOMNode>();
    child->type = pagx::DOMNodeType::Element;
    child->name = "span";
    nested->firstChild = child;
    nested = child;
  }
  auto hidden = std::make_shared<pagx::DOMNode>();
  hidden->type = pagx::DOMNodeType::Text;
  hidden->name = "too deep";
  nested->firstChild = hidden;
  ASSERT_NE(input.text.convertTextLeaf(root, {}, {}), nullptr);
  EXPECT_EQ(TextContent(input.document), "beforeafter");
  EXPECT_TRUE(input.warned("maximum recursion depth reached inside text leaf"));
  auto blank = ParseNode("<p> \t\n </p>");
  EXPECT_EQ(input.text.convertTextLeaf(blank, {}, {}), nullptr);
}

PAG_TEST(PAGXHTMLParserTest, UnsizedShadowSpreadFallsBackWithoutInvalidGeometry) {
  ParserInput input;
  pagx::HTMLBoxAttributes box;
  box.widthPct = box.heightPct = 100;
  box.boxShadow = "2px 3px 4px 5px red";
  auto* layer = input.document->makeNode<pagx::Layer>();
  ASSERT_TRUE(input.layers.applyBackgroundVisuals(layer, box));
  EXPECT_TRUE(input.warned("spread requires resolved px width/height"));
  EXPECT_FALSE(layer->styles.empty());
}

}  // namespace pag
