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

}  // namespace

//==================================================================================================
// Unit tests: CSS property mapping
//==================================================================================================

PAG_TEST(PAGXHTMLTest, ParsesCanvasSizeFromBody) {
  auto doc = ParseFromString(R"HTML(<html><body style="width:200px;height:100px"></body></html>)HTML");
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

//==================================================================================================
// End-to-end fixture tests
//==================================================================================================

static void RunFixture(const std::string& name) {
  auto fixturePath = ProjectPath::Absolute("resources/html/" + name + ".html");
  ASSERT_TRUE(std::filesystem::exists(fixturePath))
      << "fixture missing: " << fixturePath;
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
