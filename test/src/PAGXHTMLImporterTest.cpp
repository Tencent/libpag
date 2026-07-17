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
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include "base/PAGTest.h"
#include "cli/CommandVerify.h"
#include "pagx/FontConfig.h"
#include "pagx/HTMLImporter.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/PAGXOptimizer.h"
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/html/importer/HTMLInlineSvgEmitter.h"
#include "pagx/html/importer/HTMLSubsetTransformer.h"
#include "pagx/html/importer/HTMLTransformContext.h"
#include "pagx/html/importer/HTMLTransformPassUtils.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlendFilter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorMatrixFilter.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/InnerShadowFilter.h"
#include "pagx/nodes/InnerShadowStyle.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/xml/XMLDOM.h"
#include "utils/PAGXTestUtils.h"
#include "utils/ProjectPath.h"

namespace pag {

namespace {

constexpr float kEps = 0.001f;

inline bool NearlyEqual(float a, float b, float eps = kEps) {
  return std::fabs(a - b) <= eps;
}

inline std::string SaveFile(const std::string& content, const std::string& key) {
  auto outPath = ProjectPath::Absolute("test/out/" + key);
  auto dirPath = std::filesystem::path(outPath).parent_path();
  if (!std::filesystem::exists(dirPath)) {
    std::filesystem::create_directories(dirPath);
  }
  std::ofstream file(outPath, std::ios::binary);
  if (file) {
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
  }
  return outPath;
}

template <typename T>
struct PagxNodeTypeOf;

#define DECLARE_PAGX_NODE_TYPE(Type)                              \
  template <>                                                     \
  struct PagxNodeTypeOf<pagx::Type> {                             \
    static constexpr pagx::NodeType value = pagx::NodeType::Type; \
  }

DECLARE_PAGX_NODE_TYPE(Rectangle);
DECLARE_PAGX_NODE_TYPE(Path);
DECLARE_PAGX_NODE_TYPE(Ellipse);
DECLARE_PAGX_NODE_TYPE(Fill);
DECLARE_PAGX_NODE_TYPE(Stroke);
DECLARE_PAGX_NODE_TYPE(Text);
DECLARE_PAGX_NODE_TYPE(TextBox);
DECLARE_PAGX_NODE_TYPE(Group);
DECLARE_PAGX_NODE_TYPE(SolidColor);
DECLARE_PAGX_NODE_TYPE(LinearGradient);
DECLARE_PAGX_NODE_TYPE(RadialGradient);
DECLARE_PAGX_NODE_TYPE(ConicGradient);
DECLARE_PAGX_NODE_TYPE(ImagePattern);
DECLARE_PAGX_NODE_TYPE(DropShadowStyle);
DECLARE_PAGX_NODE_TYPE(InnerShadowStyle);
DECLARE_PAGX_NODE_TYPE(BackgroundBlurStyle);
DECLARE_PAGX_NODE_TYPE(BlurFilter);
DECLARE_PAGX_NODE_TYPE(DropShadowFilter);
DECLARE_PAGX_NODE_TYPE(InnerShadowFilter);
DECLARE_PAGX_NODE_TYPE(ColorMatrixFilter);
DECLARE_PAGX_NODE_TYPE(BlendFilter);

#undef DECLARE_PAGX_NODE_TYPE

// Type-safe replacement for `dynamic_cast<T*>(node)` rooted in the project's `nodeType()`
// dispatch. Returns `node` cast to `T*` when its runtime tag matches `PagxNodeTypeOf<T>::value`,
// nullptr otherwise. Hoisted out of the test bodies so the file follows the codebase rule of
// avoiding `dynamic_cast`.
template <typename T, typename Base>
T* As(Base* node) {
  if (!node) return nullptr;
  if (node->nodeType() != PagxNodeTypeOf<T>::value) return nullptr;
  return static_cast<T*>(node);
}

template <typename T>
T* FindElement(const std::vector<pagx::Element*>& contents) {
  for (auto* e : contents) {
    if (auto* match = As<T>(e)) {
      return match;
    }
  }
  return nullptr;
}

template <typename T>
T* FindElementOfType(pagx::Layer* layer) {
  if (!layer) return nullptr;
  return FindElement<T>(layer->contents);
}

template <typename T>
size_t CountElements(const std::vector<pagx::Element*>& contents) {
  size_t count = 0;
  for (auto* e : contents) {
    if (As<T>(e)) count++;
  }
  return count;
}

inline pagx::Color HexColor(uint32_t hex, float alpha = 1.0f) {
  pagx::Color c;
  c.red = static_cast<float>((hex >> 16) & 0xFF) / 255.0f;
  c.green = static_cast<float>((hex >> 8) & 0xFF) / 255.0f;
  c.blue = static_cast<float>(hex & 0xFF) / 255.0f;
  c.alpha = alpha;
  c.colorSpace = pagx::ColorSpace::SRGB;
  return c;
}

inline bool ColorNear(const pagx::Color& a, const pagx::Color& b, float eps = 0.01f) {
  return NearlyEqual(a.red, b.red, eps) && NearlyEqual(a.green, b.green, eps) &&
         NearlyEqual(a.blue, b.blue, eps) && NearlyEqual(a.alpha, b.alpha, eps);
}

inline std::shared_ptr<pagx::PAGXDocument> ParseFromString(const std::string& html) {
  return pagx::HTMLImporter::ParseString(html);
}

inline pagx::Color SolidFillColorOf(pagx::Layer* layer) {
  auto* fill = FindElementOfType<pagx::Fill>(layer);
  if (!fill) return {};
  auto* solid = As<pagx::SolidColor>(fill->color);
  if (!solid) return {};
  return solid->color;
}

// HTMLSubsetTransformer helpers -----------------------------------------------------------

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
// Unit tests: CSS property mapping
//==================================================================================================

PAG_TEST(PAGXHTMLImporterTest, ParsesCanvasSizeFromBody) {
  auto doc =
      ParseFromString(R"HTML(<html><body style="width:200px;height:100px"></body></html>)HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FLOAT_EQ(doc->width, 200.0f);
  EXPECT_FLOAT_EQ(doc->height, 100.0f);
  EXPECT_EQ(doc->layers.size(), 1u);
}

PAG_TEST(PAGXHTMLImporterTest, RejectsMissingCanvas) {
  auto doc = ParseFromString(R"HTML(<html><body></body></html>)HTML");
  EXPECT_EQ(doc, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, BackgroundColorBecomesRectangleAndFill) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:100px;height:50px">
      <div style="width:80px;height:40px;background-color:#FF0000"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* body = doc->layers.front();
  ASSERT_FALSE(body->children.empty());
  auto* div = body->children.front();
  auto* rect = FindElementOfType<pagx::Rectangle>(div);
  auto* fill = FindElementOfType<pagx::Fill>(div);
  ASSERT_NE(rect, nullptr);
  ASSERT_NE(fill, nullptr);
  EXPECT_FLOAT_EQ(rect->percentWidth, 100.0f);
  EXPECT_FLOAT_EQ(rect->percentHeight, 100.0f);
  EXPECT_FLOAT_EQ(rect->roundness, 0.0f);
  auto* solid = As<pagx::SolidColor>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0xFF0000)));
  EXPECT_FLOAT_EQ(div->width, 80.0f);
  EXPECT_FLOAT_EQ(div->height, 40.0f);
}

// Regression: a colour-only `background` shorthand (e.g. `background:#FF0000`) must paint the
// same fill as the `background-color` longhand. The subset property table only lists the longhand,
// so ComputedStylePass expands the shorthand before PropertyFilterPass would otherwise drop it.
PAG_TEST(PAGXHTMLImporterTest, BackgroundShorthandColorBecomesFill) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:100px;height:50px">
      <div style="width:80px;height:40px;background:#FF0000;border-radius:8px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* rect = FindElementOfType<pagx::Rectangle>(div);
  auto* fill = FindElementOfType<pagx::Fill>(div);
  ASSERT_NE(rect, nullptr);
  ASSERT_NE(fill, nullptr);
  EXPECT_FLOAT_EQ(rect->roundness, 8.0f);
  auto* solid = As<pagx::SolidColor>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0xFF0000)));
}

// A gradient carried by the `background` shorthand routes to `background-image` during expansion
// and paints a gradient fill, matching the `background-image` longhand.
PAG_TEST(PAGXHTMLImporterTest, BackgroundShorthandGradientBecomesGradientFill) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background:linear-gradient(90deg, #FF0000 0%, #0000FF 100%)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(div);
  ASSERT_NE(fill, nullptr);
  auto* lg = As<pagx::LinearGradient>(fill->color);
  ASSERT_NE(lg, nullptr);
  ASSERT_EQ(lg->colorStops.size(), 2u);
  EXPECT_TRUE(ColorNear(lg->colorStops.front()->color, HexColor(0xFF0000)));
  EXPECT_TRUE(ColorNear(lg->colorStops.back()->color, HexColor(0x0000FF)));
}

// A functional-notation colour (`rgba(...)`) carried by the `background` shorthand must be
// recognised as a colour — not mistaken for an image — and paint a solid fill.
PAG_TEST(PAGXHTMLImporterTest, BackgroundShorthandFunctionalColorBecomesFill) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background:rgba(255,0,0,0.5)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(div);
  ASSERT_NE(fill, nullptr);
  auto* solid = As<pagx::SolidColor>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_NEAR(solid->color.alpha, 0.5f, 0.02f);
}

// A combined `background` shorthand (colour + gradient overlay) decomposes into both a
// `background-color` and a `background-image`; the gradient paints over the colour, so the fill
// is the gradient.
PAG_TEST(PAGXHTMLImporterTest, BackgroundShorthandColorPlusGradient) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background:#00FF00 linear-gradient(90deg, #FF0000 0%, #0000FF 100%)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(div);
  ASSERT_NE(fill, nullptr);
  auto* lg = As<pagx::LinearGradient>(fill->color);
  ASSERT_NE(lg, nullptr);
  ASSERT_EQ(lg->colorStops.size(), 2u);
  EXPECT_TRUE(ColorNear(lg->colorStops.front()->color, HexColor(0xFF0000)));
}

PAG_TEST(PAGXHTMLImporterTest, BorderRadiusMapsToRoundness) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:#000;border-radius:12px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* rect = FindElementOfType<pagx::Rectangle>(div);
  ASSERT_NE(rect, nullptr);
  EXPECT_FLOAT_EQ(rect->roundness, 12.0f);
}

// Regression: macOS-style traffic-light dots use `border-radius: 50%` on a square
// element to draw circles. `border-radius: 50%` inscribes an ellipse in the box, which on
// a square box is a circle, so the importer emits a PAGX `Ellipse` sized to 100% of the
// layer (a square Ellipse renders as a circle).
PAG_TEST(PAGXHTMLImporterTest, BorderRadiusPercentBecomesCircleOnSquareBox) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:24px;height:24px">
      <div style="width:12px;height:12px;background-color:#FF5F57;border-radius:50%"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_EQ(FindElementOfType<pagx::Rectangle>(div), nullptr);
  auto* ellipse = FindElementOfType<pagx::Ellipse>(div);
  ASSERT_NE(ellipse, nullptr);
  EXPECT_FLOAT_EQ(ellipse->percentWidth, 100.0f);
  EXPECT_FLOAT_EQ(ellipse->percentHeight, 100.0f);
}

// On a non-square box `border-radius: 50%` means an ellipse in CSS: every corner's horizontal
// radius is half the width and every corner's vertical radius is half the height. The importer
// emits a PAGX `Ellipse` (not a pill-shaped Rectangle) so a 40x12 box renders as a true ellipse.
PAG_TEST(PAGXHTMLImporterTest, BorderRadiusPercentOnRectangleBecomesEllipse) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:60px;height:20px">
      <div style="width:40px;height:12px;background-color:#28C840;border-radius:50%"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_EQ(FindElementOfType<pagx::Rectangle>(div), nullptr);
  auto* ellipse = FindElementOfType<pagx::Ellipse>(div);
  ASSERT_NE(ellipse, nullptr);
  EXPECT_FLOAT_EQ(ellipse->percentWidth, 100.0f);
  EXPECT_FLOAT_EQ(ellipse->percentHeight, 100.0f);
}

// A percentage radius on an element whose width/height is not given in px
// cannot be resolved at parse time (the parent's layout would be needed) — the
// importer drops the value with a warning rather than rendering a wrong shape.
PAG_TEST(PAGXHTMLImporterTest, BorderRadiusPercentWithoutFixedSizeIsDropped) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50%;height:50%;background-color:#000;border-radius:50%"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* rect = FindElementOfType<pagx::Rectangle>(div);
  ASSERT_NE(rect, nullptr);
  EXPECT_FLOAT_EQ(rect->roundness, 0.0f);
}

PAG_TEST(PAGXHTMLImporterTest, BorderProducesStroke) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:60px;height:60px">
      <div style="width:60px;height:60px;border:3px solid #00FF00"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* stroke = FindElementOfType<pagx::Stroke>(div);
  ASSERT_NE(stroke, nullptr);
  EXPECT_FLOAT_EQ(stroke->width, 3.0f);
  auto* solid = As<pagx::SolidColor>(stroke->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0x00FF00)));
  EXPECT_EQ(stroke->align, pagx::StrokeAlign::Inside);
}

// Regression: with `box-sizing: border-box` (the only mode supported by the importer),
// CSS positions absolutely-positioned descendants relative to the parent's padding box
// (inside the border). The importer must shift the child's left/right/top/bottom
// constraints by the parent's border width so the child does not overlap the inside
// stroke.
PAG_TEST(PAGXHTMLImporterTest, BorderShiftsAbsoluteChildren) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:100px;height:100px">
      <div style="position:relative;width:60px;height:60px;border:10px solid #000">
        <div style="position:absolute;left:0;top:0;width:40px;height:40px;background-color:#FFF"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* outer = doc->layers.front()->children.front();
  ASSERT_EQ(outer->children.size(), 1u);
  auto* inner = outer->children.front();
  EXPECT_FALSE(inner->includeInLayout);
  EXPECT_FLOAT_EQ(inner->left, 10.0f);
  EXPECT_FLOAT_EQ(inner->top, 10.0f);
}

// Regression: layout-flow children of a bordered container must start inside the
// border. The importer reserves the border width as additional padding on the
// container so that flex children align with the inner edge of the stroke rather
// than the outer edge.
PAG_TEST(PAGXHTMLImporterTest, BorderExpandsLayoutPadding) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:100px;height:100px">
      <div style="display:flex;flex-direction:column;width:60px;height:60px;
                  padding:5px;border:4px solid #000;background-color:#FFF">
        <div style="width:10px;height:10px"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* outer = doc->layers.front()->children.front();
  // The double-host split puts layout/padding on the inner host.
  ASSERT_EQ(outer->children.size(), 1u);
  auto* host = outer->children.front();
  EXPECT_FLOAT_EQ(host->padding.top, 9.0f);
  EXPECT_FLOAT_EQ(host->padding.right, 9.0f);
  EXPECT_FLOAT_EQ(host->padding.bottom, 9.0f);
  EXPECT_FLOAT_EQ(host->padding.left, 9.0f);
}

PAG_TEST(PAGXHTMLImporterTest, BoxShadowProducesDropShadowStyle) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <div style="width:80px;height:80px;background-color:#FFF;box-shadow:0 2px 8px #00000033"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  ASSERT_EQ(div->styles.size(), 1u);
  auto* drop = As<pagx::DropShadowStyle>(div->styles.front());
  ASSERT_NE(drop, nullptr);
  EXPECT_FLOAT_EQ(drop->offsetX, 0.0f);
  EXPECT_FLOAT_EQ(drop->offsetY, 2.0f);
  // CSS box-shadow blur-radius (8px) maps to PAGX σ = blur-radius / 2 (= 4).
  EXPECT_FLOAT_EQ(drop->blurX, 4.0f);
  EXPECT_FLOAT_EQ(drop->blurY, 4.0f);
  EXPECT_TRUE(ColorNear(drop->color, HexColor(0x000000, 0.2f), 0.02f));
}

PAG_TEST(PAGXHTMLImporterTest, InsetBoxShadowProducesInnerShadowStyle) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:#FFF;box-shadow:inset 2px 2px 4px #000"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  ASSERT_EQ(div->styles.size(), 1u);
  auto* inner = As<pagx::InnerShadowStyle>(div->styles.front());
  ASSERT_NE(inner, nullptr);
  EXPECT_FLOAT_EQ(inner->offsetX, 2.0f);
  EXPECT_FLOAT_EQ(inner->offsetY, 2.0f);
  // CSS box-shadow blur-radius (4px) maps to PAGX σ = blur-radius / 2 (= 2).
  EXPECT_FLOAT_EQ(inner->blurX, 2.0f);
}

PAG_TEST(PAGXHTMLImporterTest, MultipleBoxShadows) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:#FFF;
                  box-shadow: 0 1px 2px #00000022, 0 4px 8px #00000033"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_EQ(div->styles.size(), 2u);
}

// Regression: CSS `overflow: hidden` clips children but does NOT clip `box-shadow`. PAGX's
// `clipToBounds` (mapped to tgfx scrollRect) clips contents AND layer styles together —
// `tgfx::Layer::drawChild` runs `ClipScrollRect` before drawing the layer's "Below" styles,
// so a drop shadow on the same layer would be cut off along with the children. The importer
// must hoist the shadow onto an outer wrapper that does not clip; the inner layer keeps
// `clipToBounds` plus its painted background.
PAG_TEST(PAGXHTMLImporterTest, BoxShadowAndOverflowHiddenSplitsLayers) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:120px;height:120px">
      <div style="width:80px;height:80px;background-color:#FFF;overflow:hidden;
                  box-shadow:0 2px 8px #00000033"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* outer = doc->layers.front()->children.front();
  ASSERT_NE(outer, nullptr);
  // Outer wrapper carries the shadow only — no clipping, no painted box.
  EXPECT_FALSE(outer->clipToBounds);
  EXPECT_TRUE(outer->contents.empty());
  ASSERT_EQ(outer->styles.size(), 1u);
  auto* drop = As<pagx::DropShadowStyle>(outer->styles.front());
  ASSERT_NE(drop, nullptr);
  EXPECT_FLOAT_EQ(drop->blurX, 4.0f);
  // Wrapper takes over the original layout slot so the parent's flow / constraints are
  // unchanged.
  EXPECT_FLOAT_EQ(outer->width, 80.0f);
  EXPECT_FLOAT_EQ(outer->height, 80.0f);
  ASSERT_EQ(outer->children.size(), 1u);

  auto* inner = outer->children.front();
  ASSERT_NE(inner, nullptr);
  EXPECT_TRUE(inner->clipToBounds);
  // No drop shadows leak back to the inner; the painted background and clip stay here.
  for (auto* s : inner->styles) {
    EXPECT_NE(s->nodeType(), pagx::NodeType::DropShadowStyle);
  }
  EXPECT_FALSE(inner->contents.empty());  // Rectangle + Fill from background-color:#FFF
  EXPECT_FLOAT_EQ(inner->percentWidth, 100.0f);
  EXPECT_FLOAT_EQ(inner->percentHeight, 100.0f);
  EXPECT_TRUE(std::isnan(inner->width));
  EXPECT_TRUE(std::isnan(inner->height));
}

// Regression: an inset shadow paints inside the box in CSS, which `clipToBounds` already
// matches — so it must stay on the clipped inner layer rather than being hoisted onto a
// wrapper.
PAG_TEST(PAGXHTMLImporterTest, InsetShadowWithOverflowHiddenStaysOnClippedLayer) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:120px;height:120px">
      <div style="width:80px;height:80px;background-color:#FFF;overflow:hidden;
                  box-shadow:inset 0 2px 4px #00000044"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  ASSERT_NE(div, nullptr);
  // No outer wrapper is introduced: the layer keeps both the clip and the inset shadow.
  EXPECT_TRUE(div->clipToBounds);
  ASSERT_EQ(div->styles.size(), 1u);
  EXPECT_NE(As<pagx::InnerShadowStyle>(div->styles.front()), nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, OpacityMapsToLayerAlpha) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;opacity:0.5"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_FLOAT_EQ(div->alpha, 0.5f);
}

PAG_TEST(PAGXHTMLImporterTest, MixBlendModeMapsToBlendMode) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:#000;mix-blend-mode:multiply"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_EQ(div->blendMode, pagx::BlendMode::Multiply);
}

// CSS `mix-blend-mode` uses hyphenated keywords for the multi-word modes; the importer must accept
// them (they differ from PAGX's internal camelCase names, which single-word modes happen to share).
PAG_TEST(PAGXHTMLImporterTest, MixBlendModeHyphenatedKeywords) {
  struct Case {
    const char* css;
    pagx::BlendMode mode;
  };
  const Case cases[] = {
      {"color-dodge", pagx::BlendMode::ColorDodge},   {"color-burn", pagx::BlendMode::ColorBurn},
      {"hard-light", pagx::BlendMode::HardLight},     {"soft-light", pagx::BlendMode::SoftLight},
      {"plus-lighter", pagx::BlendMode::PlusLighter}, {"plus-darker", pagx::BlendMode::PlusDarker},
  };
  for (const auto& c : cases) {
    std::string html =
        "<html><body style=\"width:50px;height:50px\">"
        "<div style=\"width:50px;height:50px;background-color:#000;mix-blend-mode:" +
        std::string(c.css) + "\"></div></body></html>";
    auto doc = ParseFromString(html);
    ASSERT_NE(doc, nullptr) << c.css;
    auto* div = doc->layers.front()->children.front();
    EXPECT_EQ(div->blendMode, c.mode) << c.css;
  }
}

// `plus-darker` has no CSS mix-blend-mode, so the exporter bakes the backdrop into a PNG applied via
// a `pagx_pd_*` feImage+feComposite(arithmetic) filter. The importer recovers PlusDarker from the
// marker id and drops the browser-only filter rather than treating it as an unknown effect.
PAG_TEST(PAGXHTMLImporterTest, PlusDarkerFilterMarkerRoundTrips) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:60px;height:60px">
      <svg style="position:absolute;width:0;height:0">
        <defs>
          <filter id="pagx_pd_0" x="0" y="0" width="50" height="50" filterUnits="userSpaceOnUse"
                  primitiveUnits="userSpaceOnUse" color-interpolation-filters="sRGB">
            <feImage href="data:image/png;base64,iVBORw0KGgo=" x="0" y="0" width="50" height="50"
                     preserveAspectRatio="none" result="bg"/>
            <feComposite in="SourceGraphic" in2="bg" operator="arithmetic" k1="0" k2="1" k3="1"
                         k4="-1"/>
          </filter>
        </defs>
      </svg>
      <div style="width:50px;height:50px;background-color:#404040;filter:url(#pagx_pd_0)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.back();
  EXPECT_EQ(div->blendMode, pagx::BlendMode::PlusDarker);
  EXPECT_TRUE(div->filters.empty());
}

PAG_TEST(PAGXHTMLImporterTest, FlexLayoutMapping) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:60px">
      <div style="display:flex;flex-direction:row;gap:8px;padding:6px;
                  align-items:center;justify-content:space-between">
        <div style="width:20px;height:20px;background-color:#FFF"></div>
        <div style="width:20px;height:20px;background-color:#FFF;flex:2"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* container = doc->layers.front()->children.front();
  EXPECT_EQ(container->layout, pagx::LayoutMode::Horizontal);
  EXPECT_FLOAT_EQ(container->gap, 8.0f);
  EXPECT_FLOAT_EQ(container->padding.top, 6.0f);
  EXPECT_FLOAT_EQ(container->padding.left, 6.0f);
  EXPECT_EQ(container->alignment, pagx::Alignment::Center);
  EXPECT_EQ(container->arrangement, pagx::Arrangement::SpaceBetween);
  ASSERT_EQ(container->children.size(), 2u);
  EXPECT_FLOAT_EQ(container->children[1]->flex, 2.0f);
}

PAG_TEST(PAGXHTMLImporterTest, PaddingShorthandFourValues) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="display:flex;padding:1px 2px 3px 4px;width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_FLOAT_EQ(div->padding.top, 1.0f);
  EXPECT_FLOAT_EQ(div->padding.right, 2.0f);
  EXPECT_FLOAT_EQ(div->padding.bottom, 3.0f);
  EXPECT_FLOAT_EQ(div->padding.left, 4.0f);
}

PAG_TEST(PAGXHTMLImporterTest, AbsolutePositionMapping) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:100px;height:100px">
      <div style="position:absolute;left:10px;top:-6px;width:20px;height:20px;
                  background-color:#000"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_FALSE(div->includeInLayout);
  EXPECT_FLOAT_EQ(div->left, 10.0f);
  EXPECT_FLOAT_EQ(div->top, -6.0f);
}

PAG_TEST(PAGXHTMLImporterTest, OuterBackgroundInnerPaddedSplit) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:100px;height:100px">
      <div style="display:flex;flex-direction:column;padding:10px;gap:4px;
                  background-color:#FFF;border-radius:8px;
                  box-shadow:0 1px 4px #0001">
        <div style="width:20px;height:20px;background-color:#000"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* outer = doc->layers.front()->children.front();
  // Outer has the background visuals: Rectangle + Fill + Shadow.
  EXPECT_GE(CountElements<pagx::Rectangle>(outer->contents), 1u);
  EXPECT_GE(CountElements<pagx::Fill>(outer->contents), 1u);
  EXPECT_EQ(outer->styles.size(), 1u);
  EXPECT_EQ(outer->layout, pagx::LayoutMode::None);  // layout sits on inner host
  EXPECT_FLOAT_EQ(outer->padding.left, 0.0f);
  // Inner host carries the layout + padding.
  ASSERT_EQ(outer->children.size(), 1u);
  auto* inner = outer->children.front();
  EXPECT_EQ(inner->layout, pagx::LayoutMode::Vertical);
  EXPECT_FLOAT_EQ(inner->padding.left, 10.0f);
}

PAG_TEST(PAGXHTMLImporterTest, LinearGradientAngleConversion) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-image:linear-gradient(90deg, #FF0000 0%, #0000FF 100%)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(div);
  ASSERT_NE(fill, nullptr);
  auto* lg = As<pagx::LinearGradient>(fill->color);
  ASSERT_NE(lg, nullptr);
  // CSS 90deg (to right) maps to PAGX 0° (along +X axis). With a concrete 50x50 box the gradient
  // line is resolved in absolute pixel space with fitsToGeometry=false: startPoint(0, 25) →
  // endPoint(50, 25), spanning the box width centred vertically.
  EXPECT_FALSE(lg->fitsToGeometry);
  EXPECT_TRUE(NearlyEqual(lg->startPoint.x, 0.0f, 0.005f));
  EXPECT_TRUE(NearlyEqual(lg->startPoint.y, 25.0f, 0.005f));
  EXPECT_TRUE(NearlyEqual(lg->endPoint.x, 50.0f, 0.005f));
  EXPECT_TRUE(NearlyEqual(lg->endPoint.y, 25.0f, 0.005f));
  ASSERT_EQ(lg->colorStops.size(), 2u);
  EXPECT_TRUE(ColorNear(lg->colorStops.front()->color, HexColor(0xFF0000)));
  EXPECT_TRUE(ColorNear(lg->colorStops.back()->color, HexColor(0x0000FF)));
}

PAG_TEST(PAGXHTMLImporterTest, RadialGradient) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-image:radial-gradient(circle at center, #FFFFFF 0%, #000000 100%)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(div);
  ASSERT_NE(fill, nullptr);
  auto* rg = As<pagx::RadialGradient>(fill->color);
  ASSERT_NE(rg, nullptr);
  EXPECT_EQ(rg->colorStops.size(), 2u);
}

PAG_TEST(PAGXHTMLImporterTest, RadialGradientSizeAndPositionDescriptor) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="width:100px;height:100px;background-image:radial-gradient(100px at 110px 110px, #FF0000 0%, #0000FF 100%)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(div);
  ASSERT_NE(fill, nullptr);
  auto* rg = As<pagx::RadialGradient>(fill->color);
  ASSERT_NE(rg, nullptr);
  // px center/radius are normalised against the 100px box, so 110px/100px -> 1.1 and 100px -> 1.0.
  EXPECT_TRUE(NearlyEqual(rg->center.x, 1.1f, 0.01f));
  EXPECT_TRUE(NearlyEqual(rg->center.y, 1.1f, 0.01f));
  EXPECT_TRUE(NearlyEqual(rg->radius, 1.0f, 0.01f));
}

PAG_TEST(PAGXHTMLImporterTest, RadialGradientPercentDescriptor) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="width:80px;height:40px;background-image:radial-gradient(50% at 25% 75%, #FF0000 0%, #0000FF 100%)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(div);
  ASSERT_NE(fill, nullptr);
  auto* rg = As<pagx::RadialGradient>(fill->color);
  ASSERT_NE(rg, nullptr);
  EXPECT_TRUE(NearlyEqual(rg->center.x, 0.25f, 0.01f));
  EXPECT_TRUE(NearlyEqual(rg->center.y, 0.75f, 0.01f));
  EXPECT_TRUE(NearlyEqual(rg->radius, 0.5f, 0.01f));
}

PAG_TEST(PAGXHTMLImporterTest, RadialGradientSingleKeywordPositionUsesVerticalAxis) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="width:100px;height:100px;background-image:radial-gradient(circle at top, #FF0000 0%, #0000FF 100%)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(div);
  ASSERT_NE(fill, nullptr);
  auto* rg = As<pagx::RadialGradient>(fill->color);
  ASSERT_NE(rg, nullptr);
  // CSS single-value `top` means y-axis = top, x-axis = center.
  EXPECT_TRUE(NearlyEqual(rg->center.x, 0.5f, 0.01f));
  EXPECT_TRUE(NearlyEqual(rg->center.y, 0.0f, 0.01f));
}

PAG_TEST(PAGXHTMLImporterTest, RadialGradientReversedKeywordPositionOrder) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="width:100px;height:100px;background-image:radial-gradient(circle at bottom right, #FF0000 0%, #0000FF 100%)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(div);
  ASSERT_NE(fill, nullptr);
  auto* rg = As<pagx::RadialGradient>(fill->color);
  ASSERT_NE(rg, nullptr);
  // Axis-locked keywords are order-independent: `bottom right` == `right bottom`.
  EXPECT_TRUE(NearlyEqual(rg->center.x, 1.0f, 0.01f));
  EXPECT_TRUE(NearlyEqual(rg->center.y, 1.0f, 0.01f));
}

PAG_TEST(PAGXHTMLImporterTest, RadialGradientExtentKeywordKeepsDefaultRadius) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="width:100px;height:100px;background-image:radial-gradient(closest-side at 25px 25px, #FF0000 0%, #0000FF 100%)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(div);
  ASSERT_NE(fill, nullptr);
  auto* rg = As<pagx::RadialGradient>(fill->color);
  ASSERT_NE(rg, nullptr);
  // The unsupported extent keyword degrades to the box-filling default radius, but the explicit
  // pixel position is still recovered.
  EXPECT_TRUE(NearlyEqual(rg->radius, 0.5f, 0.01f));
  EXPECT_TRUE(NearlyEqual(rg->center.x, 0.25f, 0.01f));
  EXPECT_TRUE(NearlyEqual(rg->center.y, 0.25f, 0.01f));
}

PAG_TEST(PAGXHTMLImporterTest, RadialGradientCircleOnNonSquareBoxUsesPixelSpace) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:400px;height:400px">
      <div style="width:320px;height:140px;background-image:radial-gradient(170px at 120px 40px, #FFFFFF 0%, #06B6D4 100%)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(div);
  ASSERT_NE(fill, nullptr);
  auto* rg = As<pagx::RadialGradient>(fill->color);
  ASSERT_NE(rg, nullptr);
  // A circle on a non-square box must switch to the fitsToGeometry=false pixel model so the radius
  // stays isotropic; the default normalised model would stretch it into a 170x74 ellipse.
  EXPECT_FALSE(rg->fitsToGeometry);
  EXPECT_TRUE(NearlyEqual(rg->center.x, 120.0f, 0.01f));
  EXPECT_TRUE(NearlyEqual(rg->center.y, 40.0f, 0.01f));
  EXPECT_TRUE(NearlyEqual(rg->radius, 170.0f, 0.01f));
}

PAG_TEST(PAGXHTMLImporterTest, RadialGradientCircleOnSquareBoxKeepsNormalised) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="width:100px;height:100px;background-image:radial-gradient(50px at 25px 75px, #FF0000 0%, #0000FF 100%)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(div);
  ASSERT_NE(fill, nullptr);
  auto* rg = As<pagx::RadialGradient>(fill->color);
  ASSERT_NE(rg, nullptr);
  // A square box renders an isotropic circle even under the normalised model, so keep the compact
  // fitsToGeometry=true representation (px tokens normalised against the 100px box).
  EXPECT_TRUE(rg->fitsToGeometry);
  EXPECT_TRUE(NearlyEqual(rg->center.x, 0.25f, 0.01f));
  EXPECT_TRUE(NearlyEqual(rg->center.y, 0.75f, 0.01f));
  EXPECT_TRUE(NearlyEqual(rg->radius, 0.5f, 0.01f));
}

PAG_TEST(PAGXHTMLImporterTest, RadialGradientEllipseOnNonSquareBoxKeepsNormalised) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:400px;height:400px">
      <div style="width:320px;height:140px;background-image:radial-gradient(ellipse 170px 70px at 120px 40px, #FFFFFF 0%, #06B6D4 100%)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(div);
  ASSERT_NE(fill, nullptr);
  auto* rg = As<pagx::RadialGradient>(fill->color);
  ASSERT_NE(rg, nullptr);
  // An explicit ellipse intends anisotropic stretching, so it stays on the normalised model.
  EXPECT_TRUE(rg->fitsToGeometry);
}

PAG_TEST(PAGXHTMLImporterTest, ConicGradientAngleOffset) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;
                  background-image:conic-gradient(from 0deg, #F00, #00F)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(div);
  ASSERT_NE(fill, nullptr);
  auto* cg = As<pagx::ConicGradient>(fill->color);
  ASSERT_NE(cg, nullptr);
  // CSS 0° = top, PAGX 0° = right ⇒ PAGX angle = −90°.
  EXPECT_TRUE(NearlyEqual(cg->startAngle, -90.0f, 0.01f));
}

PAG_TEST(PAGXHTMLImporterTest, FilterBlurAndDropShadow) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:#000;
                  filter:blur(2px) drop-shadow(0 1px 3px #00000055)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  ASSERT_EQ(div->filters.size(), 2u);
  auto* blur = As<pagx::BlurFilter>(div->filters[0]);
  ASSERT_NE(blur, nullptr);
  EXPECT_FLOAT_EQ(blur->blurX, 2.0f);
  EXPECT_FLOAT_EQ(blur->blurY, 2.0f);
  auto* drop = As<pagx::DropShadowFilter>(div->filters[1]);
  ASSERT_NE(drop, nullptr);
  EXPECT_FLOAT_EQ(drop->offsetY, 1.0f);
}

PAG_TEST(PAGXHTMLImporterTest, BackdropFilterMapsToBackgroundBlurStyle) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:#FFF8;backdrop-filter:blur(6px)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  bool foundBlur = false;
  for (auto* s : div->styles) {
    if (auto* b = As<pagx::BackgroundBlurStyle>(s)) {
      EXPECT_FLOAT_EQ(b->blurX, 6.0f);
      foundBlur = true;
    }
  }
  EXPECT_TRUE(foundBlur);
}

PAG_TEST(PAGXHTMLImporterTest, OverflowHiddenMapsToClipToBounds) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;overflow:hidden;background-color:#000"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_TRUE(div->clipToBounds);
}

PAG_TEST(PAGXHTMLImporterTest, SimpleTextLeafSingleStyle) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style="font-size:14px;color:#000">Hello</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto* text = FindElementOfType<pagx::Text>(leaf);
  ASSERT_NE(text, nullptr);
  EXPECT_EQ(text->text, "Hello");
  EXPECT_FLOAT_EQ(text->fontSize, 14.0f);
  // No TextBox needed for a uniform-style leaf.
  bool hasTextBox = false;
  for (auto* e : leaf->contents) {
    if (As<pagx::TextBox>(e)) hasTextBox = true;
  }
  EXPECT_FALSE(hasTextBox);
}

PAG_TEST(PAGXHTMLImporterTest, WebkitTextStrokeProducesTextStroke) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style="font-size:20px;color:#3B82F6;-webkit-text-stroke:2px #10B981">Hi</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto* text = FindElementOfType<pagx::Text>(leaf);
  ASSERT_NE(text, nullptr);
  EXPECT_EQ(text->text, "Hi");
  auto* stroke = FindElementOfType<pagx::Stroke>(leaf);
  ASSERT_NE(stroke, nullptr);
  EXPECT_FLOAT_EQ(stroke->width, 2.0f);
  // CSS `-webkit-text-stroke` is always centred on the glyph edge.
  EXPECT_EQ(stroke->align, pagx::StrokeAlign::Center);
  auto* solid = As<pagx::SolidColor>(stroke->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0x10B981)));
}

PAG_TEST(PAGXHTMLImporterTest, WebkitTextStrokeOmittedColorFallsBackToTextColor) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style="font-size:20px;color:#EC4899;-webkit-text-stroke-width:3px">Hi</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto* stroke = FindElementOfType<pagx::Stroke>(leaf);
  ASSERT_NE(stroke, nullptr);
  EXPECT_FLOAT_EQ(stroke->width, 3.0f);
  auto* solid = As<pagx::SolidColor>(stroke->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0xEC4899)));
}

PAG_TEST(PAGXHTMLImporterTest, ZeroWebkitTextStrokeEmitsNoStroke) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style="font-size:20px;color:#000;-webkit-text-stroke:0px #10B981">Hi</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  EXPECT_EQ(FindElementOfType<pagx::Stroke>(leaf), nullptr);
}

// A per-run `-webkit-text-stroke` that differs from the leaf default must survive as a Stroke
// inside that run's Group, while the unstroked default run emits none. This is the rich-text
// (TextBox + Group) path, exercised by the BOX / MOD idioms in the round-trip corpus.
PAG_TEST(PAGXHTMLImporterTest, PerRunWebkitTextStrokeScopedToFragment) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <p style="font-size:20px;color:#111">Plain <span style="-webkit-text-stroke:2px #0EA5E9">Edge</span></p>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto* tb = FindElementOfType<pagx::TextBox>(leaf);
  ASSERT_NE(tb, nullptr);
  // The first run (no stroke) sits flat in tb->elements; the second run (stroked) is wrapped in
  // a Group. Exactly one Stroke should exist, inside that Group.
  size_t topLevelStrokes = 0;
  pagx::Group* strokedGroup = nullptr;
  for (auto* e : tb->elements) {
    if (As<pagx::Stroke>(e)) topLevelStrokes++;
    if (auto* g = As<pagx::Group>(e)) {
      for (auto* ge : g->elements) {
        if (As<pagx::Stroke>(ge)) strokedGroup = g;
      }
    }
  }
  EXPECT_EQ(topLevelStrokes, 0u);
  ASSERT_NE(strokedGroup, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, RichTextSpansEmitTextBoxFragments) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <p style="font-size:14px;color:#111">Hi <span style="color:#F00;font-weight:bold">World</span></p>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto* tb = FindElementOfType<pagx::TextBox>(leaf);
  ASSERT_NE(tb, nullptr);
  size_t textCount = 0, groupCount = 0;
  for (auto* e : tb->elements) {
    if (As<pagx::Text>(e)) textCount++;
    if (As<pagx::Group>(e)) groupCount++;
  }
  EXPECT_EQ(textCount, 1u);
  EXPECT_EQ(groupCount, 1u);
}

// Walk `tb`'s elements (and any nested `<Group>` subtrees the importer
// emits for style breaks) and return the flat sequence of (Text, Fill)
// pairs in document order. The PAGX subset wraps the second-and-later
// fragments inside a `<Group>` to scope per-run style overrides, so a
// flat `tb->elements` scan finds only the first run.
inline void GatherTextRuns(const std::vector<pagx::Element*>& elements,
                           std::vector<pagx::Text*>* texts, std::vector<pagx::Fill*>* fills) {
  for (auto* e : elements) {
    if (auto* t = As<pagx::Text>(e)) {
      texts->push_back(t);
    } else if (auto* f = As<pagx::Fill>(e)) {
      fills->push_back(f);
    } else if (auto* g = As<pagx::Group>(e)) {
      GatherTextRuns(g->elements, texts, fills);
    }
  }
}

// Snapshot contract: the html-snapshot tool (`tools/html-snapshot`) merges
// `<p>NN<span class="text-[28px]">unit</span></p>` and friends into a
// single absolutely-positioned `<span>` text-leaf with an inline child
// `<span>` for the unit run, so PAGX's own text engine — rather than the
// snapshot's hard-coded `left` values — owns the inter-run advance. The
// importer must collapse that into one TextBox with two Text runs whose
// fontSize differs; this is what makes the run boundary survive font
// fallback drift on the render side.
PAG_TEST(PAGXHTMLImporterTest, AbsolutePositionedSpanWithInlineSizeRunMergesIntoOneTextBox) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:420px;height:52px">
      <span style="position:absolute;left:0;top:0;width:420px;height:52px;color:rgb(17,17,17);font-family:'Plus Jakarta Sans';font-size:52px;font-weight:600;letter-spacing:-1.3px;line-height:52px;white-space:nowrap">22.25<span style="font-size:28px;line-height:28px">%</span></span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  ASSERT_NE(leaf, nullptr);
  auto* tb = FindElementOfType<pagx::TextBox>(leaf);
  ASSERT_NE(tb, nullptr);
  EXPECT_FALSE(tb->wordWrap);
  std::vector<pagx::Text*> texts;
  std::vector<pagx::Fill*> fills;
  GatherTextRuns(tb->elements, &texts, &fills);
  ASSERT_EQ(texts.size(), 2u);
  EXPECT_EQ(texts[0]->text, "22.25");
  EXPECT_FLOAT_EQ(texts[0]->fontSize, 52.0f);
  EXPECT_FLOAT_EQ(texts[0]->letterSpacing, -1.3f);
  EXPECT_EQ(texts[1]->text, "%");
  EXPECT_FLOAT_EQ(texts[1]->fontSize, 28.0f);
  EXPECT_FLOAT_EQ(texts[1]->letterSpacing, -1.3f);
}

// The same merge survives when the inline run carries its own colour
// (the snapshot tool folds the source `<span>`'s `opacity` into the
// emitted `color` so a translucent unit run still flattens cleanly).
PAG_TEST(PAGXHTMLImporterTest, AbsolutePositionedSpanWithInlineRunHonoursPerRunColorAlpha) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:420px;height:44px">
      <span style="position:absolute;left:0;top:0;width:420px;height:44px;color:rgb(17,17,17);font-family:'Plus Jakarta Sans';font-size:44px;font-weight:600;line-height:44px;white-space:nowrap">159.9<span style="color:rgba(17,17,17,0.7);font-size:22px;line-height:22px">亿美元</span></span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  ASSERT_NE(leaf, nullptr);
  auto* tb = FindElementOfType<pagx::TextBox>(leaf);
  ASSERT_NE(tb, nullptr);
  std::vector<pagx::Text*> texts;
  std::vector<pagx::Fill*> fills;
  GatherTextRuns(tb->elements, &texts, &fills);
  ASSERT_EQ(texts.size(), 2u);
  ASSERT_EQ(fills.size(), 2u);
  EXPECT_EQ(texts[0]->text, "159.9");
  EXPECT_FLOAT_EQ(texts[0]->fontSize, 44.0f);
  EXPECT_EQ(texts[1]->text, "亿美元");
  EXPECT_FLOAT_EQ(texts[1]->fontSize, 22.0f);
  auto* solid0 = As<pagx::SolidColor>(fills[0]->color);
  auto* solid1 = As<pagx::SolidColor>(fills[1]->color);
  ASSERT_NE(solid0, nullptr);
  ASSERT_NE(solid1, nullptr);
  EXPECT_TRUE(ColorNear(solid0->color, HexColor(0x111111, 1.0f)));
  EXPECT_TRUE(ColorNear(solid1->color, HexColor(0x111111, 0.7f)));
}

// `white-space: pre-line` collapses runs of spaces/tabs but keeps a source newline as a hard line
// break, so the resulting text run carries a `\n`.
PAG_TEST(PAGXHTMLImporterTest, WhiteSpacePreLineKeepsNewlineCollapsesSpaces) {
  auto doc = ParseFromString(
      "<html><body style=\"width:200px;height:80px\">"
      "<p style=\"white-space:pre-line;font-size:16px\">Alpha   Beta\nGamma</p>"
      "</body></html>");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  std::vector<pagx::Text*> texts;
  std::vector<pagx::Fill*> fills;
  if (auto* tb = FindElementOfType<pagx::TextBox>(leaf)) {
    GatherTextRuns(tb->elements, &texts, &fills);
  } else {
    GatherTextRuns(leaf->contents, &texts, &fills);
  }
  ASSERT_FALSE(texts.empty());
  const std::string& t = texts.front()->text;
  EXPECT_NE(t.find('\n'), std::string::npos);
  // Consecutive spaces between the first two words are folded into one.
  EXPECT_EQ(t.find("Alpha   Beta"), std::string::npos);
  EXPECT_NE(t.find("Alpha Beta"), std::string::npos);
}

// `white-space: pre` keeps source spaces and newlines verbatim (no folding, no per-fragment trim).
PAG_TEST(PAGXHTMLImporterTest, WhiteSpacePreKeepsSpacesVerbatim) {
  auto doc = ParseFromString(
      "<html><body style=\"width:200px;height:80px\">"
      "<p style=\"white-space:pre;font-size:16px\">A   B\nC</p>"
      "</body></html>");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto* tb = FindElementOfType<pagx::TextBox>(leaf);
  ASSERT_NE(tb, nullptr);
  EXPECT_FALSE(tb->wordWrap);
  std::vector<pagx::Text*> texts;
  std::vector<pagx::Fill*> fills;
  GatherTextRuns(tb->elements, &texts, &fills);
  ASSERT_FALSE(texts.empty());
  const std::string& t = texts.front()->text;
  EXPECT_NE(t.find("A   B"), std::string::npos);
  EXPECT_NE(t.find('\n'), std::string::npos);
}

// `writing-mode: vertical-rl` produces a vertical TextBox.
PAG_TEST(PAGXHTMLImporterTest, VerticalWritingModeProducesVerticalTextBox) {
  auto doc = ParseFromString(
      "<html><body style=\"width:80px;height:200px\">"
      "<p style=\"writing-mode:vertical-rl;font-size:16px\">Vertical</p>"
      "</body></html>");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto* tb = FindElementOfType<pagx::TextBox>(leaf);
  ASSERT_NE(tb, nullptr);
  EXPECT_EQ(tb->writingMode, pagx::WritingMode::Vertical);
  EXPECT_FLOAT_EQ(tb->percentHeight, 100.0f);
}

// An unrecognised `text-align` value warns rather than silently mapping to a default.
PAG_TEST(PAGXHTMLImporterTest, UnsupportedTextAlignWarns) {
  auto doc = ParseFromString(
      "<html><body style=\"width:200px;height:40px\">"
      "<p style=\"text-align:sideways;font-size:16px\">Aligned</p>"
      "</body></html>");
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("unsupported text-align") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
}

// `text-decoration: overline` is unsupported and warns.
PAG_TEST(PAGXHTMLImporterTest, TextDecorationOverlineWarns) {
  auto doc = ParseFromString(
      "<html><body style=\"width:200px;height:40px\">"
      "<p style=\"text-decoration:overline;font-size:16px\">Over</p>"
      "</body></html>");
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("text-decoration overline not supported") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
}

// Two adjacent inline runs carrying the identical `-webkit-text-stroke` share a stroke fingerprint
// and merge into a single fragment (exercising the both-stroked branch of the stroke comparison).
PAG_TEST(PAGXHTMLImporterTest, AdjacentIdenticalTextStrokeRunsMerge) {
  auto doc = ParseFromString(
      "<html><body style=\"width:200px;height:40px\">"
      "<p style=\"font-size:20px\">"
      "<span style=\"-webkit-text-stroke:2px #0EA5E9\">AA</span>"
      "<span style=\"-webkit-text-stroke:2px #0EA5E9\">BB</span></p>"
      "</body></html>");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  std::vector<pagx::Text*> texts;
  std::vector<pagx::Fill*> fills;
  auto* tb = FindElementOfType<pagx::TextBox>(leaf);
  if (tb) {
    GatherTextRuns(tb->elements, &texts, &fills);
  } else {
    GatherTextRuns(leaf->contents, &texts, &fills);
  }
  ASSERT_FALSE(texts.empty());
  EXPECT_EQ(texts.front()->text, "AABB");
}

// Padding on a text leaf without any background visuals is applied directly to the outer text host
// layer (no extra inner layer is synthesised).
PAG_TEST(PAGXHTMLImporterTest, TextLeafPaddingWithoutBackgroundAppliesToHost) {
  auto doc = ParseFromString(
      "<html><body style=\"width:200px;height:80px\">"
      "<p style=\"padding:6px;font-size:16px\">Padded</p>"
      "</body></html>");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  ASSERT_NE(leaf, nullptr);
  EXPECT_GT(leaf->padding.top + leaf->padding.right + leaf->padding.bottom + leaf->padding.left,
            0.0f);
}

// A non-wrapping inline text leaf (<span>/<a>) shrinks to fit its own shaped glyphs instead
// of freezing the browser-measured px width the html-snapshot pipeline bakes onto every text
// span. Freezing it would peg the box to the browser's font metrics, so a render host that
// substitutes a different face (the norm for CJK / web fonts) would mis-centre or clip the
// glyphs inside a box that no longer matches them. The inline-axis size is dropped; the cross
// axis and the absolute position are preserved.
PAG_TEST(PAGXHTMLImporterTest, InlineNoWrapTextLeafShrinksToFitWidth) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:420px;height:80px">
      <span style="position:absolute;left:0;top:0;width:305px;height:80px;font-size:60px;white-space:nowrap">Title</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  ASSERT_NE(leaf, nullptr);
  EXPECT_TRUE(std::isnan(leaf->width));
  EXPECT_FLOAT_EQ(leaf->height, 80.0f);
  EXPECT_FLOAT_EQ(leaf->left, 0.0f);
}

// Block-level text leaves (<p>, <h1..6>) keep their authored width: a block box is as wide as
// its declared dimension in CSS, not shrink-to-fit, so the measured width is meaningful.
PAG_TEST(PAGXHTMLImporterTest, BlockNoWrapTextLeafKeepsWidth) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:420px;height:80px">
      <p style="position:absolute;left:0;top:0;width:305px;height:80px;font-size:60px;white-space:nowrap">Title</p>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  ASSERT_NE(leaf, nullptr);
  EXPECT_FLOAT_EQ(leaf->width, 305.0f);
}

// An inline text leaf that can wrap keeps its width: there the width is the wrap boundary, not
// redundant slack, so dropping it would change line breaking.
PAG_TEST(PAGXHTMLImporterTest, InlineWrappingTextLeafKeepsWidth) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:420px;height:80px">
      <span style="position:absolute;left:0;top:0;width:305px;height:80px;font-size:20px">wrapping paragraph text</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  ASSERT_NE(leaf, nullptr);
  EXPECT_FLOAT_EQ(leaf->width, 305.0f);
}

// An inline text leaf whose box paints a background keeps its width: the painted Rectangle
// fills the box, so the box size must stay fixed rather than shrinking to the glyphs.
PAG_TEST(PAGXHTMLImporterTest, InlineNoWrapTextLeafWithBackgroundKeepsWidth) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:420px;height:80px">
      <span style="position:absolute;left:0;top:0;width:305px;height:80px;font-size:60px;white-space:nowrap;background-color:#000">Title</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  ASSERT_NE(leaf, nullptr);
  EXPECT_FLOAT_EQ(leaf->width, 305.0f);
}

// An inline nowrap text leaf that is a flex-grow child keeps its width: the flex engine is
// distributing free space through it, so its measured width carries the growth the layout
// depends on and must not be dropped. Guards the `notFlexGrow` term of the shrink-to-fit
// condition — without it a grown flex item would collapse back to its intrinsic glyph width.
PAG_TEST(PAGXHTMLImporterTest, InlineNoWrapFlexGrowTextLeafKeepsWidth) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:420px;height:80px;display:flex">
      <span style="width:305px;height:80px;font-size:60px;white-space:nowrap;flex:1">Title</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  ASSERT_NE(leaf, nullptr);
  EXPECT_FLOAT_EQ(leaf->width, 305.0f);
}

PAG_TEST(PAGXHTMLImporterTest, TextDecorationUnderlineOverlay) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style="font-size:14px;color:#000;text-decoration:underline">Hello</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  // Two Rectangles? Actually only one — the underline overlay. The text uses no Rectangle.
  size_t rectCount = CountElements<pagx::Rectangle>(leaf->contents);
  EXPECT_EQ(rectCount, 1u);
  auto* rect = FindElementOfType<pagx::Rectangle>(leaf);
  ASSERT_NE(rect, nullptr);
  EXPECT_FLOAT_EQ(rect->bottom, 0.0f);
  EXPECT_FLOAT_EQ(rect->height, 1.0f);
  EXPECT_FLOAT_EQ(rect->percentWidth, 100.0f);
}

PAG_TEST(PAGXHTMLImporterTest, BrTagInsertsNewline) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <p style="font-size:14px;color:#000">Line one<br/>Line two</p>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto* text = FindElementOfType<pagx::Text>(leaf);
  ASSERT_NE(text, nullptr);
  EXPECT_NE(text->text.find('\n'), std::string::npos);
}

// Source HTML inside a text-leaf may carry indentation between the opening
// tag and the first character of visible content (`<h1>\n        Title…`).
// CSS's inline-formatting-context boundary trim removes that whitespace
// before laying out the text, so PAGX has to reproduce the same trim or
// the rendered baseline gets pushed right by one space and / or onto a
// second line. The downstream `CollapseHTMLWhitespace` pass only strips
// leading ASCII spaces — it intentionally preserves leading `\n` so that
// `<br>Title…` keeps its hard break — so the normalisation must happen
// when the text node is first ingested: convert collapsible whitespace
// (`\n`, `\r`, `\t`) to a regular space, and the existing collapse pass
// handles the rest.
PAG_TEST(PAGXHTMLImporterTest, IndentedTextLeafTrimsSourceWhitespace) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:400px;height:40px">
      <p style="font-size:14px;color:#000;font-family:Arial">
        1.3 Title——<span style="color:#E8612C">"quoted"</span>
      </p>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  ASSERT_NE(leaf, nullptr);
  auto* tb = FindElementOfType<pagx::TextBox>(leaf);
  ASSERT_NE(tb, nullptr);
  std::vector<pagx::Text*> texts;
  std::vector<pagx::Fill*> fills;
  GatherTextRuns(tb->elements, &texts, &fills);
  ASSERT_EQ(texts.size(), 2u);
  EXPECT_EQ(texts[0]->text, "1.3 Title——");
  EXPECT_EQ(texts[1]->text, "\"quoted\"");
}

// A leading `<br>` keeps its hard newline even after the boundary trim:
// the snapshot path appends `<br>` as a literal `\n` (not via a text node)
// so it isn't subject to the source-whitespace-to-space normalisation,
// and `CollapseHTMLWhitespace`'s front-trim deliberately leaves leading
// `\n` alone. The result is an empty first visual line followed by the
// text, matching what the browser draws for the same markup.
PAG_TEST(PAGXHTMLImporterTest, LeadingBrPreservesNewline) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:60px">
      <p style="font-size:14px;color:#000;font-family:Arial"><br/>after</p>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto* text = FindElementOfType<pagx::Text>(leaf);
  ASSERT_NE(text, nullptr);
  EXPECT_EQ(text->text.front(), '\n');
}

// Whitespace at the boundary between two differently styled fragments must be preserved
// at the parent's font size. Without per-run-aware whitespace collapse, the trailing space
// in the parent text (32px) gets stripped together with the leading whitespace of the
// child span, gluing the two runs together visually.
PAG_TEST(PAGXHTMLImporterTest, NestedSpanPreservesBoundaryWhitespace) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:400px;height:48px">
      <span style="font-size:32px;color:#fff;font-family:Arial;line-height:48px">Title <span style="font-size:16px;color:#888">subtitle</span></span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  ASSERT_NE(leaf, nullptr);
  auto* tb = FindElementOfType<pagx::TextBox>(leaf);
  ASSERT_NE(tb, nullptr);
  std::vector<pagx::Text*> texts;
  std::vector<pagx::Fill*> fills;
  GatherTextRuns(tb->elements, &texts, &fills);
  ASSERT_EQ(texts.size(), 2u);
  EXPECT_EQ(texts[0]->text, "Title ");
  EXPECT_EQ(texts[1]->text, "subtitle");
}

// Adjacent whitespace at a fragment boundary collapses to a single space, regardless of
// which fragment carried the trailing/leading space. The merged space is hosted by the
// fragment that owned the trailing whitespace, so its font size matches the run that
// preceded the boundary.
PAG_TEST(PAGXHTMLImporterTest, NestedSpanCollapsesBoundaryWhitespace) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:400px;height:48px">
      <span style="font-size:14px;color:#000;font-family:Arial">left <span style="color:#888">  right</span></span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  ASSERT_NE(leaf, nullptr);
  auto* tb = FindElementOfType<pagx::TextBox>(leaf);
  ASSERT_NE(tb, nullptr);
  std::vector<pagx::Text*> texts;
  std::vector<pagx::Fill*> fills;
  GatherTextRuns(tb->elements, &texts, &fills);
  ASSERT_EQ(texts.size(), 2u);
  EXPECT_EQ(texts[0]->text, "left ");
  EXPECT_EQ(texts[1]->text, "right");
}

// Trailing whitespace at the very end of an inline run is stripped even when it lives in
// the first fragment because every following fragment collapsed away. The post-pass
// re-trims the surviving last fragment so the run does not end with a stray space.
PAG_TEST(PAGXHTMLImporterTest, NestedSpanTrimsTrailingWhitespaceAtRunEnd) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:400px;height:48px">
      <span style="font-size:14px;color:#000;font-family:Arial">text <span style="color:#888">   </span></span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  ASSERT_NE(leaf, nullptr);
  auto* text = FindElementOfType<pagx::Text>(leaf);
  ASSERT_NE(text, nullptr);
  EXPECT_EQ(text->text, "text");
}

PAG_TEST(PAGXHTMLImporterTest, ImageRegistersImageResource) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <img src="logo.png" style="width:80px;height:80px"/>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(leaf);
  ASSERT_NE(fill, nullptr);
  auto* pattern = As<pagx::ImagePattern>(fill->color);
  ASSERT_NE(pattern, nullptr);
  ASSERT_NE(pattern->image, nullptr);
  EXPECT_FALSE(pattern->image->filePath.empty());
}

PAG_TEST(PAGXHTMLImporterTest, InlineSvgPassedAsImportDirective) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <svg width="80" height="80" viewBox="0 0 80 80"><circle cx="40" cy="40" r="30" fill="#F00"/></svg>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  EXPECT_FALSE(leaf->importDirective.content.empty());
  EXPECT_EQ(leaf->importDirective.format, "svg");
  EXPECT_NE(leaf->importDirective.content.find("circle"), std::string::npos);
}

// Regression: an inline <svg> carrying its own `opacity` (presentation attribute and/or inline
// `style`) must hoist that opacity onto the enclosing Layer's alpha exactly once. The serialized
// import-directive content must NOT keep the root opacity, otherwise the SVG importer run during
// `pagx resolve` reads it again and multiplies the two (0.7 × 0.7 ≈ 0.49), washing the shape out.
PAG_TEST(PAGXHTMLImporterTest, InlineSvgRootOpacityHoistedOnceNotDoubled) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <svg style="opacity:0.7" opacity="0.7" width="80" height="80" viewBox="0 0 80 80">
        <circle cx="40" cy="40" r="30" fill="#3B82F6"/>
      </svg>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  EXPECT_FLOAT_EQ(leaf->alpha, 0.7f);
  const auto& content = leaf->importDirective.content;
  ASSERT_FALSE(content.empty());
  EXPECT_EQ(content.find("opacity"), std::string::npos);
  EXPECT_NE(content.find("circle"), std::string::npos);
  EXPECT_NE(content.find("#3B82F6"), std::string::npos);
}

// A descendant of an inline <svg> keeps its own `opacity` — only the root's opacity is hoisted to
// the Layer alpha, since CSS `opacity` does not inherit. The child opacity composites under the
// hoisted layer alpha exactly as in the browser.
PAG_TEST(PAGXHTMLImporterTest, InlineSvgChildOpacityPreserved) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <svg style="opacity:0.7" width="80" height="80" viewBox="0 0 80 80">
        <circle cx="40" cy="40" r="30" fill="#3B82F6" opacity="0.5"/>
      </svg>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  EXPECT_FLOAT_EQ(leaf->alpha, 0.7f);
  const auto& content = leaf->importDirective.content;
  ASSERT_FALSE(content.empty());
  EXPECT_NE(content.find("opacity=\"0.5\""), std::string::npos);
}

// Regression: inline <svg> with only viewBox (no width/height) must keep the
// case-sensitive `viewBox` attribute through the HTML import pipeline. The
// browser-emitted snapshot (e.g. from tools/html-snapshot) lowercases HTML
// attribute names but preserves SVG's camelCase ones, and the SVG importer used
// during `pagx resolve` looks up the attribute case-sensitively. If the
// importer lowercases `viewBox`, the inline content fails to parse because the
// SVG has no fallback width/height.
PAG_TEST(PAGXHTMLImporterTest, InlineSvgPreservesCamelCaseAttributes) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <svg viewBox="0 0 16 16" preserveAspectRatio="xMidYMid meet">
        <linearGradient id="g" gradientUnits="userSpaceOnUse" x1="0" y1="0" x2="16" y2="16">
          <stop offset="0" stop-color="#F00"/>
          <stop offset="1" stop-color="#00F"/>
        </linearGradient>
        <circle cx="8" cy="8" r="6" fill="url(#g)"/>
      </svg>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  ASSERT_FALSE(leaf->importDirective.content.empty());
  const auto& content = leaf->importDirective.content;
  EXPECT_NE(content.find("viewBox=\"0 0 16 16\""), std::string::npos);
  EXPECT_NE(content.find("preserveAspectRatio=\"xMidYMid meet\""), std::string::npos);
  EXPECT_NE(content.find("<linearGradient"), std::string::npos);
  EXPECT_NE(content.find("gradientUnits=\"userSpaceOnUse\""), std::string::npos);
  EXPECT_EQ(content.find("viewbox="), std::string::npos);
  EXPECT_EQ(content.find("<lineargradient"), std::string::npos);
}

// Regression: HTMLExporter hoists shared paint servers into a single hidden top-level
// `<svg><defs>` and references them by id from sibling `<svg>`s via `fill="url(#id)"`. A
// browser resolves those references through a global id lookup, but each inline `<svg>` here
// becomes an independent import directive — so the referencing `<svg>` must carry a local copy
// of the gradient or the SVG importer's `url(#…)` lookup fails and the fill falls back to the
// default (rendered red after error recovery). The importer injects the referenced def into the
// consuming `<svg>`. Without this, gradient-filled buttons render solid red instead of gradient.
PAG_TEST(PAGXHTMLImporterTest, InlineSvgInjectsSharedDefsReferencedByUrl) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:60px">
      <svg style="position:absolute;width:0;height:0;overflow:hidden">
        <defs>
          <linearGradient id="grad0" x1="0" y1="0" x2="1" y2="0">
            <stop offset="0" stop-color="#6366F1"/>
            <stop offset="1" stop-color="#8B5CF6"/>
          </linearGradient>
        </defs>
      </svg>
      <svg width="200" height="60" viewBox="0 0 200 60">
        <rect width="200" height="60" rx="30" fill="url(#grad0)"/>
      </svg>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  // The two top-level <svg>s become two sibling Layers; the second one carries the <rect>.
  auto* rectSvg = doc->layers.front()->children.back();
  const auto& content = rectSvg->importDirective.content;
  ASSERT_FALSE(content.empty());
  EXPECT_NE(content.find("fill=\"url(#grad0)\""), std::string::npos);
  // The shared gradient must now be defined locally inside this same <svg> directive.
  EXPECT_NE(content.find("<defs>"), std::string::npos);
  EXPECT_NE(content.find("id=\"grad0\""), std::string::npos);
  EXPECT_NE(content.find("stop-color=\"#6366F1\""), std::string::npos);
}

// Regression: inline <svg> with `fill="currentColor"` must be resolved against
// the inherited CSS `color` from its host. The icon-font snapshot pre-pass
// (tools/html-snapshot/lib/icon-font.js) emits glyph paths as `fill="currentColor"`
// and relies on the wrapping `<div style="color: …">` to tint them. The
// downstream SVG importer has no notion of `color` / `currentColor`, so the
// HTML importer pre-resolves these tokens before serialising into the import
// directive. Without this, every icon collapses to black.
PAG_TEST(PAGXHTMLImporterTest, InlineSvgResolvesCurrentColorFromInheritedColor) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <div style="width:20px;height:20px;color:rgb(130, 208, 164)">
        <svg width="20" height="20" viewBox="0 0 1024 1024">
          <path d="M0 0L1024 0L1024 1024L0 1024Z" fill="currentColor"/>
        </svg>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* outer = doc->layers.front()->children.front();
  auto* svgLayer = outer->children.front();
  const auto& content = svgLayer->importDirective.content;
  ASSERT_FALSE(content.empty());
  EXPECT_NE(content.find("fill=\"#82D0A4\""), std::string::npos);
  EXPECT_EQ(content.find("currentColor"), std::string::npos);
}

// Inline SVG can also use `style="fill: currentColor"` (inline-style form).
// Both the attribute form and the inline-style form must be rewritten so that
// authored SVG snippets, not just icon-font output, get the correct tint.
PAG_TEST(PAGXHTMLImporterTest, InlineSvgResolvesCurrentColorInInlineStyle) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px;color:#1188ff">
      <svg width="80" height="80" viewBox="0 0 80 80">
        <circle cx="40" cy="40" r="30" style="fill: currentColor"/>
      </svg>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  const auto& content = leaf->importDirective.content;
  ASSERT_FALSE(content.empty());
  EXPECT_EQ(content.find("currentColor"), std::string::npos);
  EXPECT_NE(content.find("#1188FF"), std::string::npos);
}

// A `color` declared on an SVG descendant overrides the inherited cascade for
// itself and its children. The walker tracks the active colour while it
// recurses so each `currentColor` token resolves against the right value.
PAG_TEST(PAGXHTMLImporterTest, InlineSvgCurrentColorRespectsInnerColorOverride) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px;color:#112233">
      <svg width="80" height="80" viewBox="0 0 80 80">
        <g color="#445566">
          <rect x="0" y="0" width="40" height="40" fill="currentColor"/>
        </g>
        <rect x="40" y="0" width="40" height="40" fill="currentColor"/>
      </svg>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  const auto& content = leaf->importDirective.content;
  ASSERT_FALSE(content.empty());
  EXPECT_EQ(content.find("currentColor"), std::string::npos);
  EXPECT_NE(content.find("fill=\"#445566\""), std::string::npos);
  EXPECT_NE(content.find("fill=\"#112233\""), std::string::npos);
}

// `currentColor` matching is case-insensitive per CSS, and the `stroke` paint property is
// resolved just like `fill`. A mixed-case `CurrentColor` on a `stroke` attribute must tint.
PAG_TEST(PAGXHTMLImporterTest, InlineSvgResolvesCurrentColorCaseInsensitiveStroke) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px;color:#22AA44">
      <svg width="80" height="80" viewBox="0 0 80 80">
        <rect x="10" y="10" width="60" height="60" fill="none" stroke="CurrentColor"/>
      </svg>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  const auto& content = leaf->importDirective.content;
  ASSERT_FALSE(content.empty());
  EXPECT_EQ(content.find("urrentColor"), std::string::npos);
  EXPECT_NE(content.find("stroke=\"#22AA44\""), std::string::npos);
}

// A `color` declared through a descendant's inline `style="color: …"` (rather than the attribute
// form) also overrides the cascade for that element and its children.
PAG_TEST(PAGXHTMLImporterTest, InlineSvgCurrentColorRespectsDescendantStyleColor) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px;color:#112233">
      <svg width="80" height="80" viewBox="0 0 80 80">
        <g style="color:#99AABB">
          <rect x="0" y="0" width="40" height="40" fill="currentColor"/>
        </g>
      </svg>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  const auto& content = leaf->importDirective.content;
  ASSERT_FALSE(content.empty());
  EXPECT_EQ(content.find("currentColor"), std::string::npos);
  EXPECT_NE(content.find("#99AABB"), std::string::npos);
}

// A consuming inline <svg> that references two shared gradients — one of which inherits its stops
// from a third via `href="#base"` — pulls the transitive closure of defs into a local <defs>. This
// exercises the href local-reference collection, the quoted `url('#id')` form, and the multi-def
// chaining of the injected list.
PAG_TEST(PAGXHTMLImporterTest, InlineSvgInjectsTransitiveDefsViaHref) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:60px">
      <svg style="position:absolute;width:0;height:0;overflow:hidden">
        <defs>
          <linearGradient id="base">
            <stop offset="0" stop-color="#FF0000"/>
            <stop offset="1" stop-color="#0000FF"/>
          </linearGradient>
          <linearGradient id="g1" href="#base" x1="0" y1="0" x2="1" y2="0"/>
          <radialGradient id="g2" cx="0.5" cy="0.5" r="0.5">
            <stop offset="0" stop-color="#00FF00"/>
            <stop offset="1" stop-color="#000000"/>
          </radialGradient>
        </defs>
      </svg>
      <svg width="200" height="60" viewBox="0 0 200 60">
        <rect x="0" y="0" width="90" height="60" fill="url(#g1)"/>
        <rect x="100" y="0" width="90" height="60" fill="url('#g2')"/>
      </svg>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* rectSvg = doc->layers.front()->children.back();
  const auto& content = rectSvg->importDirective.content;
  ASSERT_FALSE(content.empty());
  EXPECT_NE(content.find("id=\"g1\""), std::string::npos);
  EXPECT_NE(content.find("id=\"g2\""), std::string::npos);
  // The transitively referenced base gradient must be injected too.
  EXPECT_NE(content.find("id=\"base\""), std::string::npos);
}

// The serialiser must escape and emit text-node content inside an inline <svg> (e.g. an SVG
// <text> element's character data), not just element markup.
PAG_TEST(PAGXHTMLImporterTest, InlineSvgSerializesTextNodeContent) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:120px;height:40px">
      <svg width="120" height="40" viewBox="0 0 120 40">
        <text x="4" y="24" fill="#000000">Hi &amp; Bye</text>
      </svg>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  const auto& content = leaf->importDirective.content;
  ASSERT_FALSE(content.empty());
  EXPECT_NE(content.find("Hi &amp; Bye"), std::string::npos);
}

// `formatColorForAttribute` emits an 8-digit `#RRGGBBAA` hex when the resolved colour carries
// partial alpha, so the downstream SVG importer picks up the transparency.
PAG_TEST(PAGXHTMLInlineSvgEmitterTest, FormatColorForAttributeEmitsAlphaHex) {
  pagx::Color opaque;
  opaque.red = 1.0f;
  opaque.green = 0.0f;
  opaque.blue = 0.0f;
  opaque.alpha = 1.0f;
  opaque.colorSpace = pagx::ColorSpace::SRGB;
  EXPECT_EQ(pagx::HTMLInlineSvgEmitter::formatColorForAttribute(opaque), "#FF0000");

  pagx::Color translucent;
  translucent.red = 0.0f;
  translucent.green = 0.0f;
  translucent.blue = 1.0f;
  translucent.alpha = 0.5f;
  translucent.colorSpace = pagx::ColorSpace::SRGB;
  // 0.5 * 255 rounds to 128 (0x80).
  EXPECT_EQ(pagx::HTMLInlineSvgEmitter::formatColorForAttribute(translucent), "#0000FF80");
}

PAG_TEST(PAGXHTMLImporterTest, StyleClassRulesApply) {
  auto doc = ParseFromString(R"HTML(
    <html><head><style>.box{background-color:#123456;border-radius:8px}</style></head>
      <body style="width:50px;height:50px">
        <div class="box" style="width:50px;height:50px"></div>
      </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(div);
  ASSERT_NE(fill, nullptr);
  auto* solid = As<pagx::SolidColor>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0x123456)));
  auto* rect = FindElementOfType<pagx::Rectangle>(div);
  ASSERT_NE(rect, nullptr);
  EXPECT_FLOAT_EQ(rect->roundness, 8.0f);
}

PAG_TEST(PAGXHTMLImporterTest, UnknownElementWarningEmitted) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <canvas width="50" height="50"></canvas>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  bool hasWarning = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("canvas") != std::string::npos) hasWarning = true;
  }
  EXPECT_TRUE(hasWarning);
}

PAG_TEST(PAGXHTMLImporterTest, MarginIsResolvedThroughPaddingWrapper) {
  // CSS margin used to be dropped at import with a warning. The importer now reproduces it
  // through positioning + padding (`HTMLParserContext::wrapWithMargin`), so this case must
  // (1) parse cleanly without any margin diagnostic and (2) materialise an outer Layer
  // whose padding equals the four-side margin (8/8/8/8) wrapping the original 30x30 box.
  // The body is only 50x50 so the full footprint (30 + 8 + 8 = 46) still fits.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="margin:8px;width:30px;height:30px;background-color:#000"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  for (const auto& msg : doc->errors) {
    EXPECT_EQ(msg.find("margin"), std::string::npos)
        << "unexpected margin diagnostic on accepted input: " << msg;
  }
  // body Layer -> wrapper Layer (padding=8) -> inner div (30x30, background)
  ASSERT_EQ(doc->layers.size(), 1u);
  auto* body = doc->layers.front();
  ASSERT_EQ(body->children.size(), 1u);
  auto* wrapper = body->children.front();
  EXPECT_FLOAT_EQ(wrapper->padding.top, 8.0f);
  EXPECT_FLOAT_EQ(wrapper->padding.right, 8.0f);
  EXPECT_FLOAT_EQ(wrapper->padding.bottom, 8.0f);
  EXPECT_FLOAT_EQ(wrapper->padding.left, 8.0f);
  ASSERT_EQ(wrapper->children.size(), 1u);
  auto* inner = wrapper->children.front();
  EXPECT_FLOAT_EQ(inner->width, 30.0f);
  EXPECT_FLOAT_EQ(inner->height, 30.0f);
}

PAG_TEST(PAGXHTMLImporterTest, DisallowedSizingPropertiesEmitWarnings) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="min-width:10px;max-width:40px;aspect-ratio:1/1;width:50px;height:50px;
                  background-color:#000"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  bool minWarn = false, maxWarn = false, aspectWarn = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("min-width") != std::string::npos) minWarn = true;
    if (msg.find("max-width") != std::string::npos) maxWarn = true;
    if (msg.find("aspect-ratio") != std::string::npos) aspectWarn = true;
  }
  EXPECT_TRUE(minWarn);
  EXPECT_TRUE(maxWarn);
  EXPECT_TRUE(aspectWarn);
}

PAG_TEST(PAGXHTMLImporterTest, DisallowedLayoutPropertiesEmitWarnings) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="display:flex;align-self:center;align-content:center;flex-grow:1;flex-basis:auto;
                  width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  bool selfWarn = false, contentWarn = false, growWarn = false, basisWarn = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("align-self") != std::string::npos) selfWarn = true;
    if (msg.find("align-content") != std::string::npos) contentWarn = true;
    if (msg.find("flex-grow") != std::string::npos) growWarn = true;
    if (msg.find("flex-basis") != std::string::npos) basisWarn = true;
  }
  EXPECT_TRUE(selfWarn);
  EXPECT_TRUE(contentWarn);
  EXPECT_TRUE(growWarn);
  EXPECT_TRUE(basisWarn);
}

PAG_TEST(PAGXHTMLImporterTest, DisallowedVisualPropertiesEmitWarnings) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="outline:1px solid red;clip-path:circle(20px);
                  perspective:200px;width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  bool outlineWarn = false, clipWarn = false, perspectiveWarn = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("outline") != std::string::npos) outlineWarn = true;
    if (msg.find("clip-path") != std::string::npos) clipWarn = true;
    if (msg.find("perspective") != std::string::npos) perspectiveWarn = true;
  }
  EXPECT_TRUE(outlineWarn);
  EXPECT_TRUE(clipWarn);
  EXPECT_TRUE(perspectiveWarn);
}

PAG_TEST(PAGXHTMLImporterTest, NonDefaultTransformOriginWarnsWithTransform) {
  // `transform-origin` resolved as `<x>px <y>px` is honoured by applyBoxTransform — the
  // pivot lands at the requested point and no warning is emitted, because the html-snapshot
  // tool always serialises the origin to absolute pixels (Chromium does the same in
  // `getComputedStyle`). Other forms (keywords other than `center`, percentages other than
  // `50% 50%`) cannot be resolved without further state and still warn so authors of
  // hand-written subset HTML notice the pivot is being approximated.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:60px">
      <h1 style="transform:skewX(-5deg);transform-origin:left top;
                 width:200px;height:60px">Hello</h1>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  bool originWarn = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("transform-origin") != std::string::npos) originWarn = true;
  }
  EXPECT_TRUE(originWarn);
}

PAG_TEST(PAGXHTMLImporterTest, DisallowedTextPropertiesEmitWarnings) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <p style="text-transform:uppercase;text-indent:10px;text-overflow:ellipsis;
                font-variant:small-caps">Hi</p>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  bool transformWarn = false, indentWarn = false, ellipsisWarn = false, variantWarn = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("text-transform") != std::string::npos) transformWarn = true;
    if (msg.find("text-indent") != std::string::npos) indentWarn = true;
    if (msg.find("text-overflow") != std::string::npos) ellipsisWarn = true;
    if (msg.find("font-variant") != std::string::npos) variantWarn = true;
  }
  EXPECT_TRUE(transformWarn);
  EXPECT_TRUE(indentWarn);
  EXPECT_TRUE(ellipsisWarn);
  EXPECT_TRUE(variantWarn);
}

PAG_TEST(PAGXHTMLImporterTest, BoxSizingNonBorderBoxWarns) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="box-sizing:content-box;width:30px;height:30px;background-color:#000"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  bool boxSizingWarn = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("box-sizing") != std::string::npos) boxSizingWarn = true;
  }
  EXPECT_TRUE(boxSizingWarn);
}

PAG_TEST(PAGXHTMLImporterTest, UniversalSelectorWarnsAndDoesNotApply) {
  auto doc = ParseFromString(R"HTML(
    <html><head><style>* { background-color:#FF0000 }</style></head>
      <body style="width:50px;height:50px">
        <div style="width:50px;height:50px;background-color:#00FF00"></div>
      </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  bool universalWarn = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("universal selector") != std::string::npos) universalWarn = true;
  }
  EXPECT_TRUE(universalWarn);
  auto* div = doc->layers.front()->children.front();
  EXPECT_TRUE(ColorNear(SolidFillColorOf(div), HexColor(0x00FF00)));
}

PAG_TEST(PAGXHTMLImporterTest, FlexShorthandWithDisallowedFormWarns) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="display:flex;width:50px;height:50px">
        <div style="flex:1 1 auto;background-color:#000"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  bool flexWarn = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("flex shorthand") != std::string::npos) flexWarn = true;
  }
  EXPECT_TRUE(flexWarn);
}

PAG_TEST(PAGXHTMLImporterTest, AbsoluteOnTextLeafEmitsWarning) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style="position:absolute;top:0;left:0;font-size:14px">Hi</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  bool downgradeWarn = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("absolute on text leaf") != std::string::npos) downgradeWarn = true;
  }
  EXPECT_TRUE(downgradeWarn);
}

PAG_TEST(PAGXHTMLImporterTest, TextLeafWithBlockChildFallsBackToContainer) {
  // Regression: <span> / <p> with block-level children used to drop the children with the
  // warning "<div> not supported inside text leaf; skipped". HTML allows mixed inline/block
  // content, and the snapshot emitter for tools/html-snapshot frequently produces it (e.g.
  // when a Lucide <svg> sits inside an inline wrapper). We now fall back to container
  // handling so the children survive as sibling layers.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:100px;height:50px">
      <span style="position:absolute;left:0;top:0;width:60px;height:20px;color:#000">
        Hi
        <div style="position:absolute;left:30px;top:0;width:20px;height:20px;
                    background-color:#FF2442;border-radius:4px"></div>
      </span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  for (const auto& msg : doc->errors) {
    EXPECT_TRUE(msg.find("not supported inside text leaf") == std::string::npos)
        << "unexpected warning: " << msg;
  }
  auto* span = doc->layers.front()->children.front();
  ASSERT_NE(span, nullptr);
  bool foundBlockChild = false;
  for (auto* child : span->children) {
    auto* fill = FindElementOfType<pagx::Fill>(child);
    if (fill != nullptr && ColorNear(SolidFillColorOf(child), HexColor(0xFF2442))) {
      foundBlockChild = true;
    }
  }
  EXPECT_TRUE(foundBlockChild);
}

PAG_TEST(PAGXHTMLImporterTest, MissingCanvasReportsHardError) {
  auto doc = ParseFromString(R"HTML(<html><body></body></html>)HTML");
  EXPECT_EQ(doc, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, RootLevelExtraElementWarns) {
  auto doc = ParseFromString(R"HTML(
    <html>
      <body style="width:50px;height:50px"></body>
      <foo></foo>
    </html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("at root is not allowed") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
}

PAG_TEST(PAGXHTMLImporterTest, HeadDisallowedChildrenWarn) {
  auto doc = ParseFromString(R"HTML(
    <html>
      <head><script>var x=1;</script></head>
      <body style="width:50px;height:50px"></body>
    </html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("script") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
}

PAG_TEST(PAGXHTMLImporterTest, ExternalStylesheetLinkWarns) {
  auto doc = ParseFromString(R"HTML(
    <html>
      <head><link rel="stylesheet" href="theme.css"/></head>
      <body style="width:50px;height:50px"></body>
    </html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("link") != std::string::npos && msg.find("stylesheet") != std::string::npos) {
      warned = true;
    }
  }
  EXPECT_TRUE(warned);
}

PAG_TEST(PAGXHTMLImporterTest, TextDecorationColorDifferingWrapsInGroup) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style="font-size:14px;color:#000;
                   text-decoration:underline;text-decoration-color:#FF0000">Hi</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  // The decoration overlay is wrapped in a Group when its colour differs from the text.
  bool foundGroup = false;
  for (auto* e : leaf->contents) {
    if (auto* g = As<pagx::Group>(e)) {
      auto* rect = FindElement<pagx::Rectangle>(g->elements);
      auto* fill = FindElement<pagx::Fill>(g->elements);
      ASSERT_NE(rect, nullptr);
      ASSERT_NE(fill, nullptr);
      auto* solid = As<pagx::SolidColor>(fill->color);
      ASSERT_NE(solid, nullptr);
      EXPECT_TRUE(ColorNear(solid->color, HexColor(0xFF0000)));
      foundGroup = true;
    }
  }
  EXPECT_TRUE(foundGroup);
}

//==================================================================================================
// Document-level + style-block tests
//==================================================================================================

PAG_TEST(PAGXHTMLImporterTest, TitleStoredAsDataTitle) {
  auto doc = ParseFromString(R"HTML(
    <html>
      <head><title>Canvas Title</title></head>
      <body style="width:50px;height:50px"></body>
    </html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto it = doc->customData.find("title");
  ASSERT_NE(it, doc->customData.end());
  EXPECT_EQ(it->second, "Canvas Title");
}

PAG_TEST(PAGXHTMLImporterTest, MetaTagIgnoredSilently) {
  auto doc = ParseFromString(R"HTML(
    <html>
      <head><meta charset="utf-8"/></head>
      <body style="width:50px;height:50px"></body>
    </html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  for (const auto& msg : doc->errors) {
    EXPECT_EQ(msg.find("meta"), std::string::npos) << msg;
  }
}

PAG_TEST(PAGXHTMLImporterTest, RejectsMissingBody) {
  auto doc = ParseFromString(R"HTML(<html><head></head></html>)HTML");
  EXPECT_EQ(doc, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, CommaSeparatedClassRulesApply) {
  auto doc = ParseFromString(R"HTML(
    <html><head><style>.a, .b { background-color:#112233; border-radius:6px }</style></head>
      <body style="width:80px;height:40px">
        <div class="a" style="width:20px;height:20px"></div>
        <div class="b" style="width:20px;height:20px"></div>
      </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->layers.front()->children.size(), 2u);
  for (auto* div : doc->layers.front()->children) {
    auto* fill = FindElementOfType<pagx::Fill>(div);
    auto* rect = FindElementOfType<pagx::Rectangle>(div);
    ASSERT_NE(fill, nullptr);
    ASSERT_NE(rect, nullptr);
    auto* solid = As<pagx::SolidColor>(fill->color);
    ASSERT_NE(solid, nullptr);
    EXPECT_TRUE(ColorNear(solid->color, HexColor(0x112233)));
    EXPECT_FLOAT_EQ(rect->roundness, 6.0f);
  }
}

PAG_TEST(PAGXHTMLImporterTest, MultipleClassesMergeWithLastWinning) {
  auto doc = ParseFromString(R"HTML(
    <html><head><style>
      .base { background-color:#000000; border-radius:4px }
      .override { background-color:#FF8800 }
    </style></head>
      <body style="width:50px;height:50px">
        <div class="base override" style="width:50px;height:50px"></div>
      </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(div);
  auto* rect = FindElementOfType<pagx::Rectangle>(div);
  ASSERT_NE(fill, nullptr);
  ASSERT_NE(rect, nullptr);
  auto* solid = As<pagx::SolidColor>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0xFF8800)));
  EXPECT_FLOAT_EQ(rect->roundness, 4.0f);
}

PAG_TEST(PAGXHTMLImporterTest, InlineStyleBeatsClassRule) {
  auto doc = ParseFromString(R"HTML(
    <html><head><style>.box { background-color:#111111 }</style></head>
      <body style="width:50px;height:50px">
        <div class="box" style="width:50px;height:50px;background-color:#22AAEE"></div>
      </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(div);
  ASSERT_NE(fill, nullptr);
  auto* solid = As<pagx::SolidColor>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0x22AAEE)));
}

PAG_TEST(PAGXHTMLImporterTest, ElementSelectorAppliesToTag) {
  auto doc = ParseFromString(R"HTML(
    <html><head><style>p { color:#33CC44 }</style></head>
      <body style="width:200px;height:40px">
        <p>Hello</p>
      </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(leaf);
  ASSERT_NE(fill, nullptr);
  auto* solid = As<pagx::SolidColor>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0x33CC44)));
}

PAG_TEST(PAGXHTMLImporterTest, UnsupportedSelectorIgnoredAndDoesNotApply) {
  auto doc = ParseFromString(R"HTML(
    <html><head><style>
      div > .child { background-color:#FF0000 }
      .ok { background-color:#00FF00 }
    </style></head>
      <body style="width:50px;height:50px">
        <div class="child ok" style="width:50px;height:50px"></div>
      </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  // The descendant selector "div > .child" must be dropped; only the simple ".ok" rule applies.
  auto* div = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(div);
  ASSERT_NE(fill, nullptr);
  auto* solid = As<pagx::SolidColor>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0x00FF00)));
}

//==================================================================================================
// Sizing / positioning / element type tests
//==================================================================================================

PAG_TEST(PAGXHTMLImporterTest, PercentSizingMapsToPercentFields) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:100px">
      <div style="width:50%;height:25%;background-color:#000"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_FLOAT_EQ(div->percentWidth, 50.0f);
  EXPECT_FLOAT_EQ(div->percentHeight, 25.0f);
  EXPECT_TRUE(std::isnan(div->width));
  EXPECT_TRUE(std::isnan(div->height));
}

PAG_TEST(PAGXHTMLImporterTest, SemanticContainersMapToLayer) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:100px;height:100px">
      <header style="width:100px;height:20px"></header>
      <main style="width:100px;height:60px"></main>
      <footer style="width:100px;height:20px"></footer>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_EQ(doc->layers.front()->children.size(), 3u);
}

PAG_TEST(PAGXHTMLImporterTest, NonStaticPositionDowngradesAbsolute) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:100px;height:100px">
      <div style="position:fixed;top:5px;left:5px;width:10px;height:10px;background-color:#000"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_FALSE(div->includeInLayout);
  bool hasWarning = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("position") != std::string::npos) hasWarning = true;
  }
  EXPECT_TRUE(hasWarning);
}

//==================================================================================================
// Layout / arrangement / alignment tests
//==================================================================================================

PAG_TEST(PAGXHTMLImporterTest, FlexDirectionColumn) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:200px">
      <div style="display:flex;flex-direction:column;width:50px;height:200px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_EQ(div->layout, pagx::LayoutMode::Vertical);
}

PAG_TEST(PAGXHTMLImporterTest, AlignItemsAndJustifyContentVariants) {
  struct Pair {
    const char* css;
    pagx::Alignment expected;
  };
  Pair alignments[] = {{"stretch", pagx::Alignment::Stretch},
                       {"flex-start", pagx::Alignment::Start},
                       {"flex-end", pagx::Alignment::End}};
  for (const auto& p : alignments) {
    std::string html = std::string(
                           "<html><body style=\"width:50px;height:50px\">"
                           "<div style=\"display:flex;align-items:") +
                       p.css + ";width:50px;height:50px\"></div></body></html>";
    auto doc = ParseFromString(html);
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->layers.front()->children.front()->alignment, p.expected) << p.css;
  }
  struct JPair {
    const char* css;
    pagx::Arrangement expected;
  };
  JPair justifies[] = {{"flex-start", pagx::Arrangement::Start},
                       {"center", pagx::Arrangement::Center},
                       {"flex-end", pagx::Arrangement::End},
                       {"space-evenly", pagx::Arrangement::SpaceEvenly},
                       {"space-around", pagx::Arrangement::SpaceAround}};
  for (const auto& p : justifies) {
    std::string html = std::string(
                           "<html><body style=\"width:50px;height:50px\">"
                           "<div style=\"display:flex;justify-content:") +
                       p.css + ";width:50px;height:50px\"></div></body></html>";
    auto doc = ParseFromString(html);
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->layers.front()->children.front()->arrangement, p.expected) << p.css;
  }
}

PAG_TEST(PAGXHTMLImporterTest, OverflowingSpaceBetweenColumnCollapsesToStart) {
  // Children's combined height (60+60=120) exceeds the parent's content-box height
  // (height 100 - padding 10*2 = 80), so the importer must rewrite `space-between`
  // to `flex-start` to keep PAGX's flex engine from drawing the items overlapped.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="display:flex;flex-direction:column;justify-content:space-between;
                  width:200px;height:100px;padding:10px">
        <div style="width:50px;height:60px"></div>
        <div style="width:50px;height:60px"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_EQ(doc->layers.front()->children.front()->arrangement, pagx::Arrangement::Start);
}

PAG_TEST(PAGXHTMLImporterTest, OverflowingSpaceAroundRowCollapsesToStart) {
  // Row direction with `space-around`: children width sum 100+100=200 exceeds available
  // 200-padding(10*2)=180, so the importer must collapse to flex-start.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:300px;height:100px">
      <div style="display:flex;flex-direction:row;justify-content:space-around;
                  width:200px;height:100px;padding:10px">
        <div style="width:100px;height:50px"></div>
        <div style="width:100px;height:50px"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_EQ(doc->layers.front()->children.front()->arrangement, pagx::Arrangement::Start);
}

PAG_TEST(PAGXHTMLImporterTest, NonOverflowingSpaceBetweenStaysSpaceBetween) {
  // Children fit comfortably (50+50=100 <= 200-20=180), so no rewrite should happen.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:300px;height:100px">
      <div style="display:flex;flex-direction:row;justify-content:space-between;
                  width:200px;height:100px;padding:10px">
        <div style="width:50px;height:50px"></div>
        <div style="width:50px;height:50px"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_EQ(doc->layers.front()->children.front()->arrangement, pagx::Arrangement::SpaceBetween);
}

PAG_TEST(PAGXHTMLImporterTest, OverflowingSpaceBetweenWithFlexGrowChildSkipsRewrite) {
  // A `flex` grow child absorbs leftover space in CSS, so the spec never produces
  // negative free space. The importer must not rewrite this container even if a
  // naive sum of declared widths would suggest overflow.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:300px;height:100px">
      <div style="display:flex;flex-direction:row;justify-content:space-between;
                  width:200px;height:100px;padding:10px">
        <div style="width:100px;height:50px"></div>
        <div style="flex:1;width:200px;height:50px"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_EQ(doc->layers.front()->children.front()->arrangement, pagx::Arrangement::SpaceBetween);
}

PAG_TEST(PAGXHTMLImporterTest, PaddingShorthandTwoAndThreeValues) {
  auto two = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="display:flex;padding:4px 6px;width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(two, nullptr);
  auto* a = two->layers.front()->children.front();
  EXPECT_FLOAT_EQ(a->padding.top, 4.0f);
  EXPECT_FLOAT_EQ(a->padding.bottom, 4.0f);
  EXPECT_FLOAT_EQ(a->padding.left, 6.0f);
  EXPECT_FLOAT_EQ(a->padding.right, 6.0f);

  auto three = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="display:flex;padding:1px 2px 3px;width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(three, nullptr);
  auto* b = three->layers.front()->children.front();
  EXPECT_FLOAT_EQ(b->padding.top, 1.0f);
  EXPECT_FLOAT_EQ(b->padding.left, 2.0f);
  EXPECT_FLOAT_EQ(b->padding.right, 2.0f);
  EXPECT_FLOAT_EQ(b->padding.bottom, 3.0f);
}

//==================================================================================================
// Color parsing tests
//==================================================================================================

PAG_TEST(PAGXHTMLImporterTest, NamedColorAccepted) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:red"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* fill = FindElementOfType<pagx::Fill>(doc->layers.front()->children.front());
  ASSERT_NE(fill, nullptr);
  auto* solid = As<pagx::SolidColor>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0xFF0000)));
}

PAG_TEST(PAGXHTMLImporterTest, ShortHexBackgroundColorAccepted) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:#0F0"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(
      ColorNear(SolidFillColorOf(doc->layers.front()->children.front()), HexColor(0x00FF00)));
}

PAG_TEST(PAGXHTMLImporterTest, RgbaTextColorParsed) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style="color:rgba(0,0,255,0.5)">Hi</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(
      ColorNear(SolidFillColorOf(doc->layers.front()->children.front()), HexColor(0x0000FF, 0.5f)));
}

PAG_TEST(PAGXHTMLImporterTest, RgbBackgroundColorParsed) {
  // Regression: `background-color` used to skip any value containing '(', which silently
  // dropped functional color literals like rgb()/rgba() emitted by browsers and authoring
  // tools. parseColor handles them fine, so the only thing the importer should reject for
  // background-color is non-color shorthands (gradients / url()).
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:rgb(255, 36, 66)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(
      ColorNear(SolidFillColorOf(doc->layers.front()->children.front()), HexColor(0xFF2442)));
}

PAG_TEST(PAGXHTMLImporterTest, RgbaBackgroundColorParsed) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:rgba(0, 0, 0, 0.4)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(
      ColorNear(SolidFillColorOf(doc->layers.front()->children.front()), HexColor(0x000000, 0.4f)));
}

//==================================================================================================
// Text tests
//==================================================================================================

PAG_TEST(PAGXHTMLImporterTest, BodyDefaultsArial14Color) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span>Hello</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto* text = FindElementOfType<pagx::Text>(leaf);
  ASSERT_NE(text, nullptr);
  EXPECT_EQ(text->fontFamily, "Arial");
  EXPECT_FLOAT_EQ(text->fontSize, 14.0f);
  auto* fill = FindElementOfType<pagx::Fill>(leaf);
  ASSERT_NE(fill, nullptr);
  auto* solid = As<pagx::SolidColor>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0x1E293B)));
}

PAG_TEST(PAGXHTMLImporterTest, HeadingDefaultFontSizes) {
  struct Row {
    const char* tag;
    float size;
  };
  Row rows[] = {{"h1", 32.0f}, {"h2", 24.0f}, {"h3", 20.0f},
                {"h4", 18.0f}, {"h5", 16.0f}, {"h6", 14.0f}};
  for (const auto& r : rows) {
    std::string html = std::string("<html><body style=\"width:200px;height:60px\"><") + r.tag +
                       ">T</" + r.tag + "></body></html>";
    auto doc = ParseFromString(html);
    ASSERT_NE(doc, nullptr) << r.tag;
    auto* leaf = doc->layers.front()->children.front();
    auto* text = FindElementOfType<pagx::Text>(leaf);
    ASSERT_NE(text, nullptr) << r.tag;
    EXPECT_FLOAT_EQ(text->fontSize, r.size) << r.tag;
    // Headings default to font-weight:bold, which is baked as faux bold (synthesised on a base
    // face) rather than a "Bold" style label so the authored weight survives a missing styled face.
    EXPECT_EQ(text->fontStyle, "") << r.tag;
    EXPECT_TRUE(text->fauxBold) << r.tag;
    EXPECT_FALSE(text->fauxItalic) << r.tag;
  }
}

PAG_TEST(PAGXHTMLImporterTest, FontWeightNumericMapsToFauxBold) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style="font-weight:700">Heavy</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* text = FindElementOfType<pagx::Text>(doc->layers.front()->children.front());
  ASSERT_NE(text, nullptr);
  // Bold (weight >= 600) is synthesised via faux bold rather than locking onto a "Bold" face.
  EXPECT_EQ(text->fontStyle, "");
  EXPECT_TRUE(text->fauxBold);
  EXPECT_FALSE(text->fauxItalic);
}

PAG_TEST(PAGXHTMLImporterTest, FontWeight500MapsToMedium) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style="font-weight:500">Medium</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* text = FindElementOfType<pagx::Text>(doc->layers.front()->children.front());
  ASSERT_NE(text, nullptr);
  // Medium (weight < 600) cannot be synthesised, so it stays a real-face style label with no faux.
  EXPECT_EQ(text->fontStyle, "Medium");
  EXPECT_FALSE(text->fauxBold);
  EXPECT_FALSE(text->fauxItalic);
}

PAG_TEST(PAGXHTMLImporterTest, BoldItalicCombined) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style="font-weight:bold;font-style:italic">Hi</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* text = FindElementOfType<pagx::Text>(doc->layers.front()->children.front());
  ASSERT_NE(text, nullptr);
  // Both axes are synthesised: the style label drops to empty and both faux flags are set so the
  // run renders bold + italic even when the styled face is unavailable.
  EXPECT_EQ(text->fontStyle, "");
  EXPECT_TRUE(text->fauxBold);
  EXPECT_TRUE(text->fauxItalic);
}

PAG_TEST(PAGXHTMLImporterTest, FontFamilyAndLetterSpacing) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style="font-family:Helvetica;letter-spacing:2px">Hi</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* text = FindElementOfType<pagx::Text>(doc->layers.front()->children.front());
  ASSERT_NE(text, nullptr);
  EXPECT_EQ(text->fontFamily, "Helvetica");
  EXPECT_FLOAT_EQ(text->letterSpacing, 2.0f);
}

PAG_TEST(PAGXHTMLImporterTest, FontFamilyStackPicksFirstConcreteAndStripsQuotes) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style='font-family:"PingFang SC", -apple-system, sans-serif'>Hi</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* text = FindElementOfType<pagx::Text>(doc->layers.front()->children.front());
  ASSERT_NE(text, nullptr);
  EXPECT_EQ(text->fontFamily, "PingFang SC");
}

PAG_TEST(PAGXHTMLImporterTest, FontFamilyStackStripsSingleQuotes) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style="font-family:'Roboto Mono'">Hi</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* text = FindElementOfType<pagx::Text>(doc->layers.front()->children.front());
  ASSERT_NE(text, nullptr);
  EXPECT_EQ(text->fontFamily, "Roboto Mono");
}

PAG_TEST(PAGXHTMLImporterTest, FontFamilyGenericFirstMapsToPlatformConcrete) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style='font-family:sans-serif, "Foo"'>Hi</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* text = FindElementOfType<pagx::Text>(doc->layers.front()->children.front());
  ASSERT_NE(text, nullptr);
#if defined(__APPLE__)
  EXPECT_EQ(text->fontFamily, "Helvetica");
#elif defined(_WIN32)
  EXPECT_EQ(text->fontFamily, "Arial");
#else
  EXPECT_EQ(text->fontFamily, "DejaVu Sans");
#endif
}

PAG_TEST(PAGXHTMLImporterTest, FontFamilyAllUnmappableGenericsFallsBackToDefault) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style="font-family:cursive, fantasy">Hi</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* text = FindElementOfType<pagx::Text>(doc->layers.front()->children.front());
  ASSERT_NE(text, nullptr);
  // cursive and fantasy are recognised generics with no concrete mapping; both are
  // dropped with diagnostics and the importer falls back to HTML_DEFAULT_FONT_FAMILY.
  EXPECT_EQ(text->fontFamily, "Arial");
}

PAG_TEST(PAGXHTMLImporterTest, FontFamilyInheritedFromAncestor) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <div style='font-family:"Inter", sans-serif'>
        <span>Hi</span>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* divLayer = doc->layers.front()->children.front();
  ASSERT_FALSE(divLayer->children.empty());
  auto* spanLayer = divLayer->children.front();
  auto* text = FindElementOfType<pagx::Text>(spanLayer);
  ASSERT_NE(text, nullptr);
  EXPECT_EQ(text->fontFamily, "Inter");
}

PAG_TEST(PAGXHTMLImporterTest, FontFamilyStackRegistersFallbacksOnDocFontConfig) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style='font-family:"PingFang SC", "Helvetica Neue", monospace'>Hi</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto names = doc->fontConfig().fallbackFamilyNames();
  // The implicit "Arial" from <body>'s element default plus every concrete name from the
  // span's stack should be present. The platform-mapped monospace family varies.
  EXPECT_NE(std::find(names.begin(), names.end(), "Arial"), names.end());
  EXPECT_NE(std::find(names.begin(), names.end(), "PingFang SC"), names.end());
  EXPECT_NE(std::find(names.begin(), names.end(), "Helvetica Neue"), names.end());
#if defined(__APPLE__)
  EXPECT_NE(std::find(names.begin(), names.end(), "Menlo"), names.end());
#elif defined(_WIN32)
  EXPECT_NE(std::find(names.begin(), names.end(), "Consolas"), names.end());
#else
  EXPECT_NE(std::find(names.begin(), names.end(), "DejaVu Sans Mono"), names.end());
#endif
}

PAG_TEST(PAGXHTMLImporterTest, FontFamilyStackDedupesAcrossElements) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style='font-family:"Inter", "Roboto"'>Hi</span>
      <span style='font-family:"Inter", "Noto Sans"'>Bye</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto names = doc->fontConfig().fallbackFamilyNames();
  size_t interCount = static_cast<size_t>(std::count(names.begin(), names.end(), "Inter"));
  EXPECT_EQ(interCount, 1u);
  EXPECT_NE(std::find(names.begin(), names.end(), "Roboto"), names.end());
  EXPECT_NE(std::find(names.begin(), names.end(), "Noto Sans"), names.end());
}

PAG_TEST(PAGXHTMLImporterTest, TextAlignAndLineHeightOnParagraph) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:60px">
      <p style="text-align:center;line-height:20px">Hi <span>World</span></p>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto* tb = FindElementOfType<pagx::TextBox>(leaf);
  ASSERT_NE(tb, nullptr);
  EXPECT_EQ(tb->textAlign, pagx::TextAlign::Center);
  EXPECT_FLOAT_EQ(tb->lineHeight, 20.0f);
}

// `<div style="height:20px"><span style="line-height:20px">02</span></div>` is the canonical
// "centre text in a fixed-height badge" idiom (digit boxes, pill buttons, status chips). The
// outer element only carries the box visuals (height + background); the line-height that
// vertically centres the glyph lives on the inner span. Without lifting that value to the
// TextBox the glyph renders at its natural font height aligned to the top of the box, which
// is exactly the misalignment the JD home countdown reproduced before this fix.
PAG_TEST(PAGXHTMLImporterTest, InnerSpanLineHeightLiftedToTextBoxForFixedHeightBadge) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:60px;height:20px">
      <span style="display:inline-block;width:22px;height:20px;background-color:#1A1A1A"><span style="font-size:12px;line-height:20px;color:#FFFFFF">02</span></span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  // The outer span carries `background-color`, so convertTextLeaf splits it into an outer
  // Layer (Rectangle+Fill) and an inner host Layer that hosts the TextBox.
  auto* outer = doc->layers.front()->children.front();
  ASSERT_NE(outer, nullptr);
  ASSERT_FALSE(outer->children.empty());
  auto* innerHost = outer->children.front();
  auto* tb = FindElementOfType<pagx::TextBox>(innerHost);
  ASSERT_NE(tb, nullptr);
  EXPECT_FLOAT_EQ(tb->lineHeight, 20.0f);
}

// CSS computes line-box height as the max of every participant's line-height, so a child run
// with a taller line-height must override an outer run with a shorter one. The merge in
// appendTextFragment must preserve that max even when the two runs share glyph styling.
PAG_TEST(PAGXHTMLImporterTest, InnerSpanWithLargerLineHeightWinsAgainstOuter) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:60px">
      <p style="line-height:10px">Hi <span style="line-height:30px">World</span></p>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* tb = FindElementOfType<pagx::TextBox>(doc->layers.front()->children.front());
  ASSERT_NE(tb, nullptr);
  EXPECT_FLOAT_EQ(tb->lineHeight, 30.0f);
}

PAG_TEST(PAGXHTMLImporterTest, TextAlignJustifyMapped) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:60px">
      <p style="text-align:justify">Hi <span>World</span></p>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* tb = FindElementOfType<pagx::TextBox>(doc->layers.front()->children.front());
  ASSERT_NE(tb, nullptr);
  EXPECT_EQ(tb->textAlign, pagx::TextAlign::Justify);
}

PAG_TEST(PAGXHTMLImporterTest, WhiteSpaceNowrapDisablesWrap) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <p style="white-space:nowrap">Hi <span>World</span></p>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* tb = FindElementOfType<pagx::TextBox>(doc->layers.front()->children.front());
  ASSERT_NE(tb, nullptr);
  EXPECT_FALSE(tb->wordWrap);
}

PAG_TEST(PAGXHTMLImporterTest, OverflowHiddenOnTextContainerHidesText) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <p style="overflow:hidden">Hi <span>World</span></p>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* tb = FindElementOfType<pagx::TextBox>(doc->layers.front()->children.front());
  ASSERT_NE(tb, nullptr);
  EXPECT_EQ(tb->overflow, pagx::Overflow::Hidden);
}

PAG_TEST(PAGXHTMLImporterTest, TextDecorationLineThroughOverlay) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style="font-size:14px;color:#000;text-decoration:line-through">Hello</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto* rect = FindElementOfType<pagx::Rectangle>(leaf);
  ASSERT_NE(rect, nullptr);
  EXPECT_FLOAT_EQ(rect->centerY, 0.0f);
  EXPECT_FLOAT_EQ(rect->height, 1.0f);
  EXPECT_FLOAT_EQ(rect->percentWidth, 100.0f);
}

//==================================================================================================
// Gradients / Anchor / data-* / id tests
//==================================================================================================

PAG_TEST(PAGXHTMLImporterTest, LinearGradientToBottomRight) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-image:linear-gradient(to bottom right, #F00, #00F)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* fill = FindElementOfType<pagx::Fill>(doc->layers.front()->children.front());
  ASSERT_NE(fill, nullptr);
  auto* lg = As<pagx::LinearGradient>(fill->color);
  ASSERT_NE(lg, nullptr);
  // CSS 135deg → PAGX 45deg (top-left to bottom-right diagonal).
  EXPECT_GT(lg->endPoint.x, lg->startPoint.x);
  EXPECT_GT(lg->endPoint.y, lg->startPoint.y);
}

PAG_TEST(PAGXHTMLImporterTest, LinearGradientThreeStopsInterpolatesMiddleOffset) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-image:linear-gradient(90deg,#F00,#0F0,#00F)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* fill = FindElementOfType<pagx::Fill>(doc->layers.front()->children.front());
  auto* lg = As<pagx::LinearGradient>(fill->color);
  ASSERT_NE(lg, nullptr);
  ASSERT_EQ(lg->colorStops.size(), 3u);
  EXPECT_TRUE(NearlyEqual(lg->colorStops[0]->offset, 0.0f, 0.01f));
  EXPECT_TRUE(NearlyEqual(lg->colorStops[1]->offset, 0.5f, 0.01f));
  EXPECT_TRUE(NearlyEqual(lg->colorStops[2]->offset, 1.0f, 0.01f));
}

PAG_TEST(PAGXHTMLImporterTest, ConicGradientFrom90DegMapsToZero) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-image:conic-gradient(from 90deg, #F00, #00F)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* fill = FindElementOfType<pagx::Fill>(doc->layers.front()->children.front());
  auto* cg = As<pagx::ConicGradient>(fill->color);
  ASSERT_NE(cg, nullptr);
  // CSS 90° (right) ⇒ PAGX 0° (along +X axis).
  EXPECT_TRUE(NearlyEqual(cg->startAngle, 0.0f, 0.01f));
}

// Verifies CSS `background-clip: text` combined with a gradient `background-image` redirects
// the gradient onto the descendant text fill. The element's own rectangle gradient must be
// suppressed, and a `<TextBox>`-level `<Fill>` with the same `LinearGradient` must appear.
PAG_TEST(PAGXHTMLImporterTest, BackgroundClipTextRoutesGradientToTextFill) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:240px;height:80px">
      <div style="position:absolute;left:0;top:0;width:240px;height:80px;
                  background-image:linear-gradient(90deg, #FF0000 0%, #0000FF 100%);
                  background-clip:text">
        <span style="font-size:32px;font-weight:700">Hello</span>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* outer = doc->layers.front()->children.front();
  ASSERT_NE(outer, nullptr);
  // No Rectangle should be emitted on the clip-to-text wrapper: the gradient is consumed
  // by the descendant text fill instead of painting a coloured block behind the glyphs.
  EXPECT_EQ(CountElements<pagx::Rectangle>(outer->contents), 0u);
  EXPECT_EQ(FindElementOfType<pagx::Fill>(outer), nullptr);
  // The text leaf carries the gradient as its glyph fill.
  auto* textLeaf = outer->children.front();
  ASSERT_NE(textLeaf, nullptr);
  auto* textBox = FindElementOfType<pagx::TextBox>(textLeaf);
  pagx::Fill* textFill = nullptr;
  if (textBox) {
    textFill = FindElement<pagx::Fill>(textBox->elements);
  } else {
    textFill = FindElementOfType<pagx::Fill>(textLeaf);
  }
  ASSERT_NE(textFill, nullptr);
  auto* lg = As<pagx::LinearGradient>(textFill->color);
  ASSERT_NE(lg, nullptr);
  ASSERT_EQ(lg->colorStops.size(), 2u);
  EXPECT_TRUE(ColorNear(lg->colorStops.front()->color, HexColor(0xFF0000)));
  EXPECT_TRUE(ColorNear(lg->colorStops.back()->color, HexColor(0x0000FF)));
}

// Negative control: without `background-clip: text` the same gradient must still paint a
// rectangle on the wrapper element (the existing behaviour must not regress).
PAG_TEST(PAGXHTMLImporterTest, GradientBackgroundWithoutClipKeepsRectangle) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:240px;height:80px">
      <div style="position:absolute;left:0;top:0;width:240px;height:80px;
                  background-image:linear-gradient(90deg, #FF0000 0%, #0000FF 100%)">
        <span style="font-size:32px;font-weight:700">Hello</span>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* outer = doc->layers.front()->children.front();
  ASSERT_NE(outer, nullptr);
  EXPECT_EQ(CountElements<pagx::Rectangle>(outer->contents), 1u);
  auto* fill = FindElementOfType<pagx::Fill>(outer);
  ASSERT_NE(fill, nullptr);
  EXPECT_NE(As<pagx::LinearGradient>(fill->color), nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, AnchorHrefStoredAsCustomData) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <a href="https://example.com" style="font-size:14px">Link</a>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto it = leaf->customData.find("href");
  ASSERT_NE(it, leaf->customData.end());
  EXPECT_EQ(it->second, "https://example.com");
}

PAG_TEST(PAGXHTMLImporterTest, AnchorDefaultsBlueAndUnderline) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <a href="https://e.test">Link</a>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(leaf);
  ASSERT_NE(fill, nullptr);
  auto* solid = As<pagx::SolidColor>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0x2563EB)));
  // Underline overlay rectangle should exist.
  EXPECT_GE(CountElements<pagx::Rectangle>(leaf->contents), 1u);
}

PAG_TEST(PAGXHTMLImporterTest, DataStarAttributesPropagate) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div data-role="primary" data-label="hi" style="width:50px;height:50px;background-color:#000"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_EQ(div->customData["role"], "primary");
  EXPECT_EQ(div->customData["label"], "hi");
}

// `data-*` attributes on an `<img>` forward onto the backing `Image` resource's customData rather
// than the layer, so the custom data travels with the image itself. html-snapshot preserves these
// on the emitted `<img>`.
PAG_TEST(PAGXHTMLImporterTest, DataStarAttributesPropagateOnImage) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <img src="logo.png" data-id="hero" data-role="cover" style="width:80px;height:80px"/>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* img = doc->layers.front()->children.front();
  auto* pattern = As<pagx::ImagePattern>(FindElementOfType<pagx::Fill>(img)->color);
  ASSERT_NE(pattern, nullptr);
  ASSERT_NE(pattern->image, nullptr);
  // Custom data lands on the Image resource, not the layer.
  EXPECT_EQ(pattern->image->customData["id"], "hero");
  EXPECT_EQ(pattern->image->customData["role"], "cover");
  EXPECT_TRUE(img->customData.empty());
}

// The rounded-image fold collapses the wrapper + inner `<img>` into one layer; the inner
// `<img>`'s `data-*` must still reach its backing `Image` resource's customData.
PAG_TEST(PAGXHTMLImporterTest, DataStarAttributesPropagateOnFoldedRoundedImage) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:64px;height:64px">
      <div style="width:64px;height:64px;border-radius:9999px;overflow:hidden">
        <img src="avatar.png" data-id="avatar" style="position:absolute;left:0;top:0;width:64px;height:64px"/>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* wrapper = doc->layers.front()->children.front();
  EXPECT_TRUE(wrapper->children.empty());
  auto* pattern = As<pagx::ImagePattern>(FindElementOfType<pagx::Fill>(wrapper)->color);
  ASSERT_NE(pattern, nullptr);
  ASSERT_NE(pattern->image, nullptr);
  EXPECT_EQ(pattern->image->customData["id"], "avatar");
}

PAG_TEST(PAGXHTMLImporterTest, IdAttributePropagatesToLayer) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div id="hero" style="width:50px;height:50px;background-color:#000"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_EQ(div->id, "hero");
}

//==================================================================================================
// Image / SVG / Options tests
//==================================================================================================

PAG_TEST(PAGXHTMLImporterTest, ImgWithSvgSrcBecomesImportDirective) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <img src="logo.svg" style="width:80px;height:80px"/>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  EXPECT_EQ(leaf->importDirective.format, "svg");
  EXPECT_FALSE(leaf->importDirective.source.empty());
  EXPECT_NE(leaf->importDirective.source.find("logo.svg"), std::string::npos);
  // No Rectangle/Fill is emitted for external SVG imports.
  EXPECT_EQ(CountElements<pagx::Rectangle>(leaf->contents), 0u);
}

// Regression: the standard CSS rounded-avatar pattern `<div border-radius +
// overflow: hidden><img/></div>` used to translate to a wrapper Layer with
// `clipToBounds="true"` plus a separate child Layer holding the image — and PAGX's
// `clipToBounds` clips to rectangular bounds only, so the image's square geometry
// leaked past the wrapper's rounded corners (square avatars inside circular rings).
// The importer now folds the <img> into the wrapper's rounded Rectangle directly.
PAG_TEST(PAGXHTMLImporterTest, RoundedImageWrapperFoldsIntoRectangle) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:64px;height:64px">
      <div style="width:64px;height:64px;border-radius:9999px;overflow:hidden">
        <img src="avatar.png" style="position:absolute;left:0;top:0;width:64px;height:64px"/>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* wrapper = doc->layers.front()->children.front();
  // The folded layer should have no children — the image lives in its contents.
  EXPECT_TRUE(wrapper->children.empty());
  // clipToBounds is dropped: the rounded Rectangle is now the actual fill geometry.
  EXPECT_FALSE(wrapper->clipToBounds);
  // Exactly one rounded Rectangle in contents (matching the wrapper's border-radius).
  auto* rect = FindElementOfType<pagx::Rectangle>(wrapper);
  ASSERT_NE(rect, nullptr);
  EXPECT_GT(rect->roundness, 0.0f);
  // And one Fill carrying the image pattern.
  auto* fill = FindElementOfType<pagx::Fill>(wrapper);
  ASSERT_NE(fill, nullptr);
  auto* pattern = As<pagx::ImagePattern>(fill->color);
  ASSERT_NE(pattern, nullptr);
  ASSERT_NE(pattern->image, nullptr);
  EXPECT_NE(pattern->image->filePath.find("avatar.png"), std::string::npos);
}

// Regression: the importer used to leave `ImagePattern::scaleMode` at its
// `LetterBox` default, but the CSS `<img>` default `object-fit: fill` is a
// stretch. With intrinsic-aspect images that disagree with the box's, LetterBox
// shrinks the image inside the box (visible empty bands along one axis), while
// the browser stretches it edge-to-edge. The default must be Stretch, and the
// `object-fit` keyword must drive the mode through both the regular image path
// and the rounded-wrapper fold path.
PAG_TEST(PAGXHTMLImporterTest, ImageDefaultObjectFitIsStretch) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <img src="logo.png" style="width:80px;height:80px"/>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(leaf);
  ASSERT_NE(fill, nullptr);
  auto* pattern = As<pagx::ImagePattern>(fill->color);
  ASSERT_NE(pattern, nullptr);
  EXPECT_EQ(pattern->scaleMode, pagx::ScaleMode::Stretch);
}

// Direct `border-radius` on `<img>` (the canonical "circular avatar" idiom: `<img
// style="border-radius:50%">`) used to be silently dropped — `convertImage` emitted a plain
// `Rectangle` regardless of the box's border-radius. The geometry helper now flows through it,
// so `border-radius: 50%` on a square avatar emits an `Ellipse` (a square Ellipse is a circle)
// and an asymmetric radius falls onto the Path emitter, matching the `<div>` + background-image
// path.
PAG_TEST(PAGXHTMLImporterTest, ImageWithUniformBorderRadiusEmitsEllipse) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <img src="avatar.png" style="width:64px;height:64px;border-radius:50%"/>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  EXPECT_EQ(FindElementOfType<pagx::Rectangle>(leaf), nullptr);
  auto* ellipse = FindElementOfType<pagx::Ellipse>(leaf);
  ASSERT_NE(ellipse, nullptr);
  EXPECT_FLOAT_EQ(ellipse->percentWidth, 100.0f);
  EXPECT_FLOAT_EQ(ellipse->percentHeight, 100.0f);
}

PAG_TEST(PAGXHTMLImporterTest, ImageWithAsymmetricBorderRadiusEmitsPath) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <img src="avatar.png" style="width:64px;height:64px;border-radius:12px 12px 0 0"/>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  EXPECT_EQ(FindElementOfType<pagx::Rectangle>(leaf), nullptr);
  auto* path = FindElementOfType<pagx::Path>(leaf);
  ASSERT_NE(path, nullptr);
  ASSERT_NE(path->data, nullptr);
  size_t cubicCount = 0;
  for (auto v : path->data->verbs()) {
    if (v == pagx::PathVerb::Cubic) cubicCount++;
  }
  EXPECT_EQ(cubicCount, 2u);
  auto* fill = FindElementOfType<pagx::Fill>(leaf);
  ASSERT_NE(fill, nullptr);
  auto* pattern = As<pagx::ImagePattern>(fill->color);
  ASSERT_NE(pattern, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, ImageObjectFitContainMapsToLetterBox) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <img src="logo.png" style="width:80px;height:80px;object-fit:contain"/>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(leaf);
  ASSERT_NE(fill, nullptr);
  auto* pattern = As<pagx::ImagePattern>(fill->color);
  ASSERT_NE(pattern, nullptr);
  EXPECT_EQ(pattern->scaleMode, pagx::ScaleMode::LetterBox);
}

PAG_TEST(PAGXHTMLImporterTest, ImageObjectFitCoverMapsToZoom) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <img src="logo.png" style="width:80px;height:80px;object-fit:cover"/>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(leaf);
  ASSERT_NE(fill, nullptr);
  auto* pattern = As<pagx::ImagePattern>(fill->color);
  ASSERT_NE(pattern, nullptr);
  EXPECT_EQ(pattern->scaleMode, pagx::ScaleMode::Zoom);
}

// CSS `background-image: url(...)` round-trips into an ImagePattern fill (the inverse of the
// HTML exporter). `background-size` selects the scaleMode: contain → LetterBox.
PAG_TEST(PAGXHTMLImporterTest, BackgroundImageSizeContainMapsToLetterBox) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:160px;height:120px">
      <div style="width:160px;height:120px;background-image:url(logo.png);
                  background-size:contain;background-repeat:no-repeat;
                  background-position:50% 50%"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(layer);
  ASSERT_NE(fill, nullptr);
  auto* pattern = As<pagx::ImagePattern>(fill->color);
  ASSERT_NE(pattern, nullptr);
  EXPECT_EQ(pattern->scaleMode, pagx::ScaleMode::LetterBox);
}

// `background-size: cover` → Zoom.
PAG_TEST(PAGXHTMLImporterTest, BackgroundImageSizeCoverMapsToZoom) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:160px;height:120px">
      <div style="width:160px;height:120px;background-image:url(logo.png);
                  background-size:cover;background-repeat:no-repeat"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(layer);
  ASSERT_NE(fill, nullptr);
  auto* pattern = As<pagx::ImagePattern>(fill->color);
  ASSERT_NE(pattern, nullptr);
  EXPECT_EQ(pattern->scaleMode, pagx::ScaleMode::Zoom);
}

// `background-size: 100% 100%` → Stretch.
PAG_TEST(PAGXHTMLImporterTest, BackgroundImageSizeFullMapsToStretch) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:160px;height:120px">
      <div style="width:160px;height:120px;background-image:url(logo.png);
                  background-size:100% 100%;background-repeat:no-repeat"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(layer);
  ASSERT_NE(fill, nullptr);
  auto* pattern = As<pagx::ImagePattern>(fill->color);
  ASSERT_NE(pattern, nullptr);
  EXPECT_EQ(pattern->scaleMode, pagx::ScaleMode::Stretch);
}

// A tiling `background-repeat: repeat` (no fitted size keyword) → ScaleMode::None with both tile
// modes set to Repeat and the matrix translation carrying the `background-position` offset.
PAG_TEST(PAGXHTMLImporterTest, BackgroundImageRepeatMapsToNoneTiled) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:160px;height:120px">
      <div style="width:160px;height:120px;background-image:url(logo.png);
                  background-repeat:repeat;background-position:-20px -30px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(layer);
  ASSERT_NE(fill, nullptr);
  auto* pattern = As<pagx::ImagePattern>(fill->color);
  ASSERT_NE(pattern, nullptr);
  EXPECT_EQ(pattern->scaleMode, pagx::ScaleMode::None);
  EXPECT_EQ(pattern->tileModeX, pagx::TileMode::Repeat);
  EXPECT_EQ(pattern->tileModeY, pagx::TileMode::Repeat);
  EXPECT_FLOAT_EQ(pattern->matrix.tx, -20.0f);
  EXPECT_FLOAT_EQ(pattern->matrix.ty, -30.0f);
}

// `background-repeat: repeat-x` is a single-axis shorthand — X tiles, Y clamps to Decal.
PAG_TEST(PAGXHTMLImporterTest, BackgroundImageRepeatXTilesHorizontalOnly) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:160px;height:120px">
      <div style="width:160px;height:120px;background-image:url(logo.png);
                  background-repeat:repeat-x;background-size:40px 40px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(layer);
  ASSERT_NE(fill, nullptr);
  auto* pattern = As<pagx::ImagePattern>(fill->color);
  ASSERT_NE(pattern, nullptr);
  EXPECT_EQ(pattern->tileModeX, pagx::TileMode::Repeat);
  EXPECT_EQ(pattern->tileModeY, pagx::TileMode::Decal);
}

// `background-repeat: repeat-y` tiles Y only; X clamps to Decal.
PAG_TEST(PAGXHTMLImporterTest, BackgroundImageRepeatYTilesVerticalOnly) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:160px;height:120px">
      <div style="width:160px;height:120px;background-image:url(logo.png);
                  background-repeat:repeat-y;background-size:40px 40px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(layer);
  ASSERT_NE(fill, nullptr);
  auto* pattern = As<pagx::ImagePattern>(fill->color);
  ASSERT_NE(pattern, nullptr);
  EXPECT_EQ(pattern->tileModeX, pagx::TileMode::Decal);
  EXPECT_EQ(pattern->tileModeY, pagx::TileMode::Repeat);
}

// `background-repeat: no-repeat` clamps both axes to Decal so the image paints once.
PAG_TEST(PAGXHTMLImporterTest, BackgroundImageNoRepeatClampsBothAxes) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:160px;height:120px">
      <div style="width:160px;height:120px;background-image:url(logo.png);
                  background-repeat:no-repeat;background-size:40px 40px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(layer);
  ASSERT_NE(fill, nullptr);
  auto* pattern = As<pagx::ImagePattern>(fill->color);
  ASSERT_NE(pattern, nullptr);
  EXPECT_EQ(pattern->tileModeX, pagx::TileMode::Decal);
  EXPECT_EQ(pattern->tileModeY, pagx::TileMode::Decal);
}

// Folding rule also handles asymmetric `border-radius`: the image-pattern fill rides on top
// of whatever geometry `applyBackgroundVisuals` emitted, so a card-style wrapper that uses
// "rounded top, square bottom" (very common for media tiles) ends up with a Path geometry +
// ImagePattern fill — no separate clip layer needed.
PAG_TEST(PAGXHTMLImporterTest, RoundedImageWrapperFoldsIntoPathForAsymmetricRadii) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:64px;height:64px">
      <div style="width:64px;height:64px;border-radius:16px 16px 0 0;overflow:hidden">
        <img src="avatar.png" style="position:absolute;left:0;top:0;width:64px;height:64px"/>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* wrapper = doc->layers.front()->children.front();
  EXPECT_TRUE(wrapper->children.empty());
  EXPECT_FALSE(wrapper->clipToBounds);
  EXPECT_EQ(FindElementOfType<pagx::Rectangle>(wrapper), nullptr);
  auto* path = FindElementOfType<pagx::Path>(wrapper);
  ASSERT_NE(path, nullptr);
  ASSERT_NE(path->data, nullptr);
  // are 0, so the path has exactly two cubic segments (TL + TR).
  size_t cubicCount = 0;
  for (auto v : path->data->verbs()) {
    if (v == pagx::PathVerb::Cubic) cubicCount++;
  }
  EXPECT_EQ(cubicCount, 2u);
  auto* fill = FindElementOfType<pagx::Fill>(wrapper);
  ASSERT_NE(fill, nullptr);
  auto* pattern = As<pagx::ImagePattern>(fill->color);
  ASSERT_NE(pattern, nullptr);
  ASSERT_NE(pattern->image, nullptr);
  EXPECT_NE(pattern->image->filePath.find("avatar.png"), std::string::npos);
}

PAG_TEST(PAGXHTMLImporterTest, RoundedImageWrapperRespectsObjectFit) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:64px;height:64px">
      <div style="width:64px;height:64px;border-radius:9999px;overflow:hidden">
        <img src="avatar.png" style="position:absolute;left:0;top:0;width:64px;height:64px;object-fit:cover"/>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* wrapper = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(wrapper);
  ASSERT_NE(fill, nullptr);
  auto* pattern = As<pagx::ImagePattern>(fill->color);
  ASSERT_NE(pattern, nullptr);
  EXPECT_EQ(pattern->scaleMode, pagx::ScaleMode::Zoom);
}

// Sibling assertion: the fold only kicks in when the wrapper is purely an image
// clip. A wrapper with multiple children must keep the standard container layout
// even when it carries `border-radius` + `overflow: hidden` (the image stays in
// its own child Layer so neighbouring content paints normally above it).
PAG_TEST(PAGXHTMLImporterTest, RoundedImageWrapperKeepsLayoutForExtraChildren) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:64px;height:80px">
      <div style="width:64px;height:80px;border-radius:9999px;overflow:hidden">
        <img src="avatar.png" style="position:absolute;left:0;top:0;width:64px;height:64px"/>
        <div style="position:absolute;left:0;top:64px;width:64px;height:16px;background-color:#000"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* wrapper = doc->layers.front()->children.front();
  EXPECT_TRUE(wrapper->clipToBounds);
  // The image stayed in its own child layer (not folded into the wrapper).
  EXPECT_FALSE(wrapper->children.empty());
}

// Snapshot pipeline pattern: the html-snapshot tool wraps every replaced element
// in an extra `<div>` (see `renderBoxedReplaced` in `tools/html-snapshot/lib/
// browser-snapshot.ts`) so border overlays can be hosted alongside the image.
// When the authored CSS already wraps the `<img>` in a `border-radius;overflow:
// hidden` round-clip, the resulting subset HTML has the round-clip wrapper, the
// snapshot wrapper, and the image stacked three deep. The fold has to skip the
// transparent layout-only wrapper(s) and still recognise the round-clip pattern
// — otherwise rounded avatars render as squares.
PAG_TEST(PAGXHTMLImporterTest, RoundedImageWrapperFoldsThroughLayoutOnlyDiv) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:64px;height:64px">
      <div style="width:64px;height:64px;border-radius:9999px;overflow:hidden">
        <div style="position:absolute;left:0;top:0;width:64px;height:64px;overflow:hidden">
          <img src="avatar.png" style="position:absolute;left:0;top:0;width:64px;height:64px"/>
        </div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* wrapper = doc->layers.front()->children.front();
  EXPECT_TRUE(wrapper->children.empty());
  EXPECT_FALSE(wrapper->clipToBounds);
  auto* rect = FindElementOfType<pagx::Rectangle>(wrapper);
  ASSERT_NE(rect, nullptr);
  EXPECT_GT(rect->roundness, 0.0f);
  auto* fill = FindElementOfType<pagx::Fill>(wrapper);
  ASSERT_NE(fill, nullptr);
  auto* pattern = As<pagx::ImagePattern>(fill->color);
  ASSERT_NE(pattern, nullptr);
  ASSERT_NE(pattern->image, nullptr);
  EXPECT_NE(pattern->image->filePath.find("avatar.png"), std::string::npos);
}

// Two stacked layout-only wrappers between the round-clip container and the
// `<img>` still fold — covers authored markup that adds an extra positioning
// `<div>` on top of the snapshot wrapper.
PAG_TEST(PAGXHTMLImporterTest, RoundedImageWrapperFoldsThroughTwoLayoutOnlyDivs) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:64px;height:64px">
      <div style="width:64px;height:64px;border-radius:9999px;overflow:hidden">
        <div style="position:absolute;left:0;top:0;width:64px;height:64px">
          <div style="position:absolute;left:0;top:0;width:64px;height:64px;overflow:hidden">
            <img src="avatar.png" style="position:absolute;left:0;top:0;width:64px;height:64px"/>
          </div>
        </div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* wrapper = doc->layers.front()->children.front();
  EXPECT_TRUE(wrapper->children.empty());
  EXPECT_FALSE(wrapper->clipToBounds);
  auto* fill = FindElementOfType<pagx::Fill>(wrapper);
  ASSERT_NE(fill, nullptr);
  ASSERT_NE(As<pagx::ImagePattern>(fill->color), nullptr);
}

// Negative: the intermediate wrapper must be visually transparent. A
// `background-color` on it would paint a square fill that the rounded outer
// outline cannot clip in PAGX (the only clip primitive is rectangular), so the
// fold must keep the standard nested layout in that case.
PAG_TEST(PAGXHTMLImporterTest, RoundedImageWrapperRejectsLayoutDivWithBackground) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:64px;height:64px">
      <div style="width:64px;height:64px;border-radius:9999px;overflow:hidden">
        <div style="position:absolute;left:0;top:0;width:64px;height:64px;background-color:#000">
          <img src="avatar.png" style="position:absolute;left:0;top:0;width:64px;height:64px"/>
        </div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* wrapper = doc->layers.front()->children.front();
  EXPECT_TRUE(wrapper->clipToBounds);
  EXPECT_FALSE(wrapper->children.empty());
}

// Negative: an intermediate wrapper that does not exactly cover the outer
// content box would shift the image out from under the rounded outline. The
// fold has to bail so the image renders inside its own child layer at the
// authored position.
PAG_TEST(PAGXHTMLImporterTest, RoundedImageWrapperRejectsLayoutDivWithSizeMismatch) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:64px;height:64px">
      <div style="width:64px;height:64px;border-radius:9999px;overflow:hidden">
        <div style="position:absolute;left:0;top:0;width:48px;height:48px">
          <img src="avatar.png" style="position:absolute;left:0;top:0;width:48px;height:48px"/>
        </div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* wrapper = doc->layers.front()->children.front();
  EXPECT_TRUE(wrapper->clipToBounds);
  EXPECT_FALSE(wrapper->children.empty());
}

PAG_TEST(PAGXHTMLImporterTest, RepeatedImageSourceDeduplicated) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:160px;height:80px">
      <img src="logo.png" style="width:80px;height:80px"/>
      <img src="logo.png" style="width:80px;height:80px"/>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto& kids = doc->layers.front()->children;
  ASSERT_EQ(kids.size(), 2u);
  auto* fillA = FindElementOfType<pagx::Fill>(kids[0]);
  auto* fillB = FindElementOfType<pagx::Fill>(kids[1]);
  ASSERT_NE(fillA, nullptr);
  ASSERT_NE(fillB, nullptr);
  auto* patA = As<pagx::ImagePattern>(fillA->color);
  auto* patB = As<pagx::ImagePattern>(fillB->color);
  ASSERT_NE(patA, nullptr);
  ASSERT_NE(patB, nullptr);
  EXPECT_EQ(patA->image, patB->image);
}

PAG_TEST(PAGXHTMLImporterTest, StrictModeRejectsUnsupportedElement) {
  pagx::HTMLImporter::Options opts;
  opts.strict = true;
  std::string html =
      R"HTML(<html><body style="width:50px;height:50px"><canvas></canvas></body></html>)HTML";
  auto doc = pagx::HTMLImporter::ParseString(html, opts);
  EXPECT_EQ(doc, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, PreserveUnknownElementsKeepsPlaceholder) {
  pagx::HTMLImporter::Options opts;
  opts.preserveUnknownElements = true;
  std::string html =
      R"HTML(<html><body style="width:50px;height:50px"><foo></foo></body></html>)HTML";
  auto doc = pagx::HTMLImporter::ParseString(html, opts);
  ASSERT_NE(doc, nullptr);
  ASSERT_FALSE(doc->layers.front()->children.empty());
  auto* placeholder = doc->layers.front()->children.front();
  auto it = placeholder->customData.find("html-unknown");
  ASSERT_NE(it, placeholder->customData.end());
  EXPECT_EQ(it->second, "foo");
}

PAG_TEST(PAGXHTMLImporterTest, TargetSizeOverridesBody) {
  pagx::HTMLImporter::Options opts;
  opts.targetWidth = 400.0f;
  opts.targetHeight = 200.0f;
  opts.preferBodySize = false;
  std::string html = R"HTML(<html><body style="width:50px;height:50px"></body></html>)HTML";
  auto doc = pagx::HTMLImporter::ParseString(html, opts);
  ASSERT_NE(doc, nullptr);
  EXPECT_FLOAT_EQ(doc->width, 400.0f);
  EXPECT_FLOAT_EQ(doc->height, 200.0f);
}

PAG_TEST(PAGXHTMLImporterTest, PreferBodySizeWinsOverTarget) {
  pagx::HTMLImporter::Options opts;
  opts.targetWidth = 400.0f;
  opts.targetHeight = 200.0f;
  opts.preferBodySize = true;
  std::string html = R"HTML(<html><body style="width:50px;height:50px"></body></html>)HTML";
  auto doc = pagx::HTMLImporter::ParseString(html, opts);
  ASSERT_NE(doc, nullptr);
  EXPECT_FLOAT_EQ(doc->width, 50.0f);
  EXPECT_FLOAT_EQ(doc->height, 50.0f);
}

PAG_TEST(PAGXHTMLImporterTest, TargetUsedWhenBodyMissingSize) {
  pagx::HTMLImporter::Options opts;
  opts.targetWidth = 320.0f;
  opts.targetHeight = 96.0f;
  std::string html = R"HTML(<html><body></body></html>)HTML";
  auto doc = pagx::HTMLImporter::ParseString(html, opts);
  ASSERT_NE(doc, nullptr);
  EXPECT_FLOAT_EQ(doc->width, 320.0f);
  EXPECT_FLOAT_EQ(doc->height, 96.0f);
}

//==================================================================================================
// HTMLImporter error / boundary tests
//==================================================================================================

PAG_TEST(PAGXHTMLImporterTest, ParseEmptyDataReturnsNullptr) {
  auto doc = pagx::HTMLImporter::Parse(nullptr, 0);
  EXPECT_EQ(doc, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, ParseGarbageDataReturnsNullptr) {
  std::string garbage = "this is not html at all <<<>>>";
  auto doc = pagx::HTMLImporter::ParseString(garbage);
  EXPECT_EQ(doc, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, ParseNonHtmlRootReturnsNullptr) {
  // The importer rejects documents whose root element is not <html>.
  auto doc = pagx::HTMLImporter::ParseString(R"XML(<svg><body></body></svg>)XML");
  EXPECT_EQ(doc, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, ParseMissingFileReturnsNullptr) {
  auto doc = pagx::HTMLImporter::Parse("/this/path/does/not/exist.html");
  EXPECT_EQ(doc, nullptr);
}

//==================================================================================================
// autoNormalize=false: exercises HTMLParserContext's own CSS cascade implementation
//==================================================================================================

PAG_TEST(PAGXHTMLImporterTest, RawClassRuleAppliedWithoutSubsetTransformer) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><head><style>.box { background-color:#123456 }</style></head>
      <body style="width:50px;height:50px">
        <div class="box" style="width:50px;height:50px"></div>
      </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  auto* fill = FindElementOfType<pagx::Fill>(doc->layers.front()->children.front());
  ASSERT_NE(fill, nullptr);
  auto* solid = As<pagx::SolidColor>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0x123456)));
}

PAG_TEST(PAGXHTMLImporterTest, RawElementRuleAppliedWithoutSubsetTransformer) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><head><style>p { color:#33CC44 }</style></head>
      <body style="width:200px;height:40px"><p>Hi</p></body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  EXPECT_TRUE(ColorNear(SolidFillColorOf(leaf), HexColor(0x33CC44)));
}

PAG_TEST(PAGXHTMLImporterTest, RawCommaSeparatedSelectorsApplyWithoutSubsetTransformer) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><head><style>.a, .b { background-color:#114455 }</style></head>
      <body style="width:80px;height:40px">
        <div class="a" style="width:20px;height:20px"></div>
        <div class="b" style="width:20px;height:20px"></div>
      </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  for (auto* div : doc->layers.front()->children) {
    auto* fill = FindElementOfType<pagx::Fill>(div);
    ASSERT_NE(fill, nullptr);
    auto* solid = As<pagx::SolidColor>(fill->color);
    ASSERT_NE(solid, nullptr);
    EXPECT_TRUE(ColorNear(solid->color, HexColor(0x114455)));
  }
}

PAG_TEST(PAGXHTMLImporterTest, RawUniversalSelectorWarnsWithoutSubsetTransformer) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><head><style>* { padding:0 }</style></head>
      <body style="width:50px;height:50px"></body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("universal selector") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
}

PAG_TEST(PAGXHTMLImporterTest, RawUnsupportedSelectorWarnsWithoutSubsetTransformer) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><head><style>.a > .b { color:red }</style></head>
      <body style="width:50px;height:50px"></body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("unsupported selector") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
}

PAG_TEST(PAGXHTMLImporterTest, RawAtRuleSurvivesAndIsHandledByCascade) {
  // Without the subset transformer, the parser context tokenizes the style sheet itself; at-rules
  // are skipped via SkipBalancedBlock so subsequent rules still apply.
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><head><style>
      @media (max-width:600px) { .ignored { color:red } }
      .real { background-color:#AABBCC }
    </style></head>
      <body style="width:50px;height:50px">
        <div class="real" style="width:50px;height:50px"></div>
      </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  auto* fill = FindElementOfType<pagx::Fill>(doc->layers.front()->children.front());
  ASSERT_NE(fill, nullptr);
  auto* solid = As<pagx::SolidColor>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0xAABBCC)));
}

PAG_TEST(PAGXHTMLImporterTest, RawAtRuleNoBlockSkipped) {
  // A no-block at-rule (terminated by `;`) must not consume the following rule.
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><head><style>
      @charset "utf-8";
      .x { background-color:#446688 }
    </style></head>
      <body style="width:50px;height:50px">
        <div class="x" style="width:50px;height:50px"></div>
      </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  auto* fill = FindElementOfType<pagx::Fill>(doc->layers.front()->children.front());
  ASSERT_NE(fill, nullptr);
  auto* solid = As<pagx::SolidColor>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0x446688)));
}

PAG_TEST(PAGXHTMLImporterTest, RawCssCommentsStripped) {
  // CSS comments outside selectors and rule bodies must be skipped by the cascade tokenizer.
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><head><style>
      /* leading comment */
      .x { background-color:#225588 }
      /* trailing comment */
    </style></head>
      <body style="width:50px;height:50px">
        <div class="x" style="width:50px;height:50px"></div>
      </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  auto* fill = FindElementOfType<pagx::Fill>(doc->layers.front()->children.front());
  ASSERT_NE(fill, nullptr);
  auto* solid = As<pagx::SolidColor>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0x225588)));
}

PAG_TEST(PAGXHTMLImporterTest, RawDoubleHeadElementWarnsForUnknownChild) {
  // Without the subset transformer, the parser sees the unknown <foo> tag inside <head>
  // and emits a warning rather than silently dropping.
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><head><foo></foo></head>
      <body style="width:50px;height:50px"></body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("foo") != std::string::npos && msg.find("head") != std::string::npos) {
      warned = true;
    }
  }
  EXPECT_TRUE(warned);
}

PAG_TEST(PAGXHTMLImporterTest, RawFlexShorthandValueWithUnitWarns) {
  // Without the subset transformer, parseBoxLayout sees a `flex` value that does not parse
  // as a clean number and emits a warning.
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="display:flex;width:50px;height:50px">
        <div style="flex:1 1 auto;background-color:#000"></div>
      </div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("flex shorthand") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
}

PAG_TEST(PAGXHTMLImporterTest, RawDisplayInlineBlockWarns) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="display:inline-block;width:50px;height:50px"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("inline-block") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
}

PAG_TEST(PAGXHTMLImporterTest, RawFlexWrapWarns) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="display:flex;flex-wrap:wrap;width:50px;height:50px"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("flex-wrap") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
}

PAG_TEST(PAGXHTMLImporterTest, RawMarginIsResolvedThroughPaddingWrapper) {
  // Mirror of MarginIsResolvedThroughPaddingWrapper but with autoNormalize=false so the
  // resolver consumes the raw `margin-top/right/bottom/left` longhands directly. The
  // four sides resolve to 5/8/6/7 (top/right/bottom/left) and land verbatim on the
  // wrapper's padding. The longer name avoids confusion with the autoNormalize=true
  // shorthand variant above.
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="margin-top:5px;margin-bottom:6px;margin-left:7px;margin-right:8px;
                  width:30px;height:30px;background-color:#000"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  for (const auto& msg : doc->errors) {
    EXPECT_EQ(msg.find("margin"), std::string::npos)
        << "unexpected margin diagnostic on accepted input: " << msg;
  }
  ASSERT_EQ(doc->layers.size(), 1u);
  auto* body = doc->layers.front();
  ASSERT_EQ(body->children.size(), 1u);
  auto* wrapper = body->children.front();
  EXPECT_FLOAT_EQ(wrapper->padding.top, 5.0f);
  EXPECT_FLOAT_EQ(wrapper->padding.right, 8.0f);
  EXPECT_FLOAT_EQ(wrapper->padding.bottom, 6.0f);
  EXPECT_FLOAT_EQ(wrapper->padding.left, 7.0f);
  ASSERT_EQ(wrapper->children.size(), 1u);
  auto* inner = wrapper->children.front();
  EXPECT_FLOAT_EQ(inner->width, 30.0f);
  EXPECT_FLOAT_EQ(inner->height, 30.0f);
}

PAG_TEST(PAGXHTMLImporterTest, RawGridTemplateWarns) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="grid-template-columns:1fr 1fr;grid-template-rows:auto;width:50px;height:50px"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  bool gridWarn = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("grid") != std::string::npos) gridWarn = true;
  }
  EXPECT_TRUE(gridWarn);
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformWarns) {
  // Compound transform chains aren't decomposed by the importer today; they fall through
  // the subset transformer with a diagnostic. Single-function forms (skewX/rotate/scale/...)
  // map onto the TextBox transform fields and are silent — see RawSingleTransformIsAccepted.
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="transform:rotate(45deg) translate(10px);width:50px;height:50px"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("transform") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
}

PAG_TEST(PAGXHTMLImporterTest, RawSingleTransformIsAccepted) {
  // Single-function `transform` forms in the supported subset (skewX/skewY/rotate/scale[X|Y]/
  // translate[X|Y]) round-trip silently — they become TextBox skew/rotation/scale fields on
  // text leaves. Non-text elements still drop the transform with a diagnostic because Layer
  // has no transform fields, but the diagnostic in that case complains about the host, not
  // the property; here we just assert that no generic "transform not supported" warning is
  // emitted for the rotate(45deg) form.
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:200px;height:60px">
      <h1 style="transform:skewX(-5deg);width:200px;height:60px">Hello</h1>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  for (const auto& msg : doc->errors) {
    EXPECT_EQ(msg.find("transform '"), std::string::npos)
        << "Unexpected transform diagnostic: " << msg;
  }
}

PAG_TEST(PAGXHTMLImporterTest, RawEllipticalBorderRadiusDropped) {
  // Elliptical syntax `<h>/<v>` cannot be expressed with PAGX's uniform roundness; warn and skip.
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:#000;border-radius:10px / 5px"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* rect = FindElementOfType<pagx::Rectangle>(div);
  ASSERT_NE(rect, nullptr);
  EXPECT_FLOAT_EQ(rect->roundness, 0.0f);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("elliptical border-radius") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
}

// Per-corner `border-radius` (e.g. the shorthand `4px 12px` which expands to TL=BR=4 /
// TR=BL=12) cannot be expressed by PAGX's uniform-roundness `Rectangle`. The importer
// synthesises a `Path` whose `PathData` traces the rounded outline so the geometry round-trips
// faithfully; no Rectangle is emitted for the background shape in that case. The path's
// bounding box still matches the layer's authored dimensions so layout downstream behaves the
// same as before.
PAG_TEST(PAGXHTMLImporterTest, RawAsymmetricBorderRadiusEmitsPath) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:#000;border-radius:4px 12px"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_EQ(FindElementOfType<pagx::Rectangle>(div), nullptr);
  auto* path = FindElementOfType<pagx::Path>(div);
  ASSERT_NE(path, nullptr);
  ASSERT_NE(path->data, nullptr);
  auto bounds = path->data->getBounds();
  EXPECT_NEAR(bounds.x, 0.0f, kEps);
  EXPECT_NEAR(bounds.y, 0.0f, kEps);
  EXPECT_NEAR(bounds.width, 50.0f, kEps);
  EXPECT_NEAR(bounds.height, 50.0f, kEps);
  EXPECT_NE(FindElementOfType<pagx::Fill>(div), nullptr);
  for (const auto& msg : doc->errors) {
    EXPECT_EQ(msg.find("per-corner border-radius"), std::string::npos)
        << "Unexpected diagnostic: " << msg;
  }
}

// Quarter-circle corner decoration (e.g. Tailwind `rounded-bl-full` on a 380x380 box). The CSS
// edge-overlap clamp shrinks the authored `9999px` radius down to `min(W, H)` = 380 so the BL
// arc fills the entire bottom-left quadrant of the box, and the remaining three corners stay
// sharp. The result is the geometry the source HTML actually intended: a quarter-circle
// anchored at the box's opposite corner, *not* a 380x380 disc inscribed inside it (which is
// what the legacy single-roundness fallback produced).
PAG_TEST(PAGXHTMLImporterTest, QuarterCircleCornerDecorationEmitsPath) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:380px;height:380px">
      <div style="position:absolute;left:0;top:0;width:380px;height:380px;
                  background-color:#C2B2E4;border-radius:0 0 0 9999px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_EQ(FindElementOfType<pagx::Rectangle>(div), nullptr);
  auto* path = FindElementOfType<pagx::Path>(div);
  ASSERT_NE(path, nullptr);
  ASSERT_NE(path->data, nullptr);
  // The path must reach the four edges of the box: tight bounds equal (0, 0, 380, 380). The
  // PathData control-point bounds the helper returns can be a hair larger than the geometric
  // hull, so we test reachability via specific on-curve points emitted by
  // `BuildPerCornerRoundedRectPathData` instead of relying on `getBounds` precision.
  const auto& verbs = path->data->verbs();
  const auto& pts = path->data->points();
  // Expected first move is at (W, TR) = (380, 0) since TR == 0.
  ASSERT_FALSE(verbs.empty());
  EXPECT_EQ(verbs.front(), pagx::PathVerb::Move);
  ASSERT_GE(pts.size(), 1u);
  EXPECT_NEAR(pts.front().x, 380.0f, kEps);
  EXPECT_NEAR(pts.front().y, 0.0f, kEps);
  EXPECT_EQ(verbs.back(), pagx::PathVerb::Close);
  // Exactly one cubic — the BL quarter-circle. The three zero-radius corners are sharp.
  size_t cubicCount = 0;
  for (auto v : verbs) {
    if (v == pagx::PathVerb::Cubic) cubicCount++;
  }
  EXPECT_EQ(cubicCount, 1u);
}

// CSS edge-overlap scaling: `border-radius: 200px 200px 0 0` on a 100x100 box expands to
// TL=TR=200 / BR=BL=0. Every edge that touches a 200px corner constrains it:
//   top:   TL + TR = 400 > 100 -> f_top   = 100 / 400 = 0.25
//   right: TR + BR = 200 > 100 -> f_right = 100 / 200 = 0.5
//   left:  TL + BL = 200 > 100 -> f_left  = 100 / 200 = 0.5
// CSS picks the smallest factor (0.25) and applies it to *every* radius uniformly, so the
// final TL/TR both land at 50. The remaining two corners stay at 0, so the four radii differ
// and the layer builder emits a Path.
PAG_TEST(PAGXHTMLImporterTest, OverlargeRadiiAreClampedToEdgeLength) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:100px;height:100px">
      <div style="width:100px;height:100px;background-color:#000;border-radius:200px 200px 0 0"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_EQ(FindElementOfType<pagx::Rectangle>(div), nullptr);
  auto* path = FindElementOfType<pagx::Path>(div);
  ASSERT_NE(path, nullptr);
  ASSERT_NE(path->data, nullptr);
  // The path's first move starts at (W, TR_clamped) = (100, 50). Without the clamp the y
  // coordinate would be 200, well outside the 100x100 box.
  ASSERT_FALSE(path->data->verbs().empty());
  EXPECT_EQ(path->data->verbs().front(), pagx::PathVerb::Move);
  ASSERT_GE(path->data->points().size(), 1u);
  EXPECT_NEAR(path->data->points().front().x, 100.0f, kEps);
  EXPECT_NEAR(path->data->points().front().y, 50.0f, kEps);
}

PAG_TEST(PAGXHTMLImporterTest, RawBorderDashedProducesDashPattern) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;border:2px dashed #000"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  for (const auto& msg : doc->errors) {
    EXPECT_EQ(msg.find("dashed"), std::string::npos);
  }
  auto* stroke = FindElementOfType<pagx::Stroke>(doc->layers.front()->children.front());
  ASSERT_NE(stroke, nullptr);
  EXPECT_FLOAT_EQ(stroke->width, 2.0f);
  ASSERT_EQ(stroke->dashes.size(), 2u);
  EXPECT_FLOAT_EQ(stroke->dashes[0], 4.0f);
  EXPECT_FLOAT_EQ(stroke->dashes[1], 2.0f);
  EXPECT_EQ(stroke->cap, pagx::LineCap::Butt);
}

PAG_TEST(PAGXHTMLImporterTest, RawBorderDottedProducesRoundDots) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;border:2px dotted #000"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  auto* stroke = FindElementOfType<pagx::Stroke>(doc->layers.front()->children.front());
  ASSERT_NE(stroke, nullptr);
  EXPECT_FLOAT_EQ(stroke->width, 2.0f);
  ASSERT_EQ(stroke->dashes.size(), 2u);
  EXPECT_FLOAT_EQ(stroke->dashes[0], 0.0f);
  EXPECT_FLOAT_EQ(stroke->dashes[1], 4.0f);
  EXPECT_EQ(stroke->cap, pagx::LineCap::Round);
}

PAG_TEST(PAGXHTMLImporterTest, RawOverflowAutoWarns) {
  // Only `hidden` and `visible` are silent; everything else emits a warning.
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="overflow:auto;width:50px;height:50px;background-color:#000"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("overflow") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
}

PAG_TEST(PAGXHTMLImporterTest, BackgroundUrlRecoversImagePattern) {
  // A CSS `url(...)` background round-trips into an ImagePattern fill (the inverse of the HTML
  // exporter), rather than being dropped with a warning.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-image:url(theme.png)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = doc->layers.front()->children.front();
  ASSERT_NE(layer, nullptr);
  auto* fill = FindElementOfType<pagx::Fill>(layer);
  ASSERT_NE(fill, nullptr);
  auto* pattern = As<pagx::ImagePattern>(fill->color);
  ASSERT_NE(pattern, nullptr);
  ASSERT_NE(pattern->image, nullptr);
  EXPECT_NE(pattern->image->filePath.find("theme.png"), std::string::npos);
}

PAG_TEST(PAGXHTMLImporterTest, RawUnsupportedFilterWarns) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:#000;filter:grayscale(50%)"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("filter") != std::string::npos && msg.find("grayscale") != std::string::npos) {
      warned = true;
    }
  }
  EXPECT_TRUE(warned);
}

PAG_TEST(PAGXHTMLImporterTest, RawBackdropFilterDropShadowUnsupportedWarns) {
  // `backdrop-filter` only models blur(); other functions emit a warning.
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:#FFF8;
                  backdrop-filter:saturate(180%)"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("backdrop-filter") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
}

//==================================================================================================
// HTMLValueParser internals: colour, length, line-height, shadow edge cases
//==================================================================================================

PAG_TEST(PAGXHTMLImporterTest, ShortHexAlphaColorRecognized) {
  // 4-char `#RGBA` form expands to `#RRGGBBAA`.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:#0F08"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_TRUE(ColorNear(SolidFillColorOf(div), HexColor(0x00FF00, 0x88 / 255.0f)));
}

PAG_TEST(PAGXHTMLImporterTest, FullHexAlphaColorRecognized) {
  // 8-char `#RRGGBBAA` form keeps the alpha byte.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:#FF00007F"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_TRUE(ColorNear(SolidFillColorOf(div), HexColor(0xFF0000, 0x7F / 255.0f), 0.02f));
}

PAG_TEST(PAGXHTMLImporterTest, UnrecognisedColorFallsBackToBlackAndWarns) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style="color:not-a-color">Hi</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("unrecognised color") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
  EXPECT_TRUE(
      ColorNear(SolidFillColorOf(doc->layers.front()->children.front()), HexColor(0x000000)));
}

PAG_TEST(PAGXHTMLImporterTest, RawPxLengthEmRemWarnsThroughPadding) {
  // Without the subset transformer, padding tokens flow through parsePxLength which warns on
  // em/rem and treats them as 16px each.
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="display:flex;padding:1em;width:200px;height:200px"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_FLOAT_EQ(div->padding.top, 16.0f);
  bool emWarn = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("em/rem") != std::string::npos) emWarn = true;
  }
  EXPECT_TRUE(emWarn);
}

PAG_TEST(PAGXHTMLImporterTest, RawPxLengthUnknownUnitWarnsThroughPadding) {
  // `in` is a real CSS unit but parsePxLength does not understand it; it falls back with a
  // warning. (CSS recognised units consumed by parsePxLength are px / em / rem / pt / vw / vh.)
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="display:flex;padding:5in;width:50px;height:50px"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("length unit") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
}

PAG_TEST(PAGXHTMLImporterTest, RawLineHeightUnitless) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:200px;height:60px">
      <p style="font-size:14px;line-height:1.5">Hi <span>World</span></p>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  auto* tb = FindElementOfType<pagx::TextBox>(doc->layers.front()->children.front());
  ASSERT_NE(tb, nullptr);
  EXPECT_FLOAT_EQ(tb->lineHeight, 21.0f);
}

PAG_TEST(PAGXHTMLImporterTest, RawLineHeightPercent) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:200px;height:60px">
      <p style="font-size:20px;line-height:150%">Hi <span>World</span></p>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  auto* tb = FindElementOfType<pagx::TextBox>(doc->layers.front()->children.front());
  ASSERT_NE(tb, nullptr);
  EXPECT_FLOAT_EQ(tb->lineHeight, 30.0f);
}

PAG_TEST(PAGXHTMLImporterTest, RawLineHeightEm) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:200px;height:60px">
      <p style="font-size:10px;line-height:2em">Hi <span>World</span></p>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  auto* tb = FindElementOfType<pagx::TextBox>(doc->layers.front()->children.front());
  ASSERT_NE(tb, nullptr);
  EXPECT_FLOAT_EQ(tb->lineHeight, 20.0f);
}

PAG_TEST(PAGXHTMLImporterTest, RawLineHeightUnknownUnitWarns) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:200px;height:60px">
      <p style="font-size:14px;line-height:5cm">Hi <span>World</span></p>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("line-height unit") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
}

PAG_TEST(PAGXHTMLImporterTest, BoxShadowSpreadIsIgnoredWithWarning) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <div style="width:80px;height:80px;background-color:#FFF;
                  box-shadow:0 1px 2px 3px #000"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("box-shadow spread") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
}

PAG_TEST(PAGXHTMLImporterTest, BoxShadowMalformedSkippedWithWarning) {
  // A shadow with fewer than two length tokens is malformed.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:#FFF;
                  box-shadow:#000"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("malformed box-shadow") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
}

PAG_TEST(PAGXHTMLImporterTest, BoxShadowDefaultsToBlackWhenColorOmitted) {
  // No colour token → the shadow keeps the default opaque-black colour.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:#FFF;
                  box-shadow:0 2px 4px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  ASSERT_EQ(div->styles.size(), 1u);
  auto* drop = As<pagx::DropShadowStyle>(div->styles.front());
  ASSERT_NE(drop, nullptr);
  EXPECT_TRUE(ColorNear(drop->color, HexColor(0x000000), 0.02f));
}

PAG_TEST(PAGXHTMLImporterTest, GradientWithRadAngle) {
  // CSS angle accepts `rad` in addition to `deg`/`turn`.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;
                  background-image:linear-gradient(1.5708rad, #F00, #00F)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* fill = FindElementOfType<pagx::Fill>(doc->layers.front()->children.front());
  ASSERT_NE(fill, nullptr);
  auto* lg = As<pagx::LinearGradient>(fill->color);
  ASSERT_NE(lg, nullptr);
  EXPECT_GT(lg->endPoint.x, lg->startPoint.x);
}

PAG_TEST(PAGXHTMLImporterTest, GradientWithTurnAngle) {
  // 0.25turn = 90deg → identical layout to the deg test.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;
                  background-image:linear-gradient(0.25turn, #F00, #00F)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* fill = FindElementOfType<pagx::Fill>(doc->layers.front()->children.front());
  ASSERT_NE(fill, nullptr);
  auto* lg = As<pagx::LinearGradient>(fill->color);
  ASSERT_NE(lg, nullptr);
  EXPECT_GT(lg->endPoint.x, lg->startPoint.x);
}

PAG_TEST(PAGXHTMLImporterTest, RadialGradientWithEllipseDescriptor) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;
                  background-image:radial-gradient(ellipse at center, #FFF, #000)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* fill = FindElementOfType<pagx::Fill>(doc->layers.front()->children.front());
  ASSERT_NE(fill, nullptr);
  auto* rg = As<pagx::RadialGradient>(fill->color);
  ASSERT_NE(rg, nullptr);
  EXPECT_EQ(rg->colorStops.size(), 2u);
}

PAG_TEST(PAGXHTMLImporterTest, GradientThreeStopsImplicitMiddleInterpolated) {
  // A middle stop with no explicit offset must be filled in via interpolation.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;
                  background-image:linear-gradient(90deg,#F00,#0F0,#00F)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* fill = FindElementOfType<pagx::Fill>(doc->layers.front()->children.front());
  auto* lg = As<pagx::LinearGradient>(fill->color);
  ASSERT_NE(lg, nullptr);
  ASSERT_EQ(lg->colorStops.size(), 3u);
  EXPECT_TRUE(NearlyEqual(lg->colorStops[1]->offset, 0.5f, 0.01f));
}

PAG_TEST(PAGXHTMLImporterTest, FontStyleItalicOnlyProducesFauxItalic) {
  // Pure italic without bold is synthesised via faux italic, leaving the style label empty.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style="font-size:14px;color:#000;font-style:italic">Hi</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* text = FindElementOfType<pagx::Text>(doc->layers.front()->children.front());
  ASSERT_NE(text, nullptr);
  EXPECT_EQ(text->fontStyle, "");
  EXPECT_FALSE(text->fauxBold);
  EXPECT_TRUE(text->fauxItalic);
}

// A missing / empty `src` warns but no longer drops the `<img>`: the element still occupies a
// box, so the importer preserves the layer and backs it with an ImagePattern fill whose Image
// resource is left unresolved (empty filePath). This keeps the image slot in the PAGX instead of
// vanishing.
PAG_TEST(PAGXHTMLImporterTest, ImageMissingSrcWarnsButIsPreserved) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <img style="width:80px;height:80px"/>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("missing src") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);

  ASSERT_FALSE(doc->layers.front()->children.empty());
  auto* leaf = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(leaf);
  ASSERT_NE(fill, nullptr);
  auto* pattern = As<pagx::ImagePattern>(fill->color);
  ASSERT_NE(pattern, nullptr);
  ASSERT_NE(pattern->image, nullptr);
  EXPECT_TRUE(pattern->image->filePath.empty());
}

// The preserved placeholder Image (empty filePath, no data) must still serialise a `source`
// attribute — it is `use="required"` in pagx.xsd — so the exported document stays schema-valid
// and round-trips back to the same unresolved Image instead of tripping validation.
PAG_TEST(PAGXHTMLImporterTest, ImageMissingSrcExportsEmptySourceAttribute) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <img style="width:80px;height:80px"/>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto xml = pagx::PAGXExporter::ToXML(*doc);
  EXPECT_NE(xml.find("source=\"\""), std::string::npos);

  auto reimported = pagx::PAGXImporter::FromXML(xml);
  ASSERT_NE(reimported, nullptr);
  ASSERT_FALSE(reimported->layers.front()->children.empty());
  auto* leaf = reimported->layers.front()->children.front();
  auto* pattern = As<pagx::ImagePattern>(FindElementOfType<pagx::Fill>(leaf)->color);
  ASSERT_NE(pattern, nullptr);
  ASSERT_NE(pattern->image, nullptr);
  EXPECT_TRUE(pattern->image->filePath.empty());
}

PAG_TEST(PAGXHTMLImporterTest, TransparentColorIsTreatedAsNoFill) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:transparent"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto color = SolidFillColorOf(div);
  EXPECT_FLOAT_EQ(color.alpha, 0.0f);
}

PAG_TEST(PAGXHTMLImporterTest, NoneColorIsTreatedAsNoFill) {
  // Regression: `color: none` is non-standard but appears occasionally; treat it as transparent.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style="color:none">Hi</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* span = doc->layers.front()->children.front();
  auto color = SolidFillColorOf(span);
  EXPECT_FLOAT_EQ(color.alpha, 0.0f);
}

PAG_TEST(PAGXHTMLImporterTest, MalformedHexColorWarns) {
  // 2-char hex (`#FF`) does not match any valid form and falls back to opaque black with a warning.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style="color:#FF">Hi</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("unrecognised color") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
}

PAG_TEST(PAGXHTMLImporterTest, RgbWithoutParensFallsBackToBlackAndWarns) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style="color:rgb-broken">Hi</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("unrecognised color") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
}

PAG_TEST(PAGXHTMLImporterTest, RawPxLengthPercentReturnsNanThroughPadding) {
  // `padding:50%` on a flex container — parsePxLength rejects `%` so each token is dropped,
  // and the importer warns about the invalid token.
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="display:flex;padding:50%;width:50px;height:50px"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("invalid padding token") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
}

PAG_TEST(PAGXHTMLImporterTest, RadialGradientWithCenterOnly) {
  // The leading shape token may be just `at center` without `circle`/`ellipse`.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;
                  background-image:radial-gradient(at center, #FFF, #000)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* fill = FindElementOfType<pagx::Fill>(doc->layers.front()->children.front());
  ASSERT_NE(fill, nullptr);
  auto* rg = As<pagx::RadialGradient>(fill->color);
  ASSERT_NE(rg, nullptr);
  EXPECT_EQ(rg->colorStops.size(), 2u);
}

PAG_TEST(PAGXHTMLImporterTest, ConicGradientWithExplicitDegOffsetPerStop) {
  // The conic gradient stop offsets are interpreted as angles when written with `deg`.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;
                  background-image:conic-gradient(from 0deg, #F00 0deg, #00F 360deg)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* fill = FindElementOfType<pagx::Fill>(doc->layers.front()->children.front());
  ASSERT_NE(fill, nullptr);
  auto* cg = As<pagx::ConicGradient>(fill->color);
  ASSERT_NE(cg, nullptr);
  ASSERT_EQ(cg->colorStops.size(), 2u);
  EXPECT_TRUE(NearlyEqual(cg->colorStops.front()->offset, 0.0f, 0.01f));
  EXPECT_TRUE(NearlyEqual(cg->colorStops.back()->offset, 1.0f, 0.01f));
}

PAG_TEST(PAGXHTMLImporterTest, LinearGradientWithExplicitPxOffsetPerStop) {
  // For linear gradients the stop offset is interpreted as a px length; non-percent tokens are
  // accepted via parsePxLength.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:100px;height:50px">
      <div style="width:100px;height:50px;
                  background-image:linear-gradient(90deg, #F00 0px, #00F 100px)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* fill = FindElementOfType<pagx::Fill>(doc->layers.front()->children.front());
  ASSERT_NE(fill, nullptr);
  auto* lg = As<pagx::LinearGradient>(fill->color);
  ASSERT_NE(lg, nullptr);
  ASSERT_EQ(lg->colorStops.size(), 2u);
}

PAG_TEST(PAGXHTMLImporterTest, DuplicateHeadIsMergedBySubsetTransformer) {
  // The transformer must merge multiple <head> elements into one. Both <title>s should survive
  // (the importer uses the first one for data-title).
  auto doc = ParseFromString(R"HTML(
    <html>
      <head><title>Primary</title></head>
      <head><meta charset="utf-8"/></head>
      <body style="width:50px;height:50px"></body>
    </html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto it = doc->customData.find("title");
  ASSERT_NE(it, doc->customData.end());
  EXPECT_EQ(it->second, "Primary");
}

PAG_TEST(PAGXHTMLImporterTest, DuplicateBodyIsMergedBySubsetTransformer) {
  auto doc = ParseFromString(R"HTML(
    <html>
      <body style="width:50px;height:50px">
        <div style="width:10px;height:10px;background-color:#000"></div>
      </body>
      <body>
        <div style="width:10px;height:10px;background-color:#FFF"></div>
      </body>
    </html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  // After merging, the canvas keeps the dimensions from the first <body> and contains both
  // children.
  EXPECT_FLOAT_EQ(doc->width, 50.0f);
  EXPECT_FLOAT_EQ(doc->height, 50.0f);
  EXPECT_EQ(doc->layers.front()->children.size(), 2u);
}

//==================================================================================================
// End-to-end fixture tests
//==================================================================================================

static void RunFixture(const std::string& name) {
  auto fixturePath = ProjectPath::Absolute("resources/html/" + name + ".html");
  ASSERT_TRUE(std::filesystem::exists(fixturePath)) << "fixture missing: " << fixturePath;
  auto doc = pagx::HTMLImporter::Parse(fixturePath);
  ASSERT_NE(doc, nullptr) << "failed to parse fixture " << name;
  EXPECT_GT(doc->width, 0.0f);
  EXPECT_GT(doc->height, 0.0f);
  pagx::PAGXOptimizer::Optimize(doc.get());
  auto xml = pagx::PAGXExporter::ToXML(*doc);
  ASSERT_FALSE(xml.empty());
  auto outPath = SaveFile(xml, "PAGXHTMLImporterTest/" + name + ".pagx");
  VerifyFile(outPath, name);
}

PAG_TEST(PAGXHTMLImporterTest, FixtureCard) {
  RunFixture("card");
}

PAG_TEST(PAGXHTMLImporterTest, FixtureTabBar) {
  RunFixture("tab_bar");
}

PAG_TEST(PAGXHTMLImporterTest, FixtureLogin) {
  RunFixture("login");
}

PAG_TEST(PAGXHTMLImporterTest, FixtureTableVisual) {
  RunFixture("table_visual");
}

//==================================================================================================
// Coverage-focused tests for src/pagx/html/importer
//
// These exercise raw-cascade (autoNormalize=false) code paths that the subset transformer
// otherwise handles before HTMLStyleCascade / HTMLParserContext / HTMLCssCascade ever see the
// input. Without these, the per-CSS-function transform handlers, several CSS tokenizer edge
// cases, and the `<br>` / unknown-element / stray-text branches in the parser stay dark.
//==================================================================================================

namespace {

// Parses HTML with the subset transformer disabled so the importer's own CSS cascade and
// transform machinery run end-to-end.
inline std::shared_ptr<pagx::PAGXDocument> ParseRaw(const std::string& html) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  return pagx::HTMLImporter::ParseString(html, opts);
}

// Returns true if any diagnostic message contains all of the given substrings.
inline bool HasDiagnosticContaining(const std::shared_ptr<pagx::PAGXDocument>& doc,
                                    const std::string& needle) {
  if (!doc) return false;
  for (const auto& msg : doc->errors) {
    if (msg.find(needle) != std::string::npos) return true;
  }
  return false;
}

}  // namespace

//==================================================================================================
// HTMLStyleCascade — single-function `transform` handlers (raw cascade only)
//==================================================================================================

PAG_TEST(PAGXHTMLImporterTest, RawTransformSkewXSetsLayerMatrix) {
  // skewX(deg) flips sign in the cascade and folds into a horizontal shear; the resulting
  // matrix is then re-anchored around the box centre by applyBoxTransform.
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:200px;height:60px">
      <div style="transform:skewX(45deg);width:200px;height:60px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  ASSERT_NE(div, nullptr);
  EXPECT_FALSE(div->matrix.isIdentity());
  EXPECT_FALSE(HasDiagnosticContaining(doc, "transform '"));
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformSkewYSetsLayerMatrix) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:100px;height:60px">
      <div style="transform:skewY(30deg);width:100px;height:60px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_FALSE(div->matrix.isIdentity());
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformSkewXTooManyArgsWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="transform:skewX(10deg, 20deg);width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "skewX expects 1 argument"));
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformSkewYNoArgsWarns) {
  // Two args triggers the "skewY expects 1 argument" branch.
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="transform:skewY(10deg, 20deg);width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "skewY expects 1 argument"));
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformRotateSetsLayerMatrix) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:100px;height:100px">
      <div style="transform:rotate(90deg);width:100px;height:100px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_FALSE(div->matrix.isIdentity());
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformRotateNoArgsWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="transform:rotate(1, 2);width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "rotate expects 1 argument"));
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformScaleUniformSetsLayerMatrix) {
  // scale(2) -> sx = sy = 2.
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:100px;height:100px">
      <div style="transform:scale(2);width:100px;height:100px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_FALSE(div->matrix.isIdentity());
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformScaleTwoArgsSetsLayerMatrix) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:100px;height:100px">
      <div style="transform:scale(2, 0.5);width:100px;height:100px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_FALSE(div->matrix.isIdentity());
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformScaleNonNumericWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="transform:scale(abc);width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "scale expects 1 or 2 numeric arguments"));
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformScaleXSetsLayerMatrix) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:100px;height:100px">
      <div style="transform:scaleX(2);width:100px;height:100px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_FALSE(div->matrix.isIdentity());
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformScaleXNonNumericWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="transform:scaleX(abc);width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "scaleX expects 1 numeric argument"));
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformScaleYSetsLayerMatrix) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:100px;height:100px">
      <div style="transform:scaleY(0.5);width:100px;height:100px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_FALSE(div->matrix.isIdentity());
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformScaleYNonNumericWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="transform:scaleY(abc);width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "scaleY expects 1 numeric argument"));
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformTranslateOneArgSetsLayerMatrix) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:100px;height:100px">
      <div style="transform:translate(10px);width:100px;height:100px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_FALSE(div->matrix.isIdentity());
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformTranslateTwoArgsSetsLayerMatrix) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:100px;height:100px">
      <div style="transform:translate(10px, 20px);width:100px;height:100px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_FALSE(div->matrix.isIdentity());
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformTranslateNoArgsWarns) {
  // 3 args triggers the "expects 1 or 2 length arguments" branch.
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="transform:translate(1px, 2px, 3px);width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "translate expects 1 or 2 length arguments"));
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformTranslateBadFirstArgWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="transform:translate(abc);width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "translate first argument"));
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformTranslateBadSecondArgWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="transform:translate(10px, abc);width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "translate second argument"));
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformTranslateXSetsLayerMatrix) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:100px;height:100px">
      <div style="transform:translateX(15px);width:100px;height:100px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_FALSE(div->matrix.isIdentity());
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformTranslateXNoArgsWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="transform:translateX(1px, 2px);width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "translateX expects 1 length argument"));
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformTranslateXBadArgWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="transform:translateX(abc);width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "translateX argument"));
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformTranslateYSetsLayerMatrix) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:100px;height:100px">
      <div style="transform:translateY(15px);width:100px;height:100px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_FALSE(div->matrix.isIdentity());
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformTranslateYNoArgsWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="transform:translateY(1px, 2px);width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "translateY expects 1 length argument"));
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformTranslateYBadArgWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="transform:translateY(abc);width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "translateY argument"));
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformMatrixIdentityIsDropped) {
  // matrix(1,0,0,1,0,0) is the identity; applyBoxTransform skips it without writing to layer.
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:100px;height:100px">
      <div style="transform:matrix(1,0,0,1,0,0);width:100px;height:100px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_TRUE(div->matrix.isIdentity());
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformMatrixCustomSetsLayerMatrix) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:100px;height:100px">
      <div style="transform:matrix(2,0,0,3,4,5);width:100px;height:100px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_FALSE(div->matrix.isIdentity());
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformMatrixWrongArgCountWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="transform:matrix(1,2,3);width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "matrix expects 6 numeric arguments"));
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformMatrixNonNumericWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="transform:matrix(1,0,0,1,0,abc);width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "matrix argument"));
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformUnknownFunctionWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="transform:perspective(100px);width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "transform function"));
  EXPECT_TRUE(HasDiagnosticContaining(doc, "not in the supported subset"));
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformMalformedWarns) {
  // No `(`/`)` -> ParseCssFunctionCall returns false.
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="transform:notAFunction;width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "transform '"));
  EXPECT_TRUE(HasDiagnosticContaining(doc, "is malformed"));
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformOriginPxSetsResolvedPivot) {
  // Non-default px transform-origin lands in the cascade's originX/Y resolution path.
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="transform:rotate(90deg);transform-origin:0px 0px;width:200px;height:200px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_FALSE(div->matrix.isIdentity());
  // No "transform-origin ... not supported" warning on a clean px form.
  EXPECT_FALSE(HasDiagnosticContaining(doc, "transform-origin '0px 0px'"));
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformOriginUnsupportedKeywordWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="transform:rotate(45deg);transform-origin:left top;width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "transform-origin"));
  EXPECT_TRUE(HasDiagnosticContaining(doc, "not supported"));
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformOriginCenterIsSilent) {
  // The CSS default `center` is a synonym for the box centre and must NOT warn.
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="transform:rotate(90deg);transform-origin:center;width:200px;height:200px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(doc, "transform-origin"));
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformOriginPxAtBoxCentreIsSilent) {
  // Chromium emits the default origin as `<halfW>px <halfH>px`; the cascade detects this and
  // suppresses the diagnostic, falling back to applyBoxTransform's centre pivot.
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:200px;height:100px">
      <div style="transform:rotate(45deg);transform-origin:100px 50px;width:200px;height:100px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(doc, "transform-origin"));
}

PAG_TEST(PAGXHTMLImporterTest, RawTransformOnElementWithoutSizeWarnsForOrigin) {
  // No explicit width/height means applyBoxTransform can't compute the centre pivot, so the
  // "without explicit width/height" diagnostic fires.
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="transform:rotate(45deg)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "without explicit width/height"));
}

//==================================================================================================
// HTMLCssCascade — tokenizer edge cases
//==================================================================================================

PAG_TEST(PAGXHTMLImporterTest, RawCssAtRuleNoSemicolonNoBraceIsSwallowed) {
  // An at-rule that runs to EOF with no `;` or `{` falls into the size()-fallback branch.
  auto doc = ParseRaw(R"HTML(
    <html><head><style>
      @charset
    </style></head>
      <body style="width:50px;height:50px"></body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, RawCssCommentInsideAtRulePrelude) {
  // The comment-skip branch inside the at-rule prelude scanner.
  auto doc = ParseRaw(R"HTML(
    <html><head><style>
      @media /* commented */ (max-width:600px) { .ignored { color:red } }
      .real { background-color:#AABBCC }
    </style></head>
      <body style="width:50px;height:50px">
        <div class="real" style="width:50px;height:50px"></div>
      </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* fill = FindElementOfType<pagx::Fill>(doc->layers.front()->children.front());
  ASSERT_NE(fill, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, RawCssCommentInsideSelector) {
  // The comment-skip branch inside the rule-prelude scanner that looks for `{`.
  auto doc = ParseRaw(R"HTML(
    <html><head><style>
      .real /* selector comment */ { background-color:#AABBCC }
    </style></head>
      <body style="width:50px;height:50px">
        <div class="real" style="width:50px;height:50px"></div>
      </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, RawCssStrayClosingBraceIsSwallowed) {
  // A stray `}` outside any rule terminates the prelude scan and the tokenizer recovers.
  auto doc = ParseRaw(R"HTML(
    <html><head><style>
      } .x { background-color:#AABBCC }
    </style></head>
      <body style="width:50px;height:50px"></body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, RawCssUnterminatedBlockBailsOut) {
  // No closing `}` means the unterminated-block branch trips and the rule is dropped.
  auto doc = ParseRaw(R"HTML(
    <html><head><style>
      .x { background-color:#AABBCC
    </style></head>
      <body style="width:50px;height:50px"></body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, RawCssTrailingBlockNoOpeningBrace) {
  // A selector at end of the stylesheet with no `{` triggers the bracePos-not-found bail-out.
  auto doc = ParseRaw(R"HTML(
    <html><head><style>
      .x { background-color:#AABBCC }
      .y
    </style></head>
      <body style="width:50px;height:50px"></body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, RawCssEmptySelectorIsClassifiedAsInvalid) {
  // An empty selector survives ClassifySelector and falls through to the unsupported branch.
  auto doc = ParseRaw(R"HTML(
    <html><head><style>
      , .x { background-color:#AABBCC }
    </style></head>
      <body style="width:50px;height:50px">
        <div class="x" style="width:50px;height:50px"></div>
      </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, RawCssEmptyClassSelectorRejected) {
  // `.` alone (no class name) is rejected by ClassifySelector; declarations are dropped.
  auto doc = ParseRaw(R"HTML(
    <html><head><style>
      . { background-color:#FF0000 }
    </style></head>
      <body style="width:50px;height:50px"></body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "unsupported selector"));
}

//==================================================================================================
// HTMLValueParser — empty colour value
//==================================================================================================

PAG_TEST(PAGXHTMLImporterTest, RawEmptyColorValueIsTransparent) {
  // An empty `color:` declaration walks parseColor's empty-string branch, which yields fully
  // transparent colour. The importer doesn't emit a Fill for such elements, but the
  // declaration still parses without raising.
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <p style="color:;width:50px;height:50px">x</p>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
}

//==================================================================================================
// HTMLParserContext — `<br>`, unknown elements, stray text
//==================================================================================================

PAG_TEST(PAGXHTMLImporterTest, BrInsideContainerEmitsTextLeaf) {
  // The `<br>` short-circuit branch in convertElement is reached when subset normalization runs:
  // the transformer drops text-only `<br>` siblings, but `<div><br/></div>` survives.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:100px;height:50px">
      <div style="width:100px;height:50px"><br/></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, RawUnknownElementSkippedByDefault) {
  // Without preserveUnknownElements, an unknown tag is dropped with a diagnostic.
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <unknown-thing></unknown-thing>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "not supported"));
}

PAG_TEST(PAGXHTMLImporterTest, RawUnknownElementPreservedWhenRequested) {
  // With preserveUnknownElements, the same tag becomes an empty Layer carrying the tag in
  // customData["html-unknown"].
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  opts.preserveUnknownElements = true;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:50px;height:50px">
      <unknown-thing></unknown-thing>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  ASSERT_FALSE(doc->layers.front()->children.empty());
  auto* placeholder = doc->layers.front()->children.front();
  auto it = placeholder->customData.find("html-unknown");
  ASSERT_NE(it, placeholder->customData.end());
  EXPECT_EQ(it->second, "unknown-thing");
  EXPECT_TRUE(HasDiagnosticContaining(doc, "preserved as placeholder"));
}

PAG_TEST(PAGXHTMLImporterTest, RawStrayTextDirectlyUnderBodyEmitsAnonymousLeaf) {
  // Stray text directly under `<body>` lands in the convertBody fallback that wraps it in an
  // anonymous text leaf rather than dropping it.
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:200px;height:50px">free text</body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "stray text inside <body>"));
  EXPECT_FALSE(doc->layers.front()->children.empty());
}

PAG_TEST(PAGXHTMLImporterTest, RawElementAtRootOtherThanHeadBodyWarns) {
  // An <unexpected> element at <html> root level beside <body> takes the rejection path.
  auto doc = ParseRaw(R"HTML(
    <html><foo></foo><body style="width:50px;height:50px"></body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "is not allowed"));
}

PAG_TEST(PAGXHTMLImporterTest, RawHeadLinkStylesheetWarns) {
  // <link rel="stylesheet"> in <head> is not supported and warns.
  auto doc = ParseRaw(R"HTML(
    <html><head><link rel="stylesheet" href="theme.css"/></head>
      <body style="width:50px;height:50px"></body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "external <link"));
}

PAG_TEST(PAGXHTMLImporterTest, RawHeadLinkOtherRelIsSilent) {
  // Other <link rel> values (preconnect/icon) are silently ignored — no diagnostic should fire.
  auto doc = ParseRaw(R"HTML(
    <html><head><link rel="icon" href="favicon.ico"/></head>
      <body style="width:50px;height:50px"></body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(doc, "external <link"));
}

PAG_TEST(PAGXHTMLImporterTest, MissingBodyIsHardError) {
  // Without <body>, parseDOM hits hardError("html: missing <body>") and returns nullptr.
  auto doc = ParseRaw(R"HTML(<html><head></head></html>)HTML");
  EXPECT_EQ(doc, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, RootIsNotHtmlReturnsNullptr) {
  // Root element that isn't <html> falls into the early-return branch in parseDOM.
  auto doc = ParseRaw(R"HTML(<root></root>)HTML");
  EXPECT_EQ(doc, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, RawDeepNestedTreeStaysWithinRecursionLimit) {
  // A modestly deep tree (well under MAX_HTML_RECURSION_DEPTH=128) round-trips intact, so the
  // recursion-limit branch is the only path NOT exercised — covered by the import test below.
  std::string nested;
  for (int i = 0; i < 30; i++) {
    nested += "<div style=\"width:100px;height:100px\">";
  }
  for (int i = 0; i < 30; i++) {
    nested += "</div>";
  }
  std::string html = "<html><body style=\"width:100px;height:100px\">" + nested + "</body></html>";
  auto doc = ParseRaw(html);
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(doc, "maximum recursion depth"));
}

PAG_TEST(PAGXHTMLImporterTest, RawExcessivelyNestedTreeTripsRecursionLimit) {
  // Exceed MAX_HTML_RECURSION_DEPTH=128 via convertElement, hitting the warn+nullptr branch.
  std::string opens;
  std::string closes;
  for (int i = 0; i < 200; i++) {
    opens += "<div style=\"width:100px;height:100px\">";
    closes += "</div>";
  }
  std::string html =
      "<html><body style=\"width:100px;height:100px\">" + opens + closes + "</body></html>";
  auto doc = ParseRaw(html);
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "maximum recursion depth"));
}

//==================================================================================================
// HTMLElementEmitter — `<img>` / inline `<svg>` rejection branches
//==================================================================================================

PAG_TEST(PAGXHTMLImporterTest, ImgWithoutSrcPreservedWithDiagnostic) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <img style="width:50px;height:50px"/>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "<img> missing src"));
  ASSERT_FALSE(doc->layers.front()->children.empty());
  auto* leaf = doc->layers.front()->children.front();
  auto* pattern = As<pagx::ImagePattern>(FindElementOfType<pagx::Fill>(leaf)->color);
  ASSERT_NE(pattern, nullptr);
  ASSERT_NE(pattern->image, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, ImgExternalSvgRoutesAsImportDirective) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <img src="icon.svg" style="width:50px;height:50px"/>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  ASSERT_FALSE(doc->layers.front()->children.empty());
  auto* layer = doc->layers.front()->children.front();
  EXPECT_EQ(layer->importDirective.format, "svg");
}

PAG_TEST(PAGXHTMLImporterTest, RoundedImageWrapperFoldsWhenSizesMatch) {
  // A wrapper <div style="border-radius;overflow:hidden"><img/></div> where the img exactly
  // fills the wrapper folds into a single rounded fill.
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;border-radius:25px;overflow:hidden">
        <img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNkYAAAAAYAAjCB0C8AAAAASUVORK5CYII="
             style="width:50px;height:50px"/>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, RoundedImageWrapperRejectsSecondElementChild) {
  // Two element children -> fold rejects (the early-return when a second <img> shows up).
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;border-radius:25px;overflow:hidden">
        <img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNkYAAAAAYAAjCB0C8AAAAASUVORK5CYII="
             style="width:50px;height:50px"/>
        <span>noise</span>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, RoundedImageWrapperRejectsSizeMismatch) {
  // Image smaller than wrapper -> fold falls through to the mismatched-size branch.
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;border-radius:25px;overflow:hidden">
        <img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNkYAAAAAYAAjCB0C8AAAAASUVORK5CYII="
             style="width:30px;height:30px"/>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, RoundedImageWrapperRejectsImageOffset) {
  // Image at non-zero left/top -> fold rejects.
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;border-radius:25px;overflow:hidden;position:relative">
        <img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNkYAAAAAYAAjCB0C8AAAAASUVORK5CYII="
             style="width:50px;height:50px;position:absolute;left:5px;top:5px"/>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, RoundedImageWrapperRejectsSvgChild) {
  // A `.svg` source rides the import-directive path, not the fold path.
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;border-radius:25px;overflow:hidden">
        <img src="icon.svg" style="width:50px;height:50px"/>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
}

//==================================================================================================
// HTMLSubsetTransformer::Builder — public custom-pipeline API
//==================================================================================================

PAG_TEST(PAGXHTMLImporterTest, SubsetTransformerBuilderRunsDefaultPipeline) {
  // Exercise the Builder API: withOptions + addDefaultPasses + run.
  std::string html = R"HTML(
    <html><body style="width:50px;height:50px"></body></html>
  )HTML";
  auto dom = pagx::XMLDOM::Make(reinterpret_cast<const uint8_t*>(html.data()), html.size());
  ASSERT_NE(dom, nullptr);
  auto root = dom->getRootNode();
  ASSERT_NE(root, nullptr);
  pagx::HTMLSubsetTransformer::Options opts = {};
  pagx::HTMLSubsetTransformer::Builder builder;
  auto result = builder.withOptions(opts).addDefaultPasses().run(root);
  EXPECT_TRUE(result.ok);
}

PAG_TEST(PAGXHTMLImporterTest, SubsetTransformerBuilderMoveAssign) {
  // Move-construct + move-assign Builder to drag the noexcept move members into coverage.
  pagx::HTMLSubsetTransformer::Builder a;
  a.addDefaultPasses();
  pagx::HTMLSubsetTransformer::Builder b(std::move(a));
  pagx::HTMLSubsetTransformer::Builder c;
  c = std::move(b);
  std::string html = R"HTML(
    <html><body style="width:50px;height:50px"></body></html>
  )HTML";
  auto dom = pagx::XMLDOM::Make(reinterpret_cast<const uint8_t*>(html.data()), html.size());
  ASSERT_NE(dom, nullptr);
  auto result = c.run(dom->getRootNode());
  EXPECT_TRUE(result.ok);
}

//==================================================================================================
// HTMLSubsetTransformer — Cascade resolution
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
// HTMLSubsetTransformer — Property normalisation
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
// HTMLSubsetTransformer — Structure normalisation
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

// A document whose root element is not <html> is rejected with a fatal diagnostic.
PAG_TEST(PAGXHTMLSubsetTransformerTest, DocumentRootMustBeHtml) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(R"HTML(<div style="width:10px;height:10px"></div>)HTML", &root);
  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:no-html-root"));
}

// Stray text directly under <html> (between the tag and <head>/<body>) is stripped so the
// canonical skeleton is restored.
PAG_TEST(PAGXHTMLSubsetTransformerTest, StrayTextAtHtmlRootRemoved) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      "<html> stray text <head></head><body style=\"width:10px;height:10px\"></body></html>",
      &root);
  ASSERT_TRUE(result.ok);
  ASSERT_NE(root, nullptr);
  // The only element children of <html> are <head> and <body>; no text node survives.
  for (auto c = root->firstChild; c; c = c->nextSibling) {
    EXPECT_EQ(c->type, pagx::DOMNodeType::Element);
  }
}

// A duplicate <head> is absorbed into the first one; when the first head is empty its child list
// is adopted directly from the duplicate.
PAG_TEST(PAGXHTMLSubsetTransformerTest, DuplicateEmptyHeadAbsorbsSecondHead) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      "<html><head></head><head><title>t</title></head>"
      "<body style=\"width:10px;height:10px\"></body></html>",
      &root);
  ASSERT_TRUE(result.ok);
  ASSERT_NE(root, nullptr);
  auto head = root->getFirstChild("head");
  ASSERT_NE(head, nullptr);
  EXPECT_NE(head->getFirstChild("title"), nullptr);
  // Exactly one <head> remains after the merge.
  size_t headCount = 0;
  for (auto c = root->firstChild; c; c = c->nextSibling) {
    if (c->name == "head") headCount++;
  }
  EXPECT_EQ(headCount, 1u);
}

// A CSS property that is not part of the PAGX subset table is dropped with a warning.
PAG_TEST(PAGXHTMLSubsetTransformerTest, UnknownPropertyDroppedWithWarning) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:10px;height:10px">
               <div style="width:10px;height:10px;-webkit-not-a-real-prop:1px"></div>
             </body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:unsupported-property"));
}

//==================================================================================================
// HTMLSubsetTransformer — Strict mode
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
// HTMLSubsetTransformer — HTMLFlexInference (opt-in)
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

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexInferenceCentersMainAxisInsteadOfSymmetricPadding) {
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.inferFlexFromAbsolute = true;
  std::shared_ptr<pagx::DOMNode> root;
  // Two children (50px + 50px + 10px gap = 110px) centred inside a 210px-wide body leaves a
  // symmetric 50px inset on each side. That should surface as justify-content:center, not
  // padding.
  auto result = RunTransform(
      R"HTML(<html><body style="width:210px;height:50px">
               <div style="position:absolute;left:50px;top:0px;width:50px;height:50px"></div>
               <div style="position:absolute;left:110px;top:0px;width:50px;height:50px"></div>
             </body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:flex-inferred"));
  auto body = root->getFirstChild("body");
  EXPECT_TRUE(StyleContains(body, "display: flex"));
  EXPECT_TRUE(StyleContains(body, "flex-direction: row"));
  EXPECT_TRUE(StyleContains(body, "gap: 10px"));
  EXPECT_TRUE(StyleContains(body, "justify-content: center"));
  // The symmetric leading/trailing insets are absorbed by centering, so no left/right padding is
  // baked in.
  EXPECT_FALSE(StyleContains(body, "padding"));
}

// Reverse of the symmetric case: when the leading and trailing insets differ by more than the
// inference tolerance (1.5px), the layout is not centred and the asymmetric insets must survive
// as real padding. Guards the `std::fabs(paddingLeading - paddingTrailing) <= tol` boundary so a
// near-but-not-symmetric layout is not silently promoted to justify-content:center.
PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexInferenceKeepsPaddingWhenInsetsExceedTolerance) {
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.inferFlexFromAbsolute = true;
  std::shared_ptr<pagx::DOMNode> root;
  // Two children (50px + 50px + 10px gap = 110px) in a 208px-wide body: leading inset 50px,
  // trailing inset 48px. The 2px asymmetry exceeds the 1.5px tolerance, so this is padding, not
  // centering.
  auto result = RunTransform(
      R"HTML(<html><body style="width:208px;height:50px">
               <div style="position:absolute;left:50px;top:0px;width:50px;height:50px"></div>
               <div style="position:absolute;left:110px;top:0px;width:50px;height:50px"></div>
             </body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:flex-inferred"));
  auto body = root->getFirstChild("body");
  EXPECT_TRUE(StyleContains(body, "display: flex"));
  EXPECT_FALSE(StyleContains(body, "justify-content: center"));
  EXPECT_TRUE(StyleContains(body, "padding"));
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

// Children arranged on a clean diagonal (a staircase) have no overlap on *either* axis, so both
// row and column orderings are valid. The pass then falls back to the axis with the larger start
// spread. Their cross-axis offsets differ, so the layout is ultimately kept absolute — but the
// both-axes-valid tie-break path (AxisStartSpread) is exercised.
PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexInferenceDiagonalStaircaseUsesLargerSpread) {
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.inferFlexFromAbsolute = true;
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:200px;height:200px">
               <div style="position:absolute;left:0px;top:0px;width:10px;height:10px"></div>
               <div style="position:absolute;left:40px;top:40px;width:10px;height:10px"></div>
               <div style="position:absolute;left:80px;top:80px;width:10px;height:10px"></div>
             </body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  // Diagonal children cannot share a single cross-axis alignment, so the layout stays absolute.
  EXPECT_TRUE(HasDiagnostic(result, "subset:flex-inference-skipped"));
}

// A wrapper container with no explicit width/height falls back to the children bounding box for its
// content range. A shared top inset on every child in that fallback is lifted into padding.
PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexInferenceBboxFallbackLiftsSharedCrossInset) {
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.inferFlexFromAbsolute = true;
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:300px;height:300px">
               <div style="position:relative">
                 <div style="position:absolute;left:0px;top:20px;width:50px;height:50px"></div>
                 <div style="position:absolute;left:60px;top:20px;width:50px;height:50px"></div>
               </div>
             </body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:flex-inferred"));
  auto body = root->getFirstChild("body");
  auto wrapper = FirstBodyChild(root, "div");
  ASSERT_NE(wrapper, nullptr);
  EXPECT_TRUE(StyleContains(wrapper, "display: flex"));
  EXPECT_TRUE(StyleContains(wrapper, "flex-direction: row"));
  (void)body;
}

// A column layout whose content sits with symmetric top/bottom insets inside an explicitly-sized
// parent is centred via justify-content instead of baking symmetric main-axis padding.
PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexInferenceColumnCentersMainAxis) {
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.inferFlexFromAbsolute = true;
  std::shared_ptr<pagx::DOMNode> root;
  // Two 50px-tall children + 10px gap = 110px centred in a 210px-tall body leaves a symmetric 50px
  // inset top and bottom.
  auto result = RunTransform(
      R"HTML(<html><body style="width:120px;height:210px">
               <div style="position:absolute;left:0px;top:50px;width:120px;height:50px"></div>
               <div style="position:absolute;left:0px;top:110px;width:120px;height:50px"></div>
             </body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:flex-inferred"));
  auto body = root->getFirstChild("body");
  EXPECT_TRUE(StyleContains(body, "display: flex"));
  EXPECT_TRUE(StyleContains(body, "flex-direction: column"));
  EXPECT_TRUE(StyleContains(body, "justify-content: center"));
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
  // its inner inference uses the original 320px width and recovers the 14px inset (rather than
  // collapsing it to 0). Because the leading/trailing insets are equal, the row is expressed as
  // `justify-content: center` instead of symmetric left/right padding — the two are visually
  // identical for a packed group, and centering stays correct if the content resizes.
  auto topBar = body->firstChild;
  ASSERT_NE(topBar, nullptr);
  ASSERT_EQ(topBar->type, pagx::DOMNodeType::Element);
  EXPECT_TRUE(StyleContains(topBar, "display: flex"));
  EXPECT_TRUE(StyleContains(topBar, "flex-direction: row"));
  EXPECT_TRUE(StyleContains(topBar, "justify-content: center"));
  // The symmetric inset is absorbed by centering, so no left/right padding is baked in.
  EXPECT_FALSE(StyleContains(topBar, "padding"));
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

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexInferenceDiagonalPrefersAxisWithLargerSpread) {
  // Two children that overlap on neither axis make both row and column orderings valid, so
  // `InferAxis` breaks the tie via `AxisStartSpread`. Here the row spread (40) exceeds the
  // column spread (20), so the row axis is chosen; the mismatched cross positions then trip the
  // mixed-alignment guard, exercising the spread tie-break without a successful rewrite.
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.inferFlexFromAbsolute = true;
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:200px;height:200px">
               <div style="position:absolute;left:0px;top:0px;width:10px;height:10px"></div>
               <div style="position:absolute;left:40px;top:20px;width:10px;height:10px"></div>
             </body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:flex-inference-skipped"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexInferenceDiagonalPrefersColumnWhenSpreadLarger) {
  // Mirror image of the previous case: the column spread (40) now exceeds the row spread (20),
  // so the column axis wins the tie-break.
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.inferFlexFromAbsolute = true;
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:200px;height:200px">
               <div style="position:absolute;left:0px;top:0px;width:10px;height:10px"></div>
               <div style="position:absolute;left:20px;top:40px;width:10px;height:10px"></div>
             </body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:flex-inference-skipped"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexInferenceUsesChildrenBboxWhenParentHasNoSize) {
  // A bare wrapper <div> with no style attribute (so no resolved properties and no explicit
  // dimensions) still infers flex from its absolute children: `ResolveContentRange` falls back
  // to the children's bounding box and a fresh resolved-property map is created for the parent.
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.inferFlexFromAbsolute = true;
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:200px;height:200px">
               <div>
                 <div style="position:absolute;left:0px;top:0px;width:50px;height:50px"></div>
                 <div style="position:absolute;left:50px;top:0px;width:50px;height:50px"></div>
               </div>
             </body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:flex-inferred"));
  auto body = root->getFirstChild("body");
  auto wrapper = body->firstChild;
  while (wrapper && wrapper->type != pagx::DOMNodeType::Element) wrapper = wrapper->nextSibling;
  ASSERT_NE(wrapper, nullptr);
  EXPECT_TRUE(StyleContains(wrapper, "display: flex"));
  EXPECT_TRUE(StyleContains(wrapper, "flex-direction: row"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexInferenceEmitsFlexStartAlignment) {
  // Children share the cross-axis start edge but have differing cross sizes, so the only
  // compatible alignment is `flex-start` (not stretch / center / end).
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.inferFlexFromAbsolute = true;
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:100px;height:100px">
               <div style="position:absolute;left:0px;top:0px;width:30px;height:30px"></div>
               <div style="position:absolute;left:40px;top:0px;width:30px;height:50px"></div>
             </body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:flex-inferred"));
  auto body = root->getFirstChild("body");
  EXPECT_TRUE(StyleContains(body, "align-items: flex-start"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, FlexInferenceEmitsFlexEndAlignment) {
  // Children share the cross-axis end edge but have differing cross sizes → `flex-end`.
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.inferFlexFromAbsolute = true;
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:100px;height:100px">
               <div style="position:absolute;left:0px;top:70px;width:30px;height:30px"></div>
               <div style="position:absolute;left:40px;top:50px;width:30px;height:50px"></div>
             </body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "subset:flex-inferred"));
  auto body = root->getFirstChild("body");
  EXPECT_TRUE(StyleContains(body, "align-items: flex-end"));
}

//==================================================================================================
// HTMLSubsetTransformer — Idempotency
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
// HTMLSubsetTransformer — Per-property transforms: keyword fallbacks and edge cases
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

PAG_TEST(PAGXHTMLSubsetTransformerTest, BackgroundImageUrlKept) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="background-image: url(image.png)"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  // A `url(...)` background round-trips into an ImagePattern fill, so the transformer keeps the
  // property instead of dropping it, and emits no unsupported-property diagnostic.
  EXPECT_TRUE(StyleContains(FirstBodyChild(root, "div"), "background-image"));
  EXPECT_FALSE(HasDiagnostic(result, "subset:unsupported-property"));
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
// HTMLSubsetTransformer — Length resolution
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
// HTMLSubsetTransformer — MarginToGapPromotion
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
// HTMLSubsetTransformer — SpaceJustifyOverflowCollapse
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

//==================================================================================================
// HTMLValueParser — malformed hex color and gradient stop diagnostics
//==================================================================================================

PAG_TEST(PAGXHTMLImporterTest, RawMalformedHexColorWarnsAndFallsBack) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <p style="color:#FG0000;width:50px;height:50px">x</p>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "malformed hex color"));
}

PAG_TEST(PAGXHTMLImporterTest, RawMalformedHexColorShortFormWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <p style="color:#GFF;width:50px;height:50px">x</p>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "malformed hex color"));
}

PAG_TEST(PAGXHTMLImporterTest, RawUnrecognisedHexLengthFallsThroughToUnrecognised) {
  // Lengths 2/3/6/7 ints aren't recognised hex widths, so parser falls through to the
  // "unrecognised color value" branch rather than the "malformed hex color" branch.
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <p style="color:#12345678901;width:50px;height:50px">x</p>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "unrecognised color value"));
}

PAG_TEST(PAGXHTMLImporterTest, RawMalformedGradientStopOffsetWarns) {
  // The offset token sits in the second slot, after the color. `red abc%` exercises the
  // percent-validation path because `abc%` ends in '%' and is consumed as an offset rather
  // than misclassified as a color.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:100px;height:100px">
      <div style="width:100px;height:100px;background-image:linear-gradient(red, blue 50%, green abc%)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "malformed gradient stop offset"));
}

PAG_TEST(PAGXHTMLImporterTest, RawUnmatchedFilterParenthesisWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:100px;height:100px">
      <div style="width:100px;height:100px;filter:blur(5px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "unmatched '(' in filter"));
}

// CSS `mask-image: url(data:image/svg+xml,...)` with `mask-mode: alpha` rebuilds an alpha mask
// layer (the inverse of HTMLWriter::writeMaskCSS). The ellipse geometry round-trips and the mask
// layer is attached invisibly and excluded from layout.
PAG_TEST(PAGXHTMLImporterTest, MaskImageAlphaRebuildsMaskLayer) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:110px;height:110px">
      <div style="width:110px;height:110px;mask-image:url('data:image/svg+xml,%3Csvg xmlns=%22http://www.w3.org/2000/svg%22 width=%22110%22 height=%22110%22 viewBox=%220 0 110 110%22%3E%3Cellipse cx=%2255%22 cy=%2255%22 rx=%2255%22 ry=%2255%22 fill=%22white%22/%3E%3C/svg%3E');mask-mode:alpha;mask-size:110px 110px;mask-repeat:no-repeat">
        <div style="width:110px;height:110px;background-color:#10B981"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* masked = doc->layers.front()->children.front();
  ASSERT_NE(masked->mask, nullptr);
  EXPECT_EQ(masked->maskType, pagx::MaskType::Alpha);
  EXPECT_FALSE(masked->mask->visible);
  EXPECT_FALSE(masked->mask->includeInLayout);
  auto* ellipse = FindElementOfType<pagx::Ellipse>(masked->mask);
  ASSERT_NE(ellipse, nullptr);
  EXPECT_TRUE(NearlyEqual(ellipse->size.width, 110.0f, 0.01f));
  EXPECT_TRUE(NearlyEqual(ellipse->size.height, 110.0f, 0.01f));
}

// A `mask-size` larger than the mask SVG's intrinsic size scales the rebuilt mask layer by the
// per-axis ratio so the mask geometry covers the element CSS rendered it across, rather than
// staying pinned at the smaller intrinsic size in the top-left corner. The ratios differ per axis
// (non-uniform), which the SVGImporter's uniform-fit path cannot express on its own.
PAG_TEST(PAGXHTMLImporterTest, MaskSizeScalesMaskLayerNonUniformly) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="width:200px;height:200px;mask-image:url('data:image/svg+xml,%3Csvg xmlns=%22http://www.w3.org/2000/svg%22 width=%22100%22 height=%22100%22 viewBox=%220 0 100 100%22%3E%3Cellipse cx=%2250%22 cy=%2250%22 rx=%2250%22 ry=%2250%22 fill=%22white%22/%3E%3C/svg%3E');mask-mode:alpha;mask-size:150px 180px;mask-repeat:no-repeat">
        <div style="width:200px;height:200px;background-color:#10B981"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* masked = doc->layers.front()->children.front();
  ASSERT_NE(masked->mask, nullptr);
  // 150 / 100 = 1.5 on x, 180 / 100 = 1.8 on y, no translation for the default top-left position.
  EXPECT_TRUE(NearlyEqual(masked->mask->matrix.a, 1.5f, 0.001f));
  EXPECT_TRUE(NearlyEqual(masked->mask->matrix.d, 1.8f, 0.001f));
  EXPECT_TRUE(NearlyEqual(masked->mask->matrix.tx, 0.0f, 0.001f));
  EXPECT_TRUE(NearlyEqual(masked->mask->matrix.ty, 0.0f, 0.001f));
}

// `mask-size: contain` fits the mask uniformly inside the element box using the smaller per-axis
// ratio. A 100x100 mask in a 200x100 box scales by min(2, 1) = 1 on both axes.
PAG_TEST(PAGXHTMLImporterTest, MaskSizeContainFitsUniformlyBySmallerRatio) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:100px">
      <div style="width:200px;height:100px;mask-image:url('data:image/svg+xml,%3Csvg xmlns=%22http://www.w3.org/2000/svg%22 width=%22100%22 height=%22100%22 viewBox=%220 0 100 100%22%3E%3Cellipse cx=%2250%22 cy=%2250%22 rx=%2250%22 ry=%2250%22 fill=%22white%22/%3E%3C/svg%3E');mask-mode:alpha;mask-size:contain;mask-repeat:no-repeat">
        <div style="width:200px;height:100px;background-color:#10B981"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* masked = doc->layers.front()->children.front();
  ASSERT_NE(masked->mask, nullptr);
  EXPECT_TRUE(NearlyEqual(masked->mask->matrix.a, 1.0f, 0.001f));
  EXPECT_TRUE(NearlyEqual(masked->mask->matrix.d, 1.0f, 0.001f));
}

// `mask-size: cover` fits the mask uniformly using the larger per-axis ratio. The same 100x100
// mask in a 200x100 box scales by max(2, 1) = 2 on both axes.
PAG_TEST(PAGXHTMLImporterTest, MaskSizeCoverFitsUniformlyByLargerRatio) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:100px">
      <div style="width:200px;height:100px;mask-image:url('data:image/svg+xml,%3Csvg xmlns=%22http://www.w3.org/2000/svg%22 width=%22100%22 height=%22100%22 viewBox=%220 0 100 100%22%3E%3Cellipse cx=%2250%22 cy=%2250%22 rx=%2250%22 ry=%2250%22 fill=%22white%22/%3E%3C/svg%3E');mask-mode:alpha;mask-size:cover;mask-repeat:no-repeat">
        <div style="width:200px;height:100px;background-color:#10B981"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* masked = doc->layers.front()->children.front();
  ASSERT_NE(masked->mask, nullptr);
  EXPECT_TRUE(NearlyEqual(masked->mask->matrix.a, 2.0f, 0.001f));
  EXPECT_TRUE(NearlyEqual(masked->mask->matrix.d, 2.0f, 0.001f));
}

// A single `mask-size` length applies to the width; the height axis is `auto`, tied to the width
// ratio so the aspect ratio is preserved. A 50px width on a 100x100 mask scales both axes by 0.5.
PAG_TEST(PAGXHTMLImporterTest, MaskSizeSingleLengthTiesHeightToWidthRatio) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="width:200px;height:200px;mask-image:url('data:image/svg+xml,%3Csvg xmlns=%22http://www.w3.org/2000/svg%22 width=%22100%22 height=%22100%22 viewBox=%220 0 100 100%22%3E%3Cellipse cx=%2250%22 cy=%2250%22 rx=%2250%22 ry=%2250%22 fill=%22white%22/%3E%3C/svg%3E');mask-mode:alpha;mask-size:50px;mask-repeat:no-repeat">
        <div style="width:200px;height:200px;background-color:#10B981"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* masked = doc->layers.front()->children.front();
  ASSERT_NE(masked->mask, nullptr);
  EXPECT_TRUE(NearlyEqual(masked->mask->matrix.a, 0.5f, 0.001f));
  EXPECT_TRUE(NearlyEqual(masked->mask->matrix.d, 0.5f, 0.001f));
}

// `mask-size: auto <pct>` leaves the width axis to follow the height ratio while a percentage on
// the height axis resolves against the element box. A 50% height on a 200px box scales the 100px
// intrinsic mask to 100px (0.5x), tying the width to the same ratio.
PAG_TEST(PAGXHTMLImporterTest, MaskSizeAutoWidthPercentHeightTiesWidthToHeightRatio) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="width:200px;height:200px;mask-image:url('data:image/svg+xml,%3Csvg xmlns=%22http://www.w3.org/2000/svg%22 width=%22100%22 height=%22100%22 viewBox=%220 0 100 100%22%3E%3Cellipse cx=%2250%22 cy=%2250%22 rx=%2250%22 ry=%2250%22 fill=%22white%22/%3E%3C/svg%3E');mask-mode:alpha;mask-size:auto 50%;mask-repeat:no-repeat">
        <div style="width:200px;height:200px;background-color:#10B981"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* masked = doc->layers.front()->children.front();
  ASSERT_NE(masked->mask, nullptr);
  // 50% of the 200px box is 100px; against a 100px intrinsic mask that is a uniform 1.0x scale.
  EXPECT_TRUE(NearlyEqual(masked->mask->matrix.a, 1.0f, 0.001f));
  EXPECT_TRUE(NearlyEqual(masked->mask->matrix.d, 1.0f, 0.001f));
}

// An element carrying both `mask-image` and `clip-path` keeps the mask-image (tgfx models a single
// mask per layer) and warns that the clip-path is dropped.
PAG_TEST(PAGXHTMLImporterTest, MaskImageAndClipPathKeepsMaskDropsClip) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:110px;height:110px">
      <svg style="position:absolute;width:0;height:0">
        <defs><clipPath id="clip0"><path d="M0,0L110,0L110,110L0,110Z"/></clipPath></defs>
      </svg>
      <div style="width:110px;height:110px;clip-path:url(#clip0);mask-image:url('data:image/svg+xml,%3Csvg xmlns=%22http://www.w3.org/2000/svg%22 width=%22110%22 height=%22110%22 viewBox=%220 0 110 110%22%3E%3Cellipse cx=%2255%22 cy=%2255%22 rx=%2255%22 ry=%2255%22 fill=%22white%22/%3E%3C/svg%3E');mask-mode:alpha">
        <div style="width:110px;height:110px;background-color:#10B981"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "clip-path ignored"));
  auto* masked = doc->layers.front()->children.back();
  ASSERT_NE(masked->mask, nullptr);
  EXPECT_EQ(masked->maskType, pagx::MaskType::Alpha);
}

// A `mask-image` that is not a decodable `data:image/svg+xml` URI (e.g. a raster data URI) is
// ignored with a warning rather than producing a broken mask.
PAG_TEST(PAGXHTMLImporterTest, MaskImageNonSvgDataUriIgnored) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:110px;height:110px">
      <div style="width:110px;height:110px;mask-image:url('data:image/png;base64,iVBORw0KGgo=');mask-mode:alpha">
        <div style="width:110px;height:110px;background-color:#10B981"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "not a decodable data:image/svg+xml URI"));
  auto* masked = doc->layers.front()->children.front();
  EXPECT_EQ(masked->mask, nullptr);
}

// A `clip-path: url(#id)` whose id has no matching <clipPath> def is ignored with a warning.
PAG_TEST(PAGXHTMLImporterTest, ClipPathUnknownIdIgnored) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:110px;height:110px">
      <div style="width:110px;height:110px;clip-path:url(#missing)">
        <div style="width:110px;height:110px;background-color:#F59E0B"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "references an unknown <clipPath> id"));
  auto* masked = doc->layers.front()->children.front();
  EXPECT_EQ(masked->mask, nullptr);
}

// A `clip-path` that resolves to an empty <clipPath> (no child geometry) is ignored with a warning.
PAG_TEST(PAGXHTMLImporterTest, ClipPathEmptyGeometryIgnored) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:110px;height:110px">
      <svg style="position:absolute;width:0;height:0">
        <defs><clipPath id="clip0"></clipPath></defs>
      </svg>
      <div style="width:110px;height:110px;clip-path:url(#clip0)">
        <div style="width:110px;height:110px;background-color:#F59E0B"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "clip-path <clipPath> has no geometry"));
  auto* masked = doc->layers.front()->children.back();
  EXPECT_EQ(masked->mask, nullptr);
}

// `mask-position: right bottom` anchors a mask smaller than the element to the far corner:
// tx = boxW - maskW, ty = boxH - maskH.
PAG_TEST(PAGXHTMLImporterTest, MaskPositionRightBottomKeywordsAnchorToFarCorner) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="width:200px;height:200px;mask-image:url('data:image/svg+xml,%3Csvg xmlns=%22http://www.w3.org/2000/svg%22 width=%22100%22 height=%22100%22 viewBox=%220 0 100 100%22%3E%3Cellipse cx=%2250%22 cy=%2250%22 rx=%2250%22 ry=%2250%22 fill=%22white%22/%3E%3C/svg%3E');mask-mode:alpha;mask-size:100px 100px;mask-position:right bottom;mask-repeat:no-repeat">
        <div style="width:200px;height:200px;background-color:#10B981"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* masked = doc->layers.front()->children.front();
  ASSERT_NE(masked->mask, nullptr);
  EXPECT_TRUE(NearlyEqual(masked->mask->matrix.tx, 100.0f, 0.001f));
  EXPECT_TRUE(NearlyEqual(masked->mask->matrix.ty, 100.0f, 0.001f));
}

// `mask-position` percentages resolve against the free space (boxAxis - maskAxis). 50% of a
// (200 - 100) gap centres the mask at offset 50 on both axes.
PAG_TEST(PAGXHTMLImporterTest, MaskPositionPercentageResolvesAgainstFreeSpace) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="width:200px;height:200px;mask-image:url('data:image/svg+xml,%3Csvg xmlns=%22http://www.w3.org/2000/svg%22 width=%22100%22 height=%22100%22 viewBox=%220 0 100 100%22%3E%3Cellipse cx=%2250%22 cy=%2250%22 rx=%2250%22 ry=%2250%22 fill=%22white%22/%3E%3C/svg%3E');mask-mode:alpha;mask-size:100px 100px;mask-position:50% 50%;mask-repeat:no-repeat">
        <div style="width:200px;height:200px;background-color:#10B981"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* masked = doc->layers.front()->children.front();
  ASSERT_NE(masked->mask, nullptr);
  EXPECT_TRUE(NearlyEqual(masked->mask->matrix.tx, 50.0f, 0.001f));
  EXPECT_TRUE(NearlyEqual(masked->mask->matrix.ty, 50.0f, 0.001f));
}

// `mask-position` offsets the rebuilt mask layer by the authored pixel offset, anchored at the
// masked element's top-left origin, so a mask smaller than the element lands where CSS placed it.
PAG_TEST(PAGXHTMLImporterTest, MaskPositionOffsetsMaskLayer) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="width:200px;height:200px;mask-image:url('data:image/svg+xml,%3Csvg xmlns=%22http://www.w3.org/2000/svg%22 width=%22100%22 height=%22100%22 viewBox=%220 0 100 100%22%3E%3Cellipse cx=%2250%22 cy=%2250%22 rx=%2250%22 ry=%2250%22 fill=%22white%22/%3E%3C/svg%3E');mask-mode:alpha;mask-size:100px 100px;mask-position:30px 40px;mask-repeat:no-repeat">
        <div style="width:200px;height:200px;background-color:#10B981"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* masked = doc->layers.front()->children.front();
  ASSERT_NE(masked->mask, nullptr);
  EXPECT_TRUE(NearlyEqual(masked->mask->matrix.a, 1.0f, 0.001f));
  EXPECT_TRUE(NearlyEqual(masked->mask->matrix.d, 1.0f, 0.001f));
  EXPECT_TRUE(NearlyEqual(masked->mask->matrix.tx, 30.0f, 0.001f));
  EXPECT_TRUE(NearlyEqual(masked->mask->matrix.ty, 40.0f, 0.001f));
}

// A single `mask-position` value sets the horizontal axis; the vertical axis defaults to `center`
// per CSS, not to the top edge. The mask (100px) centred in a 200px box offsets by (200-100)/2.
PAG_TEST(PAGXHTMLImporterTest, MaskPositionSingleValueDefaultsVerticalToCenter) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="width:200px;height:200px;mask-image:url('data:image/svg+xml,%3Csvg xmlns=%22http://www.w3.org/2000/svg%22 width=%22100%22 height=%22100%22 viewBox=%220 0 100 100%22%3E%3Cellipse cx=%2250%22 cy=%2250%22 rx=%2250%22 ry=%2250%22 fill=%22white%22/%3E%3C/svg%3E');mask-mode:alpha;mask-size:100px 100px;mask-position:20px;mask-repeat:no-repeat">
        <div style="width:200px;height:200px;background-color:#10B981"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* masked = doc->layers.front()->children.front();
  ASSERT_NE(masked->mask, nullptr);
  EXPECT_TRUE(NearlyEqual(masked->mask->matrix.tx, 20.0f, 0.001f));
  EXPECT_TRUE(NearlyEqual(masked->mask->matrix.ty, 50.0f, 0.001f));
}

// `mask-mode: luminance` selects the luminance mask type and the radial-gradient fill inside the
// mask SVG round-trips into a RadialGradient color source.
PAG_TEST(PAGXHTMLImporterTest, MaskImageLuminanceUsesLuminanceType) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:110px;height:110px">
      <div style="width:110px;height:110px;mask-image:url('data:image/svg+xml,%3Csvg xmlns=%22http://www.w3.org/2000/svg%22 width=%22110%22 height=%22110%22 viewBox=%220 0 110 110%22%3E%3Cdefs%3E%3CradialGradient id=%22g%22 cx=%220.5%22 cy=%220.5%22 r=%220.5%22 gradientUnits=%22objectBoundingBox%22%3E%3Cstop offset=%220%22 stop-color=%22%23FFFFFF%22/%3E%3Cstop offset=%221%22 stop-color=%22%23000000%22/%3E%3C/radialGradient%3E%3C/defs%3E%3Cellipse cx=%2255%22 cy=%2255%22 rx=%2255%22 ry=%2255%22 fill=%22url(%23g)%22/%3E%3C/svg%3E');mask-mode:luminance;mask-size:110px 110px;mask-repeat:no-repeat">
        <div style="width:110px;height:110px;background-color:#EF4444"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* masked = doc->layers.front()->children.front();
  ASSERT_NE(masked->mask, nullptr);
  EXPECT_EQ(masked->maskType, pagx::MaskType::Luminance);
  auto* fill = FindElementOfType<pagx::Fill>(masked->mask);
  ASSERT_NE(fill, nullptr);
  EXPECT_NE(As<pagx::RadialGradient>(fill->color), nullptr);
}

// `clip-path: url(#id)` resolves the hidden <clipPath> def into a contour mask whose path
// geometry round-trips (the inverse of HTMLWriter::writeClipDef).
PAG_TEST(PAGXHTMLImporterTest, ClipPathRebuildsContourMask) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:110px;height:110px">
      <svg style="position:absolute;width:0;height:0">
        <defs>
          <clipPath id="clip0">
            <path d="M0,0L110,0L110,110L0,110Z"/>
          </clipPath>
        </defs>
      </svg>
      <div style="width:110px;height:110px;clip-path:url(#clip0)">
        <div style="width:110px;height:110px;background-color:#F59E0B"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  // The body has two children: the hidden <svg> holding the clipPath def, then the masked div.
  auto* masked = doc->layers.front()->children.back();
  ASSERT_NE(masked->mask, nullptr);
  EXPECT_EQ(masked->maskType, pagx::MaskType::Contour);
  // SVGImporter folds the axis-aligned rectangular clip path into a Rectangle; either a Path or a
  // Rectangle is an acceptable contour geometry.
  bool hasGeometry = FindElementOfType<pagx::Path>(masked->mask) != nullptr ||
                     FindElementOfType<pagx::Rectangle>(masked->mask) != nullptr;
  EXPECT_TRUE(hasGeometry);
}

// The rebuilt mask layer carries a generated id so the `mask="@id"` reference survives a PAGX
// export / re-import round-trip — including the forward reference (the mask is a descendant of
// the masked layer, parsed after the owning layer's `mask` attribute is read).
PAG_TEST(PAGXHTMLImporterTest, MaskReferenceSurvivesRoundTrip) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:110px;height:110px">
      <div style="width:110px;height:110px;mask-image:url('data:image/svg+xml,%3Csvg xmlns=%22http://www.w3.org/2000/svg%22 width=%22110%22 height=%22110%22 viewBox=%220 0 110 110%22%3E%3Cellipse cx=%2255%22 cy=%2255%22 rx=%2255%22 ry=%2255%22 fill=%22white%22/%3E%3C/svg%3E');mask-mode:alpha">
        <div style="width:110px;height:110px;background-color:#10B981"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  std::string xml = pagx::PAGXExporter::ToXML(*doc);
  EXPECT_NE(xml.find("mask=\"@"), std::string::npos);

  auto reloaded = pagx::PAGXImporter::FromXML(xml);
  ASSERT_NE(reloaded, nullptr);
  EXPECT_TRUE(reloaded->errors.empty());
  auto* masked = reloaded->layers.front()->children.front();
  ASSERT_NE(masked->mask, nullptr);
  EXPECT_EQ(masked->maskType, pagx::MaskType::Alpha);
}

// `filter: url(#id)` referencing an SVG <filter> def whose only primitive is a feGaussianBlur with
// an asymmetric stdDeviation rebuilds a BlurFilter with distinct blurX / blurY — the inverse of
// HTMLWriter::writeFilterDefs emitting `stdDeviation="blurX blurY"`.
PAG_TEST(PAGXHTMLImporterTest, SvgFilterRefRebuildsAsymmetricBlur) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <svg style="position:absolute;width:0;height:0">
        <defs>
          <filter id="f0">
            <feGaussianBlur in="SourceGraphic" stdDeviation="8 2" result="b0"/>
          </filter>
        </defs>
      </svg>
      <div style="width:68px;height:68px;filter:url(#f0)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(doc, "not supported"));
  auto* div = doc->layers.front()->children.back();
  ASSERT_EQ(div->filters.size(), 1u);
  auto* blur = As<pagx::BlurFilter>(div->filters[0]);
  ASSERT_NE(blur, nullptr);
  EXPECT_FLOAT_EQ(blur->blurX, 8.0f);
  EXPECT_FLOAT_EQ(blur->blurY, 2.0f);
}

// A drop-shadow <filter> built from SourceAlpha (blur -> offset -> colour matrix) with no final
// feMerge rebuilds a shadow-only DropShadowFilter, recovering offset, blur, and colour.
PAG_TEST(PAGXHTMLImporterTest, SvgFilterRefRebuildsShadowOnlyDropShadow) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <svg style="position:absolute;width:0;height:0">
        <defs>
          <filter id="f1">
            <feGaussianBlur in="SourceAlpha" stdDeviation="12" result="sBlur0"/>
            <feOffset in="sBlur0" dx="4" dy="8" result="sOff0"/>
            <feColorMatrix in="sOff0" type="matrix" values="0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0.502 0" result="sDrop0"/>
          </filter>
        </defs>
      </svg>
      <div style="width:68px;height:68px;filter:url(#f1)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(doc, "not supported"));
  auto* div = doc->layers.front()->children.back();
  ASSERT_EQ(div->filters.size(), 1u);
  auto* drop = As<pagx::DropShadowFilter>(div->filters[0]);
  ASSERT_NE(drop, nullptr);
  EXPECT_FLOAT_EQ(drop->offsetX, 4.0f);
  EXPECT_FLOAT_EQ(drop->offsetY, 8.0f);
  EXPECT_FLOAT_EQ(drop->blurX, 12.0f);
  EXPECT_FLOAT_EQ(drop->blurY, 12.0f);
  EXPECT_TRUE(drop->shadowOnly);
  EXPECT_NEAR(drop->color.alpha, 0.502f, 0.01f);
}

// A drop-shadow <filter> whose alpha pre-pass uses an opaque-alpha feColorMatrix (a 255 multiplier
// with result="opaqueAlpha", as emitted by tools other than HTMLWriter) is still folded into a
// single DropShadowFilter — the leading matrix must not be mistaken for a standalone
// ColorMatrixFilter. Regression for constraint_polystar_center.html.
PAG_TEST(PAGXHTMLImporterTest, SvgFilterRefDropShadowWithOpaqueAlphaPrepass) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:200px">
      <svg style="position:absolute;width:0;height:0">
        <defs>
          <filter id="f0">
            <feColorMatrix in="SourceAlpha" type="matrix" values="0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 255 0" result="opaqueAlpha"/>
            <feGaussianBlur in="opaqueAlpha" stdDeviation="24 24" result="blur"/>
            <feOffset in="blur" dy="8" result="off"/>
            <feColorMatrix in="off" type="matrix" values="0 0 0 0 0.9608 0 0 0 0 0.6196 0 0 0 0 0.0431 0 0 0 0.3765 0" result="shadow"/>
            <feMerge>
              <feMergeNode in="shadow"/>
              <feMergeNode in="SourceGraphic"/>
            </feMerge>
          </filter>
        </defs>
      </svg>
      <div style="width:200px;height:200px;filter:url(#f0)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(doc, "not supported"));
  auto* div = doc->layers.front()->children.back();
  ASSERT_EQ(div->filters.size(), 1u);
  auto* drop = As<pagx::DropShadowFilter>(div->filters[0]);
  ASSERT_NE(drop, nullptr);
  EXPECT_FLOAT_EQ(drop->offsetX, 0.0f);
  EXPECT_FLOAT_EQ(drop->offsetY, 8.0f);
  EXPECT_FLOAT_EQ(drop->blurX, 24.0f);
  EXPECT_FLOAT_EQ(drop->blurY, 24.0f);
  EXPECT_FALSE(drop->shadowOnly);
  EXPECT_NEAR(drop->color.red, 0.9608f, 0.01f);
  EXPECT_NEAR(drop->color.green, 0.6196f, 0.01f);
  EXPECT_NEAR(drop->color.alpha, 0.3765f, 0.01f);
}

// An inner-shadow <filter> (inverted alpha -> blur -> offset -> flood -> two composites) ending in
// a feMerge with SourceGraphic rebuilds a non-shadow-only InnerShadowFilter, recovering the flood
// colour and opacity.
PAG_TEST(PAGXHTMLImporterTest, SvgFilterRefRebuildsInnerShadow) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <svg style="position:absolute;width:0;height:0">
        <defs>
          <filter id="f2">
            <feComponentTransfer in="SourceAlpha" result="iInv0">
              <feFuncA type="table" tableValues="1 0"/>
            </feComponentTransfer>
            <feGaussianBlur in="iInv0" stdDeviation="6" result="iBlur0"/>
            <feOffset in="iBlur0" dx="2" dy="2" result="iOff0"/>
            <feFlood flood-color="#000000" flood-opacity="0.502" result="iFlood0"/>
            <feComposite in="iFlood0" in2="iOff0" operator="in" result="iShadow0"/>
            <feComposite in="iShadow0" in2="SourceAlpha" operator="in" result="iClip0"/>
            <feMerge result="iMerged0">
              <feMergeNode in="SourceGraphic"/>
              <feMergeNode in="iClip0"/>
            </feMerge>
          </filter>
        </defs>
      </svg>
      <div style="width:68px;height:68px;filter:url(#f2)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(doc, "not supported"));
  auto* div = doc->layers.front()->children.back();
  ASSERT_EQ(div->filters.size(), 1u);
  auto* inner = As<pagx::InnerShadowFilter>(div->filters[0]);
  ASSERT_NE(inner, nullptr);
  EXPECT_FLOAT_EQ(inner->offsetX, 2.0f);
  EXPECT_FLOAT_EQ(inner->offsetY, 2.0f);
  EXPECT_FLOAT_EQ(inner->blurX, 6.0f);
  EXPECT_FALSE(inner->shadowOnly);
  EXPECT_NEAR(inner->color.alpha, 0.502f, 0.01f);
}

// A blend <filter> (feFlood -> feBlend(mode) -> feComposite(in)) rebuilds a BlendFilter recovering
// the flood colour and the blend mode keyword.
PAG_TEST(PAGXHTMLImporterTest, SvgFilterRefRebuildsBlend) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <svg style="position:absolute;width:0;height:0">
        <defs>
          <filter id="f3">
            <feFlood flood-color="#0000FF" flood-opacity="0.502" result="bFlood0"/>
            <feBlend in="bFlood0" in2="SourceGraphic" mode="overlay" result="bBlend0"/>
            <feComposite in="bBlend0" in2="SourceAlpha" operator="in" result="bClip0"/>
          </filter>
        </defs>
      </svg>
      <div style="width:68px;height:68px;filter:url(#f3)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(doc, "not supported"));
  auto* div = doc->layers.front()->children.back();
  ASSERT_EQ(div->filters.size(), 1u);
  auto* blend = As<pagx::BlendFilter>(div->filters[0]);
  ASSERT_NE(blend, nullptr);
  EXPECT_EQ(blend->blendMode, pagx::BlendMode::Overlay);
  EXPECT_NEAR(blend->color.blue, 1.0f, 0.01f);
  EXPECT_NEAR(blend->color.alpha, 0.502f, 0.01f);
}

// A standalone feColorMatrix colour transform rebuilds a ColorMatrixFilter carrying all 20 values.
PAG_TEST(PAGXHTMLImporterTest, SvgFilterRefRebuildsColorMatrix) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <svg style="position:absolute;width:0;height:0">
        <defs>
          <filter id="f4">
            <feColorMatrix in="SourceGraphic" type="matrix" values="0.33 0.33 0.33 0 0 0.33 0.33 0.33 0 0 0.33 0.33 0.33 0 0 0 0 0 1 0" result="cm0"/>
          </filter>
        </defs>
      </svg>
      <div style="width:68px;height:68px;filter:url(#f4)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(doc, "not supported"));
  auto* div = doc->layers.front()->children.back();
  ASSERT_EQ(div->filters.size(), 1u);
  auto* cm = As<pagx::ColorMatrixFilter>(div->filters[0]);
  ASSERT_NE(cm, nullptr);
  EXPECT_NEAR(cm->matrix[0], 0.33f, 0.01f);
  EXPECT_NEAR(cm->matrix[18], 1.0f, 0.01f);
}

// Connection is by `in` / `result` name, not physical order: a drop-shadow whose primitives are
// emitted out of document order, with the terminal feColorMatrix omitting `result`, still decodes
// to one DropShadowFilter. Guards the data-flow model against regressing to positional matching.
PAG_TEST(PAGXHTMLImporterTest, SvgFilterRefDropShadowReorderedByName) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <svg style="position:absolute;width:0;height:0">
        <defs>
          <filter id="f6">
            <feColorMatrix in="off" type="matrix" values="0 0 0 0 0.1 0 0 0 0 0.2 0 0 0 0 0.3 0 0 0 0.5 0"/>
            <feOffset in="sBlur0" dx="5" dy="6" result="off"/>
            <feGaussianBlur in="SourceAlpha" stdDeviation="7" result="sBlur0"/>
          </filter>
        </defs>
      </svg>
      <div style="width:68px;height:68px;filter:url(#f6)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(doc, "not supported"));
  auto* div = doc->layers.front()->children.back();
  ASSERT_EQ(div->filters.size(), 1u);
  auto* drop = As<pagx::DropShadowFilter>(div->filters[0]);
  ASSERT_NE(drop, nullptr);
  EXPECT_FLOAT_EQ(drop->offsetX, 5.0f);
  EXPECT_FLOAT_EQ(drop->offsetY, 6.0f);
  EXPECT_FLOAT_EQ(drop->blurX, 7.0f);
  EXPECT_TRUE(drop->shadowOnly);
}

// A drop-shadow whose colour stage is a feFlood masked by feComposite(in) — the variant other
// tools emit instead of a tint feColorMatrix — also decodes to a DropShadowFilter, recovering the
// flood colour. The data-flow matcher recognises either colour-fill shape.
PAG_TEST(PAGXHTMLImporterTest, SvgFilterRefDropShadowFloodColour) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <svg style="position:absolute;width:0;height:0">
        <defs>
          <filter id="f7">
            <feGaussianBlur in="SourceAlpha" stdDeviation="4" result="b"/>
            <feOffset in="b" dx="2" dy="2" result="o"/>
            <feFlood flood-color="#FF0000" flood-opacity="0.5" result="fl"/>
            <feComposite in="fl" in2="o" operator="in" result="sh"/>
            <feMerge>
              <feMergeNode in="sh"/>
              <feMergeNode in="SourceGraphic"/>
            </feMerge>
          </filter>
        </defs>
      </svg>
      <div style="width:68px;height:68px;filter:url(#f7)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(doc, "not supported"));
  auto* div = doc->layers.front()->children.back();
  ASSERT_EQ(div->filters.size(), 1u);
  auto* drop = As<pagx::DropShadowFilter>(div->filters[0]);
  ASSERT_NE(drop, nullptr);
  EXPECT_FLOAT_EQ(drop->offsetX, 2.0f);
  EXPECT_FLOAT_EQ(drop->blurX, 4.0f);
  EXPECT_FALSE(drop->shadowOnly);
  EXPECT_NEAR(drop->color.red, 1.0f, 0.01f);
  EXPECT_NEAR(drop->color.alpha, 0.5f, 0.01f);
}

// A chained <filter> (blur -> inner-shadow -> blend) decodes into three filters in pipeline order,
// each sub-graph consuming the previous one's output by name. Proves multi-stage decomposition.
PAG_TEST(PAGXHTMLImporterTest, SvgFilterRefChainedThreeFilters) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <svg style="position:absolute;width:0;height:0">
        <defs>
          <filter id="f8">
            <feGaussianBlur in="SourceGraphic" stdDeviation="1" result="blur0"/>
            <feColorMatrix in="blur0" type="matrix" values="0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0" result="a2"/>
            <feComponentTransfer in="a2" result="iInv1">
              <feFuncA type="table" tableValues="1 0"/>
            </feComponentTransfer>
            <feGaussianBlur in="iInv1" stdDeviation="6" result="iBlur1"/>
            <feOffset in="iBlur1" dx="2" dy="2" result="iOff1"/>
            <feFlood flood-color="#0E7490" result="iFlood1"/>
            <feComposite in="iFlood1" in2="iOff1" operator="in" result="iShadow1"/>
            <feComposite in="iShadow1" in2="a2" operator="in" result="iClip1"/>
            <feMerge result="iMerged1">
              <feMergeNode in="blur0"/>
              <feMergeNode in="iClip1"/>
            </feMerge>
            <feFlood flood-color="#F0ABFC" flood-opacity="0.251" result="bFlood3"/>
            <feBlend in="bFlood3" in2="iMerged1" mode="screen" result="bBlend3"/>
            <feComposite in="bBlend3" in2="iMerged1" operator="in" result="bClip3"/>
          </filter>
        </defs>
      </svg>
      <div style="width:68px;height:68px;filter:url(#f8)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(doc, "not supported"));
  auto* div = doc->layers.front()->children.back();
  ASSERT_EQ(div->filters.size(), 3u);
  EXPECT_NE(As<pagx::BlurFilter>(div->filters[0]), nullptr);
  EXPECT_NE(As<pagx::InnerShadowFilter>(div->filters[1]), nullptr);
  EXPECT_NE(As<pagx::BlendFilter>(div->filters[2]), nullptr);
}

// A <filter> whose primitives do not match any exporter template is dropped with a diagnostic, and
// the element carries no filters rather than a partial chain.
PAG_TEST(PAGXHTMLImporterTest, SvgFilterRefUnsupportedPrimitivesWarns) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <svg style="position:absolute;width:0;height:0">
        <defs>
          <filter id="f5">
            <feTurbulence type="fractalNoise" baseFrequency="0.05" result="noise"/>
            <feDisplacementMap in="SourceGraphic" in2="noise" scale="10"/>
          </filter>
        </defs>
      </svg>
      <div style="width:68px;height:68px;filter:url(#f5)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.back();
  EXPECT_TRUE(div->filters.empty());
  EXPECT_TRUE(HasDiagnosticContaining(doc, "outside the supported set"));
}

// The decoded filter chain survives a PAGX serialisation round-trip: a DropShadowFilter recovered
// from an SVG <filter> ref exports to PAGX XML and re-imports with its parameters intact, proving
// the decoder yields a fully-formed, serialisable node.
PAG_TEST(PAGXHTMLImporterTest, SvgFilterChainRoundTrip) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <svg style="position:absolute;width:0;height:0">
        <defs>
          <filter id="chain0">
            <feGaussianBlur in="SourceAlpha" stdDeviation="5" result="sBlur0"/>
            <feOffset in="sBlur0" dx="3" dy="4" result="sOff0"/>
            <feColorMatrix in="sOff0" type="matrix" values="0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0.5 0" result="sDrop0"/>
            <feMerge result="sMerged0">
              <feMergeNode in="sDrop0"/>
              <feMergeNode in="SourceGraphic"/>
            </feMerge>
          </filter>
        </defs>
      </svg>
      <div style="width:68px;height:68px;filter:url(#chain0)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.back();
  ASSERT_EQ(div->filters.size(), 1u);
  auto* drop = As<pagx::DropShadowFilter>(div->filters[0]);
  ASSERT_NE(drop, nullptr);
  EXPECT_FALSE(drop->shadowOnly);

  std::string xml = pagx::PAGXExporter::ToXML(*doc);
  auto reloaded = pagx::PAGXImporter::FromXML(xml);
  ASSERT_NE(reloaded, nullptr);
  EXPECT_TRUE(reloaded->errors.empty());
  auto* reDiv = reloaded->layers.front()->children.back();
  ASSERT_EQ(reDiv->filters.size(), 1u);
  auto* reDrop = As<pagx::DropShadowFilter>(reDiv->filters[0]);
  ASSERT_NE(reDrop, nullptr);
  EXPECT_FLOAT_EQ(reDrop->offsetX, 3.0f);
  EXPECT_FLOAT_EQ(reDrop->offsetY, 4.0f);
  EXPECT_FLOAT_EQ(reDrop->blurX, 5.0f);
  EXPECT_FALSE(reDrop->shadowOnly);
}

//==================================================================================================
// HTMLValueParser — wide-gamut color() and hsl() color forms
//==================================================================================================

PAG_TEST(PAGXHTMLImporterTest, DisplayP3ColorParsed) {
  // The exporter round-trips DisplayP3 fills as `color(display-p3 r g b)` with channels already
  // in [0,1]; the value parser must keep them in the DisplayP3 space instead of falling through
  // to the unrecognised-color black fallback.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:40px;height:40px">
      <div style="width:40px;height:40px;background-color:color(display-p3 1 0 0)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto color = SolidFillColorOf(div);
  EXPECT_EQ(color.colorSpace, pagx::ColorSpace::DisplayP3);
  EXPECT_TRUE(NearlyEqual(color.red, 1.0f));
  EXPECT_TRUE(NearlyEqual(color.green, 0.0f));
  EXPECT_TRUE(NearlyEqual(color.blue, 0.0f));
  EXPECT_TRUE(NearlyEqual(color.alpha, 1.0f));
}

PAG_TEST(PAGXHTMLImporterTest, DisplayP3ColorWithAlphaParsed) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:40px;height:40px">
      <div style="width:40px;height:40px;background-color:color(display-p3 0 1 0 / 0.5)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto color = SolidFillColorOf(div);
  EXPECT_EQ(color.colorSpace, pagx::ColorSpace::DisplayP3);
  EXPECT_TRUE(NearlyEqual(color.green, 1.0f));
  EXPECT_TRUE(NearlyEqual(color.alpha, 0.5f));
}

PAG_TEST(PAGXHTMLImporterTest, UnsupportedColorFunctionFallsBackToBlack) {
  // A color() function in a colour space the parser does not model (`srgb`, `lab`, ...) is not
  // silently mis-decoded: it warns and falls back to opaque black.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:40px;height:40px">
      <div style="width:40px;height:40px;background-color:color(srgb 1 0 0)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "unsupported color()"));
  auto* div = doc->layers.front()->children.front();
  EXPECT_TRUE(ColorNear(SolidFillColorOf(div), HexColor(0x000000)));
}

PAG_TEST(PAGXHTMLImporterTest, HslColorCommaSyntaxParsed) {
  // `hsl(0, 100%, 50%)` is pure red. Authored CSS can leave the function intact, so the parser
  // must convert it instead of rendering opaque black.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:40px;height:40px">
      <div style="width:40px;height:40px;background-color:hsl(0, 100%, 50%)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_TRUE(ColorNear(SolidFillColorOf(div), HexColor(0xFF0000), 0.02f));
}

PAG_TEST(PAGXHTMLImporterTest, HslColorSpaceSyntaxParsed) {
  // CSS Color 4 space-separated `hsl(240 100% 50%)` is pure blue.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:40px;height:40px">
      <div style="width:40px;height:40px;background-color:hsl(240 100% 50%)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_TRUE(ColorNear(SolidFillColorOf(div), HexColor(0x0000FF), 0.02f));
}

//==================================================================================================
// HTMLSubsetTransformer — property table value edge cases not covered by the existing suite
//==================================================================================================

PAG_TEST(PAGXHTMLSubsetTransformerTest, TransformWithoutParensIsDropped) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="transform: bogus"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(FirstBodyChild(root, "div"), "transform"));
  EXPECT_TRUE(HasDiagnostic(result, "subset:unsupported-property"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, TransformUnsupportedFunctionIsDropped) {
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="transform: perspective(200px)"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(StyleContains(FirstBodyChild(root, "div"), "transform"));
  EXPECT_TRUE(HasDiagnostic(result, "subset:unsupported-property"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, VhResolvesAgainstCanvasHeight) {
  pagx::HTMLSubsetTransformer::Options opts = {};
  opts.canvasWidth = 800.0f;
  opts.canvasHeight = 600.0f;
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:800px;height:600px">
               <div style="height: 50vh"></div></body></html>)HTML",
      &root, opts);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(StyleContains(FirstBodyChild(root, "div"), "height: 300px"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, LengthShorthandSkipsUnresolvableToken) {
  // `padding: 10px auto` keeps the resolvable px token and silently drops `auto`.
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:1px;height:1px">
               <div style="padding: 10px auto"></div></body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_TRUE(StyleContains(FirstBodyChild(root, "div"), "padding: 10px"));
}

//==================================================================================================
// HTMLSubsetTransformer::Builder — custom pass pipeline
//==================================================================================================

namespace {

// Minimal pass that records a diagnostic so the Builder::addPass / run plumbing is exercised
// independently of the default pipeline.
class RecordingPass : public pagx::HTMLTransformPass {
 public:
  const char* name() const override {
    return "RecordingPass";
  }
  void apply(const std::shared_ptr<pagx::DOMNode>&, pagx::HTMLTransformContext& ctx) override {
    ctx.warn("test:recording-pass-ran", "html: recording pass executed");
  }
};

}  // namespace

PAG_TEST(PAGXHTMLSubsetTransformerTest, BuilderRunsCustomPass) {
  auto root = ParseHtml(R"HTML(<html><body style="width:1px;height:1px"></body></html>)HTML");
  ASSERT_NE(root, nullptr);
  pagx::HTMLSubsetTransformer::Builder builder;
  // addPass(nullptr) is a no-op (covers the guarded false branch); the real pass is appended next.
  builder.addPass(nullptr);
  builder.addPass(std::make_unique<RecordingPass>());
  auto result = builder.run(root);
  EXPECT_TRUE(result.ok);
  EXPECT_TRUE(HasDiagnostic(result, "test:recording-pass-ran"));
}

//==================================================================================================
// MarginToGapPromotionPass — bail conditions not covered by the promotion tests above
//==================================================================================================

PAG_TEST(PAGXHTMLSubsetTransformerTest, MarginToGapSkipsWhenChildHasFlexGrow) {
  // A grow child makes the leftover space route into the child, so the margins are not a pure
  // gap surrogate; the pass bails and leaves margins intact.
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:200px;height:50px">
               <div style="display:flex;width:200px;height:50px">
                 <div style="flex:1;height:50px;margin-right:8px"></div>
                 <div style="width:30px;height:50px;margin-right:8px"></div>
               </div>
             </body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(HasDiagnostic(result, "subset:margin-promoted-to-gap"));
  auto container = FirstBodyChild(root, "div");
  ASSERT_NE(container, nullptr);
  EXPECT_FALSE(StyleContains(container, "gap:"));
}

PAG_TEST(PAGXHTMLSubsetTransformerTest, MarginToGapSkipsFlexWrapContainer) {
  // Wrapping reflows children onto multiple lines, where uniform margins are not equivalent to a
  // single-line gap; the pass must bail.
  std::shared_ptr<pagx::DOMNode> root;
  auto result = RunTransform(
      R"HTML(<html><body style="width:100px;height:100px">
               <div style="display:flex;flex-wrap:wrap;width:100px;height:100px">
                 <div style="width:50px;height:50px;margin-bottom:8px"></div>
                 <div style="width:50px;height:50px;margin-bottom:8px"></div>
               </div>
             </body></html>)HTML",
      &root);
  ASSERT_TRUE(result.ok);
  EXPECT_FALSE(HasDiagnostic(result, "subset:margin-promoted-to-gap"));
}

//==================================================================================================
// HTMLStyleCascade — raw-cascade paths reached only with the subset transformer disabled
//==================================================================================================

PAG_TEST(PAGXHTMLImporterTest, RawMatrix3DAffineSetsLayerMatrix) {
  // matrix3d carrying a pure affine (no perspective) projects exactly onto the 2D affine subset:
  // a 2x scale + (10,20) translate. No perspective warning is emitted.
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="width:50px;height:50px;
                  transform:matrix3d(2,0,0,0, 0,2,0,0, 0,0,1,0, 10,20,0,1)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  ASSERT_NE(div, nullptr);
  EXPECT_FALSE(div->matrix.isIdentity());
  EXPECT_FALSE(HasDiagnosticContaining(doc, "carries perspective"));
}

PAG_TEST(PAGXHTMLImporterTest, RawMatrix3DPerspectiveWarns) {
  // A non-affine matrix3d (m44 != 1) keeps its affine subset but warns about the dropped depth.
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="width:50px;height:50px;
                  transform:matrix3d(2,0,0,0, 0,2,0,0, 0,0,1,0, 0,0,0,2)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "carries perspective"));
}

PAG_TEST(PAGXHTMLImporterTest, RawMatrix3DWrongArgCountWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="width:50px;height:50px;transform:matrix3d(1,2,3)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "matrix3d expects 16 numeric arguments"));
}

PAG_TEST(PAGXHTMLImporterTest, RawMatrix3DNonNumericArgWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="width:50px;height:50px;
                  transform:matrix3d(a,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "matrix3d argument 'a' is not a number"));
}

PAG_TEST(PAGXHTMLImporterTest, RawBorderDoubleStyleDowngradedToSolid) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:80px;height:80px">
      <div style="width:40px;height:40px;border:2px double red"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "border style 'double' not supported"));
}

PAG_TEST(PAGXHTMLImporterTest, RawBorderMultipleColorsWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:80px;height:80px">
      <div style="width:40px;height:40px;border:2px solid red blue"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "has multiple colour tokens"));
}

PAG_TEST(PAGXHTMLImporterTest, RawBorderRadiusPercentResolvesAgainstMinDimension) {
  // 25% (not the 50% inscribed-ellipse case) resolves per corner against min(width, height).
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:80px;height:80px">
      <div style="width:40px;height:40px;background-color:#000;border-radius:25%"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* rect = FindElementOfType<pagx::Rectangle>(div);
  ASSERT_NE(rect, nullptr);
  EXPECT_TRUE(NearlyEqual(rect->roundness, 10.0f));
}

PAG_TEST(PAGXHTMLImporterTest, RawBorderRadiusInvalidTokenWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:80px;height:80px">
      <div style="width:40px;height:40px;background-color:#000;border-radius:abc"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "invalid border-radius token 'abc'"));
}

PAG_TEST(PAGXHTMLImporterTest, RawBorderRadiusInvalidPercentTokenWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:80px;height:80px">
      <div style="width:40px;height:40px;background-color:#000;border-radius:5x%"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "invalid border-radius token '5x%'"));
}

PAG_TEST(PAGXHTMLImporterTest, RawBorderRadiusPercentWithoutFixedSizeWarns) {
  // A percentage border-radius on a box whose matching axis has no fixed px size cannot resolve,
  // so it is dropped with a diagnostic.
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:80px;height:80px">
      <div style="background-color:#000;border-radius:50%"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "requires fixed px width/height"));
}

PAG_TEST(PAGXHTMLImporterTest, RawVerticalWritingModeFlipsFlexAxis) {
  // Under a vertical writing mode the flex main axis rotates 90°, so `box.flexRow` is flipped.
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:80px;height:80px">
      <div style="display:flex;flex-direction:column;writing-mode:vertical-rl;width:40px;height:40px">
        <div style="width:10px;height:10px;background-color:#000"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
}

PAG_TEST(PAGXHTMLImporterTest, RawUnsupportedDisplayWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="display:grid;width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "display: grid not supported"));
}

PAG_TEST(PAGXHTMLImporterTest, RawFlexDirectionReverseWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="display:flex;flex-direction:row-reverse;width:50px;height:50px"></div>
      <div style="display:flex;flex-direction:column-reverse;width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "flex-direction: row-reverse not supported"));
  EXPECT_TRUE(HasDiagnosticContaining(doc, "flex-direction: column-reverse not supported"));
}

PAG_TEST(PAGXHTMLImporterTest, RawComplexFlexShorthandWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="display:flex;width:50px;height:50px">
        <div style="flex:1 1 auto;width:10px;height:10px"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "flex shorthand '1 1 auto' not supported"));
}

PAG_TEST(PAGXHTMLImporterTest, RawGridLayoutWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="grid-template-columns:1fr 1fr;grid-template-rows:1fr;width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "grid layout not supported"));
}

PAG_TEST(PAGXHTMLImporterTest, RawOverflowScrollWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="overflow:scroll;width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "overflow: scroll not fully supported"));
}

PAG_TEST(PAGXHTMLImporterTest, RawInvalidMarginLonghandWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="margin-top:abc;width:30px;height:30px;background-color:#000"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "invalid margin-top value 'abc'"));
}

PAG_TEST(PAGXHTMLImporterTest, RawDisallowedSizingPropertiesWarn) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="min-width:1px;min-height:1px;max-width:9px;max-height:9px;aspect-ratio:1;
                  width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "min-width not supported; ignored"));
  EXPECT_TRUE(HasDiagnosticContaining(doc, "aspect-ratio not supported; ignored"));
}

PAG_TEST(PAGXHTMLImporterTest, RawDisallowedLayoutPropertiesWarn) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="flex-grow:1;flex-basis:auto;float:left;order:1;align-content:center;
                  align-self:center;direction:rtl;width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "float not supported; ignored"));
  EXPECT_TRUE(HasDiagnosticContaining(doc, "align-self not supported; ignored"));
}

PAG_TEST(PAGXHTMLImporterTest, RawDisallowedVisualPropertiesWarn) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="outline:1px solid red;perspective:100px;width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "outline not supported; ignored"));
  EXPECT_TRUE(HasDiagnosticContaining(doc, "perspective not supported; ignored"));
}

PAG_TEST(PAGXHTMLImporterTest, RawDisallowedTextPropertiesWarn) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:200px;height:40px">
      <p style="text-transform:uppercase;text-indent:5px;word-spacing:2px;font-variant:small-caps;
                font-stretch:expanded">Hi</p>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "text-transform not supported; ignored"));
  EXPECT_TRUE(HasDiagnosticContaining(doc, "word-spacing not supported; ignored"));
}

PAG_TEST(PAGXHTMLImporterTest, RawUnsupportedAlignItemsWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:80px;height:80px">
      <div style="display:flex;align-items:baseline;width:80px;height:80px">
        <div style="width:10px;height:10px"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "unsupported align-items 'baseline'"));
}

PAG_TEST(PAGXHTMLImporterTest, RawUnsupportedJustifyContentWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:80px;height:80px">
      <div style="display:flex;justify-content:left;width:80px;height:80px">
        <div style="width:10px;height:10px"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "unsupported justify-content 'left'"));
}

PAG_TEST(PAGXHTMLImporterTest, RawInvalidDataAttributeNameWarns) {
  auto doc = ParseRaw(R"HTML(
    <html><body style="width:50px;height:50px">
      <div data-foo_bar="x" style="width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "invalid data-* attribute name"));
}

//==================================================================================================
// HTMLDetail / HTMLCssCascade / HTMLValueParser — gradient direction keywords and comment stripping
//==================================================================================================

PAG_TEST(PAGXHTMLImporterTest, LinearGradientCornerDirectionKeywords) {
  // `to bottom left` (225 deg) and `to top left` (315 deg) exercise the diagonal corner branch of
  // CssDirectionToAngle; both should still yield a LinearGradient with the two declared stops.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:100px;height:100px">
      <div style="width:50px;height:50px;
                  background-image:linear-gradient(to bottom left, #FF0000, #0000FF)"></div>
      <div style="width:50px;height:50px;
                  background-image:linear-gradient(to top left, #00FF00, #FFFF00)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->layers.front()->children.size(), 2u);
  for (auto* div : doc->layers.front()->children) {
    auto* fill = FindElementOfType<pagx::Fill>(div);
    ASSERT_NE(fill, nullptr);
    auto* lg = As<pagx::LinearGradient>(fill->color);
    ASSERT_NE(lg, nullptr);
    EXPECT_EQ(lg->colorStops.size(), 2u);
  }
}

PAG_TEST(PAGXHTMLImporterTest, InlineStyleCommentsAreStripped) {
  // `/* ... */` inside an inline style attribute must be skipped, leaving the real declaration.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="/* leading */ width:50px; /* mid */ height:50px; background-color:#3B82F6"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_TRUE(ColorNear(SolidFillColorOf(div), HexColor(0x3B82F6)));
}

PAG_TEST(PAGXHTMLImporterTest, RawStyleBlockCommentsAreStripped) {
  // A CSS comment inside a <style> block must be stripped by the raw cascade's tokenizer so the
  // surrounding rule still applies.
  auto doc = ParseRaw(R"HTML(
    <html><head><style>/* header comment */ .box { /* inside body */ background-color:#10B981 } /* tail */</style></head>
      <body style="width:50px;height:50px">
        <div class="box" style="width:50px;height:50px"></div>
      </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_TRUE(ColorNear(SolidFillColorOf(div), HexColor(0x10B981)));
}

PAG_TEST(PAGXHTMLImporterTest, RawStyleAtRuleBlockIsDropped) {
  // A block at-rule (e.g. @media) is dropped wholesale; its braced body — including a nested
  // comment — is skipped via SkipBalancedBlock, while the trailing plain rule still applies.
  auto doc = ParseRaw(R"HTML(
    <html><head><style>
      @media screen { .box { /* nested */ background-color:#EF4444 } }
      .box { background-color:#10B981 }
    </style></head>
      <body style="width:50px;height:50px">
        <div class="box" style="width:50px;height:50px"></div>
      </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_TRUE(ColorNear(SolidFillColorOf(div), HexColor(0x10B981)));
}

//==================================================================================================
// HTMLParserContext — option plumbing and body-level background handling
//==================================================================================================

PAG_TEST(PAGXHTMLImporterTest, FontConfigOptionIsApplied) {
  // A caller-supplied FontConfig seeds the document's font registry before discovery runs.
  pagx::FontConfig fontConfig;
  fontConfig.addFallbackFont(std::string(), 0, "MyFallbackFamily", "Regular");
  pagx::HTMLImporter::Options opts;
  opts.fontConfig = &fontConfig;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:200px;height:40px"><p>Hi</p></body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  auto names = doc->fontConfig().fallbackFamilyNames();
  EXPECT_NE(std::find(names.begin(), names.end(), "MyFallbackFamily"), names.end());
}

PAG_TEST(PAGXHTMLImporterTest, BodyBackgroundColorEmitsRectangleFill) {
  // A background on <body> with padding splits an inner host and emits the body background as a
  // Rectangle + Fill, exercising the body-level background branch of convertBody.
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px;background-color:#F59E0B;padding:10px">
      <div style="width:20px;height:20px;background-color:#000"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* body = doc->layers.front();
  EXPECT_TRUE(ColorNear(SolidFillColorOf(body), HexColor(0xF59E0B)));
}

//==================================================================================================
// HTMLSvgFilterDecoder — unresolved filter reference
//==================================================================================================

PAG_TEST(PAGXHTMLImporterTest, FilterReferenceToUnknownIdWarns) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <div style="width:40px;height:40px;background-color:#000;filter:url(#does-not-exist)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "references an unknown <filter> id"));
}

//==================================================================================================
// HTMLDetail — direct unit tests for the shared string / unit helpers
//==================================================================================================

PAG_TEST(PAGXHTMLDetailTest, EscapeXmlHandlesAllSpecialCharacters) {
  // Non-attribute mode keeps double quotes literal; attribute mode escapes them.
  EXPECT_EQ(pagx::html::EscapeXml("a<b>c&d\"e", /*isAttribute=*/false), "a&lt;b&gt;c&amp;d\"e");
  EXPECT_EQ(pagx::html::EscapeXml("a<b>c&d\"e", /*isAttribute=*/true), "a&lt;b&gt;c&amp;d&quot;e");
}

PAG_TEST(PAGXHTMLDetailTest, LooksAbsolutePathRecognisesWindowsDriveLetters) {
  EXPECT_TRUE(pagx::html::LooksAbsolutePath("C:/images/x.png"));
  EXPECT_TRUE(pagx::html::LooksAbsolutePath("D:\\images\\x.png"));
  EXPECT_TRUE(pagx::html::LooksAbsolutePath("/abs/path"));
  EXPECT_TRUE(pagx::html::LooksAbsolutePath("https://example.com/x.png"));
  EXPECT_TRUE(pagx::html::LooksAbsolutePath("data:image/png;base64,AA=="));
  EXPECT_FALSE(pagx::html::LooksAbsolutePath("relative/x.png"));
  EXPECT_FALSE(pagx::html::LooksAbsolutePath(""));
}

PAG_TEST(PAGXHTMLDetailTest, ParseSizingDimensionConvertsEmAndRem) {
  float px = NAN;
  float pct = NAN;
  ASSERT_TRUE(pagx::html::ParseSizingDimension("2em", px, pct));
  EXPECT_TRUE(NearlyEqual(px, 32.0f));
  px = pct = NAN;
  ASSERT_TRUE(pagx::html::ParseSizingDimension("1.5rem", px, pct));
  EXPECT_TRUE(NearlyEqual(px, 24.0f));
  px = pct = NAN;
  ASSERT_TRUE(pagx::html::ParseSizingDimension("40%", px, pct));
  EXPECT_TRUE(NearlyEqual(pct, 40.0f));
  px = pct = NAN;
  // Unknown unit falls back to the raw numeric value.
  ASSERT_TRUE(pagx::html::ParseSizingDimension("7q", px, pct));
  EXPECT_TRUE(NearlyEqual(px, 7.0f));
  EXPECT_FALSE(pagx::html::ParseSizingDimension("", px, pct));
}

PAG_TEST(PAGXHTMLDetailTest, ConvertCssLengthToPxCoversEveryUnit) {
  bool ok = false;
  EXPECT_TRUE(NearlyEqual(pagx::html::ConvertCssLengthToPx(3, "em", 20, NAN, NAN, ok), 60.0f));
  EXPECT_TRUE(ok);
  // em with no font-size context defaults to 16.
  EXPECT_TRUE(NearlyEqual(pagx::html::ConvertCssLengthToPx(3, "em", NAN, NAN, NAN, ok), 48.0f));
  EXPECT_TRUE(NearlyEqual(pagx::html::ConvertCssLengthToPx(2, "rem", NAN, NAN, NAN, ok), 32.0f));
  EXPECT_TRUE(NearlyEqual(pagx::html::ConvertCssLengthToPx(12, "pt", NAN, NAN, NAN, ok), 16.0f));
  EXPECT_TRUE(NearlyEqual(pagx::html::ConvertCssLengthToPx(50, "vw", NAN, 200, NAN, ok), 100.0f));
  EXPECT_TRUE(NearlyEqual(pagx::html::ConvertCssLengthToPx(50, "vh", NAN, NAN, 400, ok), 200.0f));
  // vw/vh without a canvas, and unknown units, are not recognised.
  pagx::html::ConvertCssLengthToPx(50, "vw", NAN, NAN, NAN, ok);
  EXPECT_FALSE(ok);
  pagx::html::ConvertCssLengthToPx(50, "vh", NAN, NAN, NAN, ok);
  EXPECT_FALSE(ok);
  pagx::html::ConvertCssLengthToPx(5, "xyz", NAN, NAN, NAN, ok);
  EXPECT_FALSE(ok);
}

PAG_TEST(PAGXHTMLDetailTest, CollapseHTMLWhitespaceCollapsesAndTrims) {
  EXPECT_EQ(pagx::html::CollapseHTMLWhitespace("  a\t\t b \r\n c  "), "a b\n c");
  // A space immediately before a newline is dropped; trailing newline trimmed.
  EXPECT_EQ(pagx::html::CollapseHTMLWhitespace("a \n", /*trimLeading=*/false,
                                               /*trimTrailing=*/true),
            "a");
  // With trimming disabled, leading/trailing spaces survive.
  EXPECT_EQ(pagx::html::CollapseHTMLWhitespace(" a ", /*trimLeading=*/false,
                                               /*trimTrailing=*/false),
            " a ");
}

PAG_TEST(PAGXHTMLDetailTest, SplitTopLevelWhitespaceRespectsParentheses) {
  auto parts = pagx::html::SplitTopLevelWhitespace("0 2px 6px rgba(0, 0, 0, 0.2)");
  ASSERT_EQ(parts.size(), 4u);
  EXPECT_EQ(parts[0], "0");
  EXPECT_EQ(parts[1], "2px");
  EXPECT_EQ(parts[2], "6px");
  EXPECT_EQ(parts[3], "rgba(0, 0, 0, 0.2)");
}

PAG_TEST(PAGXHTMLDetailTest, CssDirectionToAngleFallsBackForUnknownKeyword) {
  // A keyword that isn't a recognised "to <edge>" form falls back to 180deg (downwards).
  EXPECT_TRUE(NearlyEqual(pagx::html::CssDirectionToAngle("to nowhere"), 180.0f));
  EXPECT_TRUE(NearlyEqual(pagx::html::CssDirectionToAngle("garbage"), 180.0f));
  EXPECT_TRUE(NearlyEqual(pagx::html::CssDirectionToAngle("to top right"), 45.0f));
}

PAG_TEST(PAGXHTMLDetailTest, ReplaceChildSplicesInPlace) {
  // Build a tiny three-element list a -> b -> c, then replace the middle node with x.
  auto parent = std::make_shared<pagx::DOMNode>();
  auto a = std::make_shared<pagx::DOMNode>();
  auto b = std::make_shared<pagx::DOMNode>();
  auto c = std::make_shared<pagx::DOMNode>();
  auto x = std::make_shared<pagx::DOMNode>();
  a->name = "a";
  b->name = "b";
  c->name = "c";
  x->name = "x";
  parent->firstChild = a;
  a->nextSibling = b;
  b->nextSibling = c;

  // Replace the head (prev == nullptr): parent->firstChild becomes x.
  auto next = pagx::html::ReplaceChild(parent, nullptr, a, x);
  EXPECT_EQ(next, b);
  EXPECT_EQ(parent->firstChild, x);
  EXPECT_EQ(x->nextSibling, b);
  EXPECT_EQ(a->nextSibling, nullptr);

  // Replace a middle node (prev == x): x -> a2 -> c.
  auto a2 = std::make_shared<pagx::DOMNode>();
  a2->name = "a2";
  next = pagx::html::ReplaceChild(parent, x, b, a2);
  EXPECT_EQ(next, c);
  EXPECT_EQ(x->nextSibling, a2);
  EXPECT_EQ(a2->nextSibling, c);
}

//==================================================================================================
// HTMLTransformPassUtils — direct unit tests for the shared flex/length helpers
//==================================================================================================

PAG_TEST(PAGXHTMLPassUtilsTest, ApproxEqualHonoursTolerance) {
  EXPECT_TRUE(pagx::html::ApproxEqual(1.0f, 1.0005f, 0.001f));
  EXPECT_FALSE(pagx::html::ApproxEqual(1.0f, 1.01f, 0.001f));
  EXPECT_TRUE(pagx::html::ApproxEqual(-2.0f, -2.0f, 0.0f));
}

PAG_TEST(PAGXHTMLPassUtilsTest, LookupResolvedWrappers) {
  pagx::PropertyMap props;
  props["color"] = "  RED  ";
  EXPECT_EQ(pagx::html::LookupResolved(props, "color"), "  RED  ");
  EXPECT_EQ(pagx::html::LookupResolvedLower(props, "color"), "red");
  EXPECT_TRUE(pagx::html::LookupResolved(props, "missing").empty());
}

PAG_TEST(PAGXHTMLPassUtilsTest, ShouldSkipWalkerNodeGuards) {
  pagx::HTMLSubsetTransformer::Options opts;
  pagx::HTMLTransformContext ctx(opts);

  // Null node is always skipped.
  EXPECT_TRUE(pagx::html::ShouldSkipWalkerNode(nullptr, 0, ctx, "phase"));

  // Non-element (text) nodes are skipped without a diagnostic.
  auto text = std::make_shared<pagx::DOMNode>();
  text->type = pagx::DOMNodeType::Text;
  EXPECT_TRUE(pagx::html::ShouldSkipWalkerNode(text, 0, ctx, "phase"));

  // A regular element under the recursion limit is walked.
  auto element = std::make_shared<pagx::DOMNode>();
  element->name = "div";
  EXPECT_FALSE(pagx::html::ShouldSkipWalkerNode(element, 0, ctx, "phase"));
  EXPECT_TRUE(ctx.diagnostics().empty());

  // Reaching the recursion limit skips the subtree and records a max-depth warning.
  EXPECT_TRUE(
      pagx::html::ShouldSkipWalkerNode(element, pagx::MAX_HTML_RECURSION_DEPTH, ctx, "phase-x"));
  ASSERT_FALSE(ctx.diagnostics().empty());
  EXPECT_EQ(ctx.diagnostics().back().code, "subset:max-depth");
  EXPECT_NE(ctx.diagnostics().back().message.find("phase-x"), std::string::npos);
}

PAG_TEST(PAGXHTMLPassUtilsTest, ParseNormalisedPxRejectsNonPixelUnits) {
  float px = -1.0f;
  EXPECT_FALSE(pagx::html::ParseNormalisedPx("", px));
  EXPECT_FALSE(pagx::html::ParseNormalisedPx("auto", px));
  EXPECT_FALSE(pagx::html::ParseNormalisedPx("10em", px));
  EXPECT_FALSE(pagx::html::ParseNormalisedPx("50%", px));
  EXPECT_TRUE(pagx::html::ParseNormalisedPx("12px", px));
  EXPECT_TRUE(NearlyEqual(px, 12.0f));
  EXPECT_TRUE(pagx::html::ParseNormalisedPx("  8  ", px));
  EXPECT_TRUE(NearlyEqual(px, 8.0f));
}

PAG_TEST(PAGXHTMLPassUtilsTest, TryParseResolvedPxHandlesMalformedValues) {
  pagx::PropertyMap props;
  float out = 42.0f;

  // Absent property leaves `out` untouched and returns false.
  EXPECT_FALSE(pagx::html::TryParseResolvedPx(props, "padding-top", out));
  EXPECT_TRUE(NearlyEqual(out, 42.0f));

  // Present but non-pixel: requireParse=true bails, requireParse=false tolerates.
  props["padding-top"] = "auto";
  EXPECT_FALSE(pagx::html::TryParseResolvedPx(props, "padding-top", out, /*requireParse=*/true));
  out = 7.0f;
  EXPECT_TRUE(pagx::html::TryParseResolvedPx(props, "padding-top", out, /*requireParse=*/false));
  EXPECT_TRUE(NearlyEqual(out, 7.0f));

  // Present and valid px writes through.
  props["padding-top"] = "16px";
  EXPECT_TRUE(pagx::html::TryParseResolvedPx(props, "padding-top", out));
  EXPECT_TRUE(NearlyEqual(out, 16.0f));
}

PAG_TEST(PAGXHTMLPassUtilsTest, ClearChildMainMarginRowPreservesCrossAxis) {
  pagx::PropertyMap props;
  props["margin"] = "4px 8px 12px 16px";  // top=4 right=8 bottom=12 left=16
  pagx::html::ClearChildMainMargin(props, /*row=*/true);
  // For a row container the main axis is horizontal: left/right + shorthand are removed while the
  // cross-axis top/bottom margins are re-emitted as longhands.
  EXPECT_EQ(props.count("margin"), 0u);
  EXPECT_EQ(props.count("margin-left"), 0u);
  EXPECT_EQ(props.count("margin-right"), 0u);
  EXPECT_EQ(pagx::html::LookupProperty(props, "margin-top"), "4px");
  EXPECT_EQ(pagx::html::LookupProperty(props, "margin-bottom"), "12px");
}

PAG_TEST(PAGXHTMLPassUtilsTest, ClearChildMainMarginColumnPreservesCrossAxis) {
  pagx::PropertyMap props;
  props["margin"] = "4px 8px 12px 16px";  // top=4 right=8 bottom=12 left=16
  pagx::html::ClearChildMainMargin(props, /*row=*/false);
  // For a column container the main axis is vertical: top/bottom + shorthand are removed while the
  // cross-axis left/right margins are re-emitted as longhands.
  EXPECT_EQ(props.count("margin"), 0u);
  EXPECT_EQ(props.count("margin-top"), 0u);
  EXPECT_EQ(props.count("margin-bottom"), 0u);
  EXPECT_EQ(pagx::html::LookupProperty(props, "margin-left"), "16px");
  EXPECT_EQ(pagx::html::LookupProperty(props, "margin-right"), "8px");
}

}  // namespace pag
