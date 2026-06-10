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
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/Channel.h"
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
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
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

// Returns the first Channel named `name` across all AnimationObjects of `anim`, or nullptr.
inline pagx::Channel* FindChannel(pagx::Animation* anim, const std::string& name) {
  if (!anim) return nullptr;
  for (auto* obj : anim->objects) {
    for (auto* ch : obj->channels) {
      if (ch && ch->name == name) return ch;
    }
  }
  return nullptr;
}

// Returns the AnimationObject whose `target` matches `targetId`, or nullptr.
inline pagx::AnimationObject* FindObjectByTarget(pagx::Animation* anim,
                                                 const std::string& targetId) {
  if (!anim) return nullptr;
  for (auto* obj : anim->objects) {
    if (obj && obj->target == targetId) return obj;
  }
  return nullptr;
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
  auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
  ASSERT_NE(solid, nullptr);
  EXPECT_TRUE(ColorNear(solid->color, HexColor(0xFF0000)));
  EXPECT_FLOAT_EQ(div->width, 80.0f);
  EXPECT_FLOAT_EQ(div->height, 40.0f);
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
// element to draw circles. Percentage values must be resolved against the box's
// known px dimensions (using min(width, height) since PAGX Rectangle exposes a
// single uniform corner radius), so a 12x12 element with `border-radius: 50%`
// becomes a true circle (roundness = 6).
PAG_TEST(PAGXHTMLImporterTest, BorderRadiusPercentBecomesCircleOnSquareBox) {
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
PAG_TEST(PAGXHTMLImporterTest, BorderRadiusPercentOnRectangleUsesShorterSide) {
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
  auto* drop = dynamic_cast<pagx::DropShadowStyle*>(div->styles.front());
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
  auto* inner = dynamic_cast<pagx::InnerShadowStyle*>(div->styles.front());
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
  auto* drop = dynamic_cast<pagx::DropShadowStyle*>(outer->styles.front());
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
  EXPECT_NE(dynamic_cast<pagx::InnerShadowStyle*>(div->styles.front()), nullptr);
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
  auto* rg = dynamic_cast<pagx::RadialGradient*>(fill->color);
  ASSERT_NE(rg, nullptr);
  EXPECT_EQ(rg->colorStops.size(), 2u);
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
  auto* cg = dynamic_cast<pagx::ConicGradient*>(fill->color);
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
  auto* blur = dynamic_cast<pagx::BlurFilter*>(div->filters[0]);
  ASSERT_NE(blur, nullptr);
  EXPECT_FLOAT_EQ(blur->blurX, 2.0f);
  EXPECT_FLOAT_EQ(blur->blurY, 2.0f);
  auto* drop = dynamic_cast<pagx::DropShadowFilter*>(div->filters[1]);
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
    if (auto* b = dynamic_cast<pagx::BackgroundBlurStyle*>(s)) {
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
    if (dynamic_cast<pagx::TextBox*>(e)) hasTextBox = true;
  }
  EXPECT_FALSE(hasTextBox);
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
    if (dynamic_cast<pagx::Text*>(e)) textCount++;
    if (dynamic_cast<pagx::Group*>(e)) groupCount++;
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
    if (auto* t = dynamic_cast<pagx::Text*>(e)) {
      texts->push_back(t);
    } else if (auto* f = dynamic_cast<pagx::Fill*>(e)) {
      fills->push_back(f);
    } else if (auto* g = dynamic_cast<pagx::Group*>(e)) {
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
  auto* solid0 = dynamic_cast<pagx::SolidColor*>(fills[0]->color);
  auto* solid1 = dynamic_cast<pagx::SolidColor*>(fills[1]->color);
  ASSERT_NE(solid0, nullptr);
  ASSERT_NE(solid1, nullptr);
  EXPECT_TRUE(ColorNear(solid0->color, HexColor(0x111111, 1.0f)));
  EXPECT_TRUE(ColorNear(solid1->color, HexColor(0x111111, 0.7f)));
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
  auto* pattern = dynamic_cast<pagx::ImagePattern*>(fill->color);
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
  auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
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
                  background-size:cover;width:50px;height:50px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  bool outlineWarn = false, clipWarn = false, bgSizeWarn = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("outline") != std::string::npos) outlineWarn = true;
    if (msg.find("clip-path") != std::string::npos) clipWarn = true;
    if (msg.find("background-size") != std::string::npos) bgSizeWarn = true;
  }
  EXPECT_TRUE(outlineWarn);
  EXPECT_TRUE(clipWarn);
  EXPECT_TRUE(bgSizeWarn);
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
    auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
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
  auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
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
  auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
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
  auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
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
  auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
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
  auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
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
  auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
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
  auto* lg = dynamic_cast<pagx::LinearGradient*>(fill->color);
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
  auto* lg = dynamic_cast<pagx::LinearGradient*>(fill->color);
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
  auto* cg = dynamic_cast<pagx::ConicGradient*>(fill->color);
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
  auto* lg = dynamic_cast<pagx::LinearGradient*>(textFill->color);
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
  EXPECT_NE(dynamic_cast<pagx::LinearGradient*>(fill->color), nullptr);
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
  auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
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
  auto* pattern = dynamic_cast<pagx::ImagePattern*>(fill->color);
  ASSERT_NE(pattern, nullptr);
  EXPECT_EQ(pattern->scaleMode, pagx::ScaleMode::Stretch);
}

// Direct `border-radius` on `<img>` (the canonical "circular avatar" idiom: `<img
// style="border-radius:50%">`) used to be silently dropped — `convertImage` emitted a plain
// `Rectangle` regardless of the box's border-radius. The geometry helper now flows through it,
// so a uniform radius rounds the Rectangle and an asymmetric one falls onto the Path emitter,
// matching the `<div>` + background-image path.
PAG_TEST(PAGXHTMLImporterTest, ImageWithUniformBorderRadiusRoundsRectangle) {
  auto doc = ParseFromString(R"HTML(
    <html><body style="width:80px;height:80px">
      <img src="avatar.png" style="width:64px;height:64px;border-radius:50%"/>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* leaf = doc->layers.front()->children.front();
  auto* rect = FindElementOfType<pagx::Rectangle>(leaf);
  ASSERT_NE(rect, nullptr);
  EXPECT_FLOAT_EQ(rect->roundness, 32.0f);
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
  auto* pattern = dynamic_cast<pagx::ImagePattern*>(fill->color);
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
  auto* pattern = dynamic_cast<pagx::ImagePattern*>(fill->color);
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
  auto* pattern = dynamic_cast<pagx::ImagePattern*>(fill->color);
  ASSERT_NE(pattern, nullptr);
  EXPECT_EQ(pattern->scaleMode, pagx::ScaleMode::Zoom);
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
  // Path emitter starts at (W, TR) = (64, 16). The bottom corners stay sharp because BR/BL
  // are 0, so the path has exactly two cubic segments (TL + TR).
  size_t cubicCount = 0;
  for (auto v : path->data->verbs()) {
    if (v == pagx::PathVerb::Cubic) cubicCount++;
  }
  EXPECT_EQ(cubicCount, 2u);
  auto* fill = FindElementOfType<pagx::Fill>(wrapper);
  ASSERT_NE(fill, nullptr);
  auto* pattern = dynamic_cast<pagx::ImagePattern*>(fill->color);
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
  auto* pattern = dynamic_cast<pagx::ImagePattern*>(fill->color);
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
  auto* patA = dynamic_cast<pagx::ImagePattern*>(fillA->color);
  auto* patB = dynamic_cast<pagx::ImagePattern*>(fillB->color);
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
  auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
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
    auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
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
  auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
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
  auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
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
  auto* solid = dynamic_cast<pagx::SolidColor*>(fill->color);
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

PAG_TEST(PAGXHTMLImporterTest, RawBackgroundUrlWarns) {
  // Without the subset transformer, the layer builder receives the raw `url(...)` and warns.
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:50px;height:50px">
      <div style="width:50px;height:50px;background-image:url(theme.png)"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("url") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
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
  auto* drop = dynamic_cast<pagx::DropShadowStyle*>(div->styles.front());
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
  auto* lg = dynamic_cast<pagx::LinearGradient*>(fill->color);
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
  auto* lg = dynamic_cast<pagx::LinearGradient*>(fill->color);
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
  auto* rg = dynamic_cast<pagx::RadialGradient*>(fill->color);
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
  auto* lg = dynamic_cast<pagx::LinearGradient*>(fill->color);
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

PAG_TEST(PAGXHTMLImporterTest, ImageMissingSrcWarnsAndIsSkipped) {
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
  auto* rg = dynamic_cast<pagx::RadialGradient*>(fill->color);
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
  auto* cg = dynamic_cast<pagx::ConicGradient*>(fill->color);
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
  auto* lg = dynamic_cast<pagx::LinearGradient*>(fill->color);
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
// Animation: @keyframes + animation -> PAGX <Animations> (spec/html_subset.md §13)
//==================================================================================================

PAG_TEST(PAGXHTMLImporterTest, AnimationOpacityProducesAlphaChannel) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><head><style>
      @keyframes fade { 0% { opacity: 0; } 100% { opacity: 1; } }
    </style></head>
    <body style="width:200px;height:100px">
      <div id="card" style="width:50px;height:50px;background-color:#000;
                            animation:fade 2s linear infinite"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->animations.size(), 1u);
  auto* anim = doc->animations.front();
  EXPECT_EQ(anim->loop, pagx::LoopMode::Loop);
  EXPECT_FLOAT_EQ(anim->frameRate, 60.0f);
  // 2s at 60fps = 120 frames, no delay.
  EXPECT_EQ(anim->duration, 120);
  auto* obj = FindObjectByTarget(anim, "card");
  ASSERT_NE(obj, nullptr);
  auto* ch = dynamic_cast<pagx::TypedChannel<float>*>(FindChannel(anim, "alpha"));
  ASSERT_NE(ch, nullptr);
  ASSERT_EQ(ch->keyframes.size(), 2u);
  EXPECT_EQ(ch->keyframes.front().time, 0);
  EXPECT_FLOAT_EQ(ch->keyframes.front().value, 0.0f);
  EXPECT_EQ(ch->keyframes.back().time, 120);
  EXPECT_FLOAT_EQ(ch->keyframes.back().value, 1.0f);
  EXPECT_EQ(ch->keyframes.front().interpolation, pagx::KeyframeInterpolationType::Linear);
}

PAG_TEST(PAGXHTMLImporterTest, AnimationTranslateProducesXYChannels) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><head><style>
      @keyframes slide {
        0%   { transform: translate(0px, 0px); }
        100% { transform: translate(40px, 20px); }
      }
    </style></head>
    <body style="width:200px;height:100px">
      <div id="box" style="width:50px;height:50px;background-color:#000;
                           animation:slide 1s linear"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->animations.size(), 1u);
  auto* anim = doc->animations.front();
  EXPECT_EQ(anim->loop, pagx::LoopMode::Once);
  EXPECT_EQ(anim->duration, 60);
  auto* xCh = dynamic_cast<pagx::TypedChannel<float>*>(FindChannel(anim, "x"));
  auto* yCh = dynamic_cast<pagx::TypedChannel<float>*>(FindChannel(anim, "y"));
  ASSERT_NE(xCh, nullptr);
  ASSERT_NE(yCh, nullptr);
  ASSERT_EQ(xCh->keyframes.size(), 2u);
  ASSERT_EQ(yCh->keyframes.size(), 2u);
  EXPECT_FLOAT_EQ(xCh->keyframes.back().value, 40.0f);
  EXPECT_FLOAT_EQ(yCh->keyframes.back().value, 20.0f);
}

PAG_TEST(PAGXHTMLImporterTest, AnimationBackgroundColorProducesColorChannel) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><head><style>
      @keyframes recolor {
        0%   { background-color: #FF0000; }
        100% { background-color: #0000FF; }
      }
    </style></head>
    <body style="width:200px;height:100px">
      <div id="swatch" style="width:50px;height:50px;background-color:#FF0000;
                              animation:recolor 1s linear"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->animations.size(), 1u);
  auto* anim = doc->animations.front();
  auto* ch = dynamic_cast<pagx::TypedChannel<pagx::Color>*>(FindChannel(anim, "color"));
  ASSERT_NE(ch, nullptr);
  ASSERT_EQ(ch->keyframes.size(), 2u);
  EXPECT_TRUE(ColorNear(ch->keyframes.front().value, HexColor(0xFF0000)));
  EXPECT_TRUE(ColorNear(ch->keyframes.back().value, HexColor(0x0000FF)));
}

PAG_TEST(PAGXHTMLImporterTest, AnimationAlternateDirectionIsPingPong) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><head><style>
      @keyframes fade { from { opacity: 0; } to { opacity: 1; } }
    </style></head>
    <body style="width:100px;height:100px">
      <div id="d" style="width:10px;height:10px;background-color:#000;
                         animation:fade 1s linear infinite alternate"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->animations.size(), 1u);
  EXPECT_EQ(doc->animations.front()->loop, pagx::LoopMode::PingPong);
}

PAG_TEST(PAGXHTMLImporterTest, AnimationDelayShiftsKeyframeTimes) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><head><style>
      @keyframes fade { 0% { opacity: 0; } 100% { opacity: 1; } }
    </style></head>
    <body style="width:100px;height:100px">
      <div id="d" style="width:10px;height:10px;background-color:#000;
                         animation:fade 1s linear 0.5s"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->animations.size(), 1u);
  auto* anim = doc->animations.front();
  // delay (0.5s = 30 frames) + duration (1s = 60 frames).
  EXPECT_EQ(anim->duration, 90);
  auto* ch = dynamic_cast<pagx::TypedChannel<float>*>(FindChannel(anim, "alpha"));
  ASSERT_NE(ch, nullptr);
  ASSERT_EQ(ch->keyframes.size(), 2u);
  EXPECT_EQ(ch->keyframes.front().time, 30);
  EXPECT_EQ(ch->keyframes.back().time, 90);
}

PAG_TEST(PAGXHTMLImporterTest, AnimationCubicBezierSetsBezierInterpolation) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><head><style>
      @keyframes fade { 0% { opacity: 0; } 100% { opacity: 1; } }
    </style></head>
    <body style="width:100px;height:100px">
      <div id="d" style="width:10px;height:10px;background-color:#000;
                         animation:fade 1s cubic-bezier(0.42, 0, 0.58, 1)"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->animations.size(), 1u);
  auto* ch =
      dynamic_cast<pagx::TypedChannel<float>*>(FindChannel(doc->animations.front(), "alpha"));
  ASSERT_NE(ch, nullptr);
  ASSERT_EQ(ch->keyframes.size(), 2u);
  EXPECT_EQ(ch->keyframes.front().interpolation, pagx::KeyframeInterpolationType::Bezier);
  EXPECT_NEAR(ch->keyframes.front().bezierOut.x, 0.42f, kEps);
  EXPECT_NEAR(ch->keyframes.back().bezierIn.x, 0.58f, kEps);
}

// CSS reverses the timing-function alongside the keyframes for `direction: reverse`. With the
// keyframes already mirrored by the builder, the resolved easing must be reversed too — applying
// `B'(t) = 1 - B(1 - t)` to the cubic-bezier control points (e.g. ease-in -> ease-out) so the
// browser and the runtime sample the same shape.
PAG_TEST(PAGXHTMLImporterTest, AnimationReverseDirectionFlipsCubicBezier) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><head><style>
      @keyframes fade { 0% { opacity: 0; } 100% { opacity: 1; } }
    </style></head>
    <body style="width:100px;height:100px">
      <div id="d" style="width:10px;height:10px;background-color:#000;
                         animation:fade 1s ease-in infinite reverse"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->animations.size(), 1u);
  auto* ch =
      dynamic_cast<pagx::TypedChannel<float>*>(FindChannel(doc->animations.front(), "alpha"));
  ASSERT_NE(ch, nullptr);
  ASSERT_EQ(ch->keyframes.size(), 2u);
  // ease-in is (0.42, 0, 1, 1); reversed it must become ease-out (0, 0, 0.58, 1).
  EXPECT_EQ(ch->keyframes.front().interpolation, pagx::KeyframeInterpolationType::Bezier);
  EXPECT_NEAR(ch->keyframes.front().bezierOut.x, 0.0f, kEps);
  EXPECT_NEAR(ch->keyframes.front().bezierOut.y, 0.0f, kEps);
  EXPECT_NEAR(ch->keyframes.back().bezierIn.x, 0.58f, kEps);
  EXPECT_NEAR(ch->keyframes.back().bezierIn.y, 1.0f, kEps);
}

// CSS `steps(n, jump-end)` reversed becomes `steps(n, jump-start)`: the discontinuity moves to
// the opposite end of the segment. After ExpandSteps fans the timing function out into hold
// keyframes, jump-start places the first jump at fraction 1/n (rather than 0/n for jump-end), so
// the very first sub-keyframe carries a non-zero output value.
PAG_TEST(PAGXHTMLImporterTest, AnimationReverseDirectionFlipsStepsJump) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><head><style>
      @keyframes fade { 0% { opacity: 0; } 100% { opacity: 1; } }
    </style></head>
    <body style="width:100px;height:100px">
      <div id="d" style="width:10px;height:10px;background-color:#000;
                         animation:fade 1s steps(4, jump-end) infinite reverse"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->animations.size(), 1u);
  auto* ch =
      dynamic_cast<pagx::TypedChannel<float>*>(FindChannel(doc->animations.front(), "alpha"));
  ASSERT_NE(ch, nullptr);
  // 4 steps + the trailing endpoint = 5 hold keyframes.
  ASSERT_EQ(ch->keyframes.size(), 5u);
  // Reversed keyframes go 1 -> 0; combined with jump-start (effective after reversal), the first
  // hold lands on 1 - 1/4 = 0.75, and the last hold reaches 0.0.
  EXPECT_EQ(ch->keyframes.front().interpolation, pagx::KeyframeInterpolationType::Hold);
  EXPECT_NEAR(ch->keyframes.front().value, 0.75f, kEps);
  EXPECT_NEAR(ch->keyframes.back().value, 0.0f, kEps);
}

PAG_TEST(PAGXHTMLImporterTest, AnimationUnsupportedTransformWarnsAndDrops) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><head><style>
      @keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }
    </style></head>
    <body style="width:100px;height:100px">
      <div id="d" style="width:10px;height:10px;background-color:#000;
                         animation:spin 1s linear infinite"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("subset:animation-unsupported-property") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
  // rotate maps to no runtime channel, so no animation is emitted.
  EXPECT_TRUE(doc->animations.empty());
}

PAG_TEST(PAGXHTMLImporterTest, AnimationMatrixKeepsTranslateDropsScale) {
  // A `matrix(...)` that combines a scale with a translation (the common shape `getComputedStyle`
  // reports for GSAP-style animations) must keep the translation on x/y while dropping the scale,
  // warning that an unsupported component was lost.
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><head><style>
      @keyframes flyin {
        0%   { transform: matrix(2, 0, 0, 2, 10, 200); }
        100% { transform: matrix(1, 0, 0, 1, 30, 0); }
      }
    </style></head>
    <body style="width:200px;height:300px">
      <div id="box" style="width:50px;height:50px;background-color:#000;
                           animation:flyin 1s linear forwards"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("subset:animation-unsupported-property") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
  ASSERT_EQ(doc->animations.size(), 1u);
  auto* anim = doc->animations.front();
  auto* xCh = dynamic_cast<pagx::TypedChannel<float>*>(FindChannel(anim, "x"));
  auto* yCh = dynamic_cast<pagx::TypedChannel<float>*>(FindChannel(anim, "y"));
  ASSERT_NE(xCh, nullptr);
  ASSERT_NE(yCh, nullptr);
  // `fill-mode: forwards` avoids the trailing baseline keyframe, so x/y carry exactly the two
  // authored stops with the translation extracted from each matrix (scale dropped).
  ASSERT_EQ(xCh->keyframes.size(), 2u);
  ASSERT_EQ(yCh->keyframes.size(), 2u);
  EXPECT_FLOAT_EQ(xCh->keyframes.front().value, 10.0f);
  EXPECT_FLOAT_EQ(yCh->keyframes.front().value, 200.0f);
  EXPECT_FLOAT_EQ(xCh->keyframes.back().value, 30.0f);
  EXPECT_FLOAT_EQ(yCh->keyframes.back().value, 0.0f);
}

PAG_TEST(PAGXHTMLImporterTest, AnimationUnknownKeyframesWarns) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><body style="width:100px;height:100px">
      <div id="d" style="width:10px;height:10px;background-color:#000;
                         animation:missing 1s linear"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("subset:animation-unknown-keyframes") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
  EXPECT_TRUE(doc->animations.empty());
}

PAG_TEST(PAGXHTMLImporterTest, AnimationFiniteCountWarns) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><head><style>
      @keyframes fade { 0% { opacity: 0; } 100% { opacity: 1; } }
    </style></head>
    <body style="width:100px;height:100px">
      <div id="d" style="width:10px;height:10px;background-color:#000;
                         animation:fade 1s linear 3"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  bool warned = false;
  for (const auto& msg : doc->errors) {
    if (msg.find("subset:animation-finite-count") != std::string::npos) warned = true;
  }
  EXPECT_TRUE(warned);
  ASSERT_EQ(doc->animations.size(), 1u);
  EXPECT_EQ(doc->animations.front()->loop, pagx::LoopMode::Once);
}

// The default pipeline (autoNormalize = true) must preserve the `@keyframes` block and the
// `animation` shorthand through the subset transformer so the builder still emits an animation.
PAG_TEST(PAGXHTMLImporterTest, AnimationSurvivesDefaultNormalization) {
  auto doc = ParseFromString(R"HTML(
    <html><head><style>
      @keyframes fade { 0% { opacity: 0; } 100% { opacity: 1; } }
    </style></head>
    <body style="width:200px;height:100px">
      <div id="card" style="width:50px;height:50px;background-color:#000;
                            animation:fade 2s linear infinite"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->animations.size(), 1u);
  auto* ch =
      dynamic_cast<pagx::TypedChannel<float>*>(FindChannel(doc->animations.front(), "alpha"));
  ASSERT_NE(ch, nullptr);
  EXPECT_EQ(ch->keyframes.size(), 2u);
}

// A round-trip through the exporter must serialise the <Animations> block; importing the
// emitted XML back reproduces the same channel.
PAG_TEST(PAGXHTMLImporterTest, AnimationExportsToXML) {
  pagx::HTMLImporter::Options opts;
  opts.autoNormalize = false;
  auto doc = pagx::HTMLImporter::ParseString(R"HTML(
    <html><head><style>
      @keyframes fade { 0% { opacity: 0; } 100% { opacity: 1; } }
    </style></head>
    <body style="width:200px;height:100px">
      <div id="card" style="width:50px;height:50px;background-color:#000;
                            animation:fade 2s linear infinite"></div>
    </body></html>
  )HTML",
                                             opts);
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->animations.size(), 1u);
  auto xml = pagx::PAGXExporter::ToXML(*doc);
  ASSERT_FALSE(xml.empty());
  EXPECT_NE(xml.find("Animation"), std::string::npos);
  EXPECT_NE(xml.find("alpha"), std::string::npos);
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

}  // namespace pag
