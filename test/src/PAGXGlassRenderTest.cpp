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
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/GlassStyle.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Pixmap.h"
#include "tgfx/core/Surface.h"
#include "tgfx/layers/DisplayList.h"
#include "tgfx/layers/Layer.h"
#include "utils/PAGXTestUtils.h"

namespace pag {

// The seven parameters every glass fixture shares, so a change lands in one place.
static pagx::GlassStyle* MakeGlassStyle(pagx::PAGXDocument* document) {
  auto glass = document->makeNode<pagx::GlassStyle>();
  glass->refraction = 80.0f;
  glass->depth = 20.0f;
  glass->frost = 40.0f;
  glass->dispersion = 50.0f;
  glass->splay = 0.0f;
  glass->lightAngle = 45.0f;
  glass->lightIntensity = 80.0f;
  return glass;
}

// A flat 300x300 red backdrop. Glass refracts the content below it, so every fixture needs one.
static void AppendBackdropLayer(pagx::PAGXDocument* document) {
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
}

static std::shared_ptr<pagx::PAGXDocument> MakeGlassDocument(bool withGlass) {
  auto document = pagx::PAGXDocument::Make(300, 300);
  AppendBackdropLayer(document.get());

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
    glassLayer->styles.push_back(MakeGlassStyle(document.get()));
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
  AppendBackdropLayer(document.get());

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
    glassLayer->styles.push_back(MakeGlassStyle(document));
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

// A control fixture carries no style, so its tested layer is located by the 200x200 frame it is
// authored with rather than by index, which merging could shift.
static pagx::Layer* FindLayerByFrame(pagx::PAGXDocument* document, float x, float y) {
  for (auto& layer : document->layers) {
    if (std::fabs(layer->x - x) < 0.01f && std::fabs(layer->y - y) < 0.01f &&
        std::fabs(layer->width - 200.0f) < 0.01f && std::fabs(layer->height - 200.0f) < 0.01f) {
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
    glassLayer->styles.push_back(MakeGlassStyle(document.get()));
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

  // The same document without the style must still be absorbed, otherwise the assertions above
  // would pass for a fixture the rule no longer matches instead of for the guard.
  auto absorbable = MakeAbsorbableChildDocument(false);
  auto absorbableResult = pagx::PAGXOptimizer::Optimize(absorbable.get());
  EXPECT_TRUE(absorbableResult.converged);
  auto* absorbed = FindLayerByFrame(absorbable.get(), 50.0f, 50.0f);
  ASSERT_NE(absorbed, nullptr) << "the unstyled fixture's own layer must survive the optimizer";
  EXPECT_TRUE(absorbed->children.empty()) << "the child was not absorbed, so this fixture cannot "
                                             "show that the guard rejected it";
  ASSERT_EQ(absorbed->contents.size(), 2u);
  EXPECT_EQ(absorbed->contents[1]->nodeType(), pagx::NodeType::Rectangle);
}

// The layer's own geometry is a 100x100 rectangle at 50..150; its child carries a green square at
// 200..300 plus an unpainted rectangle at 200..300 x 70..130. Glass masks with the own geometry
// only, so neither child region may change; the square's 50% alpha lets a leak show through.
static std::shared_ptr<pagx::PAGXDocument> MakeOversizedChildDocument(bool withGlass) {
  auto document = MakeStripedBackdropDocument();
  auto glassLayer = document->makeNode<pagx::Layer>();
  glassLayer->x = 50.0f;
  glassLayer->y = 50.0f;
  glassLayer->width = 100.0f;
  glassLayer->height = 100.0f;
  auto glassRect = document->makeNode<pagx::Rectangle>();
  glassRect->size = {100.0f, 100.0f};
  glassRect->position = {50.0f, 50.0f};
  glassLayer->contents = {glassRect};

  auto child = document->makeNode<pagx::Layer>();
  auto paintedRect = document->makeNode<pagx::Rectangle>();
  paintedRect->size = {100.0f, 100.0f};
  paintedRect->position = {200.0f, 200.0f};
  auto paintedFill = document->makeNode<pagx::Fill>();
  auto paintedColor = document->makeNode<pagx::SolidColor>();
  // Semi-transparent so a grown mask would show through; opaque paint would hide the leak.
  paintedColor->color = {0.0f, 1.0f, 0.0f, 0.5f};
  paintedFill->color = paintedColor;
  // The bare rectangle follows the painter, so the fill only paints the geometry before it.
  // Unpainted geometry is also dropped by contour recording, so this rect covers nothing.
  auto bareRect = document->makeNode<pagx::Rectangle>();
  bareRect->size = {100.0f, 60.0f};
  bareRect->position = {200.0f, 50.0f};
  child->contents = {paintedRect, paintedFill, bareRect};
  glassLayer->children.push_back(child);

  if (withGlass) {
    glassLayer->styles.push_back(MakeGlassStyle(document.get()));
  }
  document->layers.push_back(glassLayer);
  return document;
}

PAGX_TEST(PAGXGlassRenderTest, GlassStyleDoesNotCoverChildGeometry) {
  pag::DeviceLockScope deviceLock;
  ASSERT_TRUE(static_cast<bool>(deviceLock));
  auto* context = deviceLock.context();

  auto glassDocument = MakeOversizedChildDocument(true);
  auto controlDocument = MakeOversizedChildDocument(false);
  auto glass = RenderViaDisplayList(glassDocument.get(), context);
  auto control = RenderViaDisplayList(controlDocument.get(), context);
  ASSERT_FALSE(glass.isEmpty());
  ASSERT_FALSE(control.isEmpty());

  const int interior = CountDifferentPixelsInRect(glass, control, 60, 60, 140, 140);
  ASSERT_GE(interior, 0) << "a rect count must not be an error code";
  EXPECT_GT(interior, 500) << "the glass must be active inside its own geometry, got " << interior;

  const int paintedChild = CountDifferentPixelsInRect(glass, control, 205, 205, 295, 295);
  ASSERT_GE(paintedChild, 0) << "a rect count must not be an error code";
  EXPECT_EQ(paintedChild, 0) << "the glass must not reach the painted child square, got "
                             << paintedChild;
  const int bareChild = CountDifferentPixelsInRect(glass, control, 205, 75, 295, 125);
  ASSERT_GE(bareChild, 0) << "a rect count must not be an error code";
  EXPECT_EQ(bareChild, 0) << "the glass must not reach the unpainted child rectangle, got "
                          << bareChild;

  // Containment in full: outside the own geometry plus the 2px AA margin nothing may change.
  const int total = CountDifferentPixels(glass, control);
  ASSERT_GE(total, 0) << "the bitmaps must be comparable";
  const int inside = CountDifferentPixelsInRect(glass, control, 48, 48, 152, 152);
  ASSERT_GE(inside, 0) << "a rect count must not be an error code";
  EXPECT_EQ(total - inside, 0) << "the glass must paint inside its own geometry only, got "
                               << total - inside << " stray pixels";
}

// A glass layer that owns no geometry at all: tgfx then hands the style the subtree contour instead
// of an own-geometry mask, so the child's rectangle is what the glass covers.
static std::shared_ptr<pagx::PAGXDocument> MakeChildContourDocument(bool withGlass,
                                                                    float fillAlpha) {
  auto document = MakeStripedBackdropDocument();
  auto glassLayer = document->makeNode<pagx::Layer>();
  glassLayer->x = 50.0f;
  glassLayer->y = 50.0f;
  glassLayer->width = 200.0f;
  glassLayer->height = 200.0f;

  auto child = document->makeNode<pagx::Layer>();
  auto childRect = document->makeNode<pagx::Rectangle>();
  childRect->size = {200.0f, 200.0f};
  childRect->position = {100.0f, 100.0f};
  auto childFill = document->makeNode<pagx::Fill>();
  auto childColor = document->makeNode<pagx::SolidColor>();
  childColor->color = {0.0f, 0.0f, 1.0f, fillAlpha};
  childFill->color = childColor;
  // Added with no paint and kept outside the contour at 262..298 x 70..230 (child-local 212..248 x
  // 20..180): the contour drops unpainted geometry, the subtree geometry would keep it.
  auto bareRect = document->makeNode<pagx::Rectangle>();
  bareRect->size = {36.0f, 160.0f};
  bareRect->position = {230.0f, 100.0f};
  child->contents = {childRect, childFill, bareRect};
  glassLayer->children.push_back(child);

  if (withGlass) {
    glassLayer->styles.push_back(MakeGlassStyle(document.get()));
  }
  document->layers.push_back(glassLayer);
  return document;
}

// The child's fill is fully transparent, which the contour keeps and the rendered content drops, so
// the glass paints that area and nothing hides it. An opaque child would hide this fallback, since
// GlassStyle draws below the layer content. The rectangle spans 50..250, like the other fixtures.
PAGX_TEST(PAGXGlassRenderTest, GlassStyleCoversTheSubtreeContourFallback) {
  pag::DeviceLockScope deviceLock;
  ASSERT_TRUE(static_cast<bool>(deviceLock));
  auto* context = deviceLock.context();

  auto glassDocument = MakeChildContourDocument(true, 0.0f);
  auto controlDocument = MakeChildContourDocument(false, 0.0f);
  auto glass = RenderViaDisplayList(glassDocument.get(), context);
  auto control = RenderViaDisplayList(controlDocument.get(), context);
  ASSERT_FALSE(glass.isEmpty());
  ASSERT_FALSE(control.isEmpty());

  ExpectCoversGeometry(glass, control);

  // The fallback is the subtree contour, not the subtree geometry: the child's unpainted rectangle
  // must stay uncovered, so a mask grown onto it would show here instead of passing silently.
  const int unpainted = CountDifferentPixelsInRect(glass, control, 264, 72, 296, 228);
  ASSERT_GE(unpainted, 0) << "a rect count must not be an error code";
  EXPECT_EQ(unpainted, 0) << "the glass must not cover unpainted child geometry, got " << unpainted;
}

// The glass mask replaces a gradient fill with opaque white, so a layer whose gradient ends fully
// transparent is still covered end to end. The 15% fill alpha leaves the glass visible below the
// paint; the stroke switches the mask from the analytical outline to the sampled recording.
static pagx::ColorStop* MakeGradientStop(pagx::PAGXDocument* document, float offset, float alpha) {
  auto stop = document->makeNode<pagx::ColorStop>();
  stop->offset = offset;
  // Green, because the backdrop is red: a red gradient would be indistinguishable from it.
  stop->color = {0.0f, 1.0f, 0.0f, alpha};
  return stop;
}

static std::shared_ptr<pagx::PAGXDocument> MakeGradientMaskDocument(bool withGlass,
                                                                    bool gradientFill,
                                                                    bool withStroke) {
  auto document = MakeStripedBackdropDocument();
  auto glassLayer = document->makeNode<pagx::Layer>();
  auto glassRect = document->makeNode<pagx::Rectangle>();
  glassRect->size = {200.0f, 200.0f};
  glassRect->position = {150.0f, 150.0f};
  glassLayer->contents = {glassRect};

  auto fill = document->makeNode<pagx::Fill>();
  fill->alpha = 0.15f;
  if (gradientFill) {
    // The defaults are already (0, 0) -> (1, 0) over the geometry box: left to right. The stops
    // step from opaque to fully transparent across the middle, so each half has one alpha.
    auto gradient = document->makeNode<pagx::LinearGradient>();
    gradient->colorStops = {MakeGradientStop(document.get(), 0.0f, 1.0f),
                            MakeGradientStop(document.get(), 0.45f, 1.0f),
                            MakeGradientStop(document.get(), 0.55f, 0.0f),
                            MakeGradientStop(document.get(), 1.0f, 0.0f)};
    fill->color = gradient;
  } else {
    auto color = document->makeNode<pagx::SolidColor>();
    color->color = {0.0f, 0.0f, 0.0f, 0.0f};
    fill->color = color;
  }
  glassLayer->contents.push_back(fill);

  if (withStroke) {
    auto stroke = document->makeNode<pagx::Stroke>();
    auto strokeColor = document->makeNode<pagx::SolidColor>();
    strokeColor->color = {1.0f, 1.0f, 1.0f, 1.0f};
    stroke->color = strokeColor;
    stroke->width = 4.0f;
    glassLayer->contents.push_back(stroke);
  }
  if (withGlass) {
    glassLayer->styles.push_back(MakeGlassStyle(document.get()));
  }
  document->layers.push_back(glassLayer);
  return document;
}

PAGX_TEST(PAGXGlassRenderTest, GlassStyleReplacesGradientFillWithOpaqueWhite) {
  pag::DeviceLockScope deviceLock;
  ASSERT_TRUE(static_cast<bool>(deviceLock));
  auto* context = deviceLock.context();

  for (int index = 0; index < 2; index++) {
    const bool withStroke = index == 1;
    auto glassDocument = MakeGradientMaskDocument(true, true, withStroke);
    auto controlDocument = MakeGradientMaskDocument(false, true, withStroke);
    auto transparentControl = MakeGradientMaskDocument(false, false, withStroke);
    auto glass = RenderViaDisplayList(glassDocument.get(), context);
    auto control = RenderViaDisplayList(controlDocument.get(), context);
    auto transparent = RenderViaDisplayList(transparentControl.get(), context);
    ASSERT_FALSE(glass.isEmpty());
    ASSERT_FALSE(control.isEmpty());
    ASSERT_FALSE(transparent.isEmpty());

    // The fixture only proves anything if its gradient really paints an opaque left half and no
    // right half: compare it against the same layer filled with a fully transparent color.
    const int paintedLeft = CountDifferentPixelsInRect(control, transparent, 62, 62, 138, 238);
    ASSERT_GE(paintedLeft, 0) << "a rect count must not be an error code";
    EXPECT_GT(paintedLeft, 500) << "the gradient's opaque half must be visible, got "
                                << paintedLeft;
    const int paintedRight = CountDifferentPixelsInRect(control, transparent, 162, 62, 238, 238);
    ASSERT_GE(paintedRight, 0) << "a rect count must not be an error code";
    EXPECT_EQ(paintedRight, 0) << "the gradient's transparent half must paint nothing, got "
                               << paintedRight;

    const int left = CountDifferentPixelsInRect(glass, control, 62, 62, 138, 238);
    const int right = CountDifferentPixelsInRect(glass, control, 162, 62, 238, 238);
    ASSERT_GE(left, 0) << "a rect count must not be an error code";
    ASSERT_GE(right, 0) << "a rect count must not be an error code";
    EXPECT_GT(left, 500) << "the glass must be active under the opaque end, got " << left;
    EXPECT_GT(right, 500) << "the mask must cover the transparent end too, got " << right;
    const int outside = CountDifferentPixelsInRect(glass, control, 4, 4, 40, 40);
    ASSERT_GE(outside, 0) << "a rect count must not be an error code";
    EXPECT_EQ(outside, 0) << "the glass must paint inside its geometry only, got " << outside;
  }
}

}  // namespace pag
