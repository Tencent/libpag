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
#include "pagx/PAGXDocument.h"
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

  auto backgroundLayer = document->makeNode<pagx::Layer>();
  auto backgroundRect = document->makeNode<pagx::Rectangle>();
  backgroundRect->size = {300.0f, 300.0f};
  backgroundRect->position = {150.0f, 150.0f};
  auto backgroundFill = document->makeNode<pagx::Fill>();
  auto backgroundColor = document->makeNode<pagx::SolidColor>();
  backgroundColor->color = {1.0f, 0.0f, 0.0f, 1.0f};
  backgroundFill->color = backgroundColor;
  backgroundLayer->contents = {backgroundRect, backgroundFill};
  document->layers.push_back(backgroundLayer);

  auto detailLayer = document->makeNode<pagx::Layer>();
  auto detailRect = document->makeNode<pagx::Rectangle>();
  detailRect->size = {100.0f, 60.0f};
  detailRect->position = {130.0f, 150.0f};
  auto detailFill = document->makeNode<pagx::Fill>();
  auto detailColor = document->makeNode<pagx::SolidColor>();
  detailColor->color = {0.0f, 0.0f, 1.0f, 1.0f};
  detailFill->color = detailColor;
  detailLayer->contents = {detailRect, detailFill};
  document->layers.push_back(detailLayer);

  auto glassLayer = document->makeNode<pagx::Layer>();
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

}  // namespace pag
