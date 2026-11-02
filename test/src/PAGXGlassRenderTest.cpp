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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <cmath>
#include "base/PAGTest.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/GlassStyle.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Pixmap.h"
#include "tgfx/core/Surface.h"
#include "tgfx/layers/DisplayList.h"
#include "tgfx/layers/Layer.h"
#include "utils/PAGXTestUtils.h"

namespace pag {

static std::shared_ptr<pagx::PAGXDocument> MakeGlassDocument(bool withGlass) {
  auto document = pagx::PAGXDocument::Make(300, 300);

  auto backgroundLayer = document->makeNode<pagx::Layer>("background");
  auto backgroundRect = document->makeNode<pagx::Rectangle>();
  backgroundRect->size = {300.0f, 300.0f};
  backgroundRect->position = {150.0f, 150.0f};
  auto backgroundFill = document->makeNode<pagx::Fill>();
  auto backgroundColor = document->makeNode<pagx::SolidColor>();
  backgroundColor->color = {1.0f, 0.0f, 0.0f, 1.0f};
  backgroundFill->color = backgroundColor;
  backgroundLayer->contents = {backgroundRect, backgroundFill};
  document->layers.push_back(backgroundLayer);

  auto detailLayer = document->makeNode<pagx::Layer>("detail");
  auto detailRect = document->makeNode<pagx::Rectangle>();
  detailRect->size = {100.0f, 60.0f};
  detailRect->position = {130.0f, 150.0f};
  auto detailFill = document->makeNode<pagx::Fill>();
  auto detailColor = document->makeNode<pagx::SolidColor>();
  detailColor->color = {0.0f, 0.0f, 1.0f, 1.0f};
  detailFill->color = detailColor;
  detailLayer->contents = {detailRect, detailFill};
  document->layers.push_back(detailLayer);

  auto glassLayer = document->makeNode<pagx::Layer>("glass");
  auto glassRect = document->makeNode<pagx::Rectangle>();
  glassRect->size = {200.0f, 200.0f};
  glassRect->position = {150.0f, 150.0f};
  auto glassFill = document->makeNode<pagx::Fill>();
  auto glassColor = document->makeNode<pagx::SolidColor>();
  glassColor->color = {0.9f, 0.9f, 0.9f, 0.8f};
  glassFill->color = glassColor;
  glassLayer->contents = {glassRect, glassFill};
  if (withGlass) {
    auto glass = document->makeNode<pagx::GlassStyle>();
    glass->refraction = 80.0f;
    glass->depth = 20.0f;
    glass->frost = 40.0f;
    glass->dispersion = 50.0f;
    glass->splay = 0.0f;
    glass->lightAngle = 45.0f;
    glass->lightIntensity = 80.0f;
    glassLayer->styles.push_back(glass);
  }
  document->layers.push_back(glassLayer);

  return document;
}

static tgfx::Bitmap RenderViaDisplayList(pagx::PAGXDocument* document, tgfx::Context* context) {
  document->applyLayout();
  auto layer = pagx::LayerBuilder::Build(document);
  EXPECT_NE(layer, nullptr);
  if (layer == nullptr) {
    return {};
  }
  auto surface = tgfx::Surface::Make(context, 300, 300);
  EXPECT_NE(surface, nullptr);
  if (surface == nullptr) {
    return {};
  }
  tgfx::DisplayList displayList;
  displayList.root()->addChild(layer);
  displayList.render(surface.get(), false);

  tgfx::Bitmap bitmap(surface->width(), surface->height(), false, false, nullptr);
  tgfx::Pixmap pixmap(bitmap);
  return surface->readPixels(pixmap.info(), pixmap.writablePixels()) ? bitmap : tgfx::Bitmap{};
}

static tgfx::Bitmap RenderViaOrphanDirectDraw(pagx::PAGXDocument* document,
                                              tgfx::Context* context) {
  document->applyLayout();
  auto layer = pagx::LayerBuilder::Build(document);
  EXPECT_NE(layer, nullptr);
  if (layer == nullptr) {
    return {};
  }
  auto surface =
      tgfx::Surface::Make(context, 300, 300, tgfx::ColorType::RGBA_8888, 1, 0, 0, nullptr);
  EXPECT_NE(surface, nullptr);
  if (surface == nullptr) {
    return {};
  }
  layer->draw(surface->getCanvas(), layer->alpha());

  tgfx::Bitmap bitmap(surface->width(), surface->height(), false, false, nullptr);
  tgfx::Pixmap pixmap(bitmap);
  return surface->readPixels(pixmap.info(), pixmap.writablePixels()) ? bitmap : tgfx::Bitmap{};
}

static int CountDifferentPixels(const tgfx::Bitmap& first, const tgfx::Bitmap& second) {
  if (first.isEmpty() || second.isEmpty() || first.width() != second.width() ||
      first.height() != second.height()) {
    return -1;
  }
  tgfx::Pixmap firstPixmap(first);
  tgfx::Pixmap secondPixmap(second);
  const auto* firstPixels = static_cast<const uint8_t*>(firstPixmap.pixels());
  const auto* secondPixels = static_cast<const uint8_t*>(secondPixmap.pixels());
  int differentPixels = 0;
  const size_t pixelCount = static_cast<size_t>(first.width()) * first.height();
  for (size_t pixel = 0; pixel < pixelCount; pixel++) {
    for (size_t channel = 0; channel < 4; channel++) {
      const auto offset = pixel * 4 + channel;
      if (std::abs(firstPixels[offset] - secondPixels[offset]) > 8) {
        differentPixels++;
        break;
      }
    }
  }
  return differentPixels;
}

PAGX_TEST(PAGXGlassRenderTest, GlassStyleOrphanDirectDrawMatchesDisplayList) {
  auto displayListGlassDocument = MakeGlassDocument(true);
  auto orphanGlassDocument = MakeGlassDocument(true);
  auto displayListControlDocument = MakeGlassDocument(false);
  auto orphanControlDocument = MakeGlassDocument(false);

  // The fixture no longer holds the device lock across the test body (it would deadlock code
  // that fetches the shared default device), so lock locally for the render passes below.
  pag::DeviceLockScope deviceLock;
  ASSERT_TRUE(static_cast<bool>(deviceLock));
  auto* context = deviceLock.context();

  auto displayListGlass = RenderViaDisplayList(displayListGlassDocument.get(), context);
  auto orphanGlass = RenderViaOrphanDirectDraw(orphanGlassDocument.get(), context);
  auto displayListControl = RenderViaDisplayList(displayListControlDocument.get(), context);
  auto orphanControl = RenderViaOrphanDirectDraw(orphanControlDocument.get(), context);

  ASSERT_FALSE(displayListGlass.isEmpty());
  ASSERT_FALSE(orphanGlass.isEmpty());
  ASSERT_FALSE(displayListControl.isEmpty());
  ASSERT_FALSE(orphanControl.isEmpty());

  EXPECT_LE(CountDifferentPixels(displayListControl, orphanControl), 16);
  EXPECT_GT(CountDifferentPixels(displayListGlass, displayListControl), 500);
  EXPECT_GT(CountDifferentPixels(orphanGlass, orphanControl), 500);
  EXPECT_LE(CountDifferentPixels(displayListGlass, orphanGlass), 16);
}

static std::shared_ptr<pagx::PAGXDocument> MakeBlurTargetDocument(bool withBlur) {
  auto document = pagx::PAGXDocument::Make(200, 200);
  auto makeHalf = [&document](const std::string& id, float x, const pagx::Color& color) {
    auto layer = document->makeNode<pagx::Layer>(id);
    auto rect = document->makeNode<pagx::Rectangle>();
    rect->size = {100.0f, 200.0f};
    rect->position = {x + 50.0f, 100.0f};
    auto fill = document->makeNode<pagx::Fill>();
    auto solid = document->makeNode<pagx::SolidColor>();
    solid->color = color;
    fill->color = solid;
    layer->contents = {rect, fill};
    document->layers.push_back(layer);
  };
  makeHalf("left", 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
  makeHalf("right", 100.0f, {0.0f, 0.0f, 1.0f, 1.0f});

  auto target = document->makeNode<pagx::Layer>("blurTarget");
  auto rect = document->makeNode<pagx::Rectangle>();
  rect->size = {100.0f, 100.0f};
  rect->position = {100.0f, 100.0f};
  auto fill = document->makeNode<pagx::Fill>();
  auto solid = document->makeNode<pagx::SolidColor>();
  solid->color = {1.0f, 1.0f, 1.0f, 0.2f};
  fill->color = solid;
  target->contents = {rect, fill};
  if (withBlur) {
    auto blur = document->makeNode<pagx::BackgroundBlurStyle>();
    blur->blurX = 20.0f;
    blur->blurY = 20.0f;
    target->styles.push_back(blur);
  }
  document->layers.push_back(target);
  document->targetLayerId = "blurTarget";
  document->outputBounds = pagx::Rect::MakeXYWH(0, 0, 200, 200);
  return document;
}

static tgfx::Bitmap RenderTargetView(const std::shared_ptr<pagx::PAGXDocument>& document) {
  auto scene = pagx::PAGScene::Make(document);
  EXPECT_NE(scene, nullptr);
  if (scene == nullptr) {
    return {};
  }
  auto outputBounds = scene->getOutputBounds();
  EXPECT_TRUE(outputBounds.has_value());
  if (!outputBounds.has_value()) {
    return {};
  }
  int width = static_cast<int>(ceilf(outputBounds->width));
  int height = static_cast<int>(ceilf(outputBounds->height));
  auto surface = pagx::PAGSurface::MakeOffscreen(width, height);
  EXPECT_NE(surface, nullptr);
  if (surface == nullptr) {
    return {};
  }
  EXPECT_TRUE(scene->draw(surface));
  tgfx::Bitmap bitmap(width, height, false, false);
  tgfx::Pixmap pixmap(bitmap);
  if (!surface->readPixels(pixmap.writablePixels(), pixmap.rowBytes())) {
    return {};
  }
  return bitmap;
}

PAGX_TEST(PAGXGlassRenderTest, TargetViewAppliesTargetMask) {
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

PAGX_TEST(PAGXGlassRenderTest, TargetViewSurvivesXmlReload) {
  auto document = MakeGlassDocument(true);
  document->targetLayerId = "glass";
  document->outputBounds = pagx::Rect::MakeXYWH(0, 0, 300, 300);
  auto originalView = RenderTargetView(document);
  ASSERT_FALSE(originalView.isEmpty());

  auto xml = pagx::PAGXExporter::ToXML(*document);
  auto reloaded = pagx::PAGXImporter::FromXML(xml);
  ASSERT_TRUE(reloaded != nullptr);
  EXPECT_TRUE(reloaded->errors.empty());
  EXPECT_EQ(reloaded->targetLayerId, "glass");
  ASSERT_TRUE(reloaded->outputBounds.has_value());
  auto reloadedView = RenderTargetView(reloaded);
  ASSERT_FALSE(reloadedView.isEmpty());
  EXPECT_LE(CountDifferentPixels(originalView, reloadedView), 16);
}

PAGX_TEST(PAGXGlassRenderTest, TargetViewSamplesBackdropForBackgroundBlur) {
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

PAGX_TEST(PAGXGlassRenderTest, TargetViewSamplesBackdropAndKeepsOutputBounds) {
  auto glassDocument = MakeGlassDocument(true);
  glassDocument->targetLayerId = "glass";
  glassDocument->outputBounds = pagx::Rect::MakeXYWH(0, 0, 300, 300);
  auto controlDocument = MakeGlassDocument(false);
  controlDocument->targetLayerId = "glass";
  controlDocument->outputBounds = pagx::Rect::MakeXYWH(0, 0, 300, 300);

  auto glassView = RenderTargetView(glassDocument);
  auto controlView = RenderTargetView(controlDocument);
  ASSERT_FALSE(glassView.isEmpty());
  ASSERT_FALSE(controlView.isEmpty());
  EXPECT_EQ(glassView.width(), 300);
  EXPECT_EQ(glassView.height(), 300);

  // The page background and the preceding detail layer are only sampling input: outside the
  // target's own 200x200 box the delivered pixels must stay transparent.
  auto outside = glassView.getColor(10, 10);
  EXPECT_NEAR(outside.alpha, 0.0f, 0.01f);

  // Inside the target the glass effect must actually change the backdrop, proving the backdrop
  // synthesis saw the preceding layers instead of an empty canvas.
  EXPECT_GT(CountDifferentPixels(glassView, controlView), 500);
  auto center = glassView.getColor(150, 150);
  EXPECT_GT(center.alpha, 0.95f);
  EXPECT_GT(center.blue, center.red);
}

PAGX_TEST(PAGXGlassRenderTest, TargetViewAppliesTargetAlphaOnce) {
  auto opaqueDocument = MakeGlassDocument(false);
  opaqueDocument->targetLayerId = "glass";
  opaqueDocument->outputBounds = pagx::Rect::MakeXYWH(0, 0, 300, 300);
  auto translucentDocument = MakeGlassDocument(false);
  translucentDocument->targetLayerId = "glass";
  translucentDocument->outputBounds = pagx::Rect::MakeXYWH(0, 0, 300, 300);
  auto* translucentLayer = translucentDocument->findNode<pagx::Layer>("glass");
  ASSERT_NE(translucentLayer, nullptr);
  translucentLayer->alpha = 0.5f;

  auto opaqueView = RenderTargetView(opaqueDocument);
  auto translucentView = RenderTargetView(translucentDocument);
  ASSERT_FALSE(opaqueView.isEmpty());
  ASSERT_FALSE(translucentView.isEmpty());
  EXPECT_NEAR(opaqueView.getColor(150, 150).alpha, 0.8f, 0.02f);
  EXPECT_NEAR(translucentView.getColor(150, 150).alpha, 0.4f, 0.02f);
}

PAGX_TEST(PAGXGlassRenderTest, TargetViewHiddenTargetStaysTransparent) {
  auto document = MakeGlassDocument(false);
  document->targetLayerId = "glass";
  document->outputBounds = pagx::Rect::MakeXYWH(0, 0, 300, 300);
  auto* glassLayer = document->findNode<pagx::Layer>("glass");
  ASSERT_NE(glassLayer, nullptr);
  glassLayer->visible = false;

  auto view = RenderTargetView(document);
  ASSERT_FALSE(view.isEmpty());
  for (int y = 0; y < view.height(); y += 10) {
    for (int x = 0; x < view.width(); x += 10) {
      EXPECT_NEAR(view.getColor(x, y).alpha, 0.0f, 0.01f) << x << "," << y;
    }
  }
}

}  // namespace pag
