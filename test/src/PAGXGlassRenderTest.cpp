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

#include <algorithm>
#include <cmath>
#include "base/PAGTest.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/PAGXOptimizer.h"
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

// Counts pixels differing by more than 8 levels in any channel inside the given rect.
static int CountDifferentPixelsInRect(const tgfx::Bitmap& first, const tgfx::Bitmap& second,
                                      int left, int top, int right, int bottom) {
  if (first.isEmpty() || second.isEmpty() || first.width() != second.width() ||
      first.height() != second.height()) {
    return -1;
  }
  const int width = first.width();
  const int height = first.height();
  left = std::max(left, 0);
  top = std::max(top, 0);
  right = std::min(right, width);
  bottom = std::min(bottom, height);
  tgfx::Pixmap firstPixmap(first);
  tgfx::Pixmap secondPixmap(second);
  const auto* firstPixels = static_cast<const uint8_t*>(firstPixmap.pixels());
  const auto* secondPixels = static_cast<const uint8_t*>(secondPixmap.pixels());
  const size_t rowBytes = firstPixmap.rowBytes();
  int differentPixels = 0;
  for (int y = top; y < bottom; y++) {
    for (int x = left; x < right; x++) {
      const auto offset = static_cast<size_t>(y) * rowBytes + static_cast<size_t>(x) * 4;
      for (size_t channel = 0; channel < 4; channel++) {
        if (std::abs(firstPixels[offset + channel] - secondPixels[offset + channel]) > 8) {
          differentPixels++;
          break;
        }
      }
    }
  }
  return differentPixels;
}

static int CountDifferentPixels(const tgfx::Bitmap& first, const tgfx::Bitmap& second) {
  // Guard the empty bitmap before touching width(); size mismatches are handled by InRect.
  if (first.isEmpty() || second.isEmpty()) {
    return -1;
  }
  return CountDifferentPixelsInRect(first, second, 0, 0, first.width(), first.height());
}

// The glass geometry spans 50..250 on both axes. A mask that stops short leaves one of the four
// edge bands or the interior unchanged, so every one of them must show a difference.
static void ExpectCoversGeometry(const tgfx::Bitmap& glass, const tgfx::Bitmap& control) {
  ASSERT_FALSE(glass.isEmpty());
  ASSERT_FALSE(control.isEmpty());
  const int total = CountDifferentPixels(glass, control);
  ASSERT_GE(total, 0) << "the bitmaps must be comparable";
  EXPECT_GT(total, 500) << "the glass must be active at all";

  const int interior = CountDifferentPixelsInRect(glass, control, 120, 120, 180, 180);
  ASSERT_GE(interior, 0);
  EXPECT_GT(interior, 500) << "the mask must cover the interior, not just the rim";

  const int bands[4][4] = {{50, 50, 58, 250},     // left
                           {242, 50, 250, 250},   // right
                           {50, 50, 250, 58},     // top
                           {50, 242, 250, 250}};  // bottom
  for (const auto& band : bands) {
    const int changed =
        CountDifferentPixelsInRect(glass, control, band[0], band[1], band[2], band[3]);
    ASSERT_GE(changed, 0);
    EXPECT_GT(changed, 0) << "the mask must reach every edge of the geometry";
  }

  // Nothing outside the 48..252 AA margin may change: the glass must not paint beyond its geometry.
  const int inside = CountDifferentPixelsInRect(glass, control, 48, 48, 252, 252);
  ASSERT_GE(inside, 0) << "a rect count must not be an error code";
  EXPECT_EQ(total - inside, 0);
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

  // A tolerance check would silently accept the -1 error code of the counter, so assert a real
  // count first.
  const int controlDelta = CountDifferentPixels(displayListControl, orphanControl);
  ASSERT_GE(controlDelta, 0) << "the bitmaps must be comparable";
  EXPECT_LE(controlDelta, 16);
  EXPECT_GT(CountDifferentPixels(displayListGlass, displayListControl), 500);
  EXPECT_GT(CountDifferentPixels(orphanGlass, orphanControl), 500);
  const int parityDelta = CountDifferentPixels(displayListGlass, orphanGlass);
  ASSERT_GE(parityDelta, 0) << "the bitmaps must be comparable";
  EXPECT_LE(parityDelta, 16);
}

// A red backdrop with vertical stripes: frost is a no-op on a flat backdrop, so the captured
// backdrop needs texture. The stripes cross all four edge bands and the interior, so a mask that
// stops short leaves a visibly unchanged region. They sit below the glass layer, as the backdrop.
static std::shared_ptr<pagx::PAGXDocument> MakeStripedBackdropDocument() {
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

  const float stripeCenters[] = {70.0f, 150.0f, 230.0f};
  for (const auto centerX : stripeCenters) {
    auto stripeLayer = document->makeNode<pagx::Layer>();
    auto stripeRect = document->makeNode<pagx::Rectangle>();
    stripeRect->size = {40.0f, 200.0f};
    stripeRect->position = {centerX, 150.0f};
    auto stripeFill = document->makeNode<pagx::Fill>();
    auto stripeColor = document->makeNode<pagx::SolidColor>();
    stripeColor->color = {0.0f, 0.0f, 1.0f, 1.0f};
    stripeFill->color = stripeColor;
    stripeLayer->contents = {stripeRect, stripeFill};
    document->layers.push_back(stripeLayer);
  }

  return document;
}

// Appends the tested layer last: nothing may sit above it. Its rectangle spans 50..250 on both
// axes. zeroAlphaFill gives that layer a fill that paints nothing, yet still leaves geometry.
static void AppendGlassLayer(pagx::PAGXDocument* document, bool withGlass, bool zeroAlphaFill) {
  auto glassLayer = document->makeNode<pagx::Layer>();
  auto glassRect = document->makeNode<pagx::Rectangle>();
  glassRect->size = {200.0f, 200.0f};
  glassRect->position = {150.0f, 150.0f};
  glassLayer->contents = {glassRect};
  if (zeroAlphaFill) {
    auto glassFill = document->makeNode<pagx::Fill>();
    auto glassColor = document->makeNode<pagx::SolidColor>();
    glassColor->color = {0.0f, 0.0f, 0.0f, 0.0f};
    glassFill->color = glassColor;
    glassLayer->contents.push_back(glassFill);
  }
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
}

// The glass layer carries bare geometry and no painter at all: no fill, stroke, or text paints on
// that layer, so the glass mask can only come from that bare geometry.
static std::shared_ptr<pagx::PAGXDocument> MakeUnpaintedGeometryDocument(bool withGlass) {
  auto document = MakeStripedBackdropDocument();
  AppendGlassLayer(document.get(), withGlass, false);
  return document;
}

// The glass layer carries a fill whose color is fully transparent: the paint is invisible, but the
// layer still has geometry. This is the 0% fill users hit, and it reaches the renderer through a
// different branch than bare geometry, so it needs its own fixture.
static std::shared_ptr<pagx::PAGXDocument> MakeZeroAlphaFillDocument(bool withGlass) {
  auto document = MakeStripedBackdropDocument();
  AppendGlassLayer(document.get(), withGlass, true);
  return document;
}

PAGX_TEST(PAGXGlassRenderTest, GlassStyleCoversUnpaintedGeometry) {
  // The fixture releases the device lock after SetUp, so take it for the render passes below.
  pag::DeviceLockScope deviceLock;
  ASSERT_TRUE(static_cast<bool>(deviceLock));
  auto* context = deviceLock.context();

  auto glassDocument = MakeUnpaintedGeometryDocument(true);
  auto controlDocument = MakeUnpaintedGeometryDocument(false);
  auto glass = RenderViaDisplayList(glassDocument.get(), context);
  auto control = RenderViaDisplayList(controlDocument.get(), context);

  ASSERT_FALSE(glass.isEmpty());
  ASSERT_FALSE(control.isEmpty());
  ExpectCoversGeometry(glass, control);
}

// A fill whose color is fully transparent still gives the layer geometry: the coverage must not
// depend on the paint being visible, which is the case users hit with a 0% fill.
PAGX_TEST(PAGXGlassRenderTest, GlassStyleCoversZeroAlphaFill) {
  pag::DeviceLockScope deviceLock;
  ASSERT_TRUE(static_cast<bool>(deviceLock));
  auto* context = deviceLock.context();

  auto glassDocument = MakeZeroAlphaFillDocument(true);
  auto controlDocument = MakeZeroAlphaFillDocument(false);
  auto glass = RenderViaDisplayList(glassDocument.get(), context);
  auto control = RenderViaDisplayList(controlDocument.get(), context);
  ExpectCoversGeometry(glass, control);
}

// Export -> parse -> optimize, matching the path a CLI or player takes before layout.
static std::shared_ptr<pagx::PAGXDocument> ReparseAndOptimize(pagx::PAGXDocument* document) {
  document->applyLayout();
  auto xml = pagx::PAGXExporter::ToXML(*document);
  EXPECT_FALSE(xml.empty());
  if (xml.empty()) {
    return nullptr;
  }
  auto parsed = pagx::PAGXImporter::FromXML(xml);
  if (parsed == nullptr) {
    return nullptr;
  }
  auto result = pagx::PAGXOptimizer::Optimize(parsed.get());
  EXPECT_TRUE(result.converged);
  return parsed;
}

// The optimizer may merge or drop layers, so locate the styled layer by its style, not by index.
static pagx::Layer* FindStyledLayer(pagx::PAGXDocument* document) {
  for (auto& layer : document->layers) {
    if (!layer->styles.empty()) {
      return layer;
    }
  }
  return nullptr;
}

PAGX_TEST(PAGXGlassRenderTest, GlassStyleCoversUnpaintedGeometryAfterRoundTrip) {
  pag::DeviceLockScope deviceLock;
  ASSERT_TRUE(static_cast<bool>(deviceLock));
  auto* context = deviceLock.context();

  // Guards the parse path too: neither the exporter, the parser, nor the optimizer may drop the
  // bare geometry or invent a painter for it.
  auto glassDocument = MakeUnpaintedGeometryDocument(true);
  auto controlDocument = MakeUnpaintedGeometryDocument(false);
  auto reparsedGlassDocument = ReparseAndOptimize(glassDocument.get());
  auto reparsedControlDocument = ReparseAndOptimize(controlDocument.get());
  ASSERT_NE(reparsedGlassDocument, nullptr);
  ASSERT_NE(reparsedControlDocument, nullptr);

  // The round trip must neither drop the bare geometry nor synthesize a painter for it: the glass
  // layer keeps exactly one element (the Rectangle) and its single style.
  auto styled = FindStyledLayer(reparsedGlassDocument.get());
  ASSERT_NE(styled, nullptr);
  // ASSERT before the dereference below: an empty contents list must not crash the check.
  ASSERT_EQ(styled->contents.size(), 1u);
  EXPECT_EQ(styled->styles.size(), 1u);
  // The surviving element must still be bare geometry, not a substitute node.
  ASSERT_EQ(styled->contents[0]->nodeType(), pagx::NodeType::Rectangle);

  auto glass = RenderViaDisplayList(reparsedGlassDocument.get(), context);
  auto control = RenderViaDisplayList(reparsedControlDocument.get(), context);
  ASSERT_FALSE(glass.isEmpty());
  ASSERT_FALSE(control.isEmpty());

  // Same coverage checks as the direct case, now through the parser.
  ExpectCoversGeometry(glass, control);
}

// The glass layer masks with its own rounded rectangle. The shell child carries a square of the
// same bounds and no painter, so absorbing it would append that square to the layer's contents and
// square off the mask at the four corners.
static std::shared_ptr<pagx::PAGXDocument> MakeAbsorbableChildDocument(bool withGlass) {
  auto document = MakeStripedBackdropDocument();
  auto glassLayer = document->makeNode<pagx::Layer>();
  glassLayer->x = 50.0f;
  glassLayer->y = 50.0f;
  glassLayer->width = 200.0f;
  glassLayer->height = 200.0f;
  auto glassRect = document->makeNode<pagx::Rectangle>();
  glassRect->size = {200.0f, 200.0f};
  glassRect->position = {100.0f, 100.0f};
  glassRect->roundness = 60.0f;
  glassLayer->contents = {glassRect};

  auto child = document->makeNode<pagx::Layer>();
  child->width = 200.0f;
  child->height = 200.0f;
  auto childRect = document->makeNode<pagx::Rectangle>();
  childRect->size = {200.0f, 200.0f};
  childRect->position = {100.0f, 100.0f};
  child->contents = {childRect};
  glassLayer->children.push_back(child);

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

PAGX_TEST(PAGXGlassRenderTest, GlassStyleKeepsItsMaskThroughTheOptimizer) {
  pag::DeviceLockScope deviceLock;
  ASSERT_TRUE(static_cast<bool>(deviceLock));
  auto* context = deviceLock.context();

  auto reference = MakeAbsorbableChildDocument(true);
  auto optimized = MakeAbsorbableChildDocument(true);
  auto control = MakeAbsorbableChildDocument(false);

  auto result = pagx::PAGXOptimizer::Optimize(optimized.get());
  EXPECT_TRUE(result.converged);

  auto referencePixels = RenderViaDisplayList(reference.get(), context);
  auto optimizedPixels = RenderViaDisplayList(optimized.get(), context);
  auto controlPixels = RenderViaDisplayList(control.get(), context);
  ASSERT_FALSE(referencePixels.isEmpty());
  ASSERT_FALSE(optimizedPixels.isEmpty());
  ASSERT_FALSE(controlPixels.isEmpty());

  const int active = CountDifferentPixels(referencePixels, controlPixels);
  ASSERT_GE(active, 0);
  EXPECT_GT(active, 500) << "the glass must be active before optimizing, got " << active;
  const int shifted = CountDifferentPixels(referencePixels, optimizedPixels);
  ASSERT_GE(shifted, 0);
  EXPECT_LE(shifted, 16) << "optimizing must not move the glass mask off the subtree, got "
                         << shifted;

  // The guard, not the fixture, has to keep the child out of the layer's contents: an absorbed
  // child leaves none and appends its square.
  auto styled = FindStyledLayer(optimized.get());
  ASSERT_NE(styled, nullptr);
  ASSERT_EQ(styled->children.size(), 1u);
  ASSERT_EQ(styled->contents.size(), 1u);
}

}  // namespace pag
