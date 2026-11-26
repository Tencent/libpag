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
//  license is distributed on an "AS IS" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "base/PAGTest.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "tgfx/core/Bitmap.h"
#include "utils/PAGXTestUtils.h"
#include "utils/TestUtils.h"

namespace pag {

// A 300x300 document whose only layer is the target: a centered 200x200 fill with alpha 0.8.
// XML, alpha and visibility cases use it so they exercise the output view instead of Glass.
static std::shared_ptr<pagx::PAGXDocument> MakeTargetDocument() {
  auto document = pagx::PAGXDocument::Make(300, 300);
  auto* target = document->makeNode<pagx::Layer>("target");
  auto* rect = document->makeNode<pagx::Rectangle>();
  rect->position = {150.0f, 150.0f};
  rect->size = {200.0f, 200.0f};
  auto* fill = document->makeNode<pagx::Fill>();
  auto* color = document->makeNode<pagx::SolidColor>();
  color->color = {0.9f, 0.9f, 0.9f, 0.8f};
  fill->color = color;
  target->contents = {rect, fill};
  document->layers.push_back(target);
  document->targetLayerId = "target";
  document->outputBounds = pagx::Rect::MakeXYWH(0, 0, 300, 300);
  return document;
}

static void AppendHalf(pagx::PAGXDocument* document, const std::string& id, float x,
                       const pagx::Color& color) {
  auto* layer = document->makeNode<pagx::Layer>(id);
  auto* rect = document->makeNode<pagx::Rectangle>();
  rect->size = {100.0f, 200.0f};
  rect->position = {x + 50.0f, 100.0f};
  auto* fill = document->makeNode<pagx::Fill>();
  auto* solid = document->makeNode<pagx::SolidColor>();
  solid->color = color;
  fill->color = solid;
  layer->contents = {rect, fill};
  document->layers.push_back(layer);
}

// A red/blue backdrop with a translucent target on top. The background blur samples those halves,
// so the blurred edge mixes both colors while the control keeps a sharp boundary.
static std::shared_ptr<pagx::PAGXDocument> MakeBlurTargetDocument(bool withBlur) {
  auto document = pagx::PAGXDocument::Make(200, 200);
  AppendHalf(document.get(), "left", 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
  AppendHalf(document.get(), "right", 100.0f, {0.0f, 0.0f, 1.0f, 1.0f});

  auto* target = document->makeNode<pagx::Layer>("blurTarget");
  auto* rect = document->makeNode<pagx::Rectangle>();
  rect->size = {100.0f, 100.0f};
  rect->position = {100.0f, 100.0f};
  auto* fill = document->makeNode<pagx::Fill>();
  auto* solid = document->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 1.0f, 1.0f, 0.2f};
  fill->color = solid;
  target->contents = {rect, fill};
  if (withBlur) {
    auto* blur = document->makeNode<pagx::BackgroundBlurStyle>();
    blur->blurX = 20.0f;
    blur->blurY = 20.0f;
    target->styles.push_back(blur);
  }
  document->layers.push_back(target);
  document->targetLayerId = "blurTarget";
  document->outputBounds = pagx::Rect::MakeXYWH(0, 0, 200, 200);
  return document;
}

PAGX_TEST(PAGXOutputViewTest, TargetViewAppliesTargetMask) {
  auto document = pagx::PAGXDocument::Make(200, 200);
  auto* maskLayer = document->makeNode<pagx::Layer>("mask");
  auto* maskRect = document->makeNode<pagx::Rectangle>();
  maskRect->position = {100.0f, 100.0f};
  maskRect->size = {100.0f, 100.0f};
  auto* maskFill = document->makeNode<pagx::Fill>();
  auto* maskColor = document->makeNode<pagx::SolidColor>();
  maskColor->color = {1.0f, 1.0f, 1.0f, 1.0f};
  maskFill->color = maskColor;
  maskLayer->contents = {maskRect, maskFill};
  maskLayer->visible = false;
  document->layers.push_back(maskLayer);

  auto* target = document->makeNode<pagx::Layer>("target");
  target->mask = maskLayer;
  target->maskType = pagx::MaskType::Alpha;
  auto* targetRect = document->makeNode<pagx::Rectangle>();
  targetRect->position = {100.0f, 100.0f};
  targetRect->size = {200.0f, 200.0f};
  auto* targetFill = document->makeNode<pagx::Fill>();
  auto* targetColor = document->makeNode<pagx::SolidColor>();
  targetColor->color = {1.0f, 0.0f, 0.0f, 1.0f};
  targetFill->color = targetColor;
  target->contents = {targetRect, targetFill};
  document->layers.push_back(target);
  document->targetLayerId = "target";
  document->outputBounds = pagx::Rect::MakeXYWH(0, 0, 200, 200);

  auto view = RenderTargetView(document);
  ASSERT_FALSE(view.isEmpty());
  EXPECT_GT(view.getColor(100, 100).red, 0.9f);
  EXPECT_GT(view.getColor(100, 100).alpha, 0.99f);
  EXPECT_NEAR(view.getColor(10, 10).alpha, 0.0f, 0.01f);
}

PAGX_TEST(PAGXOutputViewTest, TargetViewClipsToOutputBounds) {
  auto document = pagx::PAGXDocument::Make(200, 200);
  auto* target = document->makeNode<pagx::Layer>("target");
  auto* rect = document->makeNode<pagx::Rectangle>();
  rect->size = {200.0f, 200.0f};
  auto* fill = document->makeNode<pagx::Fill>();
  auto* color = document->makeNode<pagx::SolidColor>();
  color->color = {1.0f, 0.0f, 0.0f, 1.0f};
  fill->color = color;
  target->contents = {rect, fill};
  document->layers.push_back(target);
  document->targetLayerId = "target";
  document->outputBounds = pagx::Rect::MakeXYWH(0, 0, 100, 80);

  auto scene = pagx::PAGScene::Make(document);
  ASSERT_NE(scene, nullptr);
  // The surface covers the whole canvas instead of the output window, so anything the target
  // draws outside outputBounds would land in the extra area without a clip.
  auto surface = pagx::PAGSurface::MakeOffscreen(200, 200);
  ASSERT_NE(surface, nullptr);
  ASSERT_TRUE(scene->draw(surface));
  auto bitmap = MakeSnapshot(surface);
  ASSERT_FALSE(bitmap.isEmpty());

  auto inside = bitmap.getColor(50, 40);
  EXPECT_GT(inside.red, 0.9f);
  EXPECT_NEAR(inside.alpha, 1.0f, 0.01f);
  EXPECT_NEAR(bitmap.getColor(150, 40).alpha, 0.0f, 0.01f);
  EXPECT_NEAR(bitmap.getColor(50, 120).alpha, 0.0f, 0.01f);
}

PAGX_TEST(PAGXOutputViewTest, TargetViewSurvivesXmlReload) {
  auto document = MakeTargetDocument();
  auto originalView = RenderTargetView(document);
  ASSERT_FALSE(originalView.isEmpty());

  auto xml = pagx::PAGXExporter::ToXML(*document);
  auto reloaded = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(reloaded != nullptr);
  EXPECT_TRUE(reloaded->errors.empty());
  EXPECT_EQ(reloaded->targetLayerId, "target");
  ASSERT_TRUE(reloaded->outputBounds.has_value());
  auto reloadedView = RenderTargetView(reloaded);
  ASSERT_FALSE(reloadedView.isEmpty());
  const int delta = CountDifferentPixels(originalView, reloadedView);
  ASSERT_GE(delta, 0) << "the bitmaps must be comparable";
  EXPECT_LE(delta, 16);
}

PAGX_TEST(PAGXOutputViewTest, TargetViewSamplesBackdropForBackgroundBlur) {
  auto blurView = RenderTargetView(MakeBlurTargetDocument(true));
  auto controlView = RenderTargetView(MakeBlurTargetDocument(false));
  ASSERT_FALSE(blurView.isEmpty());
  ASSERT_FALSE(controlView.isEmpty());

  EXPECT_NEAR(blurView.getColor(10, 10).alpha, 0.0f, 0.01f);
  EXPECT_GT(CountDifferentPixels(blurView, controlView), 200);
  // Near the red/blue boundary the blur mixes both halves; the control keeps a sharp edge.
  auto blurredEdge = blurView.getColor(100, 100);
  EXPECT_GT(blurredEdge.red, 0.1f);
  EXPECT_GT(blurredEdge.blue, 0.1f);
}

PAGX_TEST(PAGXOutputViewTest, TargetViewAppliesTargetAlphaOnce) {
  auto opaqueDocument = MakeTargetDocument();
  auto translucentDocument = MakeTargetDocument();
  auto* translucentLayer = translucentDocument->findNode<pagx::Layer>("target");
  ASSERT_NE(translucentLayer, nullptr);
  translucentLayer->alpha = 0.5f;

  auto opaqueView = RenderTargetView(opaqueDocument);
  auto translucentView = RenderTargetView(translucentDocument);
  ASSERT_FALSE(opaqueView.isEmpty());
  ASSERT_FALSE(translucentView.isEmpty());
  EXPECT_NEAR(opaqueView.getColor(150, 150).alpha, 0.8f, 0.02f);
  EXPECT_NEAR(translucentView.getColor(150, 150).alpha, 0.4f, 0.02f);
}

PAGX_TEST(PAGXOutputViewTest, TargetViewHiddenTargetStaysTransparent) {
  auto document = MakeTargetDocument();
  auto* target = document->findNode<pagx::Layer>("target");
  ASSERT_NE(target, nullptr);
  target->visible = false;

  auto view = RenderTargetView(document);
  ASSERT_FALSE(view.isEmpty());
  for (int y = 0; y < view.height(); y += 10) {
    for (int x = 0; x < view.width(); x += 10) {
      EXPECT_NEAR(view.getColor(x, y).alpha, 0.0f, 0.01f) << x << "," << y;
    }
  }
}

}  // namespace pag
