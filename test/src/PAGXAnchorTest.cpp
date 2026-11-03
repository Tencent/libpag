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
#include "base/PAGTest.h"
#include "pagx/PAGAnimation.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/html/HTMLWriter.h"
#include "pagx/nodes/Layer.h"
#include "pagx/utils/LayerTransformUtils.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Pixmap.h"

namespace pag {
namespace {

constexpr int DocSize = 200;

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
  auto surface = pagx::PAGSurface::MakeOffscreen(DocSize, DocSize);
  if (surface == nullptr || !scene->draw(surface)) {
    return {};
  }
  tgfx::Bitmap bitmap(DocSize, DocSize, false, false);
  if (bitmap.isEmpty()) {
    return {};
  }
  tgfx::Pixmap pixmap(bitmap);
  if (!surface->readPixels(pixmap.writablePixels(), pixmap.rowBytes())) {
    return {};
  }
  const auto* src = reinterpret_cast<const uint8_t*>(pixmap.writablePixels());
  pixels.assign(src, src + static_cast<size_t>(pixmap.rowBytes()) * DocSize);
  return pixels;
}

size_t CountOpaquePixels(const std::vector<uint8_t>& pixels) {
  size_t opaque = 0;
  for (size_t i = 3; i < pixels.size(); i += 4) {
    if (pixels[i] != 0) {
      ++opaque;
    }
  }
  return opaque;
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

std::string MakeDoc(const std::string& layerAttributes, const std::string& animations) {
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
         "<pagx width=\"200\" height=\"200\">\n"
         "  <Layer>\n"
         "    <Layer " +
         layerAttributes +
         ">\n"
         "      <Rectangle position=\"100,50\" size=\"60,20\"/>\n"
         "      <Fill color=\"#FF0000\"/>\n"
         "    </Layer>\n"
         "  </Layer>\n" +
         animations + "</pagx>\n";
}

std::string MakeRectDoc(const std::string& layerAttributes) {
  return MakeDoc(layerAttributes, "");
}

std::string MakeAnimatedRotateDoc(const std::string& layerAttributes, const std::string& keys) {
  return MakeDoc(layerAttributes,
                 "  <Animations>\n"
                 "    <Animation id=\"rotate\" duration=\"60\">\n"
                 "      <Object target=\"rotated\">\n"
                 "        <Channel name=\"matrix\" type=\"matrix\">\n" +
                     keys +
                     "        </Channel>\n"
                     "      </Object>\n"
                     "    </Animation>\n"
                     "  </Animations>\n");
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
  const auto matrix = pagx::BuildLayerTransform3DWithAnchor(layer);
  // t' = t + anchor - L * anchor, with L = rotate(90): (100,50) -> (-50,100).
  EXPECT_NEAR(matrix.getRowColumn(0, 3), 150.0f, 1e-4f);
  EXPECT_NEAR(matrix.getRowColumn(1, 3), -50.0f, 1e-4f);

  auto baked = ParseDoc(MakeRectDoc("id=\"layer\" matrix=\"0,1,-1,0,150,-50\""));
  ASSERT_TRUE(baked != nullptr);
  auto* bakedLayer = static_cast<pagx::Layer*>(baked->findNode("layer"));
  ASSERT_TRUE(bakedLayer != nullptr);
  const auto bakedMatrix = pagx::BuildLayerTransform3DWithAnchor(bakedLayer);
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
  EXPECT_GT(CountOpaquePixels(anchoredPixels), 100u) << "the frame must actually draw something";
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
  auto anchored =
      ParseDoc(MakeRectDoc("id=\"layer\" anchor=\"100,50,20\" matrix3D=\"" + rotation + "\""));
  auto baked =
      ParseDoc(MakeRectDoc("id=\"layer\" matrix3D=\"1,0,0,0,0,0.5,0.8660254,0,0,"
                           "-0.8660254,0.5,0,0,42.32051,-33.30127,1\""));
  ASSERT_TRUE(anchored != nullptr);
  ASSERT_TRUE(baked != nullptr);
  const auto anchoredPixels = RenderFrame(anchored);
  const auto bakedPixels = RenderFrame(baked);
  ASSERT_FALSE(anchoredPixels.empty());
  EXPECT_GT(CountOpaquePixels(anchoredPixels), 100u) << "the frame must actually draw something";
  EXPECT_EQ(CountDifferentPixels(anchoredPixels, bakedPixels), 0u);
}

/**
 * Test case: an animated matrix channel is evaluated around the anchor, so a keyframe animation
 * written with a zero translation matches one that bakes the pivot into every keyframe.
 */
PAGX_TEST(PAGXAnchorTest, RenderAnimatedAnchorMatchesBaked) {
  // Both documents sample the same poses: the anchored one carries the pivot, the baked one folds
  // it into the keyframe translation (a - L * a). Comparing at a keyframe time keeps the
  // element-wise matrix interpolation out of the comparison.
  const std::string identity = "1,0,0,1,0,0";
  const std::string rotate45 = "0.70710678,0.70710678,-0.70710678,0.70710678";
  const std::string rotate90 = "0,1,-1,0";
  const std::string anchored_keys = "          <Key time=\"0\" value=\"" + identity +
                                    "\"/>\n          <Key time=\"30\" value=\"" + rotate45 +
                                    ",0,0\"/>\n          <Key time=\"60\" value=\"" + rotate90 +
                                    ",0,0\"/>\n";
  const std::string baked_keys = "          <Key time=\"0\" value=\"" + identity +
                                 "\"/>\n          <Key time=\"30\" value=\"" + rotate45 +
                                 ",64.6446,-56.066\"/>\n          <Key time=\"60\" value=\"" +
                                 rotate90 + ",150,-50\"/>\n";
  auto anchored =
      ParseDoc(MakeAnimatedRotateDoc("id=\"rotated\" anchor=\"100,50\"", anchored_keys));
  auto baked = ParseDoc(MakeAnimatedRotateDoc("id=\"rotated\"", baked_keys));
  ASSERT_TRUE(anchored != nullptr);
  ASSERT_TRUE(baked != nullptr);
  const auto anchoredPixels = RenderFrame(anchored, 500'000);
  const auto bakedPixels = RenderFrame(baked, 500'000);
  ASSERT_FALSE(anchoredPixels.empty());
  EXPECT_GT(CountOpaquePixels(anchoredPixels), 100u) << "the frame must actually draw something";
  EXPECT_EQ(CountDifferentPixels(anchoredPixels, bakedPixels), 0u);
}

/**
 * Test case: the anchor is applied inside the local transform, so an authored layout position still
 * composes outside it (T(layout) * T(anchor) * L * T(-anchor)).
 */
PAGX_TEST(PAGXAnchorTest, LayoutPositionStaysOutsideAnchor) {
  // Same rotation as RenderAnchorMatchesBaked2D, shifted right by 40px through the layout x.
  auto anchored =
      ParseDoc(MakeRectDoc("id=\"layer\" left=\"40\" anchor=\"100,50\" "
                           "matrix=\"0,1,-1,0,0,0\""));
  ASSERT_TRUE(anchored != nullptr);
  const auto movedPixels = RenderFrame(anchored);
  ASSERT_FALSE(movedPixels.empty());

  auto baked = ParseDoc(MakeRectDoc("id=\"layer\" left=\"40\" matrix=\"0,1,-1,0,150,-50\""));
  ASSERT_TRUE(baked != nullptr);
  const auto bakedPixels = RenderFrame(baked);
  EXPECT_GT(CountOpaquePixels(movedPixels), 100u);
  EXPECT_EQ(CountDifferentPixels(movedPixels, bakedPixels), 0u);
  // The layout position must actually move the frame, otherwise the case compares two identical
  // documents and proves nothing.
  auto staticDoc = ParseDoc(MakeRectDoc("id=\"layer\" anchor=\"100,50\" matrix=\"0,1,-1,0,0,0\""));
  ASSERT_TRUE(staticDoc != nullptr);
  EXPECT_GT(CountDifferentPixels(movedPixels, RenderFrame(staticDoc)), 0u)
      << "the layout position must shift the rendered frame";
}

/**
 * Test case: only one axis carries a pivot; the other axis stays at the layer origin.
 */
PAGX_TEST(PAGXAnchorTest, SingleAxisAnchor) {
  auto anchored = ParseDoc(MakeRectDoc("id=\"layer\" anchor=\"0,50\" matrix=\"0,1,-1,0,0,0\""));
  ASSERT_TRUE(anchored != nullptr);
  auto* layer = static_cast<pagx::Layer*>(anchored->findNode("layer"));
  ASSERT_TRUE(layer != nullptr);
  EXPECT_FLOAT_EQ(layer->anchor.x, 0.0f);
  EXPECT_FLOAT_EQ(layer->anchor.y, 50.0f);

  // a = (0,50), L = rotate(90) → L*a = (-50,0) → t' = a - L*a = (50,50).
  const auto matrix = pagx::BuildLayerTransform3DWithAnchor(layer);
  EXPECT_NEAR(matrix.getRowColumn(0, 3), 50.0f, 1e-4f);
  EXPECT_NEAR(matrix.getRowColumn(1, 3), 50.0f, 1e-4f);

  auto baked = ParseDoc(MakeRectDoc("id=\"layer\" matrix=\"0,1,-1,0,50,50\""));
  ASSERT_TRUE(baked != nullptr);
  const auto anchoredPixels = RenderFrame(anchored);
  const auto bakedPixels = RenderFrame(baked);
  ASSERT_FALSE(anchoredPixels.empty());
  EXPECT_GT(CountOpaquePixels(anchoredPixels), 100u) << "the frame must actually draw something";
  EXPECT_EQ(CountDifferentPixels(anchoredPixels, bakedPixels), 0u);
}

/**
 * Test case: negative and fractional anchor values survive parsing and a text round-trip.
 */
PAGX_TEST(PAGXAnchorTest, NegativeDecimalAnchor) {
  auto doc = ParseDoc(MakeRectDoc("id=\"layer\" anchor=\"-10.5,20.25\" matrix=\"0,1,-1,0,0,0\""));
  ASSERT_TRUE(doc != nullptr);
  auto* layer = static_cast<pagx::Layer*>(doc->findNode("layer"));
  ASSERT_TRUE(layer != nullptr);
  EXPECT_FLOAT_EQ(layer->anchor.x, -10.5f);
  EXPECT_FLOAT_EQ(layer->anchor.y, 20.25f);

  // The text form is not asserted literally: only that re-parsing reproduces the same numbers.
  auto reparsed = ParseDoc(pagx::PAGXExporter::ToXML(*doc));
  ASSERT_TRUE(reparsed != nullptr);
  auto* roundTrip = static_cast<pagx::Layer*>(reparsed->findNode("layer"));
  ASSERT_TRUE(roundTrip != nullptr);
  EXPECT_NEAR(roundTrip->anchor.x, -10.5f, 1e-4f);
  EXPECT_NEAR(roundTrip->anchor.y, 20.25f, 1e-4f);
  EXPECT_NEAR(roundTrip->anchorZ, 0.0f, 1e-4f);
}

/**
 * Test case: malformed anchor text is rejected as a whole, leaving the layer at the default origin
 * instead of silently keeping the values that happened to parse.
 */
PAGX_TEST(PAGXAnchorTest, MalformedAnchorFallsBackToOrigin) {
  const std::vector<std::string> values = {"a,b", "10,", ",20", "10,20,30,40"};
  for (const auto& value : values) {
    auto doc = ParseDoc(MakeRectDoc("id=\"layer\" anchor=\"" + value + "\""));
    ASSERT_TRUE(doc != nullptr) << value;
    auto* layer = static_cast<pagx::Layer*>(doc->findNode("layer"));
    ASSERT_TRUE(layer != nullptr) << value;
    EXPECT_FLOAT_EQ(layer->anchor.x, 0.0f) << value;
    EXPECT_FLOAT_EQ(layer->anchor.y, 0.0f) << value;
    EXPECT_FLOAT_EQ(layer->anchorZ, 0.0f) << value;
  }
}

/**
 * Test case: an animated 2D rotation ignores a non-zero anchorZ, matching the static behaviour.
 */
PAGX_TEST(PAGXAnchorTest, AnimatedAnchorIgnoresAnchorZ) {
  const std::string identity = "1,0,0,1,0,0";
  const std::string rotate90 = "0,1,-1,0";
  const std::string keys = "          <Key time=\"0\" value=\"" + identity +
                           "\"/>\n          <Key time=\"60\" value=\"" + rotate90 + ",0,0\"/>\n";
  auto flat = ParseDoc(MakeAnimatedRotateDoc("id=\"rotated\" anchor=\"100,50\"", keys));
  auto deep = ParseDoc(MakeAnimatedRotateDoc("id=\"rotated\" anchor=\"100,50,40\"", keys));
  ASSERT_TRUE(flat != nullptr);
  ASSERT_TRUE(deep != nullptr);
  const auto flatPixels = RenderFrame(flat, 500'000);
  const auto deepPixels = RenderFrame(deep, 500'000);
  ASSERT_FALSE(flatPixels.empty());
  EXPECT_EQ(CountDifferentPixels(flatPixels, deepPixels), 0u);
}

/**
 * Test case: a three-value anchor round-trips through the exporter without losing the depth.
 */
PAGX_TEST(PAGXAnchorTest, ExportReparseThreeAxes) {
  auto doc =
      ParseDoc(MakeRectDoc("id=\"layer\" anchor=\"10,20,30\" matrix3D=\""
                           "1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1\""));
  ASSERT_TRUE(doc != nullptr);
  auto reparsed = ParseDoc(pagx::PAGXExporter::ToXML(*doc));
  ASSERT_TRUE(reparsed != nullptr);
  auto* layer = static_cast<pagx::Layer*>(reparsed->findNode("layer"));
  ASSERT_TRUE(layer != nullptr);
  EXPECT_NEAR(layer->anchor.x, 10.0f, 1e-4f);
  EXPECT_NEAR(layer->anchor.y, 20.0f, 1e-4f);
  EXPECT_NEAR(layer->anchorZ, 30.0f, 1e-4f);
}

PAGX_TEST(PAGXAnchorTest, PerspectiveAnchorMatchesBakedMatrix) {
  // rotationY=30, perspective=500, anchor=(100,50): the pivot is the affine 45-degree-free pivot
  // a - R*a = (13.39746, 0, 50), and the projection row stays unchanged. The baked document spells
  // that matrix out, so both must produce the same local transform.
  auto anchored =
      ParseDoc(MakeRectDoc("id=\"layer\" rotationY=\"30\" perspective=\"500\" "
                           "anchor=\"100,50\""));
  auto baked = ParseDoc(
      MakeRectDoc("id=\"layer\" matrix3D=\"0.866025,0,-0.5,0.001,0,1,0,0,0.5,0,0.866025,-0.001732,"
                  "13.39746,0,50,0.9\""));
  ASSERT_TRUE(anchored != nullptr);
  ASSERT_TRUE(baked != nullptr);
  auto* anchoredLayer = static_cast<pagx::Layer*>(anchored->findNode("layer"));
  auto* bakedLayer = static_cast<pagx::Layer*>(baked->findNode("layer"));
  ASSERT_TRUE(anchoredLayer != nullptr);
  ASSERT_TRUE(bakedLayer != nullptr);
  const auto anchoredMatrix = pagx::BuildLayerTransform3DWithAnchor(anchoredLayer);
  const auto bakedMatrix = pagx::BuildLayerTransform3DWithAnchor(bakedLayer);
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      EXPECT_NEAR(anchoredMatrix.getRowColumn(row, column), bakedMatrix.getRowColumn(row, column),
                  1e-3f)
          << "row " << row << " column " << column;
    }
  }
}

/**
 * Test case: with a perspective projection the pivot still lands on the same place, so the rendered
 * frame matches a document that spells the resulting matrix out as matrix3D.
 */
PAGX_TEST(PAGXAnchorTest, RenderPerspectiveAnchorMatchesBaked) {
  auto anchored =
      ParseDoc(MakeRectDoc("id=\"layer\" rotationY=\"30\" perspective=\"500\" "
                           "anchor=\"100,50\""));
  auto baked = ParseDoc(
      MakeRectDoc("id=\"layer\" matrix3D=\"0.866025,0,-0.5,0.001,0,1,0,0,0.5,0,0.866025,-0.001732,"
                  "13.39746,0,50,0.9\""));
  ASSERT_TRUE(anchored != nullptr);
  ASSERT_TRUE(baked != nullptr);
  const auto anchoredPixels = RenderFrame(anchored);
  const auto bakedPixels = RenderFrame(baked);
  ASSERT_FALSE(anchoredPixels.empty());
  EXPECT_GT(CountOpaquePixels(anchoredPixels), 100u) << "the frame must actually draw something";
  EXPECT_EQ(CountDifferentPixels(anchoredPixels, bakedPixels), 0u);
}

/**
 * Test case: the pivot also applies when the transform comes from components instead of a matrix.
 */
PAGX_TEST(PAGXAnchorTest, ComponentPathAppliesAnchor) {
  auto doc = ParseDoc(MakeRectDoc("id=\"layer\" rotationZ=\"45\" scaleX=\"2\" anchor=\"100,50\""));
  ASSERT_TRUE(doc != nullptr);
  auto* layer = static_cast<pagx::Layer*>(doc->findNode("layer"));
  ASSERT_TRUE(layer != nullptr);
  const auto matrix = pagx::BuildLayerTransform3DWithAnchor(layer);
  // L = R(45) * S(2, 1) = [[1.41421, -0.70711], [1.41421, 0.70711]]; a - L*a = (-6.066, -126.777).
  EXPECT_NEAR(matrix.getRowColumn(0, 0), 1.41421f, 1e-3f);
  EXPECT_NEAR(matrix.getRowColumn(0, 1), -0.70711f, 1e-3f);
  EXPECT_NEAR(matrix.getRowColumn(1, 0), 1.41421f, 1e-3f);
  EXPECT_NEAR(matrix.getRowColumn(1, 1), 0.70711f, 1e-3f);
  EXPECT_NEAR(matrix.getRowColumn(0, 3), -6.06602f, 1e-2f);
  EXPECT_NEAR(matrix.getRowColumn(1, 3), -126.77670f, 1e-2f);
}

/**
 * Test case: only the affine part is conjugated, so a scaled homogeneous row keeps its scale and a
 * pivot on a layer without a linear transform leaves the pose alone.
 */
PAGX_TEST(PAGXAnchorTest, PerspectiveAnchorKeepsHomogeneousScale) {
  const std::string scaled = "1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0.0000000001";
  auto doc = ParseDoc(MakeRectDoc("id=\"layer\" anchor=\"10,20\" matrix3D=\"" + scaled + "\""));
  ASSERT_TRUE(doc != nullptr);
  auto* layer = static_cast<pagx::Layer*>(doc->findNode("layer"));
  ASSERT_TRUE(layer != nullptr);
  const auto matrix = pagx::BuildLayerTransform3DWithAnchor(layer);
  // The linear part is the identity, so the pivot cannot move anything, and the homogeneous scale
  // must stay where the author put it instead of being pulled back to 1.
  EXPECT_NEAR(matrix.getRowColumn(0, 0), 1.0f, 1e-6f);
  EXPECT_NEAR(matrix.getRowColumn(0, 3), 0.0f, 1e-6f);
  EXPECT_NEAR(matrix.getRowColumn(1, 3), 0.0f, 1e-6f);
  EXPECT_NEAR(matrix.getRowColumn(3, 3), 0.0000000001f, 1e-12f);
}

/**
 * Test case: a vanishing perspective is the same rule with p = 0, so the pivot must not make the
 * matrix jump when `perspective` crosses zero.
 */
PAGX_TEST(PAGXAnchorTest, PerspectiveAnchorIsContinuousAcrossZero) {
  auto flat =
      ParseDoc(MakeRectDoc("id=\"layer\" rotationY=\"30\" perspective=\"0\" anchor=\"100,50\""));
  // `perspective` is a viewing distance: an absent projection is the d -> infinity limit, so the
  // rule must not change between no perspective and a very distant one.
  auto almost = ParseDoc(
      MakeRectDoc("id=\"layer\" rotationY=\"30\" perspective=\"1000000\" anchor=\"100,50\""));
  ASSERT_TRUE(flat != nullptr);
  ASSERT_TRUE(almost != nullptr);
  auto* flatLayer = static_cast<pagx::Layer*>(flat->findNode("layer"));
  auto* almostLayer = static_cast<pagx::Layer*>(almost->findNode("layer"));
  ASSERT_TRUE(flatLayer != nullptr);
  ASSERT_TRUE(almostLayer != nullptr);
  const auto flatMatrix = pagx::BuildLayerTransform3DWithAnchor(flatLayer);
  const auto almostMatrix = pagx::BuildLayerTransform3DWithAnchor(almostLayer);
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      EXPECT_NEAR(flatMatrix.getRowColumn(row, column), almostMatrix.getRowColumn(row, column),
                  1e-3f)
          << "row " << row << " column " << column;
    }
  }
  // The pivot has to matter, otherwise the case compares two documents that never differ.
  auto unanchored = ParseDoc(MakeRectDoc("id=\"layer\" rotationY=\"30\" perspective=\"0\""));
  ASSERT_TRUE(unanchored != nullptr);
  auto* unanchoredLayer = static_cast<pagx::Layer*>(unanchored->findNode("layer"));
  ASSERT_TRUE(unanchoredLayer != nullptr);
  const auto unanchoredMatrix = pagx::BuildLayerTransform3DWithAnchor(unanchoredLayer);
  EXPECT_GT(std::abs(flatMatrix.getRowColumn(2, 3) - unanchoredMatrix.getRowColumn(2, 3)), 40.0f);
}

/**
 * Test case: the translation sits inside the projection, so a perspective layer with an authored x
 * matches the matrix whose pivoted translation is spelled out.
 */
PAGX_TEST(PAGXAnchorTest, PerspectivePositionStaysInsideProjection) {
  auto anchored =
      ParseDoc(MakeRectDoc("id=\"layer\" x=\"40\" rotationY=\"30\" perspective=\"500\" "
                           "anchor=\"100,50\""));
  auto baked = ParseDoc(
      MakeRectDoc("id=\"layer\" matrix3D=\"0.866025,0,-0.5,0.001,0,1,0,0,0.5,0,0.866025,-0.001732,"
                  "53.39746,0,50,0.9\""));
  ASSERT_TRUE(anchored != nullptr);
  ASSERT_TRUE(baked != nullptr);
  auto* anchoredLayer = static_cast<pagx::Layer*>(anchored->findNode("layer"));
  auto* bakedLayer = static_cast<pagx::Layer*>(baked->findNode("layer"));
  ASSERT_TRUE(anchoredLayer != nullptr);
  ASSERT_TRUE(bakedLayer != nullptr);
  const auto anchoredMatrix = pagx::BuildLayerTransform3DWithAnchor(anchoredLayer);
  const auto bakedMatrix = pagx::BuildLayerTransform3DWithAnchor(bakedLayer);
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      EXPECT_NEAR(anchoredMatrix.getRowColumn(row, column), bakedMatrix.getRowColumn(row, column),
                  1e-3f)
          << "row " << row << " column " << column;
    }
  }
}

/**
 * Test case: the anchor channels drive the pivot itself, so an animated pivot matches a pivot
 * authored statically on the frame the keys land on.
 */
PAGX_TEST(PAGXAnchorTest, AnimateAnchorChannels) {
  const std::string animations =
      "  <Animations>\n"
      "    <Animation id=\"pivot\" duration=\"60\">\n"
      "      <Object target=\"rotated\">\n"
      "        <Channel name=\"anchor.x\" type=\"float\">\n"
      "          <Key time=\"0\" value=\"0\"/>\n"
      "          <Key time=\"60\" value=\"100\"/>\n"
      "        </Channel>\n"
      "        <Channel name=\"anchor.y\" type=\"float\">\n"
      "          <Key time=\"0\" value=\"0\"/>\n"
      "          <Key time=\"60\" value=\"50\"/>\n"
      "        </Channel>\n"
      "      </Object>\n"
      "    </Animation>\n"
      "  </Animations>\n";
  auto animated = ParseDoc(MakeDoc("id=\"rotated\" matrix=\"0,1,-1,0,0,0\"", animations));
  auto authored = ParseDoc(MakeRectDoc("id=\"rotated\" anchor=\"50,25\" matrix=\"0,1,-1,0,0,0\""));
  ASSERT_TRUE(animated != nullptr);
  ASSERT_TRUE(authored != nullptr);
  const auto animatedPixels = RenderFrame(animated, 500'000);
  ASSERT_FALSE(animatedPixels.empty());
  EXPECT_GT(CountOpaquePixels(animatedPixels), 100u) << "the frame must actually draw something";
  EXPECT_EQ(CountDifferentPixels(animatedPixels, RenderFrame(authored)), 0u);
  auto plain = ParseDoc(MakeRectDoc("id=\"rotated\" matrix=\"0,1,-1,0,0,0\""));
  ASSERT_TRUE(plain != nullptr);
  EXPECT_GT(CountDifferentPixels(animatedPixels, RenderFrame(plain)), 0u)
      << "the pivot write must move the frame";
}

/**
 * Test case: anchor.z is a channel as well, so an animated pivot depth matches a static one.
 */
PAGX_TEST(PAGXAnchorTest, AnimateAnchorZChannel) {
  const std::string rotation = "1,0,0,0,0,0.5,0.8660254,0,0,-0.8660254,0.5,0,0,0,0,1";
  const std::string animations =
      "  <Animations>\n"
      "    <Animation id=\"pivot\" duration=\"60\">\n"
      "      <Object target=\"rotated\">\n"
      "        <Channel name=\"anchor.z\" type=\"float\">\n"
      "          <Key time=\"0\" value=\"0\"/>\n"
      "          <Key time=\"60\" value=\"40\"/>\n"
      "        </Channel>\n"
      "      </Object>\n"
      "    </Animation>\n"
      "  </Animations>\n";
  auto animated = ParseDoc(
      MakeDoc("id=\"rotated\" anchor=\"100,50\" matrix3D=\"" + rotation + "\"", animations));
  auto authored =
      ParseDoc(MakeRectDoc("id=\"rotated\" anchor=\"100,50,20\" matrix3D=\"" + rotation + "\""));
  ASSERT_TRUE(animated != nullptr);
  ASSERT_TRUE(authored != nullptr);
  const auto animatedPixels = RenderFrame(animated, 500'000);
  ASSERT_FALSE(animatedPixels.empty());
  EXPECT_GT(CountOpaquePixels(animatedPixels), 100u) << "the frame must actually draw something";
  EXPECT_EQ(CountDifferentPixels(animatedPixels, RenderFrame(authored)), 0u);
}

/**
 * Test case: an anchor over an animated matrix3D is applied to the sampled pose, so the frame
 * matches a document whose matrix3D already carries the pivot.
 */
PAGX_TEST(PAGXAnchorTest, AnchorAppliesOverAnimatedMatrix3D) {
  const std::string rotation = "1,0,0,0,0,0.5,0.8660254,0,0,-0.8660254,0.5,0,0,0,0,1";
  const std::string keys = "          <Key time=\"0\" value=\"" + rotation +
                           "\"/>\n          <Key time=\"30\" value=\"" + rotation +
                           "\"/>\n          <Key time=\"60\" value=\"" + rotation + "\"/>\n";
  const std::string animations =
      "  <Animations>\n"
      "    <Animation id=\"rotate\" duration=\"60\">\n"
      "      <Object target=\"rotated\">\n"
      "        <Channel name=\"matrix3D\" type=\"matrix3D\">\n" +
      keys +
      "        </Channel>\n"
      "      </Object>\n"
      "    </Animation>\n"
      "  </Animations>\n";
  auto animated = ParseDoc(
      MakeDoc("id=\"rotated\" anchor=\"100,50,20\" matrix3D=\"" + rotation + "\"", animations));
  auto baked =
      ParseDoc(MakeRectDoc("id=\"rotated\" matrix3D=\"1,0,0,0,0,0.5,0.8660254,0,0,"
                           "-0.8660254,0.5,0,0,42.32051,-33.30127,1\""));
  ASSERT_TRUE(animated != nullptr);
  ASSERT_TRUE(baked != nullptr);
  const auto animatedPixels = RenderFrame(animated, 500'000);
  ASSERT_FALSE(animatedPixels.empty());
  EXPECT_GT(CountOpaquePixels(animatedPixels), 100u) << "the frame must actually draw something";
  EXPECT_EQ(CountDifferentPixels(animatedPixels, RenderFrame(baked)), 0u);
}

/**
 * Test case: a matrix3D channel that cannot be decomposed still gets the pivot, and only once: the
 * keyframe holds the unanchored pose, so the published matrix is the pivoted one.
 */
PAGX_TEST(PAGXAnchorTest, AnchorAppliesOverNonDecomposableMatrix3DChannel) {
  const std::string pose = "0.866025,0,-0.5,0.001,0,1,0,0,0.5,0,0.866025,-0.001732,0,0,0,1";
  const std::string keys = "          <Key time=\"0\" value=\"" + pose +
                           "\"/>\n          <Key time=\"30\" value=\"" + pose +
                           "\"/>\n          <Key time=\"60\" value=\"" + pose + "\"/>\n";
  const std::string animations =
      "  <Animations>\n"
      "    <Animation id=\"rotate\" duration=\"60\">\n"
      "      <Object target=\"rotated\">\n"
      "        <Channel name=\"matrix3D\" type=\"matrix3D\">\n" +
      keys +
      "        </Channel>\n"
      "      </Object>\n"
      "    </Animation>\n"
      "  </Animations>\n";
  auto animated = ParseDoc(MakeDoc("id=\"rotated\" anchor=\"100,50\"", animations));
  auto authored =
      ParseDoc(MakeRectDoc("id=\"rotated\" anchor=\"100,50\" matrix3D=\"" + pose + "\""));
  ASSERT_TRUE(animated != nullptr);
  ASSERT_TRUE(authored != nullptr);
  const auto animatedPixels = RenderFrame(animated, 500'000);
  ASSERT_FALSE(animatedPixels.empty());
  EXPECT_GT(CountOpaquePixels(animatedPixels), 100u) << "the frame must actually draw something";
  EXPECT_EQ(CountDifferentPixels(animatedPixels, RenderFrame(authored)), 0u);
  // The pivoted pose the channel produces is the projection of the affine part, so it matches the
  // matrix with the pivot baked in up to the precision of these literals.
  auto* authoredLayer = static_cast<pagx::Layer*>(authored->findNode("rotated"));
  ASSERT_TRUE(authoredLayer != nullptr);
  const auto anchored = pagx::BuildLayerTransform3DWithAnchor(authoredLayer);
  const std::vector<float> baked = {0.866025f, 0.0f, -0.5f, 0.001f, 0.0f,      1.0f,
                                    0.0f,      0.0f, 0.5f,  0.0f,   0.866025f, -0.001732f,
                                    13.39746f, 0.0f, 50.0f, 0.9f};
  for (int index = 0; index < 16; ++index) {
    const int row = index % 4;
    const int column = index / 4;
    EXPECT_NEAR(anchored.getRowColumn(row, column), baked[static_cast<size_t>(index)], 1e-3f)
        << "row " << row << " column " << column;
  }
}

/**
 * Test case: the HTML export writes the same pivoted matrix as the renderer.
 */
PAGX_TEST(PAGXAnchorTest, HtmlExportAppliesAnchor) {
  auto doc = ParseDoc(MakeRectDoc("id=\"layer\" anchor=\"100,50\" matrix=\"0,1,-1,0,0,0\""));
  ASSERT_TRUE(doc != nullptr);
  auto* layer = static_cast<pagx::Layer*>(doc->findNode("layer"));
  ASSERT_TRUE(layer != nullptr);
  const auto css = pagx::LayerTransformCSS(layer);
  // rotate(90) around (100,50): the affine pivot moves the translation to a - L*a = (150,-50).
  EXPECT_NE(css.find("translate(150px,-50px)"), std::string::npos) << css;
  EXPECT_NE(css.find("rotate(90deg)"), std::string::npos) << css;
}

}  // namespace pag
