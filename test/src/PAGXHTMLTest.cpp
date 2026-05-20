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
#include <string>
#include <vector>
#include "base/PAGTest.h"
#include "cli/CommandVerify.h"
#include "pagx/HTMLImporter.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXOptimizer.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/InnerShadowStyle.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
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
T* FindElement(const std::vector<pagx::Element*>& contents) {
  for (auto* e : contents) {
    if (auto* match = dynamic_cast<T*>(e)) {
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
    if (dynamic_cast<T*>(e)) count++;
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
  auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
  if (!solid) return {};
  return solid->color;
}

}  // namespace

//==================================================================================================
// Unit tests: CSS property mapping
//==================================================================================================

PAG_TEST(PAGXHTMLTest, ParsesCanvasSizeFromBody) {
  auto doc =
      ParseFromString(R"HTML(<html><body style="width:200px;height:100px"></body></html>)HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FLOAT_EQ(doc->width, 200.0f);
  EXPECT_FLOAT_EQ(doc->height, 100.0f);
  EXPECT_EQ(doc->layers.size(), 1u);
}

PAG_TEST(PAGXHTMLTest, RejectsMissingCanvas) {
  auto doc = ParseFromString(R"HTML(<html><body></body></html>)HTML");
  EXPECT_EQ(doc, nullptr);
}

PAG_TEST(PAGXHTMLTest, BackgroundColorBecomesRectangleAndFill) {
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
  auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0xFF0000)));
  EXPECT_FLOAT_EQ(div->width, 80.0f);
  EXPECT_FLOAT_EQ(div->height, 40.0f);
}

PAG_TEST(PAGXHTMLTest, BorderRadiusMapsToRoundness) {
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
// element to draw circles. Percentage values must be resolved against the box's
// known px dimensions (using min(width, height) since PAGX Rectangle exposes a
// single uniform corner radius), so a 12x12 element with `border-radius: 50%`
// becomes a true circle (roundness = 6).
PAG_TEST(PAGXHTMLTest, BorderRadiusPercentBecomesCircleOnSquareBox) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:24px;height:24px">
      <div style="width:12px;height:12px;background-color:#FF5F57;border-radius:50%"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* rect = FindElementOfType<pagx::Rectangle>(div);
  ASSERT_NE(rect, nullptr);
  EXPECT_FLOAT_EQ(rect->roundness, 6.0f);
}

// On a non-square box `border-radius: 50%` would mean an ellipse in CSS, but
// PAGX Rectangle only supports a single uniform radius. We resolve against the
// shorter side so the result becomes the closest pill shape the primitive can
// render (here: 40x12 -> roundness 6, i.e. semicircular ends, no over-rounding).
PAG_TEST(PAGXHTMLTest, BorderRadiusPercentOnRectangleUsesShorterSide) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:60px;height:20px">
      <div style="width:40px;height:12px;background-color:#28C840;border-radius:50%"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* rect = FindElementOfType<pagx::Rectangle>(div);
  ASSERT_NE(rect, nullptr);
  EXPECT_FLOAT_EQ(rect->roundness, 6.0f);
}

// A percentage radius on an element whose width/height is not given in px
// cannot be resolved at parse time (the parent's layout would be needed) — the
// importer drops the value with a warning rather than rendering a wrong shape.
PAG_TEST(PAGXHTMLTest, BorderRadiusPercentWithoutFixedSizeIsDropped) {
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

PAG_TEST(PAGXHTMLTest, BorderProducesStroke) {
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
  auto* solid = dynamic_cast<pagx::SolidColor*>(stroke->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0x00FF00)));
  EXPECT_EQ(stroke->align, pagx::StrokeAlign::Inside);
}

// Regression: with `box-sizing: border-box` (the only mode supported by the importer),
// CSS positions absolutely-positioned descendants relative to the parent's padding box
// (inside the border). The importer must shift the child's left/right/top/bottom
// constraints by the parent's border width so the child does not overlap the inside
// stroke.
PAG_TEST(PAGXHTMLTest, BorderShiftsAbsoluteChildren) {
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
PAG_TEST(PAGXHTMLTest, BorderExpandsLayoutPadding) {
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

PAG_TEST(PAGXHTMLTest, BoxShadowProducesDropShadowStyle) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <div style="width:80px;height:80px;background-color:#FFF;box-shadow:0 2px 8px #00000033"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  ASSERT_EQ(div->styles.size(), 1u);
  auto* drop = dynamic_cast<pagx::DropShadowStyle*>(div->styles.front());
  ASSERT_NE(drop, nullptr);
  EXPECT_FLOAT_EQ(drop->offsetX, 0.0f);
  EXPECT_FLOAT_EQ(drop->offsetY, 2.0f);
  EXPECT_FLOAT_EQ(drop->blurX, 8.0f);
  EXPECT_FLOAT_EQ(drop->blurY, 8.0f);
  EXPECT_TRUE(ColorNear(drop->color, HexColor(0x000000, 0.2f), 0.02f));
}

PAG_TEST(PAGXHTMLTest, InsetBoxShadowProducesInnerShadowStyle) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:#FFF;box-shadow:inset 2px 2px 4px #000"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  ASSERT_EQ(div->styles.size(), 1u);
  auto* inner = dynamic_cast<pagx::InnerShadowStyle*>(div->styles.front());
  ASSERT_NE(inner, nullptr);
  EXPECT_FLOAT_EQ(inner->offsetX, 2.0f);
  EXPECT_FLOAT_EQ(inner->offsetY, 2.0f);
  EXPECT_FLOAT_EQ(inner->blurX, 4.0f);
}

PAG_TEST(PAGXHTMLTest, MultipleBoxShadows) {
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

PAG_TEST(PAGXHTMLTest, OpacityMapsToLayerAlpha) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;opacity:0.5"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_FLOAT_EQ(div->alpha, 0.5f);
}

PAG_TEST(PAGXHTMLTest, MixBlendModeMapsToBlendMode) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:#000;mix-blend-mode:multiply"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_EQ(div->blendMode, pagx::BlendMode::Multiply);
}

PAG_TEST(PAGXHTMLTest, FlexLayoutMapping) {
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

PAG_TEST(PAGXHTMLTest, PaddingShorthandFourValues) {
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

PAG_TEST(PAGXHTMLTest, AbsolutePositionMapping) {
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

PAG_TEST(PAGXHTMLTest, OuterBackgroundInnerPaddedSplit) {
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

PAG_TEST(PAGXHTMLTest, LinearGradientAngleConversion) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-image:linear-gradient(90deg, #FF0000 0%, #0000FF 100%)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(div);
  ASSERT_NE(fill, nullptr);
  auto* lg = dynamic_cast<pagx::LinearGradient*>(fill->color);
  ASSERT_NE(lg, nullptr);
  // CSS 90deg (to right) maps to PAGX 0° (along +X axis). startPoint(0, 0.5) → endPoint(1, 0.5).
  EXPECT_TRUE(NearlyEqual(lg->startPoint.x, 0.0f, 0.005f));
  EXPECT_TRUE(NearlyEqual(lg->startPoint.y, 0.5f, 0.005f));
  EXPECT_TRUE(NearlyEqual(lg->endPoint.x, 1.0f, 0.005f));
  EXPECT_TRUE(NearlyEqual(lg->endPoint.y, 0.5f, 0.005f));
  ASSERT_EQ(lg->colorStops.size(), 2u);
  EXPECT_TRUE(ColorNear(lg->colorStops.front()->color, HexColor(0xFF0000)));
  EXPECT_TRUE(ColorNear(lg->colorStops.back()->color, HexColor(0x0000FF)));
}

PAG_TEST(PAGXHTMLTest, RadialGradient) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-image:radial-gradient(circle at center, #FFFFFF 0%, #000000 100%)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(div);
  ASSERT_NE(fill, nullptr);
  auto* rg = dynamic_cast<pagx::RadialGradient*>(fill->color);
  ASSERT_NE(rg, nullptr);
  EXPECT_EQ(rg->colorStops.size(), 2u);
}

PAG_TEST(PAGXHTMLTest, ConicGradientAngleOffset) {
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
  auto* cg = dynamic_cast<pagx::ConicGradient*>(fill->color);
  ASSERT_NE(cg, nullptr);
  // CSS 0° = top, PAGX 0° = right ⇒ PAGX angle = −90°.
  EXPECT_TRUE(NearlyEqual(cg->startAngle, -90.0f, 0.01f));
}

PAG_TEST(PAGXHTMLTest, FilterBlurAndDropShadow) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:#000;
                  filter:blur(2px) drop-shadow(0 1px 3px #00000055)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  ASSERT_EQ(div->filters.size(), 2u);
  auto* blur = dynamic_cast<pagx::BlurFilter*>(div->filters[0]);
  ASSERT_NE(blur, nullptr);
  EXPECT_FLOAT_EQ(blur->blurX, 2.0f);
  EXPECT_FLOAT_EQ(blur->blurY, 2.0f);
  auto* drop = dynamic_cast<pagx::DropShadowFilter*>(div->filters[1]);
  ASSERT_NE(drop, nullptr);
  EXPECT_FLOAT_EQ(drop->offsetY, 1.0f);
}

PAG_TEST(PAGXHTMLTest, BackdropFilterMapsToBackgroundBlurStyle) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:#FFF8;backdrop-filter:blur(6px)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  bool foundBlur = false;
  for (auto* s : div->styles) {
    if (auto* b = dynamic_cast<pagx::BackgroundBlurStyle*>(s)) {
      EXPECT_FLOAT_EQ(b->blurX, 6.0f);
      foundBlur = true;
    }
  }
  EXPECT_TRUE(foundBlur);
}

PAG_TEST(PAGXHTMLTest, OverflowHiddenMapsToClipToBounds) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;overflow:hidden;background-color:#000"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_TRUE(div->clipToBounds);
}

PAG_TEST(PAGXHTMLTest, SimpleTextLeafSingleStyle) {
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
    if (dynamic_cast<pagx::TextBox*>(e)) hasTextBox = true;
  }
  EXPECT_FALSE(hasTextBox);
}

PAG_TEST(PAGXHTMLTest, RichTextSpansEmitTextBoxFragments) {
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
    if (dynamic_cast<pagx::Text*>(e)) textCount++;
    if (dynamic_cast<pagx::Group*>(e)) groupCount++;
  }
  EXPECT_EQ(textCount, 1u);
  EXPECT_EQ(groupCount, 1u);
}

PAG_TEST(PAGXHTMLTest, TextDecorationUnderlineOverlay) {
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

PAG_TEST(PAGXHTMLTest, BrTagInsertsNewline) {
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

PAG_TEST(PAGXHTMLTest, ImageRegistersImageResource) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <img src="logo.png" style="width:80px;height:80px"/>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(leaf);
  ASSERT_NE(fill, nullptr);
  auto* pattern = dynamic_cast<pagx::ImagePattern*>(fill->color);
  ASSERT_NE(pattern, nullptr);
  ASSERT_NE(pattern->image, nullptr);
  EXPECT_FALSE(pattern->image->filePath.empty());
}

PAG_TEST(PAGXHTMLTest, InlineSvgPassedAsImportDirective) {
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

// Regression: inline <svg> with only viewBox (no width/height) must keep the
// case-sensitive `viewBox` attribute through the HTML import pipeline. The
// browser-emitted snapshot (e.g. from tools/html-snapshot) lowercases HTML
// attribute names but preserves SVG's camelCase ones, and the SVG importer used
// during `pagx resolve` looks up the attribute case-sensitively. If the
// importer lowercases `viewBox`, the inline content fails to parse because the
// SVG has no fallback width/height.
PAG_TEST(PAGXHTMLTest, InlineSvgPreservesCamelCaseAttributes) {
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

PAG_TEST(PAGXHTMLTest, StyleClassRulesApply) {
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
  auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0x123456)));
  auto* rect = FindElementOfType<pagx::Rectangle>(div);
  ASSERT_NE(rect, nullptr);
  EXPECT_FLOAT_EQ(rect->roundness, 8.0f);
}

PAG_TEST(PAGXHTMLTest, UnknownElementWarningEmitted) {
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

PAG_TEST(PAGXHTMLTest, MarginEmitsWarning) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="margin:8px;width:30px;height:30px;background-color:#000"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  bool hasWarning = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("margin") != std::string::npos) hasWarning = true;
  }
  EXPECT_TRUE(hasWarning);
}

PAG_TEST(PAGXHTMLTest, DisallowedSizingPropertiesEmitWarnings) {
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

PAG_TEST(PAGXHTMLTest, DisallowedLayoutPropertiesEmitWarnings) {
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

PAG_TEST(PAGXHTMLTest, DisallowedVisualPropertiesEmitWarnings) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="outline:1px solid red;clip-path:circle(20px);transform-origin:center;
                  background-size:cover;width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  bool outlineWarn = false, clipWarn = false, originWarn = false, bgSizeWarn = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("outline") != std::string::npos) outlineWarn = true;
    if (msg.find("clip-path") != std::string::npos) clipWarn = true;
    if (msg.find("transform-origin") != std::string::npos) originWarn = true;
    if (msg.find("background-size") != std::string::npos) bgSizeWarn = true;
  }
  EXPECT_TRUE(outlineWarn);
  EXPECT_TRUE(clipWarn);
  EXPECT_TRUE(originWarn);
  EXPECT_TRUE(bgSizeWarn);
}

PAG_TEST(PAGXHTMLTest, DisallowedTextPropertiesEmitWarnings) {
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

PAG_TEST(PAGXHTMLTest, BoxSizingNonBorderBoxWarns) {
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

PAG_TEST(PAGXHTMLTest, UniversalSelectorWarnsAndDoesNotApply) {
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

PAG_TEST(PAGXHTMLTest, FlexShorthandWithDisallowedFormWarns) {
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

PAG_TEST(PAGXHTMLTest, AbsoluteOnTextLeafEmitsWarning) {
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

PAG_TEST(PAGXHTMLTest, TextLeafWithBlockChildFallsBackToContainer) {
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

PAG_TEST(PAGXHTMLTest, MissingCanvasReportsHardError) {
  auto doc = ParseFromString(R"HTML(<html><body></body></html>)HTML");
  EXPECT_EQ(doc, nullptr);
}

PAG_TEST(PAGXHTMLTest, RootLevelExtraElementWarns) {
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

PAG_TEST(PAGXHTMLTest, HeadDisallowedChildrenWarn) {
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

PAG_TEST(PAGXHTMLTest, ExternalStylesheetLinkWarns) {
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

PAG_TEST(PAGXHTMLTest, TextDecorationColorDifferingWrapsInGroup) {
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
    if (auto* g = dynamic_cast<pagx::Group*>(e)) {
      auto* rect = FindElement<pagx::Rectangle>(g->elements);
      auto* fill = FindElement<pagx::Fill>(g->elements);
      ASSERT_NE(rect, nullptr);
      ASSERT_NE(fill, nullptr);
      auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
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

PAG_TEST(PAGXHTMLTest, TitleStoredAsDataTitle) {
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

PAG_TEST(PAGXHTMLTest, MetaTagIgnoredSilently) {
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

PAG_TEST(PAGXHTMLTest, RejectsMissingBody) {
  auto doc = ParseFromString(R"HTML(<html><head></head></html>)HTML");
  EXPECT_EQ(doc, nullptr);
}

PAG_TEST(PAGXHTMLTest, CommaSeparatedClassRulesApply) {
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
    auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
    ASSERT_NE(solid, nullptr);
    EXPECT_TRUE(ColorNear(solid->color, HexColor(0x112233)));
    EXPECT_FLOAT_EQ(rect->roundness, 6.0f);
  }
}

PAG_TEST(PAGXHTMLTest, MultipleClassesMergeWithLastWinning) {
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
  auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0xFF8800)));
  EXPECT_FLOAT_EQ(rect->roundness, 4.0f);
}

PAG_TEST(PAGXHTMLTest, InlineStyleBeatsClassRule) {
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
  auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0x22AAEE)));
}

PAG_TEST(PAGXHTMLTest, ElementSelectorAppliesToTag) {
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
  auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0x33CC44)));
}

PAG_TEST(PAGXHTMLTest, UnsupportedSelectorIgnoredAndDoesNotApply) {
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
  auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0x00FF00)));
}

//==================================================================================================
// Sizing / positioning / element type tests
//==================================================================================================

PAG_TEST(PAGXHTMLTest, PercentSizingMapsToPercentFields) {
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

PAG_TEST(PAGXHTMLTest, SemanticContainersMapToLayer) {
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

PAG_TEST(PAGXHTMLTest, NonStaticPositionDowngradesAbsolute) {
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

PAG_TEST(PAGXHTMLTest, FlexDirectionColumn) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:200px">
      <div style="display:flex;flex-direction:column;width:50px;height:200px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* div = doc->layers.front()->children.front();
  EXPECT_EQ(div->layout, pagx::LayoutMode::Vertical);
}

PAG_TEST(PAGXHTMLTest, AlignItemsAndJustifyContentVariants) {
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

PAG_TEST(PAGXHTMLTest, PaddingShorthandTwoAndThreeValues) {
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

PAG_TEST(PAGXHTMLTest, NamedColorAccepted) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:red"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* fill = FindElementOfType<pagx::Fill>(doc->layers.front()->children.front());
  ASSERT_NE(fill, nullptr);
  auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0xFF0000)));
}

PAG_TEST(PAGXHTMLTest, ShortHexBackgroundColorAccepted) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-color:#0F0"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(
      ColorNear(SolidFillColorOf(doc->layers.front()->children.front()), HexColor(0x00FF00)));
}

PAG_TEST(PAGXHTMLTest, RgbaTextColorParsed) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style="color:rgba(0,0,255,0.5)">Hi</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(
      ColorNear(SolidFillColorOf(doc->layers.front()->children.front()), HexColor(0x0000FF, 0.5f)));
}

PAG_TEST(PAGXHTMLTest, RgbBackgroundColorParsed) {
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

PAG_TEST(PAGXHTMLTest, RgbaBackgroundColorParsed) {
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

PAG_TEST(PAGXHTMLTest, BodyDefaultsArial14Color) {
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
  auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0x1E293B)));
}

PAG_TEST(PAGXHTMLTest, HeadingDefaultFontSizes) {
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
    EXPECT_EQ(text->fontStyle, "Bold") << r.tag;
  }
}

PAG_TEST(PAGXHTMLTest, FontWeightNumericMapsToBold) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style="font-weight:700">Heavy</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* text = FindElementOfType<pagx::Text>(doc->layers.front()->children.front());
  ASSERT_NE(text, nullptr);
  EXPECT_EQ(text->fontStyle, "Bold");
}

PAG_TEST(PAGXHTMLTest, FontWeightUnder600IsNotBold) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style="font-weight:500">Medium</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* text = FindElementOfType<pagx::Text>(doc->layers.front()->children.front());
  ASSERT_NE(text, nullptr);
  EXPECT_TRUE(text->fontStyle.empty());
}

PAG_TEST(PAGXHTMLTest, BoldItalicCombined) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <span style="font-weight:bold;font-style:italic">Hi</span>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* text = FindElementOfType<pagx::Text>(doc->layers.front()->children.front());
  ASSERT_NE(text, nullptr);
  EXPECT_EQ(text->fontStyle, "Bold Italic");
}

PAG_TEST(PAGXHTMLTest, FontFamilyAndLetterSpacing) {
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

PAG_TEST(PAGXHTMLTest, TextAlignAndLineHeightOnParagraph) {
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

PAG_TEST(PAGXHTMLTest, TextAlignJustifyMapped) {
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

PAG_TEST(PAGXHTMLTest, WhiteSpaceNowrapDisablesWrap) {
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

PAG_TEST(PAGXHTMLTest, OverflowHiddenOnTextContainerHidesText) {
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

PAG_TEST(PAGXHTMLTest, TextDecorationLineThroughOverlay) {
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

PAG_TEST(PAGXHTMLTest, LinearGradientToBottomRight) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-image:linear-gradient(to bottom right, #F00, #00F)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* fill = FindElementOfType<pagx::Fill>(doc->layers.front()->children.front());
  ASSERT_NE(fill, nullptr);
  auto* lg = dynamic_cast<pagx::LinearGradient*>(fill->color);
  ASSERT_NE(lg, nullptr);
  // CSS 135deg → PAGX 45deg (top-left to bottom-right diagonal).
  EXPECT_GT(lg->endPoint.x, lg->startPoint.x);
  EXPECT_GT(lg->endPoint.y, lg->startPoint.y);
}

PAG_TEST(PAGXHTMLTest, LinearGradientThreeStopsInterpolatesMiddleOffset) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-image:linear-gradient(90deg,#F00,#0F0,#00F)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* fill = FindElementOfType<pagx::Fill>(doc->layers.front()->children.front());
  auto* lg = dynamic_cast<pagx::LinearGradient*>(fill->color);
  ASSERT_NE(lg, nullptr);
  ASSERT_EQ(lg->colorStops.size(), 3u);
  EXPECT_TRUE(NearlyEqual(lg->colorStops[0]->offset, 0.0f, 0.01f));
  EXPECT_TRUE(NearlyEqual(lg->colorStops[1]->offset, 0.5f, 0.01f));
  EXPECT_TRUE(NearlyEqual(lg->colorStops[2]->offset, 1.0f, 0.01f));
}

PAG_TEST(PAGXHTMLTest, ConicGradientFrom90DegMapsToZero) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-image:conic-gradient(from 90deg, #F00, #00F)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* fill = FindElementOfType<pagx::Fill>(doc->layers.front()->children.front());
  auto* cg = dynamic_cast<pagx::ConicGradient*>(fill->color);
  ASSERT_NE(cg, nullptr);
  // CSS 90° (right) ⇒ PAGX 0° (along +X axis).
  EXPECT_TRUE(NearlyEqual(cg->startAngle, 0.0f, 0.01f));
}

PAG_TEST(PAGXHTMLTest, AnchorHrefStoredAsCustomData) {
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

PAG_TEST(PAGXHTMLTest, AnchorDefaultsBlueAndUnderline) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:200px;height:40px">
      <a href="https://e.test">Link</a>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(leaf);
  ASSERT_NE(fill, nullptr);
  auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0x2563EB)));
  // Underline overlay rectangle should exist.
  EXPECT_GE(CountElements<pagx::Rectangle>(leaf->contents), 1u);
}

PAG_TEST(PAGXHTMLTest, DataStarAttributesPropagate) {
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

PAG_TEST(PAGXHTMLTest, IdAttributePropagatesToLayer) {
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

PAG_TEST(PAGXHTMLTest, ImgWithSvgSrcBecomesImportDirective) {
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
PAG_TEST(PAGXHTMLTest, RoundedImageWrapperFoldsIntoRectangle) {
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
  auto* pattern = dynamic_cast<pagx::ImagePattern*>(fill->color);
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
PAG_TEST(PAGXHTMLTest, ImageDefaultObjectFitIsStretch) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <img src="logo.png" style="width:80px;height:80px"/>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(leaf);
  ASSERT_NE(fill, nullptr);
  auto* pattern = dynamic_cast<pagx::ImagePattern*>(fill->color);
  ASSERT_NE(pattern, nullptr);
  EXPECT_EQ(pattern->scaleMode, pagx::ScaleMode::Stretch);
}

PAG_TEST(PAGXHTMLTest, ImageObjectFitContainMapsToLetterBox) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <img src="logo.png" style="width:80px;height:80px;object-fit:contain"/>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(leaf);
  ASSERT_NE(fill, nullptr);
  auto* pattern = dynamic_cast<pagx::ImagePattern*>(fill->color);
  ASSERT_NE(pattern, nullptr);
  EXPECT_EQ(pattern->scaleMode, pagx::ScaleMode::LetterBox);
}

PAG_TEST(PAGXHTMLTest, ImageObjectFitCoverMapsToZoom) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <img src="logo.png" style="width:80px;height:80px;object-fit:cover"/>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto* fill = FindElementOfType<pagx::Fill>(leaf);
  ASSERT_NE(fill, nullptr);
  auto* pattern = dynamic_cast<pagx::ImagePattern*>(fill->color);
  ASSERT_NE(pattern, nullptr);
  EXPECT_EQ(pattern->scaleMode, pagx::ScaleMode::Zoom);
}

PAG_TEST(PAGXHTMLTest, RoundedImageWrapperRespectsObjectFit) {
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
  auto* pattern = dynamic_cast<pagx::ImagePattern*>(fill->color);
  ASSERT_NE(pattern, nullptr);
  EXPECT_EQ(pattern->scaleMode, pagx::ScaleMode::Zoom);
}

// Sibling assertion: the fold only kicks in when the wrapper is purely an image
// clip. A wrapper with multiple children must keep the standard container layout
// even when it carries `border-radius` + `overflow: hidden` (the image stays in
// its own child Layer so neighbouring content paints normally above it).
PAG_TEST(PAGXHTMLTest, RoundedImageWrapperKeepsLayoutForExtraChildren) {
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

PAG_TEST(PAGXHTMLTest, RepeatedImageSourceDeduplicated) {
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
  auto* patA = dynamic_cast<pagx::ImagePattern*>(fillA->color);
  auto* patB = dynamic_cast<pagx::ImagePattern*>(fillB->color);
  ASSERT_NE(patA, nullptr);
  ASSERT_NE(patB, nullptr);
  EXPECT_EQ(patA->image, patB->image);
}

PAG_TEST(PAGXHTMLTest, StrictModeRejectsUnsupportedElement) {
  pagx::HTMLImporter::Options opts;
  opts.strict = true;
  std::string html =
      R"HTML(<html><body style="width:50px;height:50px"><canvas></canvas></body></html>)HTML";
  auto doc = pagx::HTMLImporter::ParseString(html, opts);
  EXPECT_EQ(doc, nullptr);
}

PAG_TEST(PAGXHTMLTest, PreserveUnknownElementsKeepsPlaceholder) {
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

PAG_TEST(PAGXHTMLTest, TargetSizeOverridesBody) {
  pagx::HTMLImporter::Options opts;
  opts.targetWidth = 400.0f;
  opts.targetHeight = 200.0f;
  std::string html = R"HTML(<html><body style="width:50px;height:50px"></body></html>)HTML";
  auto doc = pagx::HTMLImporter::ParseString(html, opts);
  ASSERT_NE(doc, nullptr);
  EXPECT_FLOAT_EQ(doc->width, 400.0f);
  EXPECT_FLOAT_EQ(doc->height, 200.0f);
}

PAG_TEST(PAGXHTMLTest, PreferBodySizeWinsOverTarget) {
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

PAG_TEST(PAGXHTMLTest, TargetUsedWhenBodyMissingSize) {
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
  auto outPath = SaveFile(xml, "PAGXHTMLTest/" + name + ".pagx");
  VerifyFile(outPath, name);
}

PAG_TEST(PAGXHTMLTest, FixtureCard) {
  RunFixture("card");
}

PAG_TEST(PAGXHTMLTest, FixtureTabBar) {
  RunFixture("tab_bar");
}

PAG_TEST(PAGXHTMLTest, FixtureLogin) {
  RunFixture("login");
}

PAG_TEST(PAGXHTMLTest, FixtureTableVisual) {
  RunFixture("table_visual");
}

}  // namespace pag
