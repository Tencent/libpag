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
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include "pagx/PAGAnimation.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/Layer.h"
#include "pagx/utils/LayerTransformUtils.h"
#include "base/PAGTest.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Pixmap.h"

namespace pag {
namespace {

constexpr int kDocSize = 200;

std::shared_ptr<pagx::PAGXDocument> ParseDoc(const std::string& xml) {
  return pagx::PAGXImporter::FromXML(xml);
}

// Renders one frame of the document through the runtime path (PAGScene + PAGSurface), matching how
// the CLI and the player draw, and returns the RGBA pixels.
std::vector<uint8_t> RenderFrame(const std::shared_ptr<pagx::PAGXDocument>& doc,
                                 int64_t timeUs = 0) {
  std::vector<uint8_t> pixels = {};
  if (doc == nullptr) {
    return pixels;
  }
  auto scene = pagx::PAGScene::Make(doc);
  if (scene == nullptr) {
    return pixels;
  }
  if (timeUs != 0) {
    for (const auto& id : scene->getAnimationIds()) {
      auto animation = scene->getAnimation(id);
      if (animation != nullptr) {
        animation->setCurrentTime(timeUs);
        animation->apply(1.0f);
      }
    }
  }
  scene->advanceAndApply(timeUs);
  auto surface = pagx::PAGSurface::MakeOffscreen(kDocSize, kDocSize);
  if (surface == nullptr || !scene->draw(surface)) {
    return {};
  }
  tgfx::Bitmap bitmap(kDocSize, kDocSize, false, false);
  if (bitmap.isEmpty()) {
    return {};
  }
  tgfx::Pixmap pixmap(bitmap);
  if (!surface->readPixels(pixmap.writablePixels(), pixmap.rowBytes())) {
    return {};
  }
  const auto* src = reinterpret_cast<const uint8_t*>(pixmap.writablePixels());
  pixels.assign(src, src + static_cast<size_t>(pixmap.rowBytes()) * kDocSize);
  return pixels;
}

size_t CountDifferentPixels(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
  if (a.size() != b.size() || a.empty()) {
    return a.size() + 1;
  }
  size_t different = 0;
  for (size_t i = 0; i + 3 < a.size(); i += 4) {
    if (std::memcmp(&a[i], &b[i], 4) != 0) {
      ++different;
    }
  }
  return different;
}

std::string MakeRectDoc(const std::string& layerAttributes) {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
         "<pagx width=\"200\" height=\"200\">\n"
         "  <Layer>\n"
         "    <Layer " +
         layerAttributes +
         ">\n"
         "      <Rectangle position=\"100,50\" size=\"60,20\"/>\n"
         "      <Fill color=\"#FF0000\"/>\n"
         "    </Layer>\n"
         "  </Layer>\n"
         "</pagx>\n";
}

std::string MakeAnimatedRotateDoc(const std::string& layerAttributes,
                                 const std::string& firstKey, const std::string& lastKey) {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
         "<pagx width=\"200\" height=\"200\">\n"
         "  <Layer>\n"
         "    <Layer " +
         layerAttributes +
         ">\n"
         "      <Rectangle position=\"100,50\" size=\"60,20\"/>\n"
         "      <Fill color=\"#FF0000\"/>\n"
         "    </Layer>\n"
         "  </Layer>\n"
         "  <Animations>\n"
         "    <Animation id=\"rotate\" duration=\"60\">\n"
         "      <Object target=\"rotated\">\n"
         "        <Channel name=\"matrix\" type=\"matrix\">\n"
         "          <Key time=\"0\" value=\"" +
         firstKey +
         "\"/>\n"
         "          <Key time=\"60\" value=\"" +
         lastKey +
         "\"/>\n"
         "        </Channel>\n"
         "      </Object>\n"
         "    </Animation>\n"
         "  </Animations>\n"
         "</pagx>\n";
}

}  // namespace

/**
 * Test case: anchor="x,y" keeps the pivot inside the layer plane.
 */
PAGX_TEST(PAGXAnchorTest, ParseTwoValueAnchor) {
  auto doc = ParseDoc(MakeRectDoc("id=\"layer\" anchor=\"10,20\""));
  ASSERT_TRUE(doc != nullptr);
  auto* layer = static_cast<pagx::Layer*>(doc->findNode("layer"));
  ASSERT_TRUE(layer != nullptr);
  EXPECT_FLOAT_EQ(layer->anchor.x, 10.0f);
  EXPECT_FLOAT_EQ(layer->anchor.y, 20.0f);
  EXPECT_FLOAT_EQ(layer->anchorZ, 0.0f);
}

/**
 * Test case: anchor="x,y,z" carries the 3D pivot depth in the same attribute.
 */
PAGX_TEST(PAGXAnchorTest, ParseThreeValueAnchor) {
  auto doc = ParseDoc(MakeRectDoc("id=\"layer\" anchor=\"10,20,30\""));
  ASSERT_TRUE(doc != nullptr);
  auto* layer = static_cast<pagx::Layer*>(doc->findNode("layer"));
  ASSERT_TRUE(layer != nullptr);
  EXPECT_FLOAT_EQ(layer->anchor.x, 10.0f);
  EXPECT_FLOAT_EQ(layer->anchor.y, 20.0f);
  EXPECT_FLOAT_EQ(layer->anchorZ, 30.0f);
}

/**
 * Test case: only two or three values are valid; other counts are rejected and leave the default.
 */
PAGX_TEST(PAGXAnchorTest, RejectInvalidAnchorValue) {
  auto doc = ParseDoc(MakeRectDoc("id=\"layer\" anchor=\"10\""));
  ASSERT_TRUE(doc != nullptr);
  auto* layer = static_cast<pagx::Layer*>(doc->findNode("layer"));
  ASSERT_TRUE(layer != nullptr);
  EXPECT_FLOAT_EQ(layer->anchor.x, 0.0f);
  EXPECT_FLOAT_EQ(layer->anchor.y, 0.0f);

  auto doc2 = ParseDoc(MakeRectDoc("id=\"layer\" anchor=\"1,2,3,4\""));
  ASSERT_TRUE(doc2 != nullptr);
  auto* layer2 = static_cast<pagx::Layer*>(doc2->findNode("layer"));
  ASSERT_TRUE(layer2 != nullptr);
  EXPECT_FLOAT_EQ(layer2->anchor.x, 0.0f);
  EXPECT_FLOAT_EQ(layer2->anchor.y, 0.0f);
}

/**
 * Test case: the exporter omits a zero anchor, writes two values without depth and three with it.
 */
PAGX_TEST(PAGXAnchorTest, ExportAnchorRoundTrip) {
  auto plain = ParseDoc(MakeRectDoc("id=\"layer\""));
  ASSERT_TRUE(plain != nullptr);
  EXPECT_EQ(pagx::PAGXExporter::ToXML(*plain).find("anchor=\""), std::string::npos);

  auto flat = ParseDoc(MakeRectDoc("id=\"layer\" anchor=\"10,20\""));
  ASSERT_TRUE(flat != nullptr);
  const auto flatXml = pagx::PAGXExporter::ToXML(*flat);
  EXPECT_NE(flatXml.find("anchor=\"10,20\""), std::string::npos);

  auto deep = ParseDoc(MakeRectDoc("id=\"layer\" anchor=\"10,20,30\""));
  ASSERT_TRUE(deep != nullptr);
  const auto deepXml = pagx::PAGXExporter::ToXML(*deep);
  EXPECT_NE(deepXml.find("anchor=\"10,20,30\""), std::string::npos);
}

/**
 * Test case: a 90-degree rotation around (100,50) turns into a pure linear matrix once the pivot is
 * carried by anchor, and the composed local transform still lands on the same translation.
 */
PAGX_TEST(PAGXAnchorTest, LocalMatrixAppliesAnchor) {
  auto doc = ParseDoc(MakeRectDoc("id=\"layer\" anchor=\"100,50\" matrix=\"0,1,-1,0,0,0\""));
  ASSERT_TRUE(doc != nullptr);
  auto* layer = static_cast<pagx::Layer*>(doc->findNode("layer"));
  ASSERT_TRUE(layer != nullptr);
  const auto matrix = pagx::BuildLayerLocalMatrix3D(layer);
  // t' = t + anchor - L * anchor, with L = rotate(90): (100,50) -> (-50,100).
  EXPECT_NEAR(matrix.getRowColumn(0, 3), 150.0f, 1e-4f);
  EXPECT_NEAR(matrix.getRowColumn(1, 3), -50.0f, 1e-4f);

  auto baked = ParseDoc(MakeRectDoc("id=\"layer\" matrix=\"0,1,-1,0,150,-50\""));
  ASSERT_TRUE(baked != nullptr);
  auto* bakedLayer = static_cast<pagx::Layer*>(baked->findNode("layer"));
  ASSERT_TRUE(bakedLayer != nullptr);
  const auto bakedMatrix = pagx::BuildLayerLocalMatrix3D(bakedLayer);
  EXPECT_NEAR(bakedMatrix.getRowColumn(0, 3), matrix.getRowColumn(0, 3), 1e-4f);
  EXPECT_NEAR(bakedMatrix.getRowColumn(1, 3), matrix.getRowColumn(1, 3), 1e-4f);
}

/**
 * Test case: rendering an anchored 2D rotation matches the same rotation with the pivot baked into
 * the matrix translation.
 */
PAGX_TEST(PAGXAnchorTest, RenderAnchorMatchesBaked2D) {
  auto anchored = ParseDoc(MakeRectDoc("id=\"layer\" anchor=\"100,50\" matrix=\"0,1,-1,0,0,0\""));
  auto baked = ParseDoc(MakeRectDoc("id=\"layer\" matrix=\"0,1,-1,0,150,-50\""));
  ASSERT_TRUE(anchored != nullptr);
  ASSERT_TRUE(baked != nullptr);
  const auto anchoredPixels = RenderFrame(anchored);
  const auto bakedPixels = RenderFrame(baked);
  ASSERT_FALSE(anchoredPixels.empty());
  EXPECT_EQ(CountDifferentPixels(anchoredPixels, bakedPixels), 0u);
}

/**
 * Test case: a 2D transform ignores the third anchor value, so anchorZ does not change the frame.
 */
PAGX_TEST(PAGXAnchorTest, RenderIgnoresAnchorZFor2D) {
  auto flat = ParseDoc(MakeRectDoc("id=\"layer\" anchor=\"100,50\" matrix=\"0,1,-1,0,0,0\""));
  auto deep = ParseDoc(MakeRectDoc("id=\"layer\" anchor=\"100,50,40\" matrix=\"0,1,-1,0,0,0\""));
  ASSERT_TRUE(flat != nullptr);
  ASSERT_TRUE(deep != nullptr);
  const auto flatPixels = RenderFrame(flat);
  const auto deepPixels = RenderFrame(deep);
  ASSERT_FALSE(flatPixels.empty());
  EXPECT_EQ(CountDifferentPixels(flatPixels, deepPixels), 0u);
}

/**
 * Test case: a 3D rotation around a pivot outside the layer plane matches the baked matrix, which
 * is what Layer.anchorZ is for.
 */
PAGX_TEST(PAGXAnchorTest, RenderAnchorZMatchesBaked3D) {
  const std::string rotation = "1,0,0,0,0,0.5,0.8660254,0,0,-0.8660254,0.5,0,0,0,0,1";
  auto anchored = ParseDoc(MakeRectDoc("id=\"layer\" anchor=\"100,50,20\" matrix3D=\"" + rotation +
                                       "\""));
  auto baked = ParseDoc(MakeRectDoc("id=\"layer\" matrix3D=\"1,0,0,0,0,0.5,0.8660254,0,0,"
                                    "-0.8660254,0.5,0,0,42.32051,-33.30127,1\""));
  ASSERT_TRUE(anchored != nullptr);
  ASSERT_TRUE(baked != nullptr);
  const auto anchoredPixels = RenderFrame(anchored);
  const auto bakedPixels = RenderFrame(baked);
  ASSERT_FALSE(anchoredPixels.empty());
  EXPECT_EQ(CountDifferentPixels(anchoredPixels, bakedPixels), 0u);
}

/**
 * Test case: an animated matrix channel is evaluated around the anchor, so a keyframe animation
 * written with a zero translation matches one that bakes the pivot into every keyframe.
 */
PAGX_TEST(PAGXAnchorTest, RenderAnimatedAnchorMatchesBaked) {
  auto anchored = ParseDoc(MakeAnimatedRotateDoc("id=\"rotated\" anchor=\"100,50\"",
                                                 "1,0,0,1,0,0", "0,1,-1,0,0,0"));
  auto baked = ParseDoc(MakeAnimatedRotateDoc("id=\"rotated\"", "1,0,0,1,0,0",
                                              "0,1,-1,0,150,-50"));
  ASSERT_TRUE(anchored != nullptr);
  ASSERT_TRUE(baked != nullptr);
  const auto anchoredPixels = RenderFrame(anchored, 500'000);
  const auto bakedPixels = RenderFrame(baked, 500'000);
  ASSERT_FALSE(anchoredPixels.empty());
  EXPECT_EQ(CountDifferentPixels(anchoredPixels, bakedPixels), 0u);
}

/**
 * Test case: the anchor is applied inside the local transform, so an authored layout position still
 * composes outside it (T(layout) * T(anchor) * L * T(-anchor)).
 */
PAGX_TEST(PAGXAnchorTest, LayoutPositionStaysOutsideAnchor) {
  // Same rotation as RenderAnchorMatchesBaked2D, shifted right by 40px through the layout x.
  auto anchored = ParseDoc(MakeRectDoc("id=\"layer\" x=\"40\" anchor=\"100,50\" "
                                       "matrix=\"0,1,-1,0,0,0\""));
  ASSERT_TRUE(anchored != nullptr);
  const auto movedPixels = RenderFrame(anchored);
  ASSERT_FALSE(movedPixels.empty());

  auto baked = ParseDoc(MakeRectDoc("id=\"layer\" x=\"40\" matrix=\"0,1,-1,0,150,-50\""));
  ASSERT_TRUE(baked != nullptr);
  const auto bakedPixels = RenderFrame(baked);
  EXPECT_EQ(CountDifferentPixels(movedPixels, bakedPixels), 0u);
}

}  // namespace pag
