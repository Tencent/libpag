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

#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "base/PAGTest.h"
#include "pagx/HTMLSubsetTransformer.h"
#include "pagx/xml/XMLDOM.h"

namespace pag {

namespace {

// Helpers ----------------------------------------------------------------------------------

std::shared_ptr<pagx::DOMNode> ParseHtml(const std::string& html) {
  auto dom = pagx::XMLDOM::Make(reinterpret_cast<const uint8_t*>(html.data()), html.size());
  if (!dom) return nullptr;
  return dom->getRootNode();
}

pagx::HTMLSubsetTransformer::Result RunTransform(
    const std::string& html, std::shared_ptr<pagx::DOMNode>* outRoot,
    const pagx::HTMLSubsetTransformer::Options& opts = {}) {
  auto root = ParseHtml(html);
  if (!root) {
    pagx::HTMLSubsetTransformer::Result r;
    r.ok = false;
    return r;
  }
  auto result = pagx::HTMLSubsetTransformer::Transform(root, opts);
  if (outRoot) *outRoot = root;
  return result;
}

// Returns the first <body> child whose tag matches `tag`.
std::shared_ptr<pagx::DOMNode> FirstBodyChild(const std::shared_ptr<pagx::DOMNode>& root,
                                              const std::string& tag = "") {
  if (!root) return nullptr;
  auto body = root->getFirstChild("body");
  if (!body) return nullptr;
  return body->getFirstChild(tag);
}

std::string AttrValue(const std::shared_ptr<pagx::DOMNode>& node, const std::string& name) {
  if (!node) return {};
  const auto* val = node->findAttribute(name);
  return val ? *val : std::string();
}

bool HasDiagnostic(const pagx::HTMLSubsetTransformer::Result& result, const std::string& code) {
  for (const auto& d : result.diagnostics) {
    if (d.code == code) return true;
  }
  return false;
}

bool StyleContains(const std::shared_ptr<pagx::DOMNode>& node, const std::string& needle) {
  return AttrValue(node, "style").find(needle) != std::string::npos;
}

// Counts the number of element children directly under `parent`.
size_t CountElementChildren(const std::shared_ptr<pagx::DOMNode>& parent) {
  size_t n = 0;
  if (!parent) return 0;
  for (auto c = parent->firstChild; c; c = c->nextSibling) {
    if (c->type == pagx::DOMNodeType::Element) n++;
  }
  return n;
}

}  // namespace

//==================================================================================================
// Cascade resolution
//==================================================================================================

PAG_TEST(PAGXHTMLSubsetTransformerTest, ClassRuleIsAppliedAsInlineStyle) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><head><style>.btn { color: red; }</style></head>
             <body style="width:100px;height:50px">
               <span class="btn">x</span></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  auto span = FirstBodyChild(root, "span");
  ASSERT_NE(span, nullptr);
  EXPECT_TRUE(StyleContains(span, "color: red"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, InlineStyleOverridesClassRule) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><head><style>.btn { color: red; }</style></head>
             <body style="width:100px;height:50px">
               <span class="btn" style="color: blue">x</span></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  auto span = FirstBodyChild(root, "span");
  EXPECT_TRUE(StyleContains(span, "color: blue"));
  EXPECT_FALSE(StyleContains(span, "color: red"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, ElementSelectorIsAppliedToAllMatches) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><head><style>p { font-size: 20px; }</style></head>
             <body style="width:100px;height:50px">
               <p>a</p><p>b</p></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  size_t found = 0;
  for (auto c = root->getFirstChild("body")->firstChild; c; c = c->nextSibling) {
    if (c->type == pagx::DOMNodeType::Element && c->name == "p" &&
        StyleContains(c, "font-size: 20px")) {
      found++;
    }
  }
  EXPECT_EQ(found, 2u);
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, UnsupportedSelectorsAreDroppedWithDiagnostic) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><head><style>
               * { padding: 0; }
               .a .b { color: red; }
               .btn:hover { color: blue; }
             </style></head><body style="width:1px;height:1px"></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:unsupported-selector"));
  size_t selectorWarns = 0;
  for (const auto& d : result.diagnostics) {
    if (d.code == "subset:unsupported-selector") selectorWarns++;
  }
  EXPECT_GE(selectorWarns, 3u);
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, AtRuleIsDroppedWithDiagnostic) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><head><style>
               @media (max-width: 600px) { .x { color: red; } }
               .y { color: blue; }
             </style></head><body style="width:1px;height:1px">
               <span class="x">a</span><span class="y">b</span></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:unsupported-at-rule"));
  auto body = root->getFirstChild("body");
  auto child = body->firstChild;
  while (child) {
    if (child->type == pagx::DOMNodeType::Element && child->name == "span") {
      // The .x rule is inside @media — it should NOT apply.
      EXPECT_FALSE(StyleContains(child, "color: red"));
    }
    child = child->nextSibling;
  }
}

//==================================================================================================
// Property normalisation
//==================================================================================================

PAG_TEST(PAGXHTMLSubsetTransformerTest, MarginIsDroppedWithDiagnostic) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="margin: 8px; padding: 4px"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  auto div = FirstBodyChild(root, "div");
  EXPECT_FALSE(StyleContains(div, "margin"));
  EXPECT_TRUE(StyleContains(div, "padding: 4px"));
  EXPECT_TRUE(HasDiagnostic(result, "subset:unsupported-property"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, TransformIsDropped) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="transform: rotate(45deg)"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(FirstBodyChild(root, "div"), "transform"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexShorthandCollapsesToGrow) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="flex: 1 1 0%"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  auto div = FirstBodyChild(root, "div");
  EXPECT_TRUE(StyleContains(div, "flex: 1"));
  EXPECT_FALSE(StyleContains(div, "1 1 0%"));
  EXPECT_TRUE(HasDiagnostic(result, "subset:flex-shorthand-collapsed"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, RemUnitConvertsToPixels) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="width: 1.5rem"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  auto div = FirstBodyChild(root, "div");
  EXPECT_TRUE(StyleContains(div, "width: 24px"));
  EXPECT_TRUE(HasDiagnostic(result, "subset:unit-coerced"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, EmUnitResolvesAgainstInheritedFontSize) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px;font-size:20px">
               <p style="font-size: 1.5em">large</p></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  auto p = FirstBodyChild(root, "p");
  // 1.5em of 20px = 30px.
  EXPECT_TRUE(StyleContains(p, "font-size: 30px"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, VwResolvesAgainstCanvasWidth) {
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.canvasWidth = 800.0f;
  opts.canvasHeight = 600.0f;
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:800px;height:600px">
               <div style="width: 50vw"></div></body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  auto div = FirstBodyChild(root, "div");
  EXPECT_TRUE(StyleContains(div, "width: 400px"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, PositionRelativeIsDowngradedToAbsolute) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="position: relative; top: 0"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  auto div = FirstBodyChild(root, "div");
  EXPECT_TRUE(StyleContains(div, "position: absolute"));
  EXPECT_TRUE(HasDiagnostic(result, "subset:unsupported-property"));
}

//==================================================================================================
// Structure normalisation
//==================================================================================================

PAG_TEST(PAGXHTMLSubsetTransformerTest, UnsupportedTagIsDroppedByDefault) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div>ok</div>
               <table><tr><td>nope</td></tr></table>
             </body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  auto body = root->getFirstChild("body");
  // Only the <div> survives.
  EXPECT_EQ(CountElementChildren(body), 1u);
  EXPECT_EQ(body->getFirstChild()->name, "div");
  EXPECT_TRUE(HasDiagnostic(result, "subset:unsupported-tag"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, UnsupportedTagPreservedWhenOptionSet) {
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.preserveUnknownElements = true;
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <custom-element></custom-element></body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  auto preserved = FirstBodyChild(root, "div");
  ASSERT_NE(preserved, nullptr);
  EXPECT_EQ(AttrValue(preserved, "data-html-unknown"), "custom-element");
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, StrayTextInsideContainerIsWrappedInParagraph) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div>hello</div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  auto div = FirstBodyChild(root, "div");
  ASSERT_NE(div, nullptr);
  auto firstChild = div->getFirstChild();
  ASSERT_NE(firstChild, nullptr);
  EXPECT_EQ(firstChild->name, "p");
  EXPECT_TRUE(HasDiagnostic(result, "subset:text-wrapped"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, StyleBlockIsRemovedByDefault) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><head><style>.x { color: red; }</style></head>
             <body style="width:1px;height:1px"></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  auto head = root->getFirstChild("head");
  ASSERT_NE(head, nullptr);
  EXPECT_EQ(head->getFirstChild("style"), nullptr);
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, StyleBlockKeptWhenPreservationRequested) {
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.preserveStyleBlock = true;
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><head><style>.x { color: red; }</style></head>
             <body style="width:1px;height:1px"></body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  EXPECT_NE(root->getFirstChild("head")->getFirstChild("style"), nullptr);
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, ClassAttributeStrippedByDefault) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div class="card btn"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  auto div = FirstBodyChild(root, "div");
  EXPECT_EQ(AttrValue(div, "class"), "");
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, ScriptElementInHeadIsDropped) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><head><script>var x = 1;</script><title>t</title></head>
             <body style="width:1px;height:1px"></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_EQ(root->getFirstChild("head")->getFirstChild("script"), nullptr);
  EXPECT_NE(root->getFirstChild("head")->getFirstChild("title"), nullptr);
}

//==================================================================================================
// Strict mode
//==================================================================================================

PAG_TEST(PAGXHTMLSubsetTransformerTest, StrictModeFailsOnUnsupportedProperty) {
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.strict = true;
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="margin: 8px"></div></body></html>)HTML",
      &root, opts);
  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().severity, pagx::HTMLSubsetTransformer::Severity::Error);
}

//==================================================================================================
// Idempotency
//==================================================================================================

PAG_TEST(PAGXHTMLSubsetTransformerTest, IdempotentOnAlreadySubsetInput) {
  std::shared_ptr<pagx::DOMNode> rootA;
  auto resultA = RunTransform(
      R"HTML(<html><body style="width: 100px; height: 50px;">
               <div style="background-color: #FFFFFF; padding: 8px;"></div></body></html>)HTML",
      &rootA);
  ASSERT_TRUE(resultA.ok);
  // Run a second time on the already-normalised output.
  pagx::HTMLSubsetTransformer::Result resultB = pagx::HTMLSubsetTransformer::Transform(rootA);
  ASSERT_TRUE(resultB.ok);
  EXPECT_TRUE(resultB.diagnostics.empty());
}

}  // namespace pag
