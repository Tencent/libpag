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
//  license is distributed on an "AS IS" BASIS, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

// HTML importer coverage for two parallel features that share one transform pipeline:
//
//   3D:   the importer must stop projecting `matrix3d(...)` onto its 2D affine subset, compose the
//         whole `transform` function list into a 4x4, and route 3D poses to `Layer.matrix3D`.
//   anchor: `transform-origin` must land on `Layer.anchor` instead of being baked into the matrix,
//         so matrix / matrix3D keyframes stay un-pivoted and the runtime applies the pivot once.
//
// The anchor cases are animation-focused: on a static pose a baked matrix is an exact substitute,
// so anchoring only has an observable effect where keyframes are interpolated. 3D keeps its own
// coverage because it is a capability in its own right, and the coupling case (3D pose pivoted off
// centre) is asserted explicitly.

#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include "base/PAGTest.h"
#include "base/PAGXTestCommon.h"
#include "pagx/HTMLImporter.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/html/importer/HTMLDiagnosticSink.h"
#include "pagx/html/importer/HTMLTransform3D.h"
#include "pagx/html/importer/HTMLValueParser.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/Layer.h"
#include "pagx/runtime/Matrix3DDecompose.h"
#include "pagx/types/Matrix.h"
#include "pagx/types/Matrix3D.h"
#include "pagx/utils/LayerTransformUtils.h"

namespace pag {
namespace {

bool HasZeroAnchor(const pagx::Layer* layer) {
  return NearlyEqual(layer->anchor.x, 0.0f) && NearlyEqual(layer->anchor.y, 0.0f) &&
         NearlyEqual(layer->anchorZ, 0.0f);
}

// Parses HTML with the subset transformer disabled so the importer's own CSS cascade and
// transform machinery run end-to-end, matching the other importer unit tests.
std::shared_ptr<pagx::PAGXDocument> ParseHtml(const std::string& html) {
  pagx::HTMLImporter::Options options = {};
  options.autoNormalize = false;
  return pagx::HTMLImporter::ParseString(html, options);
}

// Round-trips a document through the serialized form before rendering it, because animation targets
// only resolve in the file form. The runtime looks an `Animation` target up through the document's
// id index; `PAGXImporter::FromXML` fills that index, while the HTML importer only writes
// `Layer::id` (see the note in `HTMLParserContext::assignElementId`). An in-memory document therefore
// keeps its layers and keyframes but cannot bind its animations, and every frame would show the
// pre-animation state. This is a pre-existing importer limitation, not something these tests create,
// and the CLI / eval harness renders through the same file form. Pinning the active-window frame as
// well as the hold keeps the test honest: a document whose target failed to bind passes the hold
// assertion anyway, so the visible frame is what proves the animation actually ran.
std::shared_ptr<pagx::PAGXDocument> RoundTripForRender(
    const std::shared_ptr<pagx::PAGXDocument>& doc) {
  if (doc == nullptr) {
    return nullptr;
  }
  return pagx::PAGXImporter::FromXML(pagx::PAGXExporter::ToXML(*doc));
}

pagx::Layer* FindLayerById(pagx::Layer* layer, const std::string& id) {
  if (layer == nullptr) {
    return nullptr;
  }
  if (layer->id == id) {
    return layer;
  }
  for (auto* child : layer->children) {
    if (auto* match = FindLayerById(child, id)) {
      return match;
    }
  }
  return nullptr;
}

pagx::Layer* FindLayerById(const std::shared_ptr<pagx::PAGXDocument>& doc, const std::string& id) {
  if (doc == nullptr) {
    return nullptr;
  }
  for (auto* layer : doc->layers) {
    if (auto* match = FindLayerById(layer, id)) {
      return match;
    }
  }
  return nullptr;
}

void CollectLayersWithMatrix3D(pagx::Layer* layer, std::vector<pagx::Layer*>* out) {
  if (layer == nullptr) {
    return;
  }
  if (!layer->matrix3D.isIdentity()) {
    out->push_back(layer);
  }
  for (auto* child : layer->children) {
    CollectLayersWithMatrix3D(child, out);
  }
}

std::vector<pagx::Layer*> LayersWithNonIdentityMatrix3D(
    const std::shared_ptr<pagx::PAGXDocument>& doc) {
  std::vector<pagx::Layer*> layers = {};
  if (doc == nullptr) {
    return layers;
  }
  for (auto* layer : doc->layers) {
    CollectLayersWithMatrix3D(layer, &layers);
  }
  return layers;
}

void CollectCameraLayers(pagx::Layer* layer, std::vector<pagx::Layer*>* out) {
  if (layer == nullptr || out == nullptr) {
    return;
  }
  if (layer->perspective > 0.0f) {
    out->push_back(layer);
  }
  for (auto* child : layer->children) {
    CollectCameraLayers(child, out);
  }
}

std::vector<pagx::Layer*> CameraLayers(const std::shared_ptr<pagx::PAGXDocument>& doc) {
  std::vector<pagx::Layer*> layers = {};
  if (doc == nullptr) {
    return layers;
  }
  for (auto* layer : doc->layers) {
    CollectCameraLayers(layer, &layers);
  }
  return layers;
}

pagx::Layer* CameraWrapperOf(pagx::Layer* layer, const pagx::Layer* target) {
  if (layer == nullptr || target == nullptr) {
    return nullptr;
  }
  for (auto* child : layer->children) {
    if (child == target && layer->perspective > 0.0f) {
      return layer;
    }
    if (auto* found = CameraWrapperOf(child, target)) {
      return found;
    }
  }
  return nullptr;
}

pagx::Layer* CameraWrapperOf(const std::shared_ptr<pagx::PAGXDocument>& doc,
                             const pagx::Layer* target) {
  if (doc == nullptr) {
    return nullptr;
  }
  for (auto* layer : doc->layers) {
    if (auto* found = CameraWrapperOf(layer, target)) {
      return found;
    }
  }
  return nullptr;
}

bool HasProjectionRow(const pagx::Layer* layer) {
  if (layer == nullptr) {
    return false;
  }
  return std::fabs(layer->matrix3D.getRowColumn(3, 0)) > 1e-6f ||
         std::fabs(layer->matrix3D.getRowColumn(3, 1)) > 1e-6f ||
         std::fabs(layer->matrix3D.getRowColumn(3, 2)) > 1e-6f;
}

pagx::Animation* FirstAnimation(const std::shared_ptr<pagx::PAGXDocument>& doc) {
  if (doc == nullptr || doc->animations.empty()) {
    return nullptr;
  }
  return static_cast<pagx::Animation*>(doc->animations.front());
}

pagx::Channel* FindChannel(pagx::Animation* animation, const std::string& name) {
  if (animation == nullptr) {
    return nullptr;
  }
  for (auto* object : animation->objects) {
    for (auto* channel : object->channels) {
      if (channel != nullptr && channel->name == name) {
        return channel;
      }
    }
  }
  return nullptr;
}

std::string ChannelTarget(pagx::Animation* animation, const std::string& name) {
  if (animation == nullptr) {
    return {};
  }
  for (auto* object : animation->objects) {
    for (auto* channel : object->channels) {
      if (channel != nullptr && channel->name == name) {
        return object->target;
      }
    }
  }
  return {};
}

pagx::TypedChannel<pagx::Matrix>* FindMatrixChannel(pagx::Animation* animation) {
  auto* channel = FindChannel(animation, "matrix");
  if (channel == nullptr || channel->valueType() != pagx::ChannelValueType::Matrix) {
    return nullptr;
  }
  return static_cast<pagx::TypedChannel<pagx::Matrix>*>(channel);
}

pagx::TypedChannel<pagx::Matrix3D>* FindMatrix3DChannel(pagx::Animation* animation) {
  auto* channel = FindChannel(animation, "matrix3D");
  if (channel == nullptr || channel->valueType() != pagx::ChannelValueType::Matrix3D) {
    return nullptr;
  }
  return static_cast<pagx::TypedChannel<pagx::Matrix3D>*>(channel);
}

// Counts the pixels a frame actually paints: any non-zero alpha means the layer covered part of
// that pixel. A sub-pixel pose can still touch its corner pixel, so assertions allow a few.
size_t CountVisiblePixels(const std::vector<uint8_t>& pixels) {
  size_t visible = 0;
  for (size_t i = 3; i < pixels.size(); i += 4) {
    if (pixels[i] != 0) {
      ++visible;
    }
  }
  return visible;
}

// Bounding box of the pixels a frame actually paints. Rendered geometry is what these camera tests
// are about: a wrapper that exists but is sized wrong, or a flattened subtree, still has the right
// node structure and would pass a field-only assertion.
struct VisibleBounds {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

VisibleBounds BoundsOf(const std::vector<uint8_t>& pixels) {
  VisibleBounds bounds = {};
  int minX = PAGX_TEST_DOC_SIZE;
  int minY = PAGX_TEST_DOC_SIZE;
  int maxX = -1;
  int maxY = -1;
  for (int y = 0; y < PAGX_TEST_DOC_SIZE; ++y) {
    for (int x = 0; x < PAGX_TEST_DOC_SIZE; ++x) {
      if (pixels[(static_cast<size_t>(y) * PAGX_TEST_DOC_SIZE + x) * 4 + 3] == 0) {
        continue;
      }
      minX = std::min(minX, x);
      minY = std::min(minY, y);
      maxX = std::max(maxX, x);
      maxY = std::max(maxY, y);
    }
  }
  if (maxX < 0) {
    return bounds;
  }
  bounds.x = minX;
  bounds.y = minY;
  bounds.width = maxX - minX + 1;
  bounds.height = maxY - minY + 1;
  return bounds;
}

VisibleBounds RedBoundsOf(const std::vector<uint8_t>& pixels) {
  VisibleBounds bounds = {};
  int minX = PAGX_TEST_DOC_SIZE;
  int minY = PAGX_TEST_DOC_SIZE;
  int maxX = -1;
  int maxY = -1;
  for (int y = 0; y < PAGX_TEST_DOC_SIZE; ++y) {
    for (int x = 0; x < PAGX_TEST_DOC_SIZE; ++x) {
      const auto offset = (static_cast<size_t>(y) * PAGX_TEST_DOC_SIZE + x) * 4;
      const bool red = pixels[offset] > 200 && pixels[offset + 1] < 80 && pixels[offset + 2] < 80 &&
                       pixels[offset + 3] != 0;
      if (!red) {
        continue;
      }
      minX = std::min(minX, x);
      minY = std::min(minY, y);
      maxX = std::max(maxX, x);
      maxY = std::max(maxY, y);
    }
  }
  if (maxX < 0) {
    return bounds;
  }
  bounds.x = minX;
  bounds.y = minY;
  bounds.width = maxX - minX + 1;
  bounds.height = maxY - minY + 1;
  return bounds;
}

// Composes two layer poses, so a test can assert what a child's pose becomes once an ancestor
// layer's transform is applied to it.
pagx::Matrix3D MultiplyPose(const pagx::Matrix3D& left, const pagx::Matrix3D& right) {
  pagx::Matrix3D result = {};
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      float sum = 0.0f;
      for (int i = 0; i < 4; ++i) {
        sum += left.getRowColumn(row, i) * right.getRowColumn(i, column);
      }
      result.setRowColumn(row, column, sum);
    }
  }
  return result;
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// 3D: axis rotations. Values follow the CSS column-major layout, which is also what
// `Layer.matrix3D` stores (`getRowColumn(row, column)`), so the assertions pin the exact axes.
// ---------------------------------------------------------------------------------------------

/**
 * Test case: `rotateY` reaches `matrix3D` and leaves the 2D matrix (and the Y axis) alone.
 */
PAG_TEST(PAGXHTML3DTest, RotateYEmitsMatrix3D) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100px;height:60px;background-color:#6366F1;
           transform:rotateY(30deg)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  const auto& m = layer->matrix3D;
  ASSERT_FALSE(m.isIdentity());
  EXPECT_TRUE(layer->matrix.isIdentity()) << "3D must not also fill the 2D matrix";
  EXPECT_TRUE(NearlyEqual(m.getRowColumn(1, 1), 1.0f));
  EXPECT_TRUE(NearlyEqual(m.getRowColumn(0, 0), 0.866025f, 0.001f));
  EXPECT_TRUE(NearlyEqual(m.getRowColumn(2, 2), 0.866025f, 0.001f));
  EXPECT_TRUE(NearlyEqual(m.getRowColumn(0, 2), 0.5f, 0.001f));
  EXPECT_TRUE(NearlyEqual(m.getRowColumn(2, 0), -0.5f, 0.001f));
}

/**
 * Test case: `rotateX` reaches `matrix3D` and rotates the Y/Z axes only.
 */
PAG_TEST(PAGXHTML3DTest, RotateXEmitsMatrix3D) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100px;height:60px;background-color:#6366F1;
           transform:rotateX(30deg)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  const auto& m = layer->matrix3D;
  ASSERT_FALSE(m.isIdentity());
  EXPECT_TRUE(NearlyEqual(m.getRowColumn(0, 0), 1.0f));
  EXPECT_TRUE(NearlyEqual(m.getRowColumn(1, 1), 0.866025f, 0.001f));
  EXPECT_TRUE(NearlyEqual(m.getRowColumn(1, 2), -0.5f, 0.001f));
  EXPECT_TRUE(NearlyEqual(m.getRowColumn(2, 1), 0.5f, 0.001f));
  EXPECT_TRUE(NearlyEqual(m.getRowColumn(2, 2), 0.866025f, 0.001f));
}

/**
 * Test case: in-plane rotations stay on the cheap 2D path; the 3D fields must not be touched.
 */
PAG_TEST(PAGXHTML3DTest, RotateZStaysTwoDimensional) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100px;height:60px;background-color:#6366F1;
           transform:rotateZ(45deg)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(layer->matrix3D.isIdentity());
  EXPECT_TRUE(NearlyEqual(layer->matrix.a, 0.707106f, 0.001f));
  EXPECT_TRUE(NearlyEqual(layer->matrix.b, 0.707106f, 0.001f));
  EXPECT_TRUE(NearlyEqual(layer->matrix.c, -0.707106f, 0.001f));
}

/**
 * Test case: `rotate3d` normalises its axis; a Z axis is the same as `rotate()`.
 */
PAG_TEST(PAGXHTML3DTest, Rotate3DAxisNormalized) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100px;height:60px;background-color:#6366F1;
           transform:rotate3d(0, 0, 2, 90deg)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(layer->matrix3D.isIdentity());
  const auto expected = pagx::Matrix::Rotate(90);
  EXPECT_TRUE(NearlyEqual(layer->matrix.a, expected.a, 0.001f));
  EXPECT_TRUE(NearlyEqual(layer->matrix.b, expected.b, 0.001f));
  EXPECT_TRUE(NearlyEqual(layer->matrix.c, expected.c, 0.001f));
  EXPECT_TRUE(NearlyEqual(layer->matrix.d, expected.d, 0.001f));
}

/**
 * Test case: a zero-length `rotate3d` axis is invalid and drops the declaration.
 */
PAG_TEST(PAGXHTML3DTest, Rotate3DInvalidAxisWarns) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100px;height:60px;background-color:#6366F1;
           transform:rotate3d(0, 0, 0, 45deg)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(layer->matrix.isIdentity());
  EXPECT_TRUE(layer->matrix3D.isIdentity());
  EXPECT_TRUE(HasDiagnosticContaining(doc, "rotate3d"));
}

// ---------------------------------------------------------------------------------------------
// 3D: perspective, depth translation and matrix3d passthrough.
// ---------------------------------------------------------------------------------------------

/**
 * Test case: `perspective()` is applied before the rotation (CSS list order), so the projection row
 * is `-1/d` times the rotation's Z row.
 */
PAG_TEST(PAGXHTML3DTest, PerspectiveFunctionEntersProjectionRow) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100px;height:60px;background-color:#6366F1;
           transform:perspective(500px) rotateY(30deg)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  const auto& m = layer->matrix3D;
  ASSERT_FALSE(m.isIdentity());
  // row3 = (0, 0, -1/500, 1) * rotateY(30): col2 = -0.002 * cos30, col0 = -0.002 * -sin30.
  EXPECT_TRUE(NearlyEqual(m.getRowColumn(3, 2), -0.001732f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(m.getRowColumn(3, 0), 0.001f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(m.getRowColumn(3, 1), 0.0f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(m.getRowColumn(3, 3), 1.0f, 1e-5f));
}

/**
 * Test case: a negative perspective distance is invalid and drops the declaration.
 */
PAG_TEST(PAGXHTML3DTest, NegativePerspectiveWarns) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100px;height:60px;background-color:#6366F1;
           transform:perspective(-500px) rotateY(30deg)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(layer->matrix.isIdentity());
  EXPECT_TRUE(layer->matrix3D.isIdentity());
  EXPECT_TRUE(HasDiagnosticContaining(doc, "perspective"));
}

/**
 * Test case: CSS Transforms 2 clamps a `perspective()` distance below 1px to 1px, so `0` and `0.5px`
 * stay valid and the rest of the list still applies. Rejecting the function used to drop the whole
 * declaration, losing the sibling `translateX(10px)` that a browser still applies.
 */
PAG_TEST(PAGXHTML3DTest, PerspectiveBelowOnePixelClampsAndKeepsTheList) {
  const char* distances[] = {"0", "0.5px"};
  for (const char* distance : distances) {
    std::string html = R"HTML(
      <html><body style="width:200px;height:100px">
        <div id="box" style="width:100px;height:60px;background-color:#6366F1;
             transform:perspective(DISTANCE) translateX(10px)"></div>
      </body></html>
    )HTML";
    html.replace(html.find("DISTANCE"), 8, distance);
    auto doc = ParseHtml(html);
    ASSERT_NE(doc, nullptr) << distance;
    EXPECT_FALSE(HasDiagnosticContaining(doc, "perspective")) << distance;
    auto* layer = FindLayerById(doc, "box");
    ASSERT_NE(layer, nullptr) << distance;
    const auto& pose = layer->matrix3D;
    // The clamped camera is part of the pose, so the pose is 3D: the translation sits in the 3D
    // translation column and `-1 / 1px` puts `-1` in the projection row.
    EXPECT_TRUE(NearlyEqual(pose.getRowColumn(0, 3), 10.0f, 1e-5f)) << distance;
    EXPECT_TRUE(NearlyEqual(pose.getRowColumn(3, 2), -1.0f, 1e-5f)) << distance;
    EXPECT_TRUE(layer->matrix.isIdentity()) << distance;
  }
}

/**
 * Test case: the `perspective` property clamps a value below 1px to 1px for rendering, the same rule
 * `perspective()` follows (CSS Transforms 2, §Perspective). `0` therefore still defines a camera — a
 * 1px one — instead of meaning "no perspective"; only `none` disables the projection.
 */
PAG_TEST(PAGXHTML3DTest, ZeroParentPerspectiveClampsToOnePixel) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:160px">
      <div style="position:absolute;left:0;top:0;width:200px;height:160px;perspective:0">
        <div id="card" style="position:absolute;left:10px;top:10px;width:80px;height:40px;
             background-color:#38BDF8;transform:rotateY(30deg)"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(doc, "perspective"));
  auto* card = FindLayerById(doc, "card");
  ASSERT_NE(card, nullptr);
  // `perspective: 0` clamps to a 1px camera. The camera lives on a wrapper layer now, so the child's
  // pose stays a plain rotation and the projection is applied outside interpolation.
  auto* wrapper = CameraWrapperOf(doc, card);
  ASSERT_NE(wrapper, nullptr) << "a zero distance must clamp to a 1px camera wrapper";
  EXPECT_TRUE(NearlyEqual(wrapper->perspective, 1.0f, 1e-4f));
  EXPECT_FALSE(HasProjectionRow(card)) << "the camera must not be folded into the child pose";
  EXPECT_TRUE(NearlyEqual(card->matrix3D.getRowColumn(0, 0), 0.866025f, 1e-4f));
}

/**
 * Test case: `translateZ` keeps the depth in the 3D translation column.
 */
PAG_TEST(PAGXHTML3DTest, TranslateZMovesInDepth) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100px;height:60px;background-color:#6366F1;
           transform:translateZ(100px)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  ASSERT_FALSE(layer->matrix3D.isIdentity());
  EXPECT_TRUE(NearlyEqual(layer->matrix3D.getRowColumn(0, 3), 0.0f));
  EXPECT_TRUE(NearlyEqual(layer->matrix3D.getRowColumn(1, 3), 0.0f));
  EXPECT_TRUE(NearlyEqual(layer->matrix3D.getRowColumn(2, 3), 100.0f));
  EXPECT_TRUE(layer->matrix.isIdentity());
}

/**
 * Test case: `matrix3d(...)` is written through verbatim; the orthographic projection is gone.
 */
PAG_TEST(PAGXHTML3DTest, Matrix3DPassesThroughUnprojected) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100px;height:60px;background-color:#6366F1;
           transform:matrix3d(1,0,0,0, 0,1,0,0, 0,0,1,-0.002, 10,20,30,1)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  const float expected[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, -0.002f, 10, 20, 30, 1};
  for (int i = 0; i < 16; ++i) {
    EXPECT_TRUE(NearlyEqual(layer->matrix3D.values[i], expected[i], 1e-5f)) << "index " << i;
  }
  EXPECT_TRUE(layer->matrix.isIdentity()) << "3D must not be projected onto the 2D matrix";
}

/**
 * Test case: a `matrix3d` that is really a flat 2D affine falls back to the 2D matrix, so the
 * output stays minimal for pages whose transforms are only pedantically 4x4.
 */
PAG_TEST(PAGXHTML3DTest, FlattenedMatrix3DFallsBackTo2D) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100px;height:60px;background-color:#6366F1;
           transform:matrix3d(1,0,0,0, 0,1,0,0, 0,0,1,0, 10,20,0,1)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(layer->matrix3D.isIdentity()) << "flat depth must not allocate a 3D pose";
  EXPECT_TRUE(NearlyEqual(layer->matrix.tx, 10.0f));
  EXPECT_TRUE(NearlyEqual(layer->matrix.ty, 20.0f));
}

/**
 * Test case: a singular `matrix3d` is rejected (PAGX requires finite, invertible 3D poses).
 */
PAG_TEST(PAGXHTML3DTest, Matrix3DSingularIsRejected) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100px;height:60px;background-color:#6366F1;
           transform:matrix3d(1,0,0,0, 0,1,0,0, 0,0,0,0, 0,0,0,1)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(layer->matrix3D.isIdentity());
  EXPECT_TRUE(layer->matrix.isIdentity());
  EXPECT_TRUE(HasDiagnosticContaining(doc, "not invertible"));
}

// ---------------------------------------------------------------------------------------------
// 3D: composition order and the preserve-3d flag.
// ---------------------------------------------------------------------------------------------

/**
 * Test case: a compound 2D chain composes in CSS order (T * R * S) instead of being dropped.
 */
PAG_TEST(PAGXHTML3DTest, CompoundTransformChainComposes) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100px;height:60px;background-color:#6366F1;
           transform:translateX(10px) rotate(45deg) scale(2)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  const auto expected =
      pagx::Matrix::Translate(10, 0) * pagx::Matrix::Rotate(45) * pagx::Matrix::Scale(2, 2);
  EXPECT_TRUE(NearlyEqual(layer->matrix.a, expected.a, 0.001f));
  EXPECT_TRUE(NearlyEqual(layer->matrix.b, expected.b, 0.001f));
  EXPECT_TRUE(NearlyEqual(layer->matrix.c, expected.c, 0.001f));
  EXPECT_TRUE(NearlyEqual(layer->matrix.d, expected.d, 0.001f));
  EXPECT_TRUE(NearlyEqual(layer->matrix.tx, expected.tx, 0.001f));
  EXPECT_TRUE(NearlyEqual(layer->matrix.ty, expected.ty, 0.001f));
  EXPECT_TRUE(layer->matrix3D.isIdentity());
  EXPECT_FALSE(HasDiagnosticContaining(doc, "compound transforms are not supported"));
}

/**
 * Test case: in a `rotateY(...) translateZ(...)` chain the translation happens first, so the depth
 * offset is rotated into the X/Z plane instead of staying on the Z axis.
 */
PAG_TEST(PAGXHTML3DTest, Compound3DChainRotatesTheDepthOffset) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100px;height:60px;background-color:#6366F1;
           transform:rotateY(30deg) translateZ(-60px)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  const auto& m = layer->matrix3D;
  ASSERT_FALSE(m.isIdentity());
  EXPECT_TRUE(NearlyEqual(m.getRowColumn(0, 3), -30.0f, 0.01f));
  EXPECT_TRUE(NearlyEqual(m.getRowColumn(1, 3), 0.0f, 0.01f));
  EXPECT_TRUE(NearlyEqual(m.getRowColumn(2, 3), -51.9615f, 0.01f));
}

/**
 * Test case: `transform-style: preserve-3d` maps onto the layer flag.
 */
PAG_TEST(PAGXHTML3DTest, TransformStylePreserve3DMapsToLayer) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100px;height:60px;background-color:#6366F1;
           transform-style:preserve-3d;transform:rotateY(10deg)">
        <div style="width:20px;height:20px;background-color:#F472B6;
             transform:translateZ(30px)"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(layer->preserve3D);
}

/**
 * Test case: an inline `<svg>` host sized by its `width` / `height` attributes (not CSS) still
 * resolves the box centre for the pivot, so a 3D animation keeps `anchor` at (w/2, h/2).
 */
PAG_TEST(PAGXHTML3DTest, AnimatedInlineSvgHostResolvesCenterAnchor) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes flip3d {
        0%   { transform: perspective(400px) rotateY(0deg); }
        100% { transform: perspective(400px) rotateY(60deg); }
      }
    </style></head>
    <body style="width:200px;height:120px">
      <svg width="80" height="60" viewBox="0 0 80 60" style="animation:flip3d 1s linear infinite">
        <rect x="4" y="4" width="72" height="52" fill="#6366F1"/>
      </svg>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  ASSERT_NE(FindMatrix3DChannel(animation), nullptr);
  auto* target = FindLayerById(doc, ChannelTarget(animation, "matrix3D"));
  ASSERT_NE(target, nullptr);
  EXPECT_TRUE(NearlyEqual(target->anchor.x, 40.0f));
  EXPECT_TRUE(NearlyEqual(target->anchor.y, 30.0f));
  EXPECT_FALSE(HasDiagnosticContaining(doc, "transform-origin"));
}

/**
 * Test case: a `perspective` declared on the parent projects the child pose. PAGX keeps the
 * projection on the layer that owns the pose, so the importer pushes `P` into the child matrix3D
 * while the pivot stays on the child `anchor` — exactly the CSS "parent camera + child origin"
 * order (and the AE / Lottie one).
 */
PAG_TEST(PAGXHTML3DTest, ParentPerspectiveProjectsTheChildPose) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:160px">
      <div style="position:absolute;left:40px;top:40px;width:120px;height:80px;
           perspective:600px">
        <div id="card" style="width:120px;height:80px;background-color:#38BDF8;
             transform:rotateY(30deg);transform-origin:0px 40px"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* card = FindLayerById(doc, "card");
  ASSERT_NE(card, nullptr);
  const auto& pose = card->matrix3D;
  EXPECT_FALSE(HasDiagnosticContaining(doc, "vanishing point"));
  EXPECT_EQ(LayersWithNonIdentityMatrix3D(doc).size(), 2u);
  EXPECT_TRUE(NearlyEqual(pose.getRowColumn(0, 0), 0.866025f, 0.001f));
  EXPECT_TRUE(NearlyEqual(pose.getRowColumn(0, 2), 0.5f, 0.001f));
  EXPECT_TRUE(NearlyEqual(pose.getRowColumn(3, 0), 0.0f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(pose.getRowColumn(3, 2), 0.0f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(card->anchor.x, 0.0f));
  EXPECT_TRUE(NearlyEqual(card->anchor.y, 40.0f));
}

/**
 * Test case: an authored `perspective-origin` moves the vanishing point, so it changes where the
 * projection is conjugated. At `0 0` the pose is the plain `P * R` (no centre conjugation).
 */
PAG_TEST(PAGXHTML3DTest, AuthoredPerspectiveOriginMovesTheVanishingPoint) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:160px">
      <div style="position:absolute;left:40px;top:40px;width:120px;height:80px;
           perspective:600px;perspective-origin:0px 0px">
        <div id="card" style="width:120px;height:80px;background-color:#38BDF8;
             transform:rotateY(30deg);transform-origin:0px 40px"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* card = FindLayerById(doc, "card");
  ASSERT_NE(card, nullptr);
  EXPECT_TRUE(NearlyEqual(card->matrix3D.getRowColumn(0, 0), 0.866025f, 0.001f));
  EXPECT_TRUE(NearlyEqual(card->matrix3D.getRowColumn(0, 2), 0.5f, 0.001f));
  EXPECT_FALSE(HasProjectionRow(card))
      << "an unresolvable vanishing point cannot be applied, and must not be folded either";
}

/**
 * Test case: the parent `perspective` also projects animated keyframes, and the pivot stays on the
 * child layer so the interpolation never carries a pivot translation. Only the stops that leave the
 * z = 0 plane are projected; a flat stop keeps its own pose.
 */
PAG_TEST(PAGXHTML3DTest, ParentPerspectiveProjectsAnimatedKeyframes) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes flip {
        0%   { transform: rotateY(0deg); }
        100% { transform: rotateY(60deg); }
      }
    </style></head>
    <body style="width:200px;height:160px">
      <div style="position:absolute;left:40px;top:40px;width:120px;height:80px;
           perspective:600px">
        <div id="card" style="width:120px;height:80px;background-color:#38BDF8;
             transform-origin:0px 40px;animation:flip 1s linear infinite"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  auto* channel = FindMatrix3DChannel(animation);
  ASSERT_NE(channel, nullptr);
  ASSERT_EQ(channel->keyframes.size(), 2u);
  const auto& start = channel->keyframes.front().value;
  EXPECT_TRUE(NearlyEqual(start.getRowColumn(3, 2), 0.0f, 1e-5f))
      << "the flat `0%` stop must keep its own pose, not gain the parent projection";
  EXPECT_TRUE(NearlyEqual(start.getRowColumn(0, 3), 0.0f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(start.getRowColumn(2, 3), 0.0f, 1e-5f));
  const auto& end = channel->keyframes.back().value;
  EXPECT_TRUE(NearlyEqual(end.getRowColumn(3, 0), 0.0f, 1e-5f))
      << "the camera lives on the wrapper, not in the keyframes";
  EXPECT_TRUE(NearlyEqual(end.getRowColumn(3, 2), 0.0f, 1e-5f));
  auto* target = FindLayerById(doc, ChannelTarget(animation, "matrix3D"));
  ASSERT_NE(target, nullptr);
  EXPECT_TRUE(NearlyEqual(target->anchor.y, 40.0f));
}

/**
 * Test case: the pivot / camera reference box is resolved once, so the sizing diagnostics of an
 * element are reported once instead of twice (the reference size used to re-run `parseBoxSizing`).
 */
PAG_TEST(PAGXHTML3DTest, ReferenceBoxResolutionDoesNotDuplicateDiagnostics) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div id="card" style="position:absolute;left:10px;top:10px;width:80px;height:40px;
           box-sizing:content-box;min-width:10px;background-color:#38BDF8;
           transform:rotate(15deg);transform-origin:0px 20px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  int boxSizingWarnings = 0;
  int minWidthWarnings = 0;
  for (const auto& message : doc->errors) {
    if (message.find("box-sizing") != std::string::npos) boxSizingWarnings++;
    if (message.find("min-width") != std::string::npos) minWidthWarnings++;
  }
  EXPECT_EQ(boxSizingWarnings, 1);
  EXPECT_EQ(minWidthWarnings, 1);
}

/**
 * Test case: a tiny uniform scale under a perspective parent stays on the flat path. The pose never
 * leaves the z = 0 plane, so the camera cannot change it and the authored scale reaches the 2D
 * `matrix` untouched; a fixed determinant threshold would read `scale(0.001)` as singular and drop
 * the whole transform.
 */
PAG_TEST(PAGXHTML3DTest, TinyScaleUnderPerspectiveStaysOnTheFlatPath) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div style="position:absolute;left:0;top:0;width:200px;height:120px;perspective:600px">
        <div id="tiny" style="position:absolute;left:10px;top:10px;width:80px;height:40px;
             background-color:#38BDF8;transform:scale(0.001)"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(doc, "non-invertible"));
  auto* tiny = FindLayerById(doc, "tiny");
  ASSERT_NE(tiny, nullptr);
  EXPECT_TRUE(tiny->matrix3D.isIdentity()) << "a flat pose must not become 3D";
  EXPECT_TRUE(NearlyEqual(tiny->matrix.a, 0.001f, 1e-6f));
  EXPECT_TRUE(NearlyEqual(tiny->matrix.d, 0.001f, 1e-6f));
}

/**
 * Test case: a plain flow child of a `perspective` flex container keeps no diagnostic, because the
 * camera cannot change a pose that stays on the z = 0 plane; warning there would drown the signal.
 */
PAG_TEST(PAGXHTML3DTest, PlainFlowChildUnderPerspectiveDoesNotWarn) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div style="position:absolute;left:10px;top:10px;width:180px;height:100px;display:flex;
           perspective:600px">
        <div style="width:60px;height:40px;background-color:#38BDF8"></div>
        <div style="width:60px;height:40px;background-color:#F472B6"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(doc, "vanishing point"));
}

/**
 * Test case: a pivot depth alone does not take a pose off the z = 0 plane — `T(a) * L * T(-a)` maps
 * that plane onto itself for a flat `L`, and the projection divides by a z that is zero there. A 2D
 * animation with a 3D `transform-origin` therefore must not report a dropped projection, while a
 * static `rotateY` on the same pivot still does.
 */
PAG_TEST(PAGXHTML3DTest, PivotDepthAloneDoesNotLeaveTheLayerPlane) {
  auto animated = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes spin { from { transform: rotate(0deg); } to { transform: rotate(40deg); } }
    </style></head>
    <body style="width:300px;height:120px">
      <div style="position:absolute;left:10px;top:10px;width:200px;height:100px;display:flex;
           perspective:600px">
        <div style="width:100px;height:60px;background-color:#38BDF8;
             transform-origin:50% 50% 30px;animation:spin 1s linear infinite"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(animated, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(animated, "vanishing point"))
      << "a flat animation stays on the z = 0 plane whatever the pivot depth";

  auto state = ParseHtml(R"HTML(
    <html><body style="width:300px;height:120px">
      <div style="position:absolute;left:10px;top:10px;width:200px;height:100px;display:flex;
           perspective:600px">
        <div style="width:100px;height:60px;background-color:#38BDF8;
             transform-origin:50% 50% 30px;transform:rotateY(40deg)"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(state, nullptr);
  int warnings = 0;
  for (const auto& message : state->errors) {
    if (message.find("vanishing point") != std::string::npos) {
      warnings++;
    }
  }
  EXPECT_EQ(warnings, 1) << "a static 3D pose under a flow parent still reports the dropped camera";
}

/**
 * Test case: a flow / flex child has no resolved offset from the parent's box, so the parent camera
 * cannot be projected around the exact vanishing point. That has to be reported once per document
 * instead of silently shifting the centre of every such child.
 */
PAG_TEST(PAGXHTML3DTest, FlowChildrenUnderPerspectiveWarnOnce) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div style="position:absolute;left:10px;top:10px;width:180px;height:100px;display:flex;
           perspective:600px">
        <div id="a" style="width:60px;height:40px;background-color:#38BDF8;
             transform:rotateY(20deg)"></div>
        <div id="b" style="width:60px;height:40px;background-color:#F472B6;
             transform:rotateY(20deg)"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  int warnings = 0;
  for (const auto& message : doc->errors) {
    if (message.find("vanishing point") != std::string::npos) {
      warnings++;
    }
  }
  EXPECT_EQ(warnings, 1) << "a flex container with many children must not repeat the diagnostic";
  // A flow child cannot be wrapped, so the projection is dropped rather than folded elsewhere.
  auto* a = FindLayerById(doc, "a");
  ASSERT_NE(a, nullptr);
  EXPECT_TRUE(NearlyEqual(a->matrix3D.getRowColumn(3, 2), 0.0f, 1e-5f));
}

/**
 * Test case: a `perspective` element whose own box has no px size (percentage / auto) cannot resolve
 * the default `perspective-origin` (the box centre). The projection cannot be placed, so it is
 * dropped for the children and the diagnostic has to say that instead of promising a fallback.
 */
PAG_TEST(PAGXHTML3DTest, UnknownCameraBoxSizeDropsTheProjectionWithADiagnostic) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:160px">
      <div style="position:absolute;left:0;top:0;width:100%;height:100%;perspective:600px">
        <div id="card" style="position:absolute;left:10px;top:10px;width:80px;height:40px;
             background-color:#38BDF8;transform:rotateY(30deg)"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "vanishing point"));
  EXPECT_TRUE(HasDiagnosticContaining(doc, "perspective-origin"));
  EXPECT_TRUE(HasDiagnosticContaining(doc, "not applied"));
  EXPECT_FALSE(HasDiagnosticContaining(doc, "falls back"))
      << "the drop must be reported, not a fallback that never happens";
  // No vanishing point means no camera: the pose stays unprojected.
  auto* card = FindLayerById(doc, "card");
  ASSERT_NE(card, nullptr);
  EXPECT_FALSE(HasProjectionRow(card))
      << "an unknown vanishing point is neither applied nor folded";
  EXPECT_TRUE(NearlyEqual(card->matrix3D.getRowColumn(0, 0), 0.866025f, 0.001f));
}

PAG_TEST(PAGXHTML3DTest, FlowPreserve3DLayerKeepsParentCamera) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="position:absolute;left:0;top:0;width:200px;height:200px;
           perspective:100px;perspective-origin:0px 0px">
        <div id="stage" style="width:200px;height:200px;transform-style:preserve-3d">
          <div id="card" style="position:absolute;left:0;top:0;width:50px;height:30px;
               background-color:#ff0000;transform:translateZ(50px)"></div>
        </div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(doc, "not projected"));
  const auto pixels = RenderFrame(doc);
  ASSERT_FALSE(pixels.empty());
  EXPECT_EQ(CountVisiblePixels(pixels), 100u * 60u);
}

PAG_TEST(PAGXHTML3DTest, FlowCameraPreservesPaddingPercentagesAndNonzeroOrigin) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="position:absolute;left:10px;top:10px;width:180px;height:160px;
           padding:10px 5px;perspective:100px;perspective-origin:50px 30px">
        <div id="stage" style="width:100%;height:100%;transform-style:preserve-3d">
          <div style="position:absolute;left:45px;top:20px;width:20px;height:10px;
               background-color:#ff0000;transform:translateZ(50px)"></div>
        </div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* stage = FindLayerById(doc, "stage");
  ASSERT_NE(stage, nullptr);
  doc->applyLayout();
  EXPECT_NEAR(stage->layoutBounds().width, 170.0f, 0.001f);
  EXPECT_NEAR(stage->layoutBounds().height, 140.0f, 0.001f);
  const auto pixels = RenderFrame(doc);
  ASSERT_FALSE(pixels.empty());
  EXPECT_EQ(CountVisiblePixels(pixels), 40u * 20u);
  const size_t inside = (50u * PAGX_TEST_DOC_SIZE + 70u) * 4u;
  const size_t outside = (35u * PAGX_TEST_DOC_SIZE + 70u) * 4u;
  EXPECT_EQ(pixels[inside], 255);
  EXPECT_EQ(pixels[inside + 3], 255);
  EXPECT_EQ(pixels[outside + 3], 0);
}

PAG_TEST(PAGXHTML3DTest, FlowCameraDoesNotMergeSibling3DContexts) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="position:absolute;left:0;top:0;width:200px;height:200px;
           perspective:100px;perspective-origin:0px 0px">
        <div style="width:100px;height:0px;transform-style:preserve-3d">
          <div style="position:absolute;left:0;top:0;width:30px;height:20px;
               background-color:#ff0000;transform:translateZ(50px)"></div>
        </div>
        <div style="width:100px;height:0px;transform-style:preserve-3d">
          <div style="position:absolute;left:15px;top:5px;width:30px;height:20px;
               background-color:#0000ff"></div>
        </div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  const auto pixels = RenderFrame(doc);
  ASSERT_FALSE(pixels.empty());
  const size_t overlap = (10u * PAGX_TEST_DOC_SIZE + 20u) * 4u;
  EXPECT_EQ(pixels[overlap], 0);
  EXPECT_EQ(pixels[overlap + 2], 255);
  EXPECT_EQ(pixels[overlap + 3], 255);
}

PAG_TEST(PAGXHTML3DTest, FlowCameraRespectsFlatIntermediateLayer) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="position:absolute;left:0;top:0;width:200px;height:200px;
           perspective:100px;perspective-origin:0px 0px">
        <div style="width:200px;height:200px">
          <div style="position:absolute;left:0;top:0;width:50px;height:30px;
               background-color:#ff0000;transform:translateZ(50px)"></div>
        </div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  const auto pixels = RenderFrame(doc);
  ASSERT_FALSE(pixels.empty());
  EXPECT_EQ(CountVisiblePixels(pixels), 50u * 30u);
}

PAG_TEST(PAGXHTML3DTest, FlowCameraKeepsFractionalPercentageSizingAfterRelayout) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:200px">
      <div id="parent" style="position:absolute;left:0;top:0;width:101.1px;height:101.1px;
           perspective:100px;perspective-origin:0px 0px">
        <div id="stage" style="width:75%;height:75%;transform-style:preserve-3d">
          <div style="position:absolute;left:0;top:0;width:20px;height:10px;
               background-color:#ff0000;transform:translateZ(50px)"></div>
        </div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* parent = FindLayerById(doc, "parent");
  auto* stage = FindLayerById(doc, "stage");
  ASSERT_NE(parent, nullptr);
  ASSERT_NE(stage, nullptr);
  doc->applyLayout();
  EXPECT_NEAR(stage->layoutBounds().width, 76.0f, 0.001f);
  EXPECT_NEAR(stage->layoutBounds().height, 76.0f, 0.001f);
  parent->width = 105.1f;
  parent->height = 105.1f;
  doc->applyLayout();
  EXPECT_NEAR(stage->layoutBounds().width, 79.0f, 0.001f);
  EXPECT_NEAR(stage->layoutBounds().height, 79.0f, 0.001f);
  EXPECT_EQ(CountVisiblePixels(RenderFrame(doc)), 40u * 20u);
}

PAG_TEST(PAGXHTML3DTest, FlowCameraKeepsDepthThroughMarginLayoutWrapper) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="position:absolute;left:0;top:0;width:200px;height:200px;
           perspective:100px;perspective-origin:0px 0px">
        <div style="width:100px;height:60px;margin:1px;transform-style:preserve-3d">
          <div style="position:absolute;left:0;top:0;width:50px;height:30px;
               background-color:#ff0000;transform:translateZ(50px)"></div>
        </div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(doc, "not projected"));
  const auto pixels = RenderFrame(doc);
  ASSERT_FALSE(pixels.empty());
  EXPECT_EQ(CountVisiblePixels(pixels), 100u * 60u);
  const size_t inside = (3u * PAGX_TEST_DOC_SIZE + 3u) * 4u;
  EXPECT_EQ(pixels[inside], 255);
  EXPECT_EQ(pixels[inside + 3], 255);
  EXPECT_EQ(pixels[3], 0);
}

PAG_TEST(PAGXHTML3DTest, FlowCameraStaysFixedOutsideOffsetFlexAnimation) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes depth { from { transform:translateZ(0px); } to { transform:translateZ(50px); } }
    </style></head><body style="width:200px;height:200px">
      <div style="position:absolute;left:0;top:0;width:200px;height:100px;
           perspective:100px;perspective-origin:50px 0px">
        <div style="display:flex;gap:20px;width:200px;height:100px;transform-style:preserve-3d">
          <div style="width:40px;height:10px"></div>
          <div id="card" style="width:20px;height:10px;background-color:#ff0000;
               animation:depth 1s linear 1"></div>
        </div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto rendered = RoundTripForRender(doc);
  ASSERT_NE(rendered, nullptr);
  ASSERT_NE(rendered->findNode("card"), nullptr);
  const auto middle = RenderFrame(rendered, 500000);
  ASSERT_FALSE(middle.empty());
  const size_t projected = (5u * PAGX_TEST_DOC_SIZE + 88u) * 4u;
  const size_t unprojected = (5u * PAGX_TEST_DOC_SIZE + 61u) * 4u;
  EXPECT_EQ(middle[projected], 255);
  EXPECT_EQ(middle[projected + 3], 255);
  EXPECT_EQ(middle[unprojected + 3], 0);
  const auto end = RenderFrame(rendered, 1000000);
  ASSERT_FALSE(end.empty());
  EXPECT_EQ(CountVisiblePixels(end), 40u * 20u);
}

PAG_TEST(PAGXHTML3DTest, FlowCameraKeepsIntrinsicSizeAfterContentChange) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="position:absolute;left:0;top:0;width:200px;height:200px;
           perspective:100px;perspective-origin:0px 0px">
        <div id="stage" style="display:flex;transform-style:preserve-3d">
          <div id="card" style="width:20px;height:10px;background-color:#ff0000;
               transform:translateZ(50px)"></div>
        </div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* stage = FindLayerById(doc, "stage");
  auto* card = FindLayerById(doc, "card");
  ASSERT_NE(stage, nullptr);
  ASSERT_NE(card, nullptr);
  doc->applyLayout();
  EXPECT_NEAR(stage->layoutBounds().width, 20.0f, 0.001f);
  EXPECT_NEAR(stage->layoutBounds().height, 10.0f, 0.001f);
  card->width = 30.0f;
  card->height = 15.0f;
  doc->applyLayout();
  EXPECT_NEAR(stage->layoutBounds().width, 30.0f, 0.001f);
  EXPECT_NEAR(stage->layoutBounds().height, 15.0f, 0.001f);
  const auto pixels = RenderFrame(doc);
  ASSERT_FALSE(pixels.empty());
  EXPECT_EQ(CountVisiblePixels(pixels), 60u * 30u);
}

PAG_TEST(PAGXHTML3DTest, FlowCameraDoesNotOpenAnOverflowFlatteningBoundary) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="position:absolute;left:0;top:0;width:200px;height:200px;
           perspective:100px;perspective-origin:0px 0px">
        <div style="width:80px;height:60px;transform-style:preserve-3d;overflow:hidden">
          <div style="position:absolute;left:0;top:0;width:50px;height:30px;
               background-color:#ff0000;transform:translateZ(50px)"></div>
        </div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  const auto pixels = RenderFrame(doc);
  ASSERT_FALSE(pixels.empty());
  EXPECT_EQ(CountVisiblePixels(pixels), 50u * 30u);
}

PAG_TEST(PAGXHTML3DTest, FlowCameraRejectsAnUnrepresentableProjectionMatrix) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="position:absolute;left:0;top:0;width:200px;height:200px;
           perspective:100px;perspective-origin:1000000000000px 0px">
        <div style="width:100px;height:60px;transform-style:preserve-3d">
          <div style="position:absolute;left:0;top:0;width:20px;height:10px;
               background-color:#ff0000;transform:translateZ(50px)"></div>
        </div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "camera matrix"));
  auto serialized = RoundTripForRender(doc);
  ASSERT_NE(serialized, nullptr);
  EXPECT_EQ(CountVisiblePixels(RenderFrame(serialized)), 20u * 10u);
}

/**
 * Test case: an intermediate flow layer retains its parent camera while its own flex layout
 * continues to place the descendants independently of their transforms.
 */
PAG_TEST(PAGXHTML3DTest, Preserve3DFlowLayerKeepsCameraForFlexDescendants) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:300px;height:150px">
      <div style="position:absolute;left:10px;top:10px;width:200px;height:100px;
           perspective:600px">
        <div style="display:flex;width:200px;height:100px;transform-style:preserve-3d">
          <div id="g" style="width:100px;height:60px;background-color:#38BDF8;
               transform:rotateY(40deg)"></div>
        </div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  int warnings = 0;
  for (const auto& message : doc->errors) {
    if (message.find("preserve-3d layer") != std::string::npos) {
      warnings++;
    }
  }
  EXPECT_EQ(warnings, 0);
  EXPECT_EQ(LayersWithNonIdentityMatrix3D(doc).size(), 2u);
  auto* g = FindLayerById(doc, "g");
  ASSERT_NE(g, nullptr);
  EXPECT_FALSE(HasProjectionRow(g)) << "the camera is separate from the animated element";
  EXPECT_TRUE(NearlyEqual(g->matrix3D.getRowColumn(0, 0), 0.766044f, 0.001f));

  // The same structure with only flat poses below the preserve-3d layer keeps the camera irrelevant.
  auto flat = ParseHtml(R"HTML(
    <html><body style="width:300px;height:150px">
      <div style="position:absolute;left:10px;top:10px;width:200px;height:100px;
           perspective:600px">
        <div style="display:flex;width:200px;height:100px;transform-style:preserve-3d">
          <div style="width:100px;height:60px;background-color:#38BDF8;transform:rotate(20deg)"></div>
        </div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(flat, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(flat, "preserve-3d layer"));
}

/**
 * Test case: a `data-pagx-text` host is converted outside `convertElement`, so the parent camera has
 * to be folded in explicitly; otherwise a 3D transform on a restored text host loses its projection.
 */
PAG_TEST(PAGXHTML3DTest, PAGXTextHostKeepsParentPerspective) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div style="position:absolute;left:0;top:0;width:200px;height:120px;perspective:600px">
        <div data-pagx-text="Kept" data-pagx-font-family="CustomFont" data-pagx-font-style=""
             data-pagx-faux-bold="0" data-pagx-faux-italic="0"
             style="position:absolute;left:10px;top:20px;width:80px;height:40px;font-size:16px;
                    transform:rotateY(25deg)"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(doc, "vanishing point"));
  // The parent camera is carried by exactly one wrapper layer; the restored text host keeps its own
  // (unprojected) pose inside it, so the camera cannot drift with the host's animation.
  auto cameras = CameraLayers(doc);
  ASSERT_EQ(cameras.size(), 1u) << "the parent camera must live on a single wrapper layer";
  EXPECT_TRUE(NearlyEqual(cameras.front()->perspective, 600.0f, 0.01f));
  auto layers = LayersWithNonIdentityMatrix3D(doc);
  ASSERT_FALSE(layers.empty()) << "the restored text host must carry its 3D pose";
  for (auto* layer : layers) {
    EXPECT_FALSE(HasProjectionRow(layer)) << "the camera must not be folded into the host pose";
  }
}

/**
 * Test case: a `transform-style: preserve-3d` layer without a transform of its own still has to
 * carry the camera its parent declared. CSS keeps the grandchildren in the same 3D space, so the
 * projection has to be materialised on the intermediate layer; without it the camera chain ends
 * there and every descendant pose is rendered orthographically, with no diagnostic.
 */
PAG_TEST(PAGXHTML3DTest, Preserve3DWithoutOwnTransformCarriesTheParentCamera) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div style="position:absolute;left:0;top:0;width:200px;height:120px;perspective:600px;
           perspective-origin:0px 0px">
        <div id="stage" style="position:absolute;left:0;top:0;width:200px;height:120px;
             transform-style:preserve-3d">
          <div id="card" style="position:absolute;left:20px;top:20px;width:80px;height:60px;
               background-color:#38BDF8;transform:rotateY(45deg);transform-origin:0px 30px"></div>
        </div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(doc, "vanishing point"));
  auto* stage = FindLayerById(doc, "stage");
  auto* card = FindLayerById(doc, "card");
  ASSERT_NE(stage, nullptr);
  ASSERT_NE(card, nullptr);
  ASSERT_TRUE(stage->preserve3D);
  // The camera is a static property of the wrapper that sits above the intermediate layer, so it is
  // never interpolated with a pose and needs no materialisation on the layer itself.
  auto* wrapper = CameraWrapperOf(doc, stage);
  ASSERT_NE(wrapper, nullptr) << "the parent camera must wrap the intermediate layer";
  EXPECT_TRUE(NearlyEqual(wrapper->perspective, 600.0f, 0.01f));
  EXPECT_TRUE(NearlyEqual(wrapper->left, 0.0f, 1e-4f)) << "perspective-origin is the parent corner";
  EXPECT_TRUE(NearlyEqual(wrapper->top, 0.0f, 1e-4f));
  EXPECT_TRUE(stage->matrix3D.isIdentity()) << "the layer keeps its own (absent) pose";
  // The pivot is skipped with a pose that does not exist, so nothing claims an anchor.
  EXPECT_TRUE(HasZeroAnchor(stage));
  // The card keeps its own pose and stays inside the camera wrapper's 3D context.
  EXPECT_TRUE(NearlyEqual(card->matrix3D.getRowColumn(0, 0), 0.707106f, 0.001f));
  EXPECT_TRUE(NearlyEqual(card->matrix3D.getRowColumn(2, 0), -0.707106f, 0.001f));
  EXPECT_FALSE(HasProjectionRow(card)) << "the camera must not be folded into the grandchild pose";
  ASSERT_EQ(wrapper->children.size(), 1u) << "one wrapper carries exactly the projected layer";
  EXPECT_EQ(wrapper->children.front(), stage);
}

/**
 * Test case: a pure XY animation on a `preserve-3d` layer splits it into a layout outer plus an
 * animated inner. The camera lives on a wrapper above the outer, so neither surface may drop
 * `preserve3D`: flattening anywhere on the way down loses the projection of a `translateZ` grandchild
 * (`perspective: 100px` + `translateZ(50px)` must still scale it up by 2).
 */
PAG_TEST(PAGXHTML3DTest, SplitForTranslateAnimationKeepsThe3DSpace) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes slide { 0% { transform: translateX(0px); } 100% { transform: translateX(20px); } }
    </style></head>
    <body style="width:300px;height:150px">
      <div style="position:absolute;left:20px;top:20px;width:200px;height:100px;perspective:100px">
        <div id="mid" style="position:absolute;left:0;top:0;width:100px;height:60px;
             transform-style:preserve-3d;animation:slide 1s linear infinite">
          <div id="deep" style="position:absolute;left:0;top:0;width:100px;height:60px;
               background-color:#38BDF8;transform:translateZ(50px)"></div>
        </div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* mid = FindLayerById(doc, "mid");
  ASSERT_NE(mid, nullptr);
  auto* deep = FindLayerById(doc, "deep");
  ASSERT_NE(deep, nullptr);
  auto* wrapper = CameraWrapperOf(doc, mid);
  ASSERT_NE(wrapper, nullptr) << "the parent camera must wrap the split outer layer";
  EXPECT_TRUE(NearlyEqual(wrapper->perspective, 100.0f, 0.01f));
  EXPECT_TRUE(wrapper->preserve3D);
  // The layout outer no longer owns a pose, but it still sits between the camera and the animated
  // inner, so it has to keep the 3D space open instead of flattening the subtree into its plane.
  EXPECT_TRUE(mid->preserve3D) << "the split outer must keep the camera's 3D space";
  EXPECT_TRUE(mid->matrix3D.isIdentity()) << "the pose stays on the animated inner layer";
  ASSERT_EQ(mid->children.size(), 1u);
  auto* inner = mid->children.front();
  EXPECT_TRUE(inner->preserve3D) << "the animated inner keeps the space down to its children";
  EXPECT_TRUE(NearlyEqual(deep->matrix3D.getRowColumn(2, 3), 50.0f, 0.001f));
  EXPECT_FALSE(HasProjectionRow(deep)) << "the projection belongs to the wrapper, not the child";
}

/**
 * Test case: `perspective-origin` still conjugates the camera materialised on a `preserve-3d` layer.
 * The default origin is the parent box's centre, so the projection picks up the shear that moves the
 * vanishing point away from the layer origin.
 */
PAG_TEST(PAGXHTML3DTest, MaterializedCameraConjugatesPerspectiveOrigin) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div style="position:absolute;left:0;top:0;width:200px;height:120px;perspective:600px">
        <div id="stage" style="position:absolute;left:0;top:0;width:200px;height:120px;
             transform-style:preserve-3d">
          <div id="card" style="position:absolute;left:20px;top:20px;width:80px;height:60px;
               background-color:#38BDF8;transform:rotateY(45deg)"></div>
        </div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* stage = FindLayerById(doc, "stage");
  ASSERT_NE(stage, nullptr);
  // `perspective-origin` is where the wrapper sits: PAGX projects about the layer origin, so the
  // wrapper is translated to the parent box centre (100, 60) and the child is offset by `-o`.
  auto* wrapper = CameraWrapperOf(doc, stage);
  ASSERT_NE(wrapper, nullptr);
  EXPECT_TRUE(NearlyEqual(wrapper->perspective, 600.0f, 0.01f));
  EXPECT_TRUE(NearlyEqual(wrapper->left, 100.0f, 1e-4f));
  EXPECT_TRUE(NearlyEqual(wrapper->top, 60.0f, 1e-4f));
  EXPECT_TRUE(NearlyEqual(stage->left, -100.0f, 1e-4f)) << "the child is offset back by the origin";
  EXPECT_TRUE(NearlyEqual(stage->top, -60.0f, 1e-4f));
  EXPECT_TRUE(stage->matrix3D.isIdentity()) << "the pose stays unprojected";
}

/**
 * Test case: moving the child into the camera wrapper must not change its parent-facing layout.
 * The wrapper sits at `perspective-origin` (100, 50 here), so the child is re-expressed in a
 * container translated by `-origin`: the near edges move by `-origin`, the far edges by `+origin`.
 * A child pinned to all four edges (`inset: 0`) used to keep `right` / `bottom` at 0 while only
 * `left` / `top` moved, which stretched the 200x100 span to 300x150.
 */
PAG_TEST(PAGXHTML3DTest, CameraWrapperKeepsTwoEdgeConstraintsEquivalent) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div style="position:absolute;left:0;top:0;width:200px;height:100px;perspective:600px">
        <div id="fill" style="position:absolute;left:0;right:0;top:0;bottom:0;
             background-color:#38BDF8"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* fill = FindLayerById(doc, "fill");
  ASSERT_NE(fill, nullptr);
  auto* wrapper = CameraWrapperOf(doc, fill);
  ASSERT_NE(wrapper, nullptr);
  EXPECT_TRUE(NearlyEqual(wrapper->left, 100.0f, 1e-4f));
  EXPECT_TRUE(NearlyEqual(wrapper->top, 50.0f, 1e-4f));
  EXPECT_TRUE(NearlyEqual(wrapper->width, 200.0f, 1e-4f));
  EXPECT_TRUE(NearlyEqual(wrapper->height, 100.0f, 1e-4f));
  EXPECT_TRUE(NearlyEqual(fill->left, -100.0f, 1e-4f));
  EXPECT_TRUE(NearlyEqual(fill->right, 100.0f, 1e-4f));
  EXPECT_TRUE(NearlyEqual(fill->top, -50.0f, 1e-4f));
  EXPECT_TRUE(NearlyEqual(fill->bottom, 50.0f, 1e-4f));
  EXPECT_TRUE(std::isnan(fill->centerX)) << "a translation must not invent a centre constraint";
  EXPECT_TRUE(std::isnan(fill->centerY));

  doc->applyLayout();
  EXPECT_FLOAT_EQ(fill->layoutWidth, 200.0f)
      << "the two-edge span must keep the parent's width, not grow to 300px";
  EXPECT_FLOAT_EQ(fill->layoutHeight, 100.0f)
      << "the two-edge span must keep the parent's height, not grow to 150px";
  // Wrapper space -> parent space: the pinned rect still covers the parent box exactly.
  EXPECT_TRUE(NearlyEqual(wrapper->renderPosition().x + fill->renderPosition().x, 0.0f, 1e-4f));
  EXPECT_TRUE(NearlyEqual(wrapper->renderPosition().y + fill->renderPosition().y, 0.0f, 1e-4f));
}

/**
 * Test case: a centre-anchored child is translated with the wrapper too. The
 * `data-pagx-intrinsic-width="center"` line is the import path that expresses a horizontal anchor
 * as `centerX`; it used to keep `centerX = 0` while the wrapper moved to the origin, which drifted
 * the line by `origin.x` (half the parent box). `centerY` has no producer in this importer, so it
 * must stay NaN instead of gaining a default from the translation.
 */
PAG_TEST(PAGXHTML3DTest, CameraWrapperKeepsCenterConstraintEquivalent) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div style="position:absolute;left:0;top:0;width:200px;height:100px;perspective:600px">
        <span id="line" data-pagx-intrinsic-width="center"
              style="position:absolute;left:60px;top:20px;width:80px;height:20px;font-size:13px;
                     line-height:20px;text-align:center;white-space:nowrap">Center</span>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* line = FindLayerById(doc, "line");
  ASSERT_NE(line, nullptr);
  auto* wrapper = CameraWrapperOf(doc, line);
  ASSERT_NE(wrapper, nullptr);
  EXPECT_TRUE(NearlyEqual(wrapper->left, 100.0f, 1e-4f));
  EXPECT_TRUE(NearlyEqual(wrapper->top, 50.0f, 1e-4f));
  EXPECT_TRUE(NearlyEqual(line->centerX, -100.0f, 1e-4f))
      << "the centre offset must move with the wrapper origin";
  EXPECT_TRUE(std::isnan(line->centerY)) << "a centre anchor the author never wrote stays absent";
  EXPECT_TRUE(std::isnan(line->left)) << "the marker's cleared edge anchors stay absent";
  EXPECT_TRUE(std::isnan(line->right));
  EXPECT_TRUE(std::isnan(line->bottom));
  EXPECT_TRUE(NearlyEqual(line->top, -30.0f, 1e-4f));

  doc->applyLayout();
  const auto bounds = line->layoutBounds();
  const float centerX = wrapper->renderPosition().x + bounds.x + bounds.width * 0.5f;
  // Text shaping is sub-pixel, so the line box may shift a fraction of a pixel while staying
  // centred in the parent box.
  EXPECT_TRUE(NearlyEqual(centerX, 100.0f, 0.5f))
      << "the centred line must stay on the parent's centre, not drift by the origin";
}

/**
 * Test case: without `transform-style: preserve-3d` the intermediate layer flattens its subtree, so
 * the parent camera stops there. That is the documented CSS behaviour of a plain 2D layer and must
 * not become a three-level camera push.
 */
PAG_TEST(PAGXHTML3DTest, PlainIntermediateLayerEndsTheCameraChain) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div style="position:absolute;left:0;top:0;width:200px;height:120px;perspective:600px;
           perspective-origin:0px 0px">
        <div id="stage" style="position:absolute;left:0;top:0;width:200px;height:120px">
          <div id="card" style="position:absolute;left:20px;top:20px;width:80px;height:60px;
               background-color:#38BDF8;transform:rotateY(45deg)"></div>
        </div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* stage = FindLayerById(doc, "stage");
  auto* card = FindLayerById(doc, "card");
  ASSERT_NE(stage, nullptr);
  ASSERT_NE(card, nullptr);
  EXPECT_FALSE(stage->preserve3D);
  EXPECT_TRUE(stage->matrix3D.isIdentity());
  EXPECT_TRUE(stage->matrix.isIdentity());
  // The card keeps its own 3D pose but is no longer projected by the ancestor camera.
  EXPECT_TRUE(NearlyEqual(card->matrix3D.getRowColumn(0, 0), 0.707106f, 0.001f));
  const auto composed = MultiplyPose(stage->matrix3D, card->matrix3D);
  EXPECT_TRUE(NearlyEqual(composed.getRowColumn(3, 0), 0.0f, 1e-6f));
  EXPECT_TRUE(NearlyEqual(composed.getRowColumn(3, 2), 0.0f, 1e-6f));
}

/**
 * Test case: a `preserve-3d` layer that has its own non-identity flat transform keeps the existing
 * single-fold route. A flat pose is pivoted on the plane, so the camera does not apply and the layer
 * writes no projection — the flat pose itself is the layer's whole transform.
 */
PAG_TEST(PAGXHTML3DTest, Preserve3DWithOwnFlatTransformKeepsTheExistingRoute) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div style="position:absolute;left:0;top:0;width:200px;height:120px;perspective:600px;
           perspective-origin:0px 0px">
        <div id="stage" style="position:absolute;left:0;top:0;width:200px;height:120px;
             transform-style:preserve-3d;transform:translateX(10px)">
          <div id="card" style="position:absolute;left:20px;top:20px;width:80px;height:60px;
               background-color:#38BDF8;transform:rotateY(45deg)"></div>
        </div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* stage = FindLayerById(doc, "stage");
  auto* card = FindLayerById(doc, "card");
  ASSERT_NE(stage, nullptr);
  ASSERT_NE(card, nullptr);
  EXPECT_TRUE(stage->preserve3D);
  EXPECT_TRUE(stage->matrix3D.isIdentity());
  EXPECT_TRUE(NearlyEqual(stage->matrix.tx, 10.0f, 1e-6f));
  const auto composed = MultiplyPose(stage->matrix3D, card->matrix3D);
  EXPECT_TRUE(NearlyEqual(composed.getRowColumn(3, 0), 0.0f, 1e-6f));
  EXPECT_TRUE(NearlyEqual(composed.getRowColumn(3, 2), 0.0f, 1e-6f));
}

/**
 * Test case: a `preserve-3d` layer whose transform resolves to the exact identity declares no pose
 * of its own, so it carries the parent camera exactly like a layer that omits `transform`. `none`, a
 * computed `matrix(1, 0, 0, 1, 0, 0)`, `rotate(0deg)` and `translateZ(0px)` all compose to that
 * exact identity, so they are indistinguishable from each other and from `none`; special-casing the
 * authored function name would split a computed `matrix(...)` from a hand-written component list
 * into two behaviours. A flat but non-identity pose (`scale(0)`, `translateX(10px)`) does not, and
 * keeps the route above.
 */
PAG_TEST(PAGXHTML3DTest, IdentityTransformKeepsTheBarePreserve3DCamera) {
  const char* values[] = {"none", "matrix(1,0,0,1,0,0)", "rotate(0deg)", "translateZ(0px)"};
  for (const char* value : values) {
    std::string html = R"HTML(
      <html><body style="width:200px;height:120px">
        <div style="position:absolute;left:0;top:0;width:200px;height:120px;perspective:600px;
             perspective-origin:0px 0px">
          <div id="stage" style="position:absolute;left:0;top:0;width:200px;height:120px;
               transform-style:preserve-3d;transform:VALUE">
            <div id="card" style="position:absolute;left:20px;top:20px;width:80px;height:60px;
                 background-color:#38BDF8;transform:rotateY(45deg)"></div>
          </div>
        </div>
      </body></html>
    )HTML";
    html.replace(html.find("VALUE"), 5, value);
    auto doc = ParseHtml(html);
    ASSERT_NE(doc, nullptr) << value;
    EXPECT_FALSE(HasDiagnosticContaining(doc, "html: transform")) << value;
    auto* stage = FindLayerById(doc, "stage");
    auto* card = FindLayerById(doc, "card");
    ASSERT_NE(stage, nullptr) << value;
    ASSERT_NE(card, nullptr) << value;
    EXPECT_TRUE(stage->preserve3D) << value;
    EXPECT_TRUE(stage->matrix.isIdentity()) << value;
    // The camera rides on the wrapper above the identity layer, so the layer itself keeps no pose and
    // the projection is applied outside any animation of its children.
    auto* wrapper = CameraWrapperOf(doc, stage);
    ASSERT_NE(wrapper, nullptr) << value;
    EXPECT_TRUE(NearlyEqual(wrapper->perspective, 600.0f, 0.01f)) << value;
    EXPECT_TRUE(NearlyEqual(wrapper->left, 0.0f, 1e-4f)) << value;
    EXPECT_TRUE(NearlyEqual(wrapper->top, 0.0f, 1e-4f)) << value;
    EXPECT_TRUE(stage->matrix3D.isIdentity()) << value;
    // The skipped pivot must not leave a residue behind.
    EXPECT_TRUE(NearlyEqual(stage->anchor.x, 0.0f)) << value;
    EXPECT_TRUE(NearlyEqual(stage->anchor.y, 0.0f)) << value;
    EXPECT_TRUE(NearlyEqual(stage->anchorZ, 0.0f)) << value;
    // The grandchild keeps its own pose.
    EXPECT_TRUE(NearlyEqual(card->matrix3D.getRowColumn(0, 0), 0.70710678f, 0.001f)) << value;
    EXPECT_FALSE(HasProjectionRow(card)) << value;
  }

  // An identity pose is no pose, so a flat but non-identity one must not take the bare camera route:
  // `scale(0)` has to stay a zero linear block, otherwise the invisible-element fix regresses.
  auto scaleZero = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div style="position:absolute;left:0;top:0;width:200px;height:120px;perspective:600px;
           perspective-origin:0px 0px">
        <div id="stage" style="position:absolute;left:0;top:0;width:200px;height:120px;
             transform-style:preserve-3d;transform:scale(0)">
          <div id="card" style="position:absolute;left:20px;top:20px;width:80px;height:60px;
               background-color:#38BDF8;transform:rotateY(45deg)"></div>
        </div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(scaleZero, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(scaleZero, "non-invertible"));
  auto* scaleZeroStage = FindLayerById(scaleZero, "stage");
  ASSERT_NE(scaleZeroStage, nullptr);
  EXPECT_TRUE(scaleZeroStage->matrix3D.isIdentity()) << "must not materialise the bare camera";
  EXPECT_TRUE(NearlyEqual(scaleZeroStage->matrix.a, 0.0f, 1e-6f));
  EXPECT_TRUE(NearlyEqual(scaleZeroStage->matrix.d, 0.0f, 1e-6f));
  EXPECT_NE(CameraWrapperOf(scaleZero, scaleZeroStage), nullptr)
      << "the camera still wraps the layer; only the flat pose stays on the 2D channel";

  // The pivot is skipped with the pose, so a bad `transform-origin` is not reported on a layer that
  // has no transform of its own.
  auto badOrigin = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div id="box" style="width:100px;height:60px;background-color:#000;
           transform:none;transform-origin:10foo 20px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(badOrigin, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(badOrigin, "not a valid CSS position"));
}

/**
 * Test case: `scale(0)` under a perspective parent is a legal flat pose. The camera cannot change a
 * pose that stays on the z = 0 plane, so the pose keeps its 2D matrix (a zero linear block, i.e. the
 * element renders invisible) instead of being folded into a non-invertible 3D pose and dropped,
 * which would restore it to full size.
 */
PAG_TEST(PAGXHTML3DTest, ScaleZeroUnderPerspectiveStaysInvisible) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:160px">
      <div style="position:absolute;left:0;top:0;width:200px;height:160px;perspective:600px">
        <div id="card" style="position:absolute;left:10px;top:10px;width:80px;height:40px;
             background-color:#38BDF8;transform:scale(0)"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(doc, "non-invertible"));
  auto* card = FindLayerById(doc, "card");
  ASSERT_NE(card, nullptr);
  EXPECT_TRUE(card->matrix3D.isIdentity()) << "a flat pose must not become 3D";
  EXPECT_TRUE(NearlyEqual(card->matrix.a, 0.0f, 1e-6f));
  EXPECT_TRUE(NearlyEqual(card->matrix.b, 0.0f, 1e-6f));
  EXPECT_TRUE(NearlyEqual(card->matrix.c, 0.0f, 1e-6f));
  EXPECT_TRUE(NearlyEqual(card->matrix.d, 0.0f, 1e-6f));
}

/**
 * Test case: an inline `<svg>` host carries its CSS 3D transform the same way a plain box does.
 */
PAG_TEST(PAGXHTML3DTest, InlineSvgHostWithPerspectiveEmitsMatrix3D) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <svg id="icon" width="80" height="60" viewBox="0 0 80 60"
           style="transform:perspective(400px) rotateY(20deg)">
        <rect x="4" y="4" width="72" height="52" fill="#6366F1"/>
      </svg>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto layers = LayersWithNonIdentityMatrix3D(doc);
  ASSERT_EQ(layers.size(), 1u) << "exactly the svg host must carry the 3D transform";
  EXPECT_TRUE(NearlyEqual(layers.front()->matrix3D.getRowColumn(3, 2), -0.002349f, 1e-5f));
}

// ---------------------------------------------------------------------------------------------
// 3D animation channels: 3D keyframes need the matrix3D channel, 2D ones must not pay for it.
// ---------------------------------------------------------------------------------------------

/**
 * Test case: a 3D rotation animation without any perspective still routes to `matrix3D`.
 */
PAG_TEST(PAGXHTML3DTest, AnimatedRotateXUsesMatrix3DChannel) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes tilt {
        0%   { transform: rotateX(0deg); }
        100% { transform: rotateX(60deg); }
      }
    </style></head>
    <body style="width:200px;height:200px">
      <div id="card" style="width:100px;height:60px;background-color:#000;
           animation:tilt 1s linear infinite"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  EXPECT_EQ(FindChannel(animation, "matrix"), nullptr) << "3D keys must not collapse to matrix";
  auto* channel = FindMatrix3DChannel(animation);
  ASSERT_NE(channel, nullptr);
  ASSERT_EQ(channel->keyframes.size(), 2u);
  EXPECT_TRUE(NearlyEqual(channel->keyframes.front().value.getRowColumn(1, 1), 1.0f, 0.001f));
  EXPECT_TRUE(NearlyEqual(channel->keyframes.back().value.getRowColumn(1, 1), 0.5f, 0.001f));
  EXPECT_TRUE(NearlyEqual(channel->keyframes.back().value.getRowColumn(2, 1), 0.866025f, 0.001f));
}

/**
 * Test case: `matrix3d` keyframes keep their authored values.
 */
PAG_TEST(PAGXHTML3DTest, AnimatedMatrix3DKeyframesPassThrough) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes turn {
        0%   { transform: matrix3d(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1); }
        100% { transform: matrix3d(0.5,0,0,0, 0,1,0,0, 0,0,0.5,-0.002, 0,0,40,1); }
      }
    </style></head>
    <body style="width:200px;height:200px">
      <div id="card" style="width:100px;height:60px;background-color:#000;
           animation:turn 1s linear infinite"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* channel = FindMatrix3DChannel(FirstAnimation(doc));
  ASSERT_NE(channel, nullptr);
  ASSERT_EQ(channel->keyframes.size(), 2u);
  const auto& last = channel->keyframes.back().value;
  EXPECT_TRUE(NearlyEqual(last.getRowColumn(0, 0), 0.5f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(last.getRowColumn(2, 3), 40.0f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(last.getRowColumn(3, 2), -0.002f, 1e-5f));
}

/**
 * Test case: an in-plane rotation animation keeps the cheap 2D `matrix` channel.
 */
PAG_TEST(PAGXHTML3DTest, AnimatedRotationUsesMatrixChannel) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes spin {
        0%   { transform: rotate(0deg); }
        100% { transform: rotate(90deg); }
      }
    </style></head>
    <body style="width:200px;height:200px">
      <div id="box" style="width:50px;height:50px;background-color:#000;
           animation:spin 1s linear infinite"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  EXPECT_NE(FindMatrixChannel(animation), nullptr);
  EXPECT_EQ(FindChannel(animation, "matrix3D"), nullptr);
}

// ---------------------------------------------------------------------------------------------
// Anchor: the pivot must stay on the layer instead of being baked into the keyframes, because a
// baked pivot translation is interpolated on its own and makes the element drift.
// ---------------------------------------------------------------------------------------------

/**
 * Test case: an animated 2D rotation keeps the pivot on the layer and its keyframes un-pivoted;
 * no structural pivot wrappers are generated.
 */
PAG_TEST(PAGXHTML3DTest, AnimatedRotationKeepsAnchorUnbaked) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes spin {
        0%   { transform: rotate(-30deg); }
        50%  { transform: rotate(30deg); }
        100% { transform: rotate(-30deg); }
      }
    </style></head>
    <body style="width:200px;height:200px">
      <div id="box" style="position:absolute;left:40px;top:40px;width:90px;height:16px;
           background-color:#000;transform-origin:0px 8px;
           animation:spin 2s linear infinite"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(NearlyEqual(layer->anchor.x, 0.0f));
  EXPECT_TRUE(NearlyEqual(layer->anchor.y, 8.0f));
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  EXPECT_EQ(ChannelTarget(animation, "matrix"), "box") << "no pivot wrapper may intercept it";
  EXPECT_TRUE(layer->children.empty()) << "the element must not be re-parented";
  auto* matrixChannel = FindMatrixChannel(animation);
  ASSERT_NE(matrixChannel, nullptr);
  ASSERT_EQ(matrixChannel->keyframes.size(), 3u);
  for (const auto& keyframe : matrixChannel->keyframes) {
    EXPECT_TRUE(NearlyEqual(keyframe.value.tx, 0.0f)) << "keyframes must stay un-pivoted";
    EXPECT_TRUE(NearlyEqual(keyframe.value.ty, 0.0f)) << "keyframes must stay un-pivoted";
  }
}

/**
 * Test case: a general affine animation (scale) about a corner keeps the same contract.
 */
PAG_TEST(PAGXHTML3DTest, AnimatedScaleKeepsAnchorUnbaked) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes breathe {
        0%   { transform: scale(0.4); }
        100% { transform: scale(1.2); }
      }
    </style></head>
    <body style="width:200px;height:200px">
      <div id="box" style="width:90px;height:60px;background-color:#000;
           transform-origin:100% 100%;animation:breathe 2s ease-in-out infinite"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(NearlyEqual(layer->anchor.x, 90.0f));
  EXPECT_TRUE(NearlyEqual(layer->anchor.y, 60.0f));
  auto* matrixChannel = FindMatrixChannel(FirstAnimation(doc));
  ASSERT_NE(matrixChannel, nullptr);
  ASSERT_EQ(matrixChannel->keyframes.size(), 2u);
  for (const auto& keyframe : matrixChannel->keyframes) {
    EXPECT_TRUE(NearlyEqual(keyframe.value.tx, 0.0f));
    EXPECT_TRUE(NearlyEqual(keyframe.value.ty, 0.0f));
  }
  EXPECT_TRUE(NearlyEqual(matrixChannel->keyframes.back().value.a, 1.2f));
  EXPECT_TRUE(NearlyEqual(matrixChannel->keyframes.back().value.d, 1.2f));
}

/**
 * Test case: a corner origin (the `rotate(deg, cx, cy)` authoring form) needs no anchor at all.
 */
PAG_TEST(PAGXHTML3DTest, AnimatedCornerOriginKeepsZeroAnchor) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes spin {
        0%   { transform: rotate(0deg); }
        100% { transform: rotate(180deg); }
      }
    </style></head>
    <body style="width:200px;height:200px">
      <div id="box" style="width:20px;height:20px;background-color:#000;
           transform-origin:0px 0px;animation:spin 1s linear infinite"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(HasZeroAnchor(layer));
  auto* matrixChannel = FindMatrixChannel(FirstAnimation(doc));
  ASSERT_NE(matrixChannel, nullptr);
  for (const auto& keyframe : matrixChannel->keyframes) {
    EXPECT_TRUE(NearlyEqual(keyframe.value.tx, 0.0f));
    EXPECT_TRUE(NearlyEqual(keyframe.value.ty, 0.0f));
  }
}

/**
 * Test case: keyword origins and the optional pivot depth resolve before they reach the anchor.
 */
PAG_TEST(PAGXHTML3DTest, AnimatedKeywordOriginResolvesAnchor) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes spin {
        0%   { transform: rotate(0deg); }
        100% { transform: rotate(90deg); }
      }
    </style></head>
    <body style="width:200px;height:200px">
      <div id="box" style="width:90px;height:60px;background-color:#000;
           transform-origin:right bottom 20px;animation:spin 1s linear infinite"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(NearlyEqual(layer->anchor.x, 90.0f));
  EXPECT_TRUE(NearlyEqual(layer->anchor.y, 60.0f));
  EXPECT_TRUE(NearlyEqual(layer->anchorZ, 20.0f));
  EXPECT_FALSE(HasDiagnosticContaining(doc, "transform-origin"));
}

/**
 * Test case: a two- or three-value `transform-origin` puts each token on the axis its keyword
 * belongs to. The keyword may come first (`bottom left` is x=left, y=bottom), a bare length takes
 * the X axis, and the optional third value is the pivot depth.
 */
/**
 * Test case: `perspective-origin` goes through the same resolver, so a keyword pair has to land on
 * the same vanishing point as the equivalent px pair.
 */
PAG_TEST(PAGXHTML3DTest, PerspectiveOriginKeywordMatchesTheEquivalentPixels) {
  const std::string templateHtml = R"HTML(
    <html><body style="width:200px;height:200px">
      <div style="width:100px;height:60px;perspective:600px;perspective-origin:ORIGIN">
        <div id="card" style="width:100px;height:60px;background-color:#000;
             transform:rotateY(30deg)"></div>
      </div>
    </body></html>
  )HTML";
  auto keywords = templateHtml;
  keywords.replace(keywords.find("ORIGIN"), 6, "bottom left");
  auto pixels = templateHtml;
  pixels.replace(pixels.find("ORIGIN"), 6, "0px 60px");

  auto keywordDoc = ParseHtml(keywords);
  auto pixelDoc = ParseHtml(pixels);
  ASSERT_NE(keywordDoc, nullptr);
  ASSERT_NE(pixelDoc, nullptr);
  auto* keywordCard = FindLayerById(keywordDoc, "card");
  auto* pixelCard = FindLayerById(pixelDoc, "card");
  ASSERT_NE(keywordCard, nullptr);
  ASSERT_NE(pixelCard, nullptr);
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      EXPECT_TRUE(NearlyEqual(keywordCard->matrix3D.getRowColumn(row, column),
                              pixelCard->matrix3D.getRowColumn(row, column), 1e-4f))
          << "row " << row << " column " << column;
    }
  }
}

/**
 * Test case: `perspective-origin` is measured in the parent's border box, the child's `left` / `top`
 * in the parent's padding box. A 20px border therefore shifts the camera origin by 20px; the same
 * page written with `perspective-origin: -20px -20px` and no border has to produce the same pose.
 */
PAG_TEST(PAGXHTML3DTest, BorderWidthInsetsTheParentCameraOrigin) {
  auto withBorder = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div style="position:absolute;left:0;top:0;width:160px;height:100px;border:20px solid #888;
           perspective:600px;perspective-origin:0px 0px">
        <div id="card" style="position:absolute;left:0;top:0;width:80px;height:60px;
             background-color:#38BDF8;transform:rotateY(30deg)"></div>
      </div>
    </body></html>
  )HTML");
  auto noBorder = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div style="position:absolute;left:0;top:0;width:200px;height:140px;
           perspective:600px;perspective-origin:-20px -20px">
        <div id="card" style="position:absolute;left:0;top:0;width:80px;height:60px;
             background-color:#38BDF8;transform:rotateY(30deg)"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(withBorder, nullptr);
  ASSERT_NE(noBorder, nullptr);
  auto* bordered = FindLayerById(withBorder, "card");
  auto* plain = FindLayerById(noBorder, "card");
  ASSERT_NE(bordered, nullptr);
  ASSERT_NE(plain, nullptr);
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      EXPECT_TRUE(NearlyEqual(bordered->matrix3D.getRowColumn(row, column),
                              plain->matrix3D.getRowColumn(row, column), 1e-5f))
          << "row " << row << " column " << column;
    }
  }
}

/**
 * Test case: one bad length argument yields one diagnostic. `parseAbsoluteLengthPx` reports the unit
 * itself, and the transform resolver names the offending argument, so the two used to double-report
 * `translate(10foo)`.
 */
PAG_TEST(PAGXHTML3DTest, BadLengthUnitReportsOnce) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div id="box" style="position:absolute;left:0;top:0;width:60px;height:40px;
           background-color:#000;transform:translate(10foo, 0)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  int unitWarnings = 0;
  int argumentWarnings = 0;
  for (const auto& message : doc->errors) {
    if (message.find("length unit") != std::string::npos) unitWarnings++;
    if (message.find("not a px length") != std::string::npos) argumentWarnings++;
  }
  EXPECT_EQ(unitWarnings, 0) << "the resolver names the argument, the unit warning is redundant";
  EXPECT_EQ(argumentWarnings, 1);
}

/**
 * Test case: an unresolvable `transform-origin` only matters for a pose with a non-identity linear
 * part. A pure translation (or pure projection) pivots identically at any origin, so it must not
 * warn — and the message names the default origin instead of an empty string.
 */
PAG_TEST(PAGXHTML3DTest, UnresolvedOriginOnlyWarnsWhenThePivotMatters) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div id="slide" style="position:absolute;left:0;top:0;width:100%;
           transform:translateX(10px)"></div>
      <div id="spin" style="position:absolute;left:0;top:60px;width:100%;
           transform:rotate(15deg)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  int warnings = 0;
  bool namedDefault = false;
  for (const auto& message : doc->errors) {
    if (message.find("transform-origin") == std::string::npos) {
      continue;
    }
    warnings++;
    if (message.find("(default)") != std::string::npos) {
      namedDefault = true;
    }
  }
  EXPECT_EQ(warnings, 1) << "only the rotation needs a pivot";
  EXPECT_TRUE(namedDefault) << "the message must name the default origin, not an empty string";
  // The translation still applies; only the diagnostic is skipped.
  auto* slide = FindLayerById(doc, "slide");
  ASSERT_NE(slide, nullptr);
  EXPECT_TRUE(NearlyEqual(slide->matrix.tx, 10.0f, 0.01f));
}

/**
 * Test case: a flow child that only reaches 3D through `@keyframes` still gets the parent camera
 * folded into every keyframe, so it has to be reported the same way a static 3D pose is. A child
 * animating only `opacity` keeps quiet.
 */
PAG_TEST(PAGXHTML3DTest, AnimatedFlowChildUnderPerspectiveWarns) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes spin { 0% { transform: rotateY(0deg); } 100% { transform: rotateY(40deg); } }
      @keyframes fade { 0% { opacity: 0.2; } 100% { opacity: 1; } }
    </style></head>
    <body style="width:200px;height:120px">
      <div style="position:absolute;left:0;top:0;width:200px;height:120px;display:flex;
           perspective:600px">
        <div id="a" style="width:80px;height:60px;background-color:#38BDF8;
             animation:spin 2s linear infinite"></div>
        <div id="b" style="width:80px;height:60px;background-color:#F472B6;
             animation:fade 2s linear infinite"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  int warnings = 0;
  for (const auto& message : doc->errors) {
    if (message.find("vanishing point") != std::string::npos) {
      warnings++;
    }
  }
  EXPECT_EQ(warnings, 1) << "the transform animation must be reported, the opacity one must not";
  // The warning is not theoretical: the `100%` stop leaves the plane and really does carry the
  // projection. The flat `0%` stop keeps its own pose.
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  auto* channel = FindMatrix3DChannel(animation);
  ASSERT_NE(channel, nullptr);
  ASSERT_GE(channel->keyframes.size(), 2u);
  for (const auto& key : channel->keyframes) {
    EXPECT_TRUE(NearlyEqual(key.value.getRowColumn(3, 2), 0.0f, 1e-6f))
        << "a flow child keeps its poses unprojected";
  }
}

/**
 * Test case: a child positioned by `right` alone is positioned, so the parent camera can wrap it. It
 * used to be judged un-wrappable and its projection was silently dropped (one document-level note at
 * most), which moved corner-anchored elements.
 */
PAG_TEST(PAGXHTML3DTest, RightPositionedChildUnderPerspectiveKeepsTheProjection) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div style="position:absolute;left:0;top:0;width:200px;height:120px;perspective:600px">
        <div id="card" style="position:absolute;right:10px;top:10px;width:80px;height:60px;
             background-color:#38BDF8;transform:rotateY(30deg)"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* card = FindLayerById(doc, "card");
  ASSERT_NE(card, nullptr);
  auto* wrapper = CameraWrapperOf(doc, card);
  ASSERT_NE(wrapper, nullptr) << "a far-edge anchor is an anchor";
  EXPECT_TRUE(NearlyEqual(wrapper->perspective, 600.0f, 0.01f));
  // Default `perspective-origin` is the box centre; the far anchor grows by the wrapper offset.
  EXPECT_TRUE(NearlyEqual(card->right, 110.0f, 1e-3f));
  EXPECT_FALSE(HasProjectionRow(card));
  EXPECT_FALSE(HasDiagnosticContaining(doc, "cannot be projected"))
      << "the camera is applied, so nothing has to be reported";
}

/**
 * Test case: `perspective` declared on `<body>` projects its direct children, the same way it does on
 * any other container. `convertBody` used to hand the inherited style down unchanged, so a page-level
 * camera was dropped and every 3D transform rendered without perspective.
 */
PAG_TEST(PAGXHTML3DTest, BodyPerspectiveReachesItsChildren) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>body { perspective: 600px; }</style></head>
    <body style="width:200px;height:120px">
      <div id="card" style="position:absolute;left:20px;top:20px;width:80px;height:60px;
           background-color:#38BDF8;transform:rotateY(30deg)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(doc, "vanishing point"));
  auto* card = FindLayerById(doc, "card");
  ASSERT_NE(card, nullptr);
  // The camera is carried by a wrapper layer, so the child pose itself is never projected.
  auto* wrapper = CameraWrapperOf(doc, card);
  ASSERT_NE(wrapper, nullptr) << "the body camera must reach a wrapper above the child";
  EXPECT_TRUE(NearlyEqual(wrapper->perspective, 600.0f, 0.01f));
  EXPECT_FALSE(HasProjectionRow(card)) << "the camera must not be folded into the child pose";

  // Without the camera the same pose stays a plain rotation.
  auto plain = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div id="card" style="position:absolute;left:20px;top:20px;width:80px;height:60px;
           background-color:#38BDF8;transform:rotateY(30deg)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(plain, nullptr);
  auto* plainCard = FindLayerById(plain, "card");
  ASSERT_NE(plainCard, nullptr);
  EXPECT_TRUE(NearlyEqual(plainCard->matrix3D.getRowColumn(3, 0), 0.0f, 1e-6f));
  EXPECT_TRUE(NearlyEqual(plainCard->matrix3D.getRowColumn(3, 2), 0.0f, 1e-6f));
}

/**
 * Test case: a singular flat stop (`scale(0)`) under a perspective parent, on a track whose other
 * stop is 3D so the whole track rides the `matrix3D` channel. PAGX rejects a non-invertible key, so
 * the zero factor is clamped to `SINGULAR_SCALE_CLAMP` before the list multiplies: the stop survives
 * as a sub-pixel pose instead of being dropped (dropping it would start the animation at the
 * rotated pose).
 */
PAG_TEST(PAGXHTML3DTest, SingularFlatStopKeepsItsSubPixelScaleOnA3DTrack) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes k { 0% { transform: scale(0); } 100% { transform: rotateY(45deg); } }
    </style></head>
    <body style="width:200px;height:120px">
      <div style="position:absolute;left:0;top:0;width:200px;height:120px;perspective:600px">
        <div id="card" style="position:absolute;left:20px;top:20px;width:80px;height:60px;
             background-color:#38BDF8;animation:k 1s linear infinite"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(doc, "cannot be represented"))
      << "a clamped stop is representable, so nothing has to be dropped";
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  auto* channel = FindMatrix3DChannel(animation);
  ASSERT_NE(channel, nullptr);
  ASSERT_GE(channel->keyframes.size(), 2u);
  EXPECT_EQ(channel->keyframes.front().time, 0);
  const auto& start = channel->keyframes.front().value;
  ASSERT_TRUE(pagx::IsInvertibleMatrix3D(start))
      << "the 0% stop must survive, so the pop-in starts from nothing visible";
  pagx::DecomposedMatrix3D decomposed = {};
  ASSERT_TRUE(pagx::DecomposeMatrix3D(start, &decomposed));
  // `scale(0)` only zeroes the two in-plane factors; the z factor stays 1, which is what keeps the
  // lifted pose invertible.
  EXPECT_NEAR(decomposed.scale[0], 1e-3, 1e-6) << "the clamp is sub-pixel, not the authored zero";
  EXPECT_NEAR(decomposed.scale[1], 1e-3, 1e-6);
  bool sawAuthoredRotation = false;
  for (const auto& frame : channel->keyframes) {
    EXPECT_TRUE(pagx::IsInvertibleMatrix3D(frame.value));
    EXPECT_TRUE(NearlyEqual(frame.value.getRowColumn(3, 0), 0.0f, 1e-6f));
    if (NearlyEqual(std::fabs(frame.value.getRowColumn(0, 0)), 0.707106f, 0.01f)) {
      sawAuthoredRotation = true;
    }
  }
  EXPECT_TRUE(sawAuthoredRotation)
      << "the surviving tail stop is the authored rotateY(45deg), not a substituted scale";
  auto* card = FindLayerById(doc, "card");
  ASSERT_NE(card, nullptr);
  ASSERT_NE(CameraWrapperOf(doc, card), nullptr) << "the camera must survive on a wrapper";
}

/**
 * Test case: `rotateY(-90deg) scale(0)` is rank-deficient, and applying the zero factor first
 * collapses the rotation away with it. Clamping the factor before the multiply keeps the direction,
 * so the stop is representable and the animation still starts at -90 degrees (30 degrees per frame
 * of error would be visible immediately).
 */
PAG_TEST(PAGXHTML3DTest, SingularRotatedZeroScaleStopKeepsItsDirection) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes flip {
        0%   { transform: rotateY(-90deg) scale(0); }
        100% { transform: rotateY(90deg) scale(1); }
      }
    </style></head>
    <body style="width:200px;height:120px">
      <div style="position:absolute;left:0;top:0;width:200px;height:120px;perspective:600px">
        <div id="card" style="position:absolute;left:20px;top:20px;width:80px;height:60px;
             background-color:#38BDF8;animation:flip 1s linear infinite"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  auto* channel = FindMatrix3DChannel(animation);
  ASSERT_NE(channel, nullptr);
  ASSERT_GE(channel->keyframes.size(), 2u);
  const auto& start = channel->keyframes.front().value;
  ASSERT_TRUE(pagx::IsInvertibleMatrix3D(start)) << "epsilon * R(-90) must be invertible";
  // `scale(0)` zeroes the two in-plane factors only, so `R(-90) * S(epsilon, epsilon, 1)` keeps the
  // rotation in the untouched z column and its sign shows up in row 2.
  EXPECT_TRUE(NearlyEqual(start.getRowColumn(0, 2), -1.0f, 1e-5f))
      << "the -90 degree direction must survive the clamp";
  EXPECT_TRUE(NearlyEqual(start.getRowColumn(2, 0), 1e-3f, 1e-6f))
      << "the in-plane column stays sub-pixel";
  pagx::DecomposedMatrix3D decomposed = {};
  ASSERT_TRUE(pagx::DecomposeMatrix3D(start, &decomposed));
  EXPECT_NEAR(decomposed.scale[0], 1e-3, 1e-6);
  EXPECT_NEAR(decomposed.scale[1], 1e-3, 1e-6);
  // A quaternion and its negation are the same rotation, so the components are compared by
  // magnitude.
  EXPECT_NEAR(std::fabs(decomposed.rotation.y), 0.7071068, 1e-3);
  EXPECT_NEAR(std::fabs(decomposed.rotation.w), 0.7071068, 1e-3);
}

/**
 * Test case: the clamp must not borrow a rotation the author never wrote. `rotateY(0deg) scale(0)`
 * starts at zero degrees, so the clamped start pose is `epsilon * I` (a sub-pixel unrotated box),
 * not the 60 degrees of the tail stop.
 */
PAG_TEST(PAGXHTML3DTest, SingularZeroScaleAtZeroDegreesStaysUnrotated) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes grow {
        0%   { transform: rotateY(0deg) scale(0); }
        100% { transform: rotateY(60deg) scale(1); }
      }
    </style></head>
    <body style="width:200px;height:120px">
      <div style="position:absolute;left:0;top:0;width:200px;height:120px;perspective:600px">
        <div id="card" style="position:absolute;left:20px;top:20px;width:80px;height:60px;
             background-color:#38BDF8;animation:grow 1s linear infinite"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  auto* channel = FindMatrix3DChannel(animation);
  ASSERT_NE(channel, nullptr);
  ASSERT_GE(channel->keyframes.size(), 2u);
  pagx::DecomposedMatrix3D decomposed = {};
  ASSERT_TRUE(pagx::DecomposeMatrix3D(channel->keyframes.front().value, &decomposed));
  EXPECT_NEAR(decomposed.scale[0], 1e-3, 1e-6);
  EXPECT_NEAR(decomposed.scale[1], 1e-3, 1e-6);
  EXPECT_NEAR(std::fabs(decomposed.rotation.y), 0.0, 1e-6)
      << "the start keeps its own zero degrees";
  EXPECT_NEAR(std::fabs(decomposed.rotation.w), 1.0, 1e-6);
}

/**
 * Test case: the clamp is a keyframe-only policy. A static `transform: scale(0)` keeps the authored
 * zero: the 2D `matrix` channel can carry a singular pose, so the element stays exactly invisible
 * and nothing is reported.
 */
PAG_TEST(PAGXHTML3DTest, StaticZeroScaleStaysExactlyZero) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div id="card" style="position:absolute;left:20px;top:20px;width:80px;height:60px;
           background-color:#38BDF8;transform:scale(0)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* card = FindLayerById(doc, "card");
  ASSERT_NE(card, nullptr);
  EXPECT_TRUE(card->matrix3D.isIdentity()) << "a flat pose must not become 3D";
  EXPECT_TRUE(NearlyEqual(card->matrix.a, 0.0f, 1e-6f));
  EXPECT_TRUE(NearlyEqual(card->matrix.b, 0.0f, 1e-6f));
  EXPECT_TRUE(NearlyEqual(card->matrix.c, 0.0f, 1e-6f));
  EXPECT_TRUE(NearlyEqual(card->matrix.d, 0.0f, 1e-6f));
  EXPECT_TRUE(doc->errors.empty()) << "a legal static pose reports nothing";
}

/**
 * Test case: a singular pose that came from `matrix` / `matrix3d` carries no direction to recover,
 * so the zero-scale clamp cannot apply: the stop is still dropped and reported instead of being
 * approximated into something the author never wrote. This is the boundary between the two cases.
 */
PAG_TEST(PAGXHTML3DTest, MatrixSourceSingularStopIsStillDropped) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes k {
        0%   { transform: matrix3d(0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1); }
        100% { transform: rotateY(45deg); }
      }
    </style></head>
    <body style="width:200px;height:120px">
      <div style="position:absolute;left:0;top:0;width:200px;height:120px;perspective:600px">
        <div id="card" style="position:absolute;left:20px;top:20px;width:80px;height:60px;
             background-color:#38BDF8;animation:k 1s linear infinite"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "not invertible"))
      << "a matrix-form singular pose has to be reported, never clamped";
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  auto* channel = FindMatrix3DChannel(animation);
  ASSERT_NE(channel, nullptr);
  bool sawAuthoredRotation = false;
  for (const auto& frame : channel->keyframes) {
    EXPECT_TRUE(pagx::IsInvertibleMatrix3D(frame.value));
    EXPECT_FALSE(std::fabs(frame.value.getRowColumn(0, 0)) < 0.01f &&
                 std::fabs(frame.value.getRowColumn(0, 2)) < 0.01f)
        << "no substitute for the dropped matrix stop";
    if (NearlyEqual(std::fabs(frame.value.getRowColumn(0, 0)), 0.707106f, 0.01f)) {
      sawAuthoredRotation = true;
    }
  }
  EXPECT_TRUE(sawAuthoredRotation) << "the authored rotateY(45deg) stop survives";
}

/**
 * Test case: the same `matrix(0,0,0,0,0,0)` form on a 3D track is flat, so it survives parsing and
 * is dropped later, when the flat stop is lifted onto the `matrix3D` channel. That drop is reported
 * by the routing step ("cannot be represented"), not by the parser.
 */
PAG_TEST(PAGXHTML3DTest, FlatMatrixSourceSingularStopIsDroppedByRouting) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes k {
        0%   { transform: matrix(0,0,0,0,0,0); }
        100% { transform: rotateY(45deg); }
      }
    </style></head>
    <body style="width:200px;height:120px">
      <div style="position:absolute;left:0;top:0;width:200px;height:120px;perspective:600px">
        <div id="card" style="position:absolute;left:20px;top:20px;width:80px;height:60px;
             background-color:#38BDF8;animation:k 1s linear infinite"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "cannot be represented"));
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  auto* channel = FindMatrix3DChannel(animation);
  ASSERT_NE(channel, nullptr);
  bool sawAuthoredRotation = false;
  for (const auto& frame : channel->keyframes) {
    EXPECT_TRUE(pagx::IsInvertibleMatrix3D(frame.value));
    if (NearlyEqual(std::fabs(frame.value.getRowColumn(0, 0)), 0.707106f, 0.01f)) {
      sawAuthoredRotation = true;
    }
  }
  EXPECT_TRUE(sawAuthoredRotation) << "the authored rotateY(45deg) stop survives";
}

/**
 * Test case: a `scale(0)` → `scale(1)` pop-in under a perspective parent stays entirely flat, so it
 * keeps the cheap `matrix` channel and the `0%` stop survives as a zero linear block.
 */
PAG_TEST(PAGXHTML3DTest, ScaleZeroPopInUnderPerspectiveKeepsItsStart) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes pop { 0% { transform: scale(0); } 100% { transform: scale(1); } }
    </style></head>
    <body style="width:200px;height:120px">
      <div style="position:absolute;left:0;top:0;width:200px;height:120px;perspective:600px">
        <div id="card" style="position:absolute;left:20px;top:20px;width:80px;height:60px;
             background-color:#38BDF8;animation:pop 1s linear infinite"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  EXPECT_EQ(FindMatrix3DChannel(animation), nullptr) << "a flat track must stay on the 2D channel";
  auto* channel = FindMatrixChannel(animation);
  ASSERT_NE(channel, nullptr);
  ASSERT_EQ(channel->keyframes.size(), 2u) << "the scale(0) start must not be dropped";
  EXPECT_TRUE(NearlyEqual(channel->keyframes.front().value.a, 0.0f, 1e-6f));
  EXPECT_TRUE(NearlyEqual(channel->keyframes.front().value.d, 0.0f, 1e-6f));
  EXPECT_TRUE(NearlyEqual(channel->keyframes.back().value.a, 1.0f, 1e-6f));
}

/**
 * Test case: a singular static pose (`scale(0)`) under a 3D keyframe track. PAGX persists
 * `matrix3D` keyframes only when they are invertible, so the fill-mode holds carry the clamped pose
 * instead of the authored zero: outside the active window the element stays sub-pixel, which is what
 * CSS shows, rather than snapping back to full size the way an identity hold would. The element's
 * own static `matrix` keeps the authored zero — only the hold frames are clamped.
 */
PAG_TEST(PAGXHTML3DTest, SingularStaticPoseHoldsStaySubPixel) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes k { 0% { transform: rotateY(0deg); } 100% { transform: rotateY(60deg); } }
    </style></head>
    <body style="width:200px;height:120px">
      <div id="card" style="position:absolute;left:20px;top:20px;width:80px;height:60px;
           background-color:#38BDF8;transform:scale(0);
           animation:k 1s linear 0.5s 1"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  auto* channel = FindMatrix3DChannel(animation);
  ASSERT_NE(channel, nullptr);
  ASSERT_GE(channel->keyframes.size(), 4u) << "delay + once needs a hold on both sides";
  for (const auto& key : channel->keyframes) {
    EXPECT_TRUE(pagx::IsInvertibleMatrix3D(key.value)) << "time " << key.time;
  }
  for (const auto& hold : {channel->keyframes.front(), channel->keyframes.back()}) {
    EXPECT_FALSE(hold.value.isIdentity()) << "an identity hold would show the card at full size";
    pagx::DecomposedMatrix3D decomposed = {};
    ASSERT_TRUE(pagx::DecomposeMatrix3D(hold.value, &decomposed));
    EXPECT_NEAR(decomposed.scale[0], 1e-3, 1e-6);
    EXPECT_NEAR(decomposed.scale[1], 1e-3, 1e-6);
  }
  int warnings = 0;
  for (const auto& message : doc->errors) {
    if (message.find("animation-fill-mode") != std::string::npos) {
      warnings++;
    }
  }
  EXPECT_EQ(warnings, 0) << "a clamped hold is faithful, so nothing has to be reported";
  auto* card = FindLayerById(doc, "card");
  ASSERT_NE(card, nullptr);
  EXPECT_TRUE(NearlyEqual(card->matrix.a, 0.0f, 1e-6f))
      << "the element's own static matrix keeps the authored zero";
  EXPECT_TRUE(NearlyEqual(card->matrix.d, 0.0f, 1e-6f));
}

/**
 * Test case: the zero-scale substitute is a *ratio*, so a fixed `1e-3` only bounds the rendered size
 * for boxes up to 500px — a 20000px box would paint a 20px patch. Sizing it from the reference box
 * (`0.5 / 20000`) keeps the fill-mode hold inside half a pixel, so a large element is still
 * invisible while the animation waits out its delay.
 */
PAG_TEST(PAGXHTML3DTest, SingularZeroScaleHoldStaysSubPixelOnALargeBox) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes k { 0% { transform: rotateY(0deg); } 100% { transform: rotateY(60deg); } }
    </style></head>
    <body style="width:200px;height:120px">
      <div id="card" style="position:absolute;left:0;top:0;width:20000px;height:20000px;
           background-color:#38BDF8;transform:scale(0);transform-origin:0px 0px;
           animation:k 1s linear 0.5s 1"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  auto* channel = FindMatrix3DChannel(animation);
  ASSERT_NE(channel, nullptr);
  ASSERT_GE(channel->keyframes.size(), 4u) << "delay + once needs a hold on both sides";
  pagx::DecomposedMatrix3D decomposed = {};
  ASSERT_TRUE(pagx::DecomposeMatrix3D(channel->keyframes.front().value, &decomposed));
  // 0.5 / 20000 = 2.5e-5, which is below the `1e-3` upper bound the small-box cases keep.
  EXPECT_NEAR(decomposed.scale[0], 0.000025, 1e-9);
  EXPECT_NEAR(decomposed.scale[1], 0.000025, 1e-9);
  auto rendered = RoundTripForRender(doc);
  ASSERT_NE(rendered, nullptr);
  ASSERT_NE(rendered->findNode("card"), nullptr)
      << "the round-trip has to bind the animation target";
  // 0.1s sits inside the 0.5s delay, so the frame is the leading fill-mode hold.
  const auto holdPixels = RenderFrame(rendered, 100000);
  ASSERT_FALSE(holdPixels.empty());
  EXPECT_LE(CountVisiblePixels(holdPixels), 8u)
      << "a 20000px box held at 1e-3 would paint a 20px patch";
  // 1.0s is half-way through the 1s active window: the animation ran, so the element is clearly
  // visible. Without this frame the hold assertion would also pass on a document whose animation was
  // never bound at all.
  const auto activePixels = RenderFrame(rendered, 1000000);
  ASSERT_FALSE(activePixels.empty());
  EXPECT_GT(CountVisiblePixels(activePixels), 1000u)
      << "the active window has to render the animated element";
}

/**
 * Test case: a singular static pose that came from a `matrix` factor cannot be recovered by the
 * zero-scale clamp, but holding the identity would show the element at full size outside the
 * active window. The hold is `diag(epsilon, epsilon, 1)` — invertible, sub-pixel — and keeps the
 * authored translation, so the element stays invisible at its own position.
 */
PAG_TEST(PAGXHTML3DTest, MatrixSourceSingularFillHoldStaysSubPixel) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes k { 0% { transform: rotateY(0deg); } 100% { transform: rotateY(60deg); } }
    </style></head>
    <body style="width:200px;height:120px">
      <div id="card" style="position:absolute;left:20px;top:20px;width:80px;height:60px;
           background-color:#38BDF8;transform:matrix(0,0,0,0,30,20);
           animation:k 1s linear 0.5s 1"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  auto* channel = FindMatrix3DChannel(animation);
  ASSERT_NE(channel, nullptr);
  ASSERT_GE(channel->keyframes.size(), 4u) << "delay + once needs a hold on both sides";
  const auto& hold = channel->keyframes.front().value;
  ASSERT_TRUE(pagx::IsInvertibleMatrix3D(hold)) << "the hold has to be persistable";
  // 0.5 / 80 is above the `1e-3` upper bound, so a 80px box keeps the small-box factor.
  EXPECT_NEAR(hold.getRowColumn(0, 0), 1e-3f, 1e-6f)
      << "the linear block is the sub-pixel substitute, not the identity";
  EXPECT_NEAR(hold.getRowColumn(1, 1), 1e-3f, 1e-6f);
  EXPECT_NEAR(hold.getRowColumn(2, 2), 1.0f, 1e-6f)
      << "z stays 1, which is what keeps it invertible";
  EXPECT_NEAR(hold.getRowColumn(0, 3), 30.0f, 1e-6f) << "the authored translation is preserved";
  EXPECT_NEAR(hold.getRowColumn(1, 3), 20.0f, 1e-6f);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "animation-fill-mode"))
      << "a matrix-source singular pose cannot be restored exactly, so it has to be reported";
  // 0.1s sits inside the 0.5s delay, so the frame is the leading fill-mode hold.
  auto rendered = RoundTripForRender(doc);
  ASSERT_NE(rendered, nullptr);
  ASSERT_NE(rendered->findNode("card"), nullptr)
      << "the round-trip has to bind the animation target";
  const auto holdPixels = RenderFrame(rendered, 100000);
  ASSERT_FALSE(holdPixels.empty());
  EXPECT_LE(CountVisiblePixels(holdPixels), 8u)
      << "an identity hold would paint the whole 80x60 box during the delay";
  // Same reasoning as the large-box case: the visible frame inside the active window proves the
  // animation actually played, which the hold assertion alone cannot.
  const auto activePixels = RenderFrame(rendered, 1000000);
  ASSERT_FALSE(activePixels.empty());
  EXPECT_GT(CountVisiblePixels(activePixels), 1000u)
      << "the active window has to render the animated element";
}

/**
 * Test case: a 3D keyframe track on an element whose static pose is 2D. The fill-mode holds (before
 * a `delay` and after a `Once` run) must restore that static pose instead of the identity, which
 * would hide the element's authored `transform` outside the active window.
 */
PAG_TEST(PAGXHTML3DTest, Matrix3DFillHoldsKeepTheStatic2DTransform) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes k { 0% { transform: rotateY(0deg); } 100% { transform: rotateY(60deg); } }
    </style></head>
    <body style="width:200px;height:120px">
      <div id="card" style="position:absolute;left:20px;top:20px;width:80px;height:60px;
           background-color:#38BDF8;transform:rotate(20deg);
           animation:k 1s linear 0.5s 1"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  auto* channel = FindMatrix3DChannel(animation);
  ASSERT_NE(channel, nullptr);
  ASSERT_GE(channel->keyframes.size(), 4u) << "delay + once needs a hold on both sides";
  // `rotate(20deg)` in the layer's matrix form: m00 = cos, m10 = sin.
  const float cos20 = 0.939693f;
  const float sin20 = 0.342020f;
  const auto& leading = channel->keyframes.front();
  const auto& trailing = channel->keyframes.back();
  EXPECT_TRUE(NearlyEqual(leading.value.getRowColumn(0, 0), cos20, 0.001f))
      << "during the delay the static transform must show, not the identity";
  EXPECT_TRUE(NearlyEqual(leading.value.getRowColumn(1, 0), sin20, 0.001f));
  EXPECT_TRUE(NearlyEqual(trailing.value.getRowColumn(0, 0), cos20, 0.001f))
      << "after a Once run with fill-mode none the static transform comes back";
  EXPECT_TRUE(NearlyEqual(trailing.value.getRowColumn(1, 0), sin20, 0.001f));
}

/**
 * Test case: a huge `rotate3d` axis still normalizes. Squaring `1e20` overflows a float and used to
 * slip past the zero-vector check, which collapsed the rotation to `cos * I` and wrote it out as a
 * valid 3D pose; the axis is now normalized in double precision.
 */
PAG_TEST(PAGXHTML3DTest, HugeRotate3DAxisStillNormalizes) {
  const std::string templateHtml = R"HTML(
    <html><body style="width:200px;height:120px">
      <div id="box" style="width:60px;height:40px;background-color:#000;
           transform:rotate3d(AXIS, 0, 0, 45deg)"></div>
    </body></html>
  )HTML";
  auto huge = templateHtml;
  huge.replace(huge.find("AXIS"), 4, "1e20");
  auto unit = templateHtml;
  unit.replace(unit.find("AXIS"), 4, "1");

  auto hugeDoc = ParseHtml(huge);
  auto unitDoc = ParseHtml(unit);
  ASSERT_NE(hugeDoc, nullptr);
  ASSERT_NE(unitDoc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(hugeDoc, "axis"));
  auto* hugeBox = FindLayerById(hugeDoc, "box");
  auto* unitBox = FindLayerById(unitDoc, "box");
  ASSERT_NE(hugeBox, nullptr);
  ASSERT_NE(unitBox, nullptr);
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      EXPECT_TRUE(NearlyEqual(hugeBox->matrix3D.getRowColumn(row, column),
                              unitBox->matrix3D.getRowColumn(row, column), 1e-4f))
          << "row " << row << " column " << column;
    }
  }
  // `cos * I` would put cos(45) on the diagonal instead of 1.
  EXPECT_TRUE(NearlyEqual(hugeBox->matrix3D.getRowColumn(0, 0), 1.0f, 1e-4f));
  EXPECT_TRUE(NearlyEqual(hugeBox->matrix3D.getRowColumn(1, 1), 0.707107f, 1e-4f));
}

/**
 * Test case: `skewX` / `skew` carry the CSS sign. `skewX(a)` is `matrix(1, 0, tan a, 1)`, so a
 * positive angle leans the bottom edge to the right; the importer used to negate it, which mirrored
 * the shear on the HTML -> PAGX -> HTML round trip.
 */
PAG_TEST(PAGXHTML3DTest, SkewKeepsTheCssSign) {
  auto skewX = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div id="box" style="width:60px;height:40px;background-color:#000;transform:skewX(30deg)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(skewX, nullptr);
  auto* skewXBox = FindLayerById(skewX, "box");
  ASSERT_NE(skewXBox, nullptr);
  EXPECT_TRUE(NearlyEqual(skewXBox->matrix.c, 0.577350f, 0.001f)) << "c = +tan(30deg)";
  EXPECT_TRUE(NearlyEqual(skewXBox->matrix.b, 0.0f, 0.001f));

  auto skewY = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div id="box" style="width:60px;height:40px;background-color:#000;transform:skewY(30deg)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(skewY, nullptr);
  auto* skewYBox = FindLayerById(skewY, "box");
  ASSERT_NE(skewYBox, nullptr);
  EXPECT_TRUE(NearlyEqual(skewYBox->matrix.b, 0.577350f, 0.001f)) << "b = +tan(30deg)";
  EXPECT_TRUE(NearlyEqual(skewYBox->matrix.c, 0.0f, 0.001f));

  auto skewBoth = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div id="box" style="width:60px;height:40px;background-color:#000;
           transform:skew(20deg, 10deg)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(skewBoth, nullptr);
  auto* skewBothBox = FindLayerById(skewBoth, "box");
  ASSERT_NE(skewBothBox, nullptr);
  EXPECT_TRUE(NearlyEqual(skewBothBox->matrix.c, 0.363970f, 0.001f)) << "c = +tan(20deg)";
  EXPECT_TRUE(NearlyEqual(skewBothBox->matrix.b, 0.176327f, 0.001f)) << "b = +tan(10deg)";
}

/**
 * Test case: `<transform-list>` needs no separator between functions, so minified CSS
 * (`rotate(45deg)scale(2)`) composes exactly like the spaced form.
 */
PAG_TEST(PAGXHTML3DTest, CompoundTransformWithoutSpacesIsAccepted) {
  const std::string templateHtml = R"HTML(
    <html><body style="width:200px;height:120px">
      <div id="box" style="width:80px;height:40px;background-color:#000;
           transform:SPACED"></div>
    </body></html>
  )HTML";
  auto spaced = templateHtml;
  spaced.replace(spaced.find("SPACED"), 6, "rotate(45deg) scale(2)");
  auto compact = templateHtml;
  compact.replace(compact.find("SPACED"), 6, "rotate(45deg)scale(2)");

  auto spacedDoc = ParseHtml(spaced);
  auto compactDoc = ParseHtml(compact);
  ASSERT_NE(spacedDoc, nullptr);
  ASSERT_NE(compactDoc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(compactDoc, "malformed"));
  auto* spacedBox = FindLayerById(spacedDoc, "box");
  auto* compactBox = FindLayerById(compactDoc, "box");
  ASSERT_NE(spacedBox, nullptr);
  ASSERT_NE(compactBox, nullptr);
  EXPECT_TRUE(NearlyEqual(spacedBox->matrix.a, compactBox->matrix.a, 1e-5f));
  EXPECT_TRUE(NearlyEqual(spacedBox->matrix.b, compactBox->matrix.b, 1e-5f));
  EXPECT_TRUE(NearlyEqual(spacedBox->matrix.c, compactBox->matrix.c, 1e-5f));
  EXPECT_TRUE(NearlyEqual(spacedBox->matrix.d, compactBox->matrix.d, 1e-5f));
  EXPECT_TRUE(NearlyEqual(spacedBox->matrix.a, 1.41421356f, 1e-4f));
}

/**
 * Test case: angle arguments accept the CSS units (`deg` / `grad` / `rad` / `turn`) case
 * insensitively. `100grad` is a right angle, `0.5TURN` is a half turn, and `1rad` is ~57.3deg.
 */
PAG_TEST(PAGXHTML3DTest, AngleUnitsAreConvertedCaseInsensitively) {
  struct AngleCase {
    const char* transform;
    float m00;
    float m10;
    float m01;
  };
  const AngleCase cases[] = {
      {"rotate(100grad)", 0.0f, 1.0f, -1.0f},
      {"rotate(0.5turn)", -1.0f, 0.0f, 0.0f},
      {"rotate(0.5TURN)", -1.0f, 0.0f, 0.0f},
      {"rotate(1rad)", 0.540302f, 0.841471f, -0.841471f},
      {"rotate3d(0, 0, 1, 0.25turn)", 0.0f, 1.0f, -1.0f},
  };
  for (const auto& item : cases) {
    std::string html = R"HTML(
      <html><body style="width:200px;height:120px">
        <div id="box" style="width:80px;height:40px;background-color:#000;transform:VALUE"></div>
      </body></html>
    )HTML";
    html.replace(html.find("VALUE"), 5, item.transform);
    auto doc = ParseHtml(html);
    ASSERT_NE(doc, nullptr) << item.transform;
    EXPECT_FALSE(HasDiagnosticContaining(doc, "angle")) << item.transform;
    auto* box = FindLayerById(doc, "box");
    ASSERT_NE(box, nullptr) << item.transform;
    EXPECT_TRUE(NearlyEqual(box->matrix.a, item.m00, 0.002f))
        << item.transform << " a=" << box->matrix.a;
    EXPECT_TRUE(NearlyEqual(box->matrix.b, item.m10, 0.002f))
        << item.transform << " b=" << box->matrix.b;
    EXPECT_TRUE(NearlyEqual(box->matrix.c, item.m01, 0.002f))
        << item.transform << " c=" << box->matrix.c;
  }
}

/**
 * Test case: an angle with an unknown or missing unit invalidates the declaration, the way CSS does
 * (`rotate(45px)` and `rotate(45)` are both invalid; a bare `0` is allowed).
 */
PAG_TEST(PAGXHTML3DTest, InvalidAngleUnitsDropTheTransform) {
  const char* invalid[] = {"rotate(45px)",   "rotate(45)",
                           "skewX(1foo)",    "rotate3d(0, 0, 1, 30px)",
                           "rotate(45 deg)", "rotate(100 grad)"};
  for (const char* value : invalid) {
    std::string html = R"HTML(
      <html><body style="width:200px;height:120px">
        <div id="box" style="width:80px;height:40px;background-color:#000;transform:VALUE"></div>
      </body></html>
    )HTML";
    html.replace(html.find("VALUE"), 5, value);
    auto doc = ParseHtml(html);
    ASSERT_NE(doc, nullptr) << value;
    EXPECT_TRUE(HasDiagnosticContaining(doc, "angle")) << value;
    auto* box = FindLayerById(doc, "box");
    ASSERT_NE(box, nullptr) << value;
    EXPECT_TRUE(box->matrix.isIdentity()) << value << " must not be applied";
    EXPECT_TRUE(box->matrix3D.isIdentity()) << value << " must not be applied";
  }

  // A bare zero is a legal `<angle>`.
  auto zeroDoc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div id="box" style="width:80px;height:40px;background-color:#000;transform:rotate(0)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(zeroDoc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(zeroDoc, "angle"));
}

/**
 * Test case: a percentage length argument resolves against the element box (width on X, height on
 * Y). `calc()` is not supported: it needs CSS type checking, and a `calc()` argument has to be
 * reported as unsupported rather than guessed.
 */
PAG_TEST(PAGXHTML3DTest, PercentLengthArgumentsResolveAgainstTheElementBox) {
  struct PercentCase {
    const char* transform;
    float tx;
    float ty;
  };
  // Box is 80x40.
  const PercentCase cases[] = {
      {"translateX(50%)", 40.0f, 0.0f},
      {"translateY(25%)", 0.0f, 10.0f},
      {"translate(10%, 50%)", 8.0f, 20.0f},
  };
  for (const auto& item : cases) {
    std::string html = R"HTML(
      <html><body style="width:200px;height:120px">
        <div id="box" style="width:80px;height:40px;background-color:#000;transform:VALUE"></div>
      </body></html>
    )HTML";
    html.replace(html.find("VALUE"), 5, item.transform);
    auto doc = ParseHtml(html);
    ASSERT_NE(doc, nullptr) << item.transform;
    auto* box = FindLayerById(doc, "box");
    ASSERT_NE(box, nullptr) << item.transform;
    EXPECT_TRUE(NearlyEqual(box->matrix.tx, item.tx, 0.01f))
        << item.transform << " tx=" << box->matrix.tx;
    EXPECT_TRUE(NearlyEqual(box->matrix.ty, item.ty, 0.01f))
        << item.transform << " ty=" << box->matrix.ty;
  }

  auto calcDoc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div id="box" style="width:80px;height:40px;background-color:#000;
           transform:translateX(calc(100% - 10px))"></div>
    </body></html>
  )HTML");
  ASSERT_NE(calcDoc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(calcDoc, "not a px length"));
  auto* calcBox = FindLayerById(calcDoc, "box");
  ASSERT_NE(calcBox, nullptr);
  EXPECT_TRUE(calcBox->matrix.isIdentity()) << "calc() must not be guessed into a length";
}

/**
 * Test case: an argument the subset cannot evaluate is reported as such (not as a malformed list),
 * so the diagnostic points at the function that owns it.
 */
PAG_TEST(PAGXHTML3DTest, UnsupportedLengthArgumentIsNotReportedAsMalformed) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div id="box" style="width:80px;height:40px;background-color:#000;
           transform:translateX(var(--dx))"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_FALSE(HasDiagnosticContaining(doc, "is malformed"));
  EXPECT_TRUE(HasDiagnosticContaining(doc, "translateX argument"));
}

/**
 * Test case: a two- or three-value `transform-origin` puts each token on the axis its keyword
 * belongs to. Keyword pairs are order-independent (`bottom left` is x=left, y=bottom), a length may
 * only take a slot whose keyword class allows it (`20px top`, `left 20px`), and the forms Chromium
 * rejects (`top 20px`, `20px left`, `left right`) fall back to the initial `50% 50%`.
 */
PAG_TEST(PAGXHTML3DTest, MultiValueOriginPutsEachKeywordOnItsOwnAxis) {
  struct OriginCase {
    const char* value;
    float x;
    float y;
    float z;
  };
  // Box is 100x60, so left=0 / right=100 and top=0 / bottom=60.
  const OriginCase valid[] = {
      {"bottom left", 0.0f, 60.0f, 0.0f},    {"top center", 50.0f, 0.0f, 0.0f},
      {"center bottom", 50.0f, 60.0f, 0.0f}, {"center left", 0.0f, 30.0f, 0.0f},
      {"right bottom", 100.0f, 60.0f, 0.0f}, {"20px top", 20.0f, 0.0f, 0.0f},
      {"left 20px", 0.0f, 20.0f, 0.0f},      {"20px center", 20.0f, 30.0f, 0.0f},
      {"20px 40px", 20.0f, 40.0f, 0.0f},     {"bottom right 20px", 100.0f, 60.0f, 20.0f},
      {"left top 10px", 0.0f, 0.0f, 10.0f},
  };
  for (const auto& item : valid) {
    std::string html = R"HTML(
      <html><body style="width:200px;height:200px">
        <div id="box" style="width:100px;height:60px;background-color:#000;
             transform:rotate(10deg);transform-origin:VALUE"></div>
      </body></html>
    )HTML";
    html.replace(html.find("VALUE"), 5, item.value);
    auto doc = ParseHtml(html);
    ASSERT_NE(doc, nullptr) << item.value;
    EXPECT_FALSE(HasDiagnosticContaining(doc, "not a valid CSS position")) << item.value;
    auto* box = FindLayerById(doc, "box");
    ASSERT_NE(box, nullptr) << item.value;
    EXPECT_TRUE(NearlyEqual(box->anchor.x, item.x))
        << "transform-origin: " << item.value << " x=" << box->anchor.x;
    EXPECT_TRUE(NearlyEqual(box->anchor.y, item.y))
        << "transform-origin: " << item.value << " y=" << box->anchor.y;
    EXPECT_TRUE(NearlyEqual(box->anchorZ, item.z))
        << "transform-origin: " << item.value << " z=" << box->anchorZ;
  }

  // Chromium drops these declarations, so the initial `50% 50%` (the box centre) applies.
  const char* invalid[] = {"top 20px", "bottom 20px", "20px left", "left right", "top bottom"};
  for (const char* value : invalid) {
    std::string html = R"HTML(
      <html><body style="width:200px;height:200px">
        <div id="box" style="width:100px;height:60px;background-color:#000;
             transform:rotate(10deg);transform-origin:VALUE"></div>
      </body></html>
    )HTML";
    html.replace(html.find("VALUE"), 5, value);
    auto doc = ParseHtml(html);
    ASSERT_NE(doc, nullptr) << value;
    EXPECT_TRUE(HasDiagnosticContaining(doc, "not a valid CSS position")) << value;
    auto* box = FindLayerById(doc, "box");
    ASSERT_NE(box, nullptr) << value;
    EXPECT_TRUE(NearlyEqual(box->anchor.x, 50.0f)) << value << " x=" << box->anchor.x;
    EXPECT_TRUE(NearlyEqual(box->anchor.y, 30.0f)) << value << " y=" << box->anchor.y;
  }
}

/**
 * Test case: a `transform-origin` token that does not parse (`10foo`, `nan%`, a percentage depth,
 * four components) drops the whole declaration, exactly like a bad keyword pair: Chromium falls back
 * to the initial `50% 50%` (the box centre) instead of pivoting at the top-left corner. The value is
 * reported once — the per-token unit warning is suppressed so the position diagnostic stands alone.
 *
 * Every token is validated before the box is consulted, so this case is `valid = false` with the
 * default pivot. A token that parses but needs a size the moment does not have yet (a percentage or
 * an `em` against an unresolved box) is a different path: the declaration stays valid and the layer
 * size resolves it later.
 */
PAG_TEST(PAGXHTML3DTest, UnparsableOriginTokenFallsBackToTheBoxCentre) {
  const char* values[] = {"10foo 20px", "nan% 20px", "10px 20px 30%", "10px 20px 30px 40px"};
  for (const char* value : values) {
    std::string html = R"HTML(
      <html><body style="width:200px;height:200px">
        <div id="box" style="width:100px;height:60px;background-color:#000;
             transform:rotate(10deg);transform-origin:VALUE"></div>
      </body></html>
    )HTML";
    html.replace(html.find("VALUE"), 5, value);
    auto doc = ParseHtml(html);
    ASSERT_NE(doc, nullptr) << value;
    int unitWarnings = 0;
    int positionWarnings = 0;
    for (const auto& message : doc->errors) {
      if (message.find("length unit") != std::string::npos) unitWarnings++;
      if (message.find("not a valid CSS position") != std::string::npos) positionWarnings++;
    }
    EXPECT_EQ(unitWarnings, 0) << value;
    EXPECT_EQ(positionWarnings, 1) << value;
    auto* box = FindLayerById(doc, "box");
    ASSERT_NE(box, nullptr) << value;
    EXPECT_TRUE(NearlyEqual(box->anchor.x, 50.0f)) << value << " x=" << box->anchor.x;
    EXPECT_TRUE(NearlyEqual(box->anchor.y, 30.0f)) << value << " y=" << box->anchor.y;
    EXPECT_TRUE(NearlyEqual(box->anchorZ, 0.0f)) << value;
  }
}

/**
 * Test case: a single vertical keyword is the Y component (`transform-origin: top` == `center top`),
 * matching CSS rather than being read as an X value.
 */
PAG_TEST(PAGXHTML3DTest, SingleKeywordOriginMapsToTheVerticalAxis) {
  const std::string templateHtml = R"HTML(
    <html><body style="width:200px;height:200px">
      <div id="box" style="width:100px;height:60px;background-color:#000;
           transform:rotate(10deg);transform-origin:KEYWORD"></div>
    </body></html>
  )HTML";
  std::string topHtml = templateHtml;
  topHtml.replace(topHtml.find("KEYWORD"), 7, "top");
  std::string bottomHtml = templateHtml;
  bottomHtml.replace(bottomHtml.find("KEYWORD"), 7, "bottom");

  auto top = ParseHtml(topHtml);
  ASSERT_NE(top, nullptr);
  auto* topLayer = FindLayerById(top, "box");
  ASSERT_NE(topLayer, nullptr);
  EXPECT_TRUE(NearlyEqual(topLayer->anchor.x, 50.0f));
  EXPECT_TRUE(NearlyEqual(topLayer->anchor.y, 0.0f));

  auto bottom = ParseHtml(bottomHtml);
  ASSERT_NE(bottom, nullptr);
  auto* bottomLayer = FindLayerById(bottom, "box");
  ASSERT_NE(bottomLayer, nullptr);
  EXPECT_TRUE(NearlyEqual(bottomLayer->anchor.x, 50.0f));
  EXPECT_TRUE(NearlyEqual(bottomLayer->anchor.y, 60.0f));
}

/**
 * Test case: an `<svg>` sized by attributes keeps its CSS-derived (unset) layer size — the attribute
 * values only feed the pivot reference box.
 */
PAG_TEST(PAGXHTML3DTest, InlineSvgAttributeSizeOnlyFeedsThePivot) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <svg id="icon" width="80" height="60" viewBox="0 0 80 60"
           style="transform:rotateY(20deg);transform-origin:50% 50%">
        <rect x="4" y="4" width="72" height="52" fill="#6366F1"/>
      </svg>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto layers = LayersWithNonIdentityMatrix3D(doc);
  ASSERT_EQ(layers.size(), 1u);
  auto* host = layers.front();
  EXPECT_TRUE(std::isnan(host->width)) << "the attribute size must not resize the layer";
  EXPECT_TRUE(std::isnan(host->height));
  EXPECT_TRUE(NearlyEqual(host->anchor.x, 40.0f));
  EXPECT_TRUE(NearlyEqual(host->anchor.y, 30.0f));
}

/**
 * Test case: text leaves route the animated transform (and the pivot) through their host layer.
 */
PAG_TEST(PAGXHTML3DTest, AnimatedTextLeafKeepsAnchor) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes tilt {
        0%   { transform: rotate(0deg); }
        100% { transform: rotate(12deg); }
      }
    </style></head>
    <body style="width:200px;height:60px">
      <h1 id="title" style="width:200px;height:40px;transform-origin:100px 20px;
          animation:tilt 1s linear infinite">Title</h1>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  auto* matrixChannel = FindMatrixChannel(animation);
  ASSERT_NE(matrixChannel, nullptr);
  auto* target = FindLayerById(doc, ChannelTarget(animation, "matrix"));
  ASSERT_NE(target, nullptr);
  EXPECT_TRUE(NearlyEqual(target->anchor.x, 100.0f));
  EXPECT_TRUE(NearlyEqual(target->anchor.y, 20.0f));
  for (const auto& keyframe : matrixChannel->keyframes) {
    EXPECT_TRUE(NearlyEqual(keyframe.value.tx, 0.0f));
    EXPECT_TRUE(NearlyEqual(keyframe.value.ty, 0.0f));
  }
}

/**
 * Test case: an auxiliary wrapper split (filter brightness) keeps the anchor on the layer that owns
 * the matrix, so the pivot never dangles on a wrapper with an identity pose.
 */
PAG_TEST(PAGXHTML3DTest, AnimatedBrightnessSplitKeepsAnchorWithMatrix) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes dim {
        0%   { filter: brightness(1); }
        100% { filter: brightness(0.5); }
      }
    </style></head>
    <body style="width:200px;height:200px">
      <div id="box" style="width:50px;height:50px;background-color:#000;
           transform:rotate(30deg);animation:dim 1s linear forwards"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_FALSE(layer->matrix.isIdentity());
  EXPECT_TRUE(NearlyEqual(layer->anchor.x, 25.0f));
  EXPECT_TRUE(NearlyEqual(layer->anchor.y, 25.0f));
  for (auto* child : layer->children) {
    EXPECT_TRUE(HasZeroAnchor(child)) << "generated wrappers must not inherit the pivot";
  }
}

/**
 * Test case: a static 3D pose does not swallow a flat translation animation. `matrix3D` outranks
 * `matrix` in the runtime priority, so the keyframes must be lifted onto the matrix3D channel.
 */
PAG_TEST(PAGXHTML3DTest, Static3DDoesNotSwallowTranslateAnimation) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes move {
        0%   { transform: translateX(0px); }
        100% { transform: translateX(100px); }
      }
    </style></head>
    <body style="width:200px;height:200px">
      <div id="box" style="width:80px;height:60px;background-color:#000;
           transform:rotateY(30deg);animation:move 1s linear infinite"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  EXPECT_EQ(FindChannel(animation, "matrix"), nullptr)
      << "a matrix channel would be hidden by the static matrix3D";
  auto* channel = FindMatrix3DChannel(animation);
  ASSERT_NE(channel, nullptr);
  ASSERT_EQ(channel->keyframes.size(), 2u);
  EXPECT_TRUE(NearlyEqual(channel->keyframes.front().value.getRowColumn(0, 3), 0.0f));
  EXPECT_TRUE(NearlyEqual(channel->keyframes.back().value.getRowColumn(0, 3), 100.0f));
  // The animation replaces the whole `transform`, so the keyframes are pure translations even though
  // the element carries a static 3D pose; only the channel they ride on changes.
  EXPECT_TRUE(NearlyEqual(channel->keyframes.back().value.getRowColumn(0, 0), 1.0f, 0.001f));
  EXPECT_TRUE(NearlyEqual(channel->keyframes.back().value.getRowColumn(3, 2), 0.0f, 1e-5f))
      << "the lifted keyframe must not inherit the static projection row";
}

/**
 * Test case: a static 3D pose does not swallow a 2D rotation animation either.
 */
PAG_TEST(PAGXHTML3DTest, Static3DDoesNotSwallow2DRotationAnimation) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes spin {
        0%   { transform: rotate(0deg); }
        100% { transform: rotate(90deg); }
      }
    </style></head>
    <body style="width:200px;height:200px">
      <div id="box" style="width:80px;height:60px;background-color:#000;
           transform:rotateY(30deg);animation:spin 1s linear infinite"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  EXPECT_EQ(FindChannel(animation, "matrix"), nullptr);
  auto* channel = FindMatrix3DChannel(animation);
  ASSERT_NE(channel, nullptr);
  ASSERT_EQ(channel->keyframes.size(), 2u);
  const auto& last = channel->keyframes.back().value;
  EXPECT_TRUE(NearlyEqual(last.getRowColumn(0, 0), 0.0f, 0.001f));
  EXPECT_TRUE(NearlyEqual(last.getRowColumn(1, 0), 1.0f, 0.001f));
  EXPECT_TRUE(NearlyEqual(last.getRowColumn(0, 1), -1.0f, 0.001f));
  EXPECT_TRUE(NearlyEqual(last.getRowColumn(1, 1), 0.0f, 0.001f));
}

/**
 * Test case: inline SVG shapes keep their own animation path (scalar rotation with a structural
 * pivot) instead of being pulled into the Layer anchor path.
 */
PAG_TEST(PAGXHTML3DTest, AnimatedInlineSvgShapeKeepsScalarRotation) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes spin {
        0%   { transform: matrix(1, 0, 0, 1, 0, 0); }
        100% { transform: matrix(0, 1, -1, 0, 0, 0); }
      }
    </style></head>
    <body style="width:100px;height:100px">
      <svg width="80" height="80" viewBox="0 0 80 80">
        <circle id="spinner" cx="40" cy="40" r="30" fill="none" stroke="#8B5CF6"
                style="transform-origin:40px 40px;animation:spin 1s linear infinite"/>
      </svg>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  auto* rotation = FindChannel(animation, "rotation");
  ASSERT_NE(rotation, nullptr);
  ASSERT_EQ(rotation->valueType(), pagx::ChannelValueType::Float);
  auto* typed = static_cast<pagx::TypedChannel<float>*>(rotation);
  ASSERT_EQ(typed->keyframes.size(), 2u);
  EXPECT_TRUE(NearlyEqual(typed->keyframes.back().value, 90.0f));
  EXPECT_EQ(FindChannel(animation, "matrix"), nullptr);
}

// ---------------------------------------------------------------------------------------------
// 3D x anchor coupling: a 3D pose pivoted off centre. The projection row must stay outside the
// pivot, so keyframes carry no pivot translation and the anchor is applied once by the runtime.
// ---------------------------------------------------------------------------------------------

/**
 * Test case: 3D keyframes with a non-centre origin keep the pivot on the layer: the translation
 * column stays zero and the projection row is untouched by the anchor.
 */
PAG_TEST(PAGXHTML3DTest, Animated3DKeyframesKeepAnchorOutsideProjection) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes flip {
        0%   { transform: perspective(500px) rotateY(0deg); }
        100% { transform: perspective(500px) rotateY(60deg); }
      }
    </style></head>
    <body style="width:200px;height:200px">
      <div id="card" style="position:absolute;left:40px;top:40px;width:120px;height:80px;
           background-color:#000;transform-origin:0px 40px;
           animation:flip 1s linear infinite"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "card");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(NearlyEqual(layer->anchor.x, 0.0f));
  EXPECT_TRUE(NearlyEqual(layer->anchor.y, 40.0f));
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  auto* channel = FindMatrix3DChannel(animation);
  ASSERT_NE(channel, nullptr);
  ASSERT_EQ(channel->keyframes.size(), 2u);
  const auto& start = channel->keyframes.front().value;
  // rotateY(0): row3 = (0, 0, -0.002, 1); the pivot must not leak into the translation column.
  EXPECT_TRUE(NearlyEqual(start.getRowColumn(3, 2), -0.002f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(start.getRowColumn(0, 3), 0.0f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(start.getRowColumn(1, 3), 0.0f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(start.getRowColumn(2, 3), 0.0f, 1e-5f));
  const auto& end = channel->keyframes.back().value;
  // rotateY(60): the projection row follows the rotation, it is not conjugated by the pivot.
  EXPECT_TRUE(NearlyEqual(end.getRowColumn(3, 2), -0.001f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(end.getRowColumn(0, 3), 0.0f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(end.getRowColumn(2, 3), 0.0f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(end.getRowColumn(0, 0), 0.5f, 0.001f));
  EXPECT_TRUE(NearlyEqual(end.getRowColumn(2, 0), -0.866025f, 0.001f));
}

/**
 * Test case: a pivot with depth. `transform-origin: 20px 10px 30px` puts the rotation centre off
 * the layer plane, so rotating about X also slides the plane along Z by `anchorZ - L_z * anchorZ`
 * while the stored pose stays un-pivoted.
 *
 * Known gap, deliberately not worked around here: CSS composes `T(a) * P * L * T(-a)` (the pivot
 * conjugates the projection too) while PAGX / AE / Lottie compose `P * T(a) * L * T(-a)` (the
 * camera is fixed, `anchor` only moves the object). The two agree only when the pivot has no depth,
 * because then the camera can be moved onto the pivot in the layer plane.
 *
 * A double wrapper (`perspective` + the 3D conjugation on a parent element) reproduces the CSS order
 * exactly and is what the PAGX -> HTML direction does; it cannot be used here because PAGX does not
 * compose a parent layer's *projection* into its children (an affine parent `matrix3D` does compose,
 * a projective one is dropped), so the wrapper would only add a layer without changing the render.
 * If the renderer ever composes it, the wrapper becomes viable and this case can move from 0.915 to
 * ~0.999 without touching the animation encoding.
 */
PAG_TEST(PAGXHTML3DTest, DepthPivotOffsetsThePlaneInZ) {
  const std::string templateHtml = R"HTML(
    <html><body style="width:200px;height:200px">
      <div id="card" style="width:120px;height:80px;background-color:#6366F1;
           transform:perspective(600px) rotateX(60deg);transform-origin:ORIGIN"></div>
    </body></html>
  )HTML";
  std::string withDepth = templateHtml;
  withDepth.replace(withDepth.find("ORIGIN"), 6, "20px 10px 30px");
  std::string withoutDepth = templateHtml;
  withoutDepth.replace(withoutDepth.find("ORIGIN"), 6, "20px 10px");

  auto doc = ParseHtml(withDepth);
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "card");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(NearlyEqual(layer->anchor.x, 20.0f));
  EXPECT_TRUE(NearlyEqual(layer->anchor.y, 10.0f));
  EXPECT_TRUE(NearlyEqual(layer->anchorZ, 30.0f));

  // The stored pose is the raw CSS value: a depth pivot must not leak into the translation column.
  // Equivalently, PAGX composes `P * T(a) * pose * T(-a)` while CSS composes
  // `T(a) * P * pose * T(-a)`; with a depth pivot no camera placement reconciles the two, so the
  // anchor form is the accepted encoding (see spec/html_subset.md §13.7 and the
  // `49_rotate3d_depth_pivot` corpus case).
  EXPECT_TRUE(NearlyEqual(layer->matrix3D.getRowColumn(0, 3), 0.0f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(layer->matrix3D.getRowColumn(1, 3), 0.0f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(layer->matrix3D.getRowColumn(2, 3), 0.0f, 1e-5f));

  // The pivot is applied once, after the projection: t' = a - L * a with L = rotateX(60).
  const auto local = pagx::BuildLayerTransform3DWithAnchor(layer);
  EXPECT_TRUE(NearlyEqual(local.getRowColumn(0, 3), 0.0f, 0.01f));
  EXPECT_TRUE(NearlyEqual(local.getRowColumn(1, 3), 30.980762f, 0.01f));
  EXPECT_TRUE(NearlyEqual(local.getRowColumn(2, 3), 6.339746f, 0.01f));

  // The depth itself is what moves the plane: dropping anchorZ pulls the pivot back onto the layer,
  // which changes the composed translation by (0, 0, anchorZ) - L * (0, 0, anchorZ). rotateX(60)
  // mixes Y and Z, so both components move: Z by (1 - cos60) * 30 = 15, Y by sin60 * 30 = 25.98.
  auto flatDoc = ParseHtml(withoutDepth);
  ASSERT_NE(flatDoc, nullptr);
  auto* flatLayer = FindLayerById(flatDoc, "card");
  ASSERT_NE(flatLayer, nullptr);
  ASSERT_TRUE(NearlyEqual(flatLayer->anchorZ, 0.0f));
  const auto flatLocal = pagx::BuildLayerTransform3DWithAnchor(flatLayer);
  EXPECT_TRUE(NearlyEqual(local.getRowColumn(2, 3) - flatLocal.getRowColumn(2, 3), 15.0f, 0.01f));
  EXPECT_TRUE(
      NearlyEqual(local.getRowColumn(1, 3) - flatLocal.getRowColumn(1, 3), 25.980762f, 0.01f));
}

/**
 * Test case: compound keyframes compose in CSS order and keep the translation authored.
 */
PAG_TEST(PAGXHTML3DTest, AnimatedCompoundKeyframesCompose) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes move {
        0%   { transform: translateX(0px) rotate(0deg); }
        100% { transform: translateX(40px) rotate(90deg); }
      }
    </style></head>
    <body style="width:200px;height:200px">
      <div id="box" style="width:50px;height:50px;background-color:#000;
           animation:move 1s linear infinite"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(NearlyEqual(layer->anchor.x, 25.0f));
  EXPECT_TRUE(NearlyEqual(layer->anchor.y, 25.0f));
  auto* matrixChannel = FindMatrixChannel(FirstAnimation(doc));
  ASSERT_NE(matrixChannel, nullptr);
  ASSERT_EQ(matrixChannel->keyframes.size(), 2u);
  const auto& last = matrixChannel->keyframes.back().value;
  const auto expected = pagx::Matrix::Translate(40, 0) * pagx::Matrix::Rotate(90);
  EXPECT_TRUE(NearlyEqual(last.a, expected.a, 0.001f));
  EXPECT_TRUE(NearlyEqual(last.b, expected.b, 0.001f));
  EXPECT_TRUE(NearlyEqual(last.c, expected.c, 0.001f));
  EXPECT_TRUE(NearlyEqual(last.d, expected.d, 0.001f));
  EXPECT_TRUE(NearlyEqual(last.tx, 40.0f, 0.001f));
  EXPECT_TRUE(NearlyEqual(last.ty, 0.0f, 0.001f));
}

// ---------------------------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------------------------

/**
 * Test case: the anchored form and the legacy baked form agree pixel for pixel at a keyframe time,
 * including the wrapper-sensitive content (filter, box-shadow, child layer). This is the
 * new-vs-old equivalence check for the 2D path: same pose, two encodings.
 */
PAGX_TEST(PAGXHTML3DRenderTest, Animated2DAnchorRendersLikeBakedMatrix) {
  const std::string html = R"HTML(
    <html><head><style>
      @keyframes spinFx {
        0%   { transform: rotate(0deg); }
        50%  { transform: rotate(90deg); }
        100% { transform: rotate(180deg); }
      }
    </style></head>
    <body style="width:200px;height:200px">
      <div id="box" style="position:absolute;left:60px;top:50px;width:60px;height:40px;
           background-color:#6366F1;box-shadow:0 2px 6px #0008;
           filter:drop-shadow(4px 0 0 #F472B6);
           transform-origin:10px 30px;animation:spinFx 1s linear infinite">
        <div style="position:absolute;left:6px;top:6px;width:20px;height:20px;
             background-color:#F8FAFC"></div>
      </div>
    </body></html>
  )HTML";
  auto anchored = ParseHtml(html);
  auto baked = ParseHtml(html);
  ASSERT_NE(anchored, nullptr);
  ASSERT_NE(baked, nullptr);
  auto* anchoredAnimation = FirstAnimation(anchored);
  auto* bakedAnimation = FirstAnimation(baked);
  ASSERT_NE(anchoredAnimation, nullptr);
  ASSERT_NE(bakedAnimation, nullptr);
  auto* bakedLayer = FindLayerById(baked, ChannelTarget(bakedAnimation, "matrix"));
  ASSERT_NE(bakedLayer, nullptr);
  ASSERT_FALSE(HasZeroAnchor(bakedLayer)) << "the HTML origin must produce an anchor";
  auto* matrixChannel = FindMatrixChannel(bakedAnimation);
  ASSERT_NE(matrixChannel, nullptr);
  ASSERT_GE(matrixChannel->keyframes.size(), 2u);
  // Legacy encoding: fold the pivot into every keyframe and drop the anchor.
  for (auto& keyframe : matrixChannel->keyframes) {
    keyframe.value = pagx::Matrix::Translate(bakedLayer->anchor.x, bakedLayer->anchor.y) *
                     keyframe.value *
                     pagx::Matrix::Translate(-bakedLayer->anchor.x, -bakedLayer->anchor.y);
  }
  bakedLayer->anchor = {};
  bakedLayer->anchorZ = 0.0f;

  // Sample the middle keyframe (50% of a 1s animation) so both documents hold an exact pose.
  constexpr int64_t kMiddleKeyframeUs = 500'000;
  auto anchoredPixels = RenderFrame(anchored, kMiddleKeyframeUs);
  auto bakedPixels = RenderFrame(baked, kMiddleKeyframeUs);
  ASSERT_FALSE(anchoredPixels.empty());
  ASSERT_FALSE(bakedPixels.empty());
  ASSERT_EQ(anchoredPixels.size(), bakedPixels.size());
  EXPECT_EQ(CountDifferentPixels(anchoredPixels, bakedPixels), 0u);
}

/**
 * Test case: a static 3D pose with a non-centre origin renders exactly like the same document with
 * the pivot baked into `matrix3D`, which pins "anchor is applied once, outside the projection".
 */
PAGX_TEST(PAGXHTML3DRenderTest, Imported3DAnchorRendersLikeBakedMatrix3D) {
  const std::string html = R"HTML(
    <html><body style="width:200px;height:200px">
      <div id="box" style="position:absolute;left:60px;top:50px;width:80px;height:60px;
           background-color:#6366F1;
           transform:perspective(500px) rotateY(25deg);transform-origin:10px 40px"></div>
    </body></html>
  )HTML";
  auto anchored = ParseHtml(html);
  auto baked = ParseHtml(html);
  ASSERT_NE(anchored, nullptr);
  ASSERT_NE(baked, nullptr);
  auto* anchoredLayer = FindLayerById(anchored, "box");
  auto* bakedLayer = FindLayerById(baked, "box");
  ASSERT_NE(anchoredLayer, nullptr);
  ASSERT_NE(bakedLayer, nullptr);
  ASSERT_FALSE(anchoredLayer->matrix3D.isIdentity());
  ASSERT_FALSE(HasZeroAnchor(anchoredLayer)) << "the HTML origin must produce an anchor";

  bakedLayer->matrix3D =
      pagx::ApplyAnchorToMatrix(pagx::ComposeLayerTransform3DWithoutAnchor(bakedLayer),
                                bakedLayer->anchor, bakedLayer->anchorZ);
  bakedLayer->matrix = {};
  bakedLayer->anchor = {};
  bakedLayer->anchorZ = 0.0f;

  auto anchoredPixels = RenderFrame(anchored);
  auto bakedPixels = RenderFrame(baked);
  ASSERT_FALSE(anchoredPixels.empty());
  ASSERT_EQ(anchoredPixels.size(), bakedPixels.size());
  EXPECT_EQ(CountDifferentPixels(anchoredPixels, bakedPixels), 0u);
}

/**
 * Test case: a percentage / keyword `transform-origin` cannot resolve against an unknown reference
 * box, but the shared size chain falls back to the layer's final size. Without that step the static
 * and the animated pivot could disagree on the same element.
 */
PAG_TEST(PAGXHTML3DTest, LayerSizeRescuesAnUnresolvablePercentageOrigin) {
  pagx::HTMLDiagnosticSink diagnostics(false);
  float canvasWidth = 200.0f;
  float canvasHeight = 120.0f;
  pagx::HTMLValueParser parser(diagnostics, canvasWidth, canvasHeight);
  const pagx::HTMLTransformReference unknown = {NAN, NAN};

  const pagx::HTMLElementAnchor withoutLayer =
      pagx::ResolveElementAnchor("50% 50%", unknown, nullptr, parser);
  EXPECT_FALSE(withoutLayer.resolved)
      << "no reference box and no layer: nothing to resolve against";
  EXPECT_TRUE(withoutLayer.valid) << "the declaration itself is legal";

  pagx::Layer layer = {};
  layer.width = 100.0f;
  layer.height = 60.0f;
  const pagx::HTMLElementAnchor withLayer =
      pagx::ResolveElementAnchor("50% 50%", unknown, &layer, parser);
  ASSERT_TRUE(withLayer.resolved) << "the layer size is the last step of the fallback chain";
  EXPECT_TRUE(NearlyEqual(withLayer.point.x, 50.0f, 0.001f));
  EXPECT_TRUE(NearlyEqual(withLayer.point.y, 30.0f, 0.001f));
}

/**
 * Test case: an `em` pivot is approximated as 16px, and that approximation is reported once. The
 * cascade resolves the value provisionally before the layer exists, so it must not repeat the note.
 */
PAG_TEST(PAGXHTML3DTest, EmOriginWarnsOnce) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div id="box" style="position:absolute;left:0;top:0;width:60px;height:40px;
           background-color:#000;transform:rotate(10deg);transform-origin:1em 0"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  int emWarnings = 0;
  for (const auto& message : doc->errors) {
    if (message.find("em/rem") != std::string::npos) {
      emWarnings++;
    }
  }
  EXPECT_EQ(emWarnings, 1) << "the provisional cascade pass must stay silent";
  auto* box = FindLayerById(doc, "box");
  ASSERT_NE(box, nullptr);
  EXPECT_TRUE(NearlyEqual(box->anchor.x, 16.0f, 0.001f));
}
/**
 * Test case: a `preserve-3d` element with a background and padding gets an inner host, and the
 * children hang from that host. The outer layer keeps `preserve-3d`, but if the host drops it the
 * subtree is flattened there and the parent camera silently stops at the host.
 */
PAG_TEST(PAGXHTML3DTest, Preserve3DSurvivesTheInnerHost) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div style="position:absolute;left:0;top:0;width:200px;height:120px;perspective:100px;
           perspective-origin:0px 0px">
        <div id="stage" style="position:absolute;left:0;top:0;width:200px;height:120px;
             transform-style:preserve-3d;background-color:#ffffff;padding:4px">
          <div id="card" style="position:absolute;left:0;top:0;width:20px;height:10px;
               background-color:#FF0000;transform:translateZ(50px)"></div>
        </div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* stage = FindLayerById(doc, "stage");
  ASSERT_NE(stage, nullptr);
  pagx::Layer* host = nullptr;
  for (auto* candidate : stage->children) {
    if (NearlyEqual(candidate->percentWidth, 100.0f, 1e-4f) &&
        NearlyEqual(candidate->percentHeight, 100.0f, 1e-4f)) {
      host = candidate;
    }
  }
  ASSERT_NE(host, nullptr)
      << "a background + padding element hangs its children from an inner host";
  EXPECT_TRUE(host->preserve3D)
      << "the host is the layer the children hang from, so it has to keep the 3D context too";
  // Rendering is what the flag is for: at `perspective: 100` and `translateZ(50)` the 20x10 block
  // has to be painted twice its size. A flattened host would paint it at 20x10.
  const auto bounds = RedBoundsOf(RenderFrame(RoundTripForRender(doc)));
  EXPECT_NEAR(bounds.width, 40, 1) << "the parent camera has to reach through the inner host";
  EXPECT_NEAR(bounds.height, 20, 1);
}

/**
 * Test case: `perspective-origin: 0 0` resolves while the declaring box has no absolute size, so the
 * wrapper would be sized NaN and collapse every percentage inside it. The camera is dropped and
 * reported instead.
 */
PAG_TEST(PAGXHTML3DTest, CameraWrapperRequiresAFiniteReferenceSize) {
  auto cameraDoc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div style="position:absolute;left:0;top:0;perspective:600px;perspective-origin:0px 0px">
        <div id="card" style="width:50%;height:50%;background-color:#FF0000;
             transform:rotateY(20deg)"></div>
      </div>
    </body></html>
  )HTML");
  auto plainDoc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div style="position:absolute;left:0;top:0">
        <div id="card" style="width:50%;height:50%;background-color:#FF0000;
             transform:rotateY(20deg)"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(cameraDoc, nullptr);
  ASSERT_NE(plainDoc, nullptr);
  auto* card = FindLayerById(cameraDoc, "card");
  ASSERT_NE(card, nullptr);
  EXPECT_EQ(CameraWrapperOf(cameraDoc, card), nullptr)
      << "a NaN sized wrapper would be the child's containing block";
  EXPECT_TRUE(HasDiagnosticContaining(cameraDoc, "cannot be projected"))
      << "dropping the projection has to be reported";
  // The percentage child is the point of the test: a NaN sized wrapper collapsed it, so compare the
  // painted frame against the same page without a camera instead of only reading the fields.
  const auto withCamera = RenderFrame(RoundTripForRender(cameraDoc));
  const auto without = RenderFrame(RoundTripForRender(plainDoc));
  ASSERT_FALSE(withCamera.empty());
  ASSERT_FALSE(without.empty());
  EXPECT_EQ(withCamera, without) << "an unusable camera must leave the layout untouched";
  // 50% of the 200x120 box, so a wrapper that collapsed the percentage would paint nothing here.
  EXPECT_TRUE(NearlyEqual(card->percentWidth, 50.0f, 1e-4f));
}

/**
 * Test case: a child positioned only by `right` / `bottom` is still positioned. It used to be judged
 * un-wrappable, which silently dropped the parent projection for corner badges and similar elements.
 */
PAG_TEST(PAGXHTML3DTest, RightBottomPositionedChildKeepsTheProjection) {
  auto cameraDoc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div style="position:absolute;left:0;top:0;width:200px;height:120px;perspective:600px;
           perspective-origin:40px 30px">
        <div id="card" style="position:absolute;right:10px;bottom:10px;width:50%;height:50%;
             background-color:#FF0000"></div>
      </div>
    </body></html>
  )HTML");
  auto plainDoc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div style="position:absolute;left:0;top:0;width:200px;height:120px">
        <div id="card" style="position:absolute;right:10px;bottom:10px;width:50%;height:50%;
             background-color:#FF0000"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(cameraDoc, nullptr);
  ASSERT_NE(plainDoc, nullptr);
  auto* card = FindLayerById(cameraDoc, "card");
  ASSERT_NE(card, nullptr);
  auto* wrapper = CameraWrapperOf(cameraDoc, card);
  ASSERT_NE(wrapper, nullptr) << "right / bottom is an anchor like left / top";
  EXPECT_TRUE(NearlyEqual(wrapper->perspective, 600.0f, 0.01f));
  EXPECT_TRUE(NearlyEqual(wrapper->left, 40.0f, 1e-3f));
  EXPECT_TRUE(NearlyEqual(wrapper->top, 30.0f, 1e-3f));
  EXPECT_TRUE(NearlyEqual(card->right, 50.0f, 1e-3f)) << "right grows by the wrapper offset";
  EXPECT_TRUE(NearlyEqual(card->bottom, 40.0f, 1e-3f));
  EXPECT_FALSE(HasProjectionRow(card)) << "the camera stays on the wrapper";
  // A flat pose is the identity under the camera, so the painted frame must match the page without
  // one: 50% of 200x100 is 100x60, pinned to the bottom-right corner.
  const auto withCamera = RenderFrame(RoundTripForRender(cameraDoc));
  const auto without = RenderFrame(RoundTripForRender(plainDoc));
  ASSERT_FALSE(withCamera.empty());
  ASSERT_FALSE(without.empty());
  EXPECT_EQ(withCamera, without) << "a flat child must be placed exactly like CSS places it";
  const auto bounds = BoundsOf(withCamera);
  EXPECT_NEAR(bounds.width, 100, 1);
  EXPECT_NEAR(bounds.height, 60, 1);
}

}  // namespace pag
