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

PAG_TEST(PAGXHTMLSubsetTransformerTest, MarginIsForwardedAsResolvedPx) {
  // CSS `margin` (and the four longhands) are now first-class subset properties: the
  // transformer routes them through `ResolveLength{,Shorthand}` to normalise units to
  // plain px and then forwards them on the inline `style="…"` attribute so the resolver
  // can fold them onto Layer positioning / padding. No diagnostic should fire.
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="margin: 8px; padding: 4px"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  auto div = FirstBodyChild(root, "div");
  EXPECT_TRUE(StyleContains(div, "margin: 8px"));
  EXPECT_TRUE(StyleContains(div, "padding: 4px"));
  EXPECT_FALSE(HasDiagnostic(result, "subset:unsupported-property"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, SupportedTransformIsKept) {
  // Single-function `transform` forms in the supported subset (skewX/skewY/rotate/scale[X|Y]/
  // translate[X|Y]) are forwarded untouched; the resolver later maps them onto the TextBox's
  // transform fields. Earlier the subset transformer dropped every `transform` declaration
  // unconditionally because Layer has no transform model, and text leaves had no way to
  // honour skew/rotate either.
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="transform: rotate(45deg)"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(StyleContains(FirstBodyChild(root, "div"), "transform: rotate(45deg)"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, UnsupportedTransformIsDropped) {
  // Compound chains and `matrix(...)` would need 2x2 matrix decomposition the importer
  // doesn't implement; they're dropped with a diagnostic rather than passed through and
  // mis-applied later.
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="transform: rotate(45deg) translate(10px)"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(FirstBodyChild(root, "div"), "transform"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, BackgroundClipTextIsKept) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="background-image: linear-gradient(90deg, #F00, #00F);
                           background-clip: text"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(StyleContains(FirstBodyChild(root, "div"), "background-clip: text"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, BackgroundClipBorderBoxIsDroppedSilently) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="background-clip: border-box"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(FirstBodyChild(root, "div"), "background-clip"));
  EXPECT_FALSE(HasDiagnostic(result, "subset:unsupported-property"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, WebkitBackgroundClipCoalescesToStandardName) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="background-image: linear-gradient(90deg, #F00, #00F);
                           -webkit-background-clip: text"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  auto div = FirstBodyChild(root, "div");
  EXPECT_TRUE(StyleContains(div, "background-clip: text"));
  EXPECT_FALSE(StyleContains(div, "-webkit-background-clip"));
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

PAG_TEST(PAGXHTMLSubsetTransformerTest, PositionRelativeIsDroppedSilently) {
  // PAGX has no CSS-style containing-block concept (children always anchor to
  // their direct parent), so `position: relative` is a no-op and we drop it
  // without a warning. This lets the html-snapshot pipeline emit
  // `position: relative` on flex items — needed so abs descendants anchor
  // correctly when the snapshot is opened in a real browser — without the
  // importer downgrading those flex items to `position: absolute` and yanking
  // them out of the parent's flex flow.
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="position: relative; top: 0"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  auto div = FirstBodyChild(root, "div");
  EXPECT_FALSE(StyleContains(div, "position:"));
  EXPECT_FALSE(HasDiagnostic(result, "subset:unsupported-property"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, PositionFixedIsDowngradedToAbsolute) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="position: fixed; top: 0"></div></body></html>)HTML",
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
  // `clip-path` stays on the explicit-drop list (PAGX has no equivalent geometry primitive),
  // so it remains a clean trigger for strict-mode failures even after `margin` was promoted
  // out of the drop set in MarginIsResolvedThroughPaddingWrapper.
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="clip-path: inset(10px)"></div></body></html>)HTML",
      &root, opts);
  EXPECT_FALSE(result.ok);
  EXPECT_FALSE(result.diagnostics.empty());
  EXPECT_EQ(result.diagnostics.front().severity, pagx::HTMLSubsetTransformer::Severity::Error);
}

//==================================================================================================
// AbsoluteToFlexInference (opt-in)
//==================================================================================================

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexInferenceIsOffByDefault) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:200px;height:50px">
               <div style="position:absolute;left:0px;top:0px;width:50px;height:50px"></div>
               <div style="position:absolute;left:60px;top:0px;width:50px;height:50px"></div>
             </body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  auto body = root->getFirstChild("body");
  ASSERT_NE(body, nullptr);
  EXPECT_FALSE(StyleContains(body, "display: flex"));
  // Both divs keep their absolute positioning by default.
  size_t absCount = 0;
  for (auto c = body->firstChild; c; c = c->nextSibling) {
    if (c->type == pagx::DOMNodeType::Element && StyleContains(c, "position: absolute")) {
      absCount++;
    }
  }
  EXPECT_EQ(absCount, 2u);
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexInferenceRecoversRowLayoutWhenEnabled) {
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.inferFlexFromAbsolute = true;
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:200px;height:50px">
               <div style="position:absolute;left:10px;top:0px;width:50px;height:50px"></div>
               <div style="position:absolute;left:70px;top:0px;width:50px;height:50px"></div>
               <div style="position:absolute;left:130px;top:0px;width:50px;height:50px"></div>
             </body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:flex-inferred"));
  auto body = root->getFirstChild("body");
  EXPECT_TRUE(StyleContains(body, "display: flex"));
  EXPECT_TRUE(StyleContains(body, "flex-direction: row"));
  EXPECT_TRUE(StyleContains(body, "gap: 10px"));
  // padTop=0, padRight=20, padBottom=0, padLeft=10 → 4-token shorthand.
  EXPECT_TRUE(StyleContains(body, "padding: 0px 20px 0px 10px"));
  // All children span the body's full height (50px) → align-items: stretch.
  EXPECT_TRUE(StyleContains(body, "align-items: stretch"));
  // Every child loses its position/left/top, and (because of stretch) its cross-axis size.
  for (auto c = body->firstChild; c; c = c->nextSibling) {
    if (c->type != pagx::DOMNodeType::Element) continue;
    EXPECT_FALSE(StyleContains(c, "position"));
    EXPECT_FALSE(StyleContains(c, "left:"));
    EXPECT_FALSE(StyleContains(c, "top:"));
    EXPECT_FALSE(StyleContains(c, "height:"));
    // Main-axis (width) is preserved.
    EXPECT_TRUE(StyleContains(c, "width: 50px"));
  }
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexInferenceRecoversColumnLayoutWithCenter) {
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.inferFlexFromAbsolute = true;
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:120px;height:200px">
               <div style="position:absolute;left:10px;top:10px;width:100px;height:50px"></div>
               <div style="position:absolute;left:35px;top:70px;width:50px;height:50px"></div>
               <div style="position:absolute;left:10px;top:130px;width:100px;height:50px"></div>
             </body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:flex-inferred"));
  auto body = root->getFirstChild("body");
  EXPECT_TRUE(StyleContains(body, "display: flex"));
  EXPECT_TRUE(StyleContains(body, "flex-direction: column"));
  EXPECT_TRUE(StyleContains(body, "gap: 10px"));
  // padTop=10, padRight=0, padBottom=20, padLeft=0 → "T R B" 3-token form.
  EXPECT_TRUE(StyleContains(body, "padding: 10px 0px 20px"));
  // First/last children span the inner content width (10..110); middle one is centred at
  // x=35..85. Their common cross-axis property is `center`, not `stretch`.
  EXPECT_TRUE(StyleContains(body, "align-items: center"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexInferenceSkipsOverlappingChildren) {
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.inferFlexFromAbsolute = true;
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:200px;height:200px">
               <div style="position:absolute;left:0px;top:0px;width:100px;height:100px"></div>
               <div style="position:absolute;left:50px;top:50px;width:100px;height:100px"></div>
             </body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:flex-inference-skipped"));
  auto body = root->getFirstChild("body");
  EXPECT_FALSE(StyleContains(body, "display: flex"));
  // Both children keep their absolute positioning.
  for (auto c = body->firstChild; c; c = c->nextSibling) {
    if (c->type == pagx::DOMNodeType::Element) {
      EXPECT_TRUE(StyleContains(c, "position: absolute"));
    }
  }
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexInferenceSkipsMixedCrossAlignment) {
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.inferFlexFromAbsolute = true;
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:300px;height:100px">
               <div style="position:absolute;left:0px;top:0px;width:50px;height:50px"></div>
               <div style="position:absolute;left:60px;top:25px;width:50px;height:50px"></div>
               <div style="position:absolute;left:120px;top:50px;width:50px;height:50px"></div>
             </body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:flex-inference-skipped"));
  auto body = root->getFirstChild("body");
  EXPECT_FALSE(StyleContains(body, "display: flex"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexInferenceSkipsInconsistentGap) {
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.inferFlexFromAbsolute = true;
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:300px;height:50px">
               <div style="position:absolute;left:0px;top:0px;width:50px;height:50px"></div>
               <div style="position:absolute;left:60px;top:0px;width:50px;height:50px"></div>
               <div style="position:absolute;left:200px;top:0px;width:50px;height:50px"></div>
             </body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:flex-inference-skipped"));
  auto body = root->getFirstChild("body");
  EXPECT_FALSE(StyleContains(body, "display: flex"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexInferenceLeavesExistingFlexAlone) {
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.inferFlexFromAbsolute = true;
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:200px;height:50px;display:flex;gap:5px">
               <div style="width:50px;height:50px"></div>
               <div style="width:50px;height:50px"></div>
             </body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  // No flex-inferred diagnostic — body already declared display:flex, we don't second-guess.
  EXPECT_FALSE(HasDiagnostic(result, "subset:flex-inferred"));
  auto body = root->getFirstChild("body");
  // The original gap survives untouched.
  EXPECT_TRUE(StyleContains(body, "gap: 5px"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexInferenceReordersChildrenWhenDomOrderDiffersFromAxis) {
  // The three children are declared out of visual order: source order is left, right, middle
  // (by `left:`). Without DOM reordering, applying `display:flex` would render them in source
  // order, swapping the visible right and middle columns. Verify the fix puts them back in
  // position order so the visual layout is preserved.
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.inferFlexFromAbsolute = true;
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:300px;height:50px">
               <div data-id="left"   style="position:absolute;left:0px;top:0px;width:100px;height:50px"></div>
               <div data-id="right"  style="position:absolute;left:200px;top:0px;width:100px;height:50px"></div>
               <div data-id="middle" style="position:absolute;left:100px;top:0px;width:100px;height:50px"></div>
             </body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:flex-inferred"));
  auto body = root->getFirstChild("body");
  ASSERT_NE(body, nullptr);
  EXPECT_TRUE(StyleContains(body, "display: flex"));
  EXPECT_TRUE(StyleContains(body, "flex-direction: row"));
  std::vector<std::string> orderedIds;
  for (auto c = body->firstChild; c; c = c->nextSibling) {
    if (c->type != pagx::DOMNodeType::Element) continue;
    orderedIds.push_back(AttrValue(c, "data-id"));
  }
  ASSERT_EQ(orderedIds.size(), 3u);
  EXPECT_EQ(orderedIds[0], "left");
  EXPECT_EQ(orderedIds[1], "middle");
  EXPECT_EQ(orderedIds[2], "right");
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexInferenceKeepsChildrenWhenDomOrderMatchesAxis) {
  // Sanity-check: when the source order already matches position order, no reordering is
  // needed and the children appear in their original sequence.
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.inferFlexFromAbsolute = true;
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:300px;height:50px">
               <div data-id="a" style="position:absolute;left:0px;top:0px;width:100px;height:50px"></div>
               <div data-id="b" style="position:absolute;left:100px;top:0px;width:100px;height:50px"></div>
               <div data-id="c" style="position:absolute;left:200px;top:0px;width:100px;height:50px"></div>
             </body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:flex-inferred"));
  auto body = root->getFirstChild("body");
  std::vector<std::string> orderedIds;
  for (auto c = body->firstChild; c; c = c->nextSibling) {
    if (c->type != pagx::DOMNodeType::Element) continue;
    orderedIds.push_back(AttrValue(c, "data-id"));
  }
  ASSERT_EQ(orderedIds.size(), 3u);
  EXPECT_EQ(orderedIds[0], "a");
  EXPECT_EQ(orderedIds[1], "b");
  EXPECT_EQ(orderedIds[2], "c");
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexInferenceRecoversNestedPaddingUnderStretchParent) {
  // Regression for the snapshot.js → pagx import pipeline (`html2pagx`). The body has a
  // column-stretch flex layout (children span the full body width), so the outer pass
  // erases each child's `width: 320px`. The inner child is itself a row container whose
  // own children stop short of both edges (`left:14..286`), and its row padding must be
  // recovered from the original 320px width rather than from the children's bbox. With
  // pre-order traversal the inner inference happened *after* the outer strip and silently
  // collapsed the padding to 0; the fix walks children first so the inner pass still sees
  // the explicit width.
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.inferFlexFromAbsolute = true;
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:320px;height:120px">
               <div style="position:absolute;left:0px;top:0px;width:320px;height:60px;background-color:#fff">
                 <div style="position:absolute;left:14px;top:18px;width:24px;height:24px"></div>
                 <div style="position:absolute;left:48px;top:18px;width:200px;height:24px"></div>
                 <div style="position:absolute;left:258px;top:18px;width:24px;height:24px"></div>
                 <div style="position:absolute;left:292px;top:18px;width:14px;height:24px"></div>
               </div>
               <div style="position:absolute;left:0px;top:60px;width:320px;height:60px;background-color:#fff">
                 <div style="position:absolute;left:0px;top:0px;width:160px;height:60px"></div>
                 <div style="position:absolute;left:160px;top:0px;width:160px;height:60px"></div>
               </div>
             </body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:flex-inferred"));
  auto body = root->getFirstChild("body");
  ASSERT_NE(body, nullptr);
  EXPECT_TRUE(StyleContains(body, "display: flex"));
  EXPECT_TRUE(StyleContains(body, "flex-direction: column"));
  EXPECT_TRUE(StyleContains(body, "align-items: stretch"));

  // First child: header row whose direct children stop 14px from both edges. After the fix
  // its inner inference uses the original 320px width and recovers padding-left/right = 14.
  auto topBar = body->firstChild;
  ASSERT_NE(topBar, nullptr);
  ASSERT_EQ(topBar->type, pagx::DOMNodeType::Element);
  EXPECT_TRUE(StyleContains(topBar, "display: flex"));
  EXPECT_TRUE(StyleContains(topBar, "flex-direction: row"));
  // Asymmetric main-axis padding (14 left, 14 right) emitted as the 2-token shorthand.
  EXPECT_TRUE(StyleContains(topBar, "padding: 0px 14px"));
  // Because the parent stretches its cross axis, the top bar's width is erased.
  EXPECT_FALSE(StyleContains(topBar, "width:"));

  // Second child: a row of two siblings that exactly tile the parent. Its inner inference
  // should produce zero horizontal padding (and is allowed to use stretch on its own
  // children's height).
  auto grid = topBar->nextSibling;
  while (grid && grid->type != pagx::DOMNodeType::Element) grid = grid->nextSibling;
  ASSERT_NE(grid, nullptr);
  EXPECT_TRUE(StyleContains(grid, "display: flex"));
  EXPECT_TRUE(StyleContains(grid, "flex-direction: row"));
  // No padding emitted (all four sides are zero — the shorthand is suppressed entirely).
  EXPECT_FALSE(StyleContains(grid, "padding:"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexInferenceTolerancePicksUpSubpixelDrift) {
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.inferFlexFromAbsolute = true;
  // Gaps are 10.4 / 9.6 (drift < default 1.5px tolerance) and tops drift by 0.7px (< tol).
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:200px;height:50px">
               <div style="position:absolute;left:10px;top:0px;width:50px;height:50px"></div>
               <div style="position:absolute;left:70.4px;top:0.7px;width:50px;height:50px"></div>
               <div style="position:absolute;left:130px;top:0px;width:50px;height:50px"></div>
             </body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:flex-inferred"));
  auto body = root->getFirstChild("body");
  EXPECT_TRUE(StyleContains(body, "display: flex"));
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

//==================================================================================================
// Per-property transforms: keyword fallbacks and edge cases
//==================================================================================================

PAG_TEST(PAGXHTMLSubsetTransformerTest, DisplayInlineDowngradesToBlock) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="display: inline"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(StyleContains(FirstBodyChild(root, "div"), "display: block"));
  EXPECT_TRUE(HasDiagnostic(result, "subset:unsupported-property"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, DisplayInlineBlockDowngradesToBlock) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="display: inline-block"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(StyleContains(FirstBodyChild(root, "div"), "display: block"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, DisplayUnknownDropped) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="display: grid"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(FirstBodyChild(root, "div"), "display:"));
  EXPECT_TRUE(HasDiagnostic(result, "subset:unsupported-property"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexDirectionRowReverseFallsBackToRow) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px;display:flex;flex-direction:row-reverse">
             </body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  auto body = root->getFirstChild("body");
  EXPECT_TRUE(StyleContains(body, "flex-direction: row"));
  EXPECT_FALSE(StyleContains(body, "row-reverse"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexDirectionColumnReverseFallsBackToColumn) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px;display:flex;flex-direction:column-reverse">
             </body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  auto body = root->getFirstChild("body");
  EXPECT_TRUE(StyleContains(body, "flex-direction: column"));
  EXPECT_FALSE(StyleContains(body, "column-reverse"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexDirectionUnknownDropped) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px;display:flex;flex-direction:diagonal">
             </body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(root->getFirstChild("body"), "flex-direction:"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, AlignItemsBaselineKept) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px;display:flex;align-items:baseline">
             </body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(StyleContains(root->getFirstChild("body"), "align-items: baseline"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, AlignItemsInvalidDropped) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px;display:flex;align-items:floaty">
             </body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(root->getFirstChild("body"), "align-items:"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, JustifyContentInvalidDropped) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px;display:flex;justify-content:bouncy">
             </body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(root->getFirstChild("body"), "justify-content:"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexShrinkZeroDroppedSilently) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="flex-shrink: 0"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  auto div = FirstBodyChild(root, "div");
  EXPECT_FALSE(StyleContains(div, "flex-shrink"));
  EXPECT_FALSE(HasDiagnostic(result, "subset:unsupported-property"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexShrinkNonzeroWarns) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="flex-shrink: 2"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:unsupported-property"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexNoneMapsToZero) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="flex: none"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(StyleContains(FirstBodyChild(root, "div"), "flex: 0"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexAutoMapsToOne) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="flex: auto"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(StyleContains(FirstBodyChild(root, "div"), "flex: 1"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexUnknownKeywordDropped) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="flex: stretchy"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(FirstBodyChild(root, "div"), "flex:"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexWithUnitOnGrowWarns) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="flex: 2px"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:unsupported-property"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, PositionStickyDowngradedToAbsolute) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="position: sticky; top: 0"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(StyleContains(FirstBodyChild(root, "div"), "position: absolute"));
  EXPECT_TRUE(HasDiagnostic(result, "subset:unsupported-property"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, PositionStaticDroppedSilently) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="position: static"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(FirstBodyChild(root, "div"), "position:"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, PositionInvalidDropped) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="position: floating"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(FirstBodyChild(root, "div"), "position:"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, BackgroundImageUrlDropped) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="background-image: url(image.png)"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(FirstBodyChild(root, "div"), "background-image"));
  EXPECT_TRUE(HasDiagnostic(result, "subset:unsupported-property"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, BackgroundImageNoneDroppedSilently) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="background-image: none"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(FirstBodyChild(root, "div"), "background-image"));
  EXPECT_FALSE(HasDiagnostic(result, "subset:unsupported-property"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, BackgroundImageInvalidDropped) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="background-image: weirdvalue"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(FirstBodyChild(root, "div"), "background-image"));
  EXPECT_TRUE(HasDiagnostic(result, "subset:unsupported-property"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, BackgroundClipPaddingBoxDroppedSilently) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="background-clip: padding-box"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(FirstBodyChild(root, "div"), "background-clip"));
  EXPECT_FALSE(HasDiagnostic(result, "subset:unsupported-property"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, BackgroundClipInvalidDropped) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="background-clip: weird-clip"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:unsupported-property"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, BorderDashedPreserved) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="border: 2px dashed #000"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(HasDiagnostic(result, "subset:unsupported-property"));
  EXPECT_TRUE(StyleContains(FirstBodyChild(root, "div"), "dashed"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, BorderDottedPreserved) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="border: 2px dotted #000"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(HasDiagnostic(result, "subset:unsupported-property"));
  EXPECT_TRUE(StyleContains(FirstBodyChild(root, "div"), "dotted"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, BorderDoubleDowngradedToSolid) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="border: 2px double #000"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:unsupported-property"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, BorderNoneDropped) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="border: none"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(FirstBodyChild(root, "div"), "border:"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, OpacityNonNumberDropped) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="opacity: invalid"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(FirstBodyChild(root, "div"), "opacity:"));
  EXPECT_TRUE(HasDiagnostic(result, "subset:unsupported-property"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, OpacityPercentNormalised) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="opacity: 50%"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  // Internally clamps to [0,1]; 50% becomes 0.5.
  EXPECT_TRUE(StyleContains(FirstBodyChild(root, "div"), "opacity: 0.5"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, OpacityClampedAbove1) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="opacity: 2"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(StyleContains(FirstBodyChild(root, "div"), "opacity: 1"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, ObjectFitNoneDowngradedToContain) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <img src="x.png" style="object-fit: none"/></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(StyleContains(FirstBodyChild(root, "img"), "object-fit: contain"));
  EXPECT_TRUE(HasDiagnostic(result, "subset:unsupported-property"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, ObjectFitScaleDownDowngraded) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <img src="x.png" style="object-fit: scale-down"/></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(StyleContains(FirstBodyChild(root, "img"), "object-fit: contain"));
  EXPECT_TRUE(HasDiagnostic(result, "subset:unsupported-property"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, ObjectFitInvalidDropped) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <img src="x.png" style="object-fit: zoomy"/></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(FirstBodyChild(root, "img"), "object-fit"));
}

//==================================================================================================
// Length resolution
//==================================================================================================

PAG_TEST(PAGXHTMLSubsetTransformerTest, LengthCalcDropped) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="width: calc(100% - 10px)"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(FirstBodyChild(root, "div"), "width:"));
  EXPECT_TRUE(HasDiagnostic(result, "subset:unsupported-property"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, LengthVarDropped) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="width: var(--w)"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(FirstBodyChild(root, "div"), "width:"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, LengthClampDropped) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="width: clamp(10px, 50%, 200px)"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(FirstBodyChild(root, "div"), "width:"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, LengthMinDropped) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="width: min(10px, 50%)"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(FirstBodyChild(root, "div"), "width:"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, LengthAutoDroppedSilently) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="width: auto; height: inherit"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  auto div = FirstBodyChild(root, "div");
  EXPECT_FALSE(StyleContains(div, "width:"));
  EXPECT_FALSE(StyleContains(div, "height:"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, LengthPtConvertedToPx) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="font-size: 12pt"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  // 12pt = 16px (12 * 4 / 3).
  EXPECT_TRUE(StyleContains(FirstBodyChild(root, "div"), "font-size: 16px"));
  EXPECT_TRUE(HasDiagnostic(result, "subset:unit-coerced"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, LengthVwWithoutCanvasDropped) {
  pagx::HTMLSubsetTransformer::Options opts = {};
  // Default canvasWidth/canvasHeight = 0 → vw/vh cannot resolve.
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:200px;height:200px">
               <div style="width: 50vw"></div></body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(FirstBodyChild(root, "div"), "width:"));
  EXPECT_TRUE(HasDiagnostic(result, "subset:unsupported-property"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, LengthInvalidUnitDropped) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="width: 5xq"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(FirstBodyChild(root, "div"), "width:"));
  EXPECT_TRUE(HasDiagnostic(result, "subset:unsupported-property"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, LengthMalformedDropped) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="width: notalength"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(FirstBodyChild(root, "div"), "width:"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, LengthShorthandResolvesEachToken) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="padding: 1rem 2pt"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  // 1rem -> 16px, 2pt -> 2.667px.
  EXPECT_TRUE(StyleContains(FirstBodyChild(root, "div"), "padding: 16px"));
}

//==================================================================================================
// MarginToGapPromotion
//==================================================================================================

PAG_TEST(PAGXHTMLSubsetTransformerTest, MarginToGapTrailingWithBlankLastChild) {
  // The Tailwind-style pattern: every flex child has the same `mb`, except the last one.
  // The pass should lift the shared margin onto the container's `gap` and clear all per-child
  // margins, so PAGX's vertical layout reproduces the CSS gutters.
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:200px;height:300px">
               <div style="display:flex;flex-direction:column;width:200px;height:300px">
                 <div style="width:100px;height:50px;margin-bottom:12px"></div>
                 <div style="width:100px;height:50px;margin-bottom:12px"></div>
                 <div style="width:100px;height:50px"></div>
               </div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:margin-promoted-to-gap"));
  auto wrapper = FirstBodyChild(root, "div");
  ASSERT_NE(wrapper, nullptr);
  EXPECT_TRUE(StyleContains(wrapper, "gap: 12px"));
  for (auto c = wrapper->firstChild; c; c = c->nextSibling) {
    if (c->type != pagx::DOMNodeType::Element) continue;
    EXPECT_FALSE(StyleContains(c, "margin-bottom"));
    EXPECT_FALSE(StyleContains(c, "margin-top"));
  }
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, MarginToGapTrailingUniformAllChildren) {
  // The same uniform `mb` on every child (including the last). The pass still lifts onto
  // `gap` and zero out the trailing margins; the trailing `mb` on the last child becomes
  // empty bottom padding which CSS accepts.
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:200px;height:400px">
               <div style="display:flex;flex-direction:column;width:200px;height:400px">
                 <div style="width:100px;height:50px;margin-bottom:8px"></div>
                 <div style="width:100px;height:50px;margin-bottom:8px"></div>
                 <div style="width:100px;height:50px;margin-bottom:8px"></div>
               </div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:margin-promoted-to-gap"));
  auto wrapper = FirstBodyChild(root, "div");
  EXPECT_TRUE(StyleContains(wrapper, "gap: 8px"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, MarginToGapLeadingPattern) {
  // The leading pattern: first child has no margin-top, every subsequent child has the same
  // margin-top. Pass should still promote.
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:200px;height:300px">
               <div style="display:flex;flex-direction:column;width:200px;height:300px">
                 <div style="width:100px;height:50px"></div>
                 <div style="width:100px;height:50px;margin-top:16px"></div>
                 <div style="width:100px;height:50px;margin-top:16px"></div>
               </div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:margin-promoted-to-gap"));
  auto wrapper = FirstBodyChild(root, "div");
  EXPECT_TRUE(StyleContains(wrapper, "gap: 16px"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, MarginToGapWorksOnRowFlex) {
  // Same logic on the row axis: uniform margin-right across siblings becomes the container's
  // `gap`. Ensures the pass is axis-symmetric, not column-only.
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:300px;height:60px">
               <div style="display:flex;flex-direction:row;width:300px;height:60px">
                 <div style="width:50px;height:50px;margin-right:10px"></div>
                 <div style="width:50px;height:50px;margin-right:10px"></div>
                 <div style="width:50px;height:50px"></div>
               </div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:margin-promoted-to-gap"));
  auto wrapper = FirstBodyChild(root, "div");
  EXPECT_TRUE(StyleContains(wrapper, "gap: 10px"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, MarginToGapSkipsInconsistentMargins) {
  // Inconsistent trailing margins (8px vs 12px) and no leading margins: neither pattern
  // matches, so the pass leaves the tree untouched and emits no promotion diagnostic.
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:200px;height:300px">
               <div style="display:flex;flex-direction:column;width:200px;height:300px">
                 <div style="width:100px;height:50px;margin-bottom:8px"></div>
                 <div style="width:100px;height:50px;margin-bottom:12px"></div>
                 <div style="width:100px;height:50px"></div>
               </div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(HasDiagnostic(result, "subset:margin-promoted-to-gap"));
  auto wrapper = FirstBodyChild(root, "div");
  EXPECT_FALSE(StyleContains(wrapper, "gap:"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, MarginToGapSkipsWhenContainerHasGap) {
  // Container already declares its own positive `gap`: leave the per-child margins alone.
  // We deliberately set the gap to a value distinct from the per-child margin so a buggy
  // implementation that overwrites the gap is easy to spot.
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:200px;height:300px">
               <div style="display:flex;flex-direction:column;gap:4px;width:200px;height:300px">
                 <div style="width:100px;height:50px;margin-bottom:12px"></div>
                 <div style="width:100px;height:50px;margin-bottom:12px"></div>
                 <div style="width:100px;height:50px"></div>
               </div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(HasDiagnostic(result, "subset:margin-promoted-to-gap"));
  auto wrapper = FirstBodyChild(root, "div");
  EXPECT_TRUE(StyleContains(wrapper, "gap: 4px"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, MarginToGapSkipsWhenShorthandHasBothEdgeMargins) {
  // `margin: 4px 12px` makes EVERY child carry a non-zero leading AND trailing main-axis
  // margin (top=bottom=4 for a column flex). The actual CSS gutter between siblings is
  // mb + mt = 8, not 4, so a naive lift to `gap: 4px` would change the rendering. The pass
  // must conservatively decline both the trailing and leading patterns and leave the
  // shorthand untouched.
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:200px;height:300px">
               <div style="display:flex;flex-direction:column;width:200px;height:300px">
                 <div style="width:100px;height:50px;margin: 4px 12px"></div>
                 <div style="width:100px;height:50px;margin: 4px 12px"></div>
                 <div style="width:100px;height:50px;margin: 4px 12px"></div>
               </div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(HasDiagnostic(result, "subset:margin-promoted-to-gap"));
  auto wrapper = FirstBodyChild(root, "div");
  EXPECT_FALSE(StyleContains(wrapper, "gap:"));
  // Children keep their original shorthand verbatim.
  for (auto c = wrapper->firstChild; c; c = c->nextSibling) {
    if (c->type != pagx::DOMNodeType::Element) continue;
    EXPECT_TRUE(StyleContains(c, "margin: 4px 12px"));
  }
}

//==================================================================================================
// SpaceJustifyOverflowCollapse
//==================================================================================================

PAG_TEST(PAGXHTMLSubsetTransformerTest, SpaceJustifyCollapseAccountsForChildMargin) {
  // Children fit the container by `width` alone (3 × 50 = 150 ≤ 200) but overflow once
  // their `margin-right` is included (3 × 50 + 2 × 30 = 210 > 200). The pass must count
  // the margins; otherwise it would leave `space-between` in place and PAGX would render
  // the leftover space as a positive gap that doesn't exist in the browser.
  //
  // We disable MarginToGap (by using a leading + trailing mix that doesn't match either
  // pattern) so the SpaceJustify pass is the one observed.
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:200px;height:80px">
               <div style="display:flex;flex-direction:row;justify-content:space-between;width:200px;height:80px">
                 <div style="width:50px;height:50px;margin-right:30px"></div>
                 <div style="width:50px;height:50px;margin-left:30px;margin-right:30px"></div>
                 <div style="width:50px;height:50px;margin-left:30px"></div>
               </div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:space-justify-collapsed-on-overflow"));
  auto wrapper = FirstBodyChild(root, "div");
  EXPECT_TRUE(StyleContains(wrapper, "justify-content: flex-start"));
  // MarginToGap should have stayed away from this mixed-leading/trailing layout.
  EXPECT_FALSE(HasDiagnostic(result, "subset:margin-promoted-to-gap"));
}

}  // namespace pag
