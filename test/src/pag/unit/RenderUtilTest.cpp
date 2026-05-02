// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 9.5 unit tests for pagx::test::RenderUtil — verify the helper itself,
// not the semantic equivalence between LayerBuilder and LayerInflater (that's
// InflaterRenderConsistencyTest / Phase 12 RenderEquivalenceTest).

#include "base/PAGTest.h"
#include "pag/support/RenderUtil.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/ImageInfo.h"
#include "tgfx/core/Pixmap.h"
#include "tgfx/core/Surface.h"
#include "tgfx/layers/Layer.h"
#include "tgfx/layers/SolidLayer.h"

namespace pag {

PAGX_TEST(RenderUtil, NullLayerReturnsNull) {
  auto surface = pagx::test::RenderLayerToSurface(context, nullptr, 32, 32);
  EXPECT_EQ(surface, nullptr);
}

PAGX_TEST(RenderUtil, ZeroDimensionReturnsNull) {
  auto layer = tgfx::SolidLayer::Make();
  layer->setWidth(32);
  layer->setHeight(32);
  layer->setColor({1.0f, 0.0f, 0.0f, 1.0f});

  EXPECT_EQ(pagx::test::RenderLayerToSurface(context, layer, 0, 32), nullptr);
  EXPECT_EQ(pagx::test::RenderLayerToSurface(context, layer, 32, 0), nullptr);
  EXPECT_EQ(pagx::test::RenderLayerToSurface(context, layer, -1, 32), nullptr);
}

PAGX_TEST(RenderUtil, SolidLayerPaintsExpectedColor) {
  constexpr int W = 16;
  constexpr int H = 16;

  auto layer = tgfx::SolidLayer::Make();
  layer->setWidth(W);
  layer->setHeight(H);
  // Pure red, fully opaque; premultiplied readback yields the same byte
  // pattern so we can memcmp against expected values below.
  layer->setColor({1.0f, 0.0f, 0.0f, 1.0f});

  auto surface = pagx::test::RenderLayerToSurface(context, layer, W, H);
  ASSERT_NE(surface, nullptr);
  EXPECT_EQ(surface->width(), W);
  EXPECT_EQ(surface->height(), H);

  tgfx::Bitmap bitmap(W, H);
  ASSERT_FALSE(bitmap.isEmpty());
  tgfx::Pixmap pixmap(bitmap);
  pixmap.clear();
  ASSERT_TRUE(surface->readPixels(pixmap.info(), pixmap.writablePixels()));

  // Use getColor() rather than raw-pointer dereference: the underlying byte
  // order depends on whether tgfx chose RGBA_8888 or BGRA_8888 (macOS
  // CVPixelBuffer paths default to BGRA), and getColor normalises that away.
  // It also returns unpremultiplied RGBA so the expected values match
  // SolidLayer::setColor verbatim.
  const auto colour = pixmap.getColor(W / 2, H / 2);
  EXPECT_NEAR(colour.red, 1.0f, 1.0f / 255.0f);
  EXPECT_NEAR(colour.green, 0.0f, 1.0f / 255.0f);
  EXPECT_NEAR(colour.blue, 0.0f, 1.0f / 255.0f);
  EXPECT_NEAR(colour.alpha, 1.0f, 1.0f / 255.0f);
}

}  // namespace pag
