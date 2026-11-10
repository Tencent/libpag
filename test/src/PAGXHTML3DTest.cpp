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
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "base/PAGTest.h"
#include "pagx/HTMLImporter.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/AnimationObject.h"
#include "pagx/nodes/Channel.h"
#include "pagx/nodes/Layer.h"
#include "pagx/runtime/Matrix3DDecompose.h"
#include "pagx/types/Matrix.h"
#include "pagx/types/Matrix3D.h"
#include "pagx/utils/LayerTransformUtils.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Pixmap.h"

namespace pag {
namespace {

constexpr float kEps = 0.01f;
constexpr int DocSize = 200;

bool NearlyEqual(float a, float b, float eps = kEps) {
  return std::fabs(a - b) <= eps;
}

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

bool HasDiagnosticContaining(const std::shared_ptr<pagx::PAGXDocument>& doc,
                             const std::string& needle) {
  if (doc == nullptr) {
    return false;
  }
  for (const auto& message : doc->errors) {
    if (message.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
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

// Renders one frame through the runtime path (PAGScene + PAGSurface), matching the CLI player.
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
  EXPECT_FALSE(doc->errors.empty());
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
  // T(60, 40) * P(600) * T(-60, -40) * rotateY(30): the projection is conjugated with the parent's
  // `perspective-origin` (its centre by default), which also mixes into the linear rows.
  EXPECT_TRUE(NearlyEqual(pose.getRowColumn(0, 0), 0.916025f, 0.001f));
  EXPECT_TRUE(NearlyEqual(pose.getRowColumn(0, 2), 0.413397f, 0.001f));
  // The projection row is `-1/600` times the rotation's Z row: (sin30, 0, -cos30) / 600.
  EXPECT_TRUE(NearlyEqual(pose.getRowColumn(3, 0), 0.000833f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(pose.getRowColumn(3, 1), 0.0f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(pose.getRowColumn(3, 2), -0.001443f, 1e-5f));
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
  EXPECT_TRUE(NearlyEqual(card->matrix3D.getRowColumn(3, 0), 0.000833f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(card->matrix3D.getRowColumn(3, 2), -0.001443f, 1e-5f));
}

/**
 * Test case: the parent `perspective` also projects animated keyframes, and the pivot stays on the
 * child layer so the interpolation never carries a pivot translation.
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
  EXPECT_TRUE(NearlyEqual(start.getRowColumn(3, 2), -0.001667f, 1e-5f))
      << "the projection row comes from the parent perspective, not from the keyframe";
  EXPECT_TRUE(NearlyEqual(start.getRowColumn(0, 3), 0.0f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(start.getRowColumn(2, 3), 0.0f, 1e-5f));
  const auto& end = channel->keyframes.back().value;
  EXPECT_TRUE(NearlyEqual(end.getRowColumn(3, 0), 0.001443f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(end.getRowColumn(3, 2), -0.000833f, 1e-5f));
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
 * Test case: a tiny uniform scale under a perspective parent stays invertible. A fixed determinant
 * threshold would read `scale(0.001)` as singular (det ~ 1e-6) and drop the whole transform, while
 * PAGX's own validator accepts it.
 */
PAG_TEST(PAGXHTML3DTest, TinyScaleUnderPerspectiveStaysInvertible) {
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
  EXPECT_FALSE(tiny->matrix3D.isIdentity()) << "the transform must survive the sanity check";
  EXPECT_TRUE(NearlyEqual(tiny->matrix3D.getRowColumn(0, 0), 0.001f, 1e-6f));
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
  // The projection itself still applies; only the centre is approximate.
  auto* a = FindLayerById(doc, "a");
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->matrix3D.getRowColumn(3, 2), 0.0f);
}

/**
 * Test case: a `perspective` element whose own box has no px size (percentage / auto) cannot resolve
 * the default `perspective-origin` (the box centre). That has to be reported once per declaring
 * element instead of silently projecting about the layer origin.
 */
PAG_TEST(PAGXHTML3DTest, UnknownCameraBoxSizeWarnsAboutTheVanishingPoint) {
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
  // The documented fallback is the layer origin, i.e. the plain projection: no centre conjugation.
  auto* card = FindLayerById(doc, "card");
  ASSERT_NE(card, nullptr);
  EXPECT_TRUE(NearlyEqual(card->matrix3D.getRowColumn(3, 0), 0.000833f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(card->matrix3D.getRowColumn(3, 2), -0.001443f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(card->matrix3D.getRowColumn(0, 0), 0.866025f, 0.001f));
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
  auto layers = LayersWithNonIdentityMatrix3D(doc);
  ASSERT_FALSE(layers.empty()) << "the restored text host must carry its 3D pose";
  bool sawProjection = false;
  for (auto* layer : layers) {
    if (std::fabs(layer->matrix3D.getRowColumn(3, 2)) > 1e-6f) {
      sawProjection = true;
    }
  }
  EXPECT_TRUE(sawProjection) << "the parent perspective must reach the text host pose";
}

/**
 * Test case: `scale(0)` under a perspective parent only becomes non-invertible after the camera is
 * folded in. PAGX requires a 3D pose to be invertible, so it has to be rejected with a diagnostic
 * rather than written out.
 */
PAG_TEST(PAGXHTML3DTest, ScaleZeroUnderPerspectiveIsRejected) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:160px">
      <div style="position:absolute;left:0;top:0;width:200px;height:160px;perspective:600px">
        <div id="card" style="position:absolute;left:10px;top:10px;width:80px;height:40px;
             background-color:#38BDF8;transform:scale(0)"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "non-invertible"));
  auto* card = FindLayerById(doc, "card");
  ASSERT_NE(card, nullptr);
  EXPECT_TRUE(card->matrix3D.isIdentity()) << "an invalid 3D pose must not be written";
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
  // The warning is not theoretical: the keyframes really do carry the projection.
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  auto* channel = FindMatrix3DChannel(animation);
  ASSERT_NE(channel, nullptr);
  ASSERT_FALSE(channel->keyframes.empty());
  EXPECT_GT(std::fabs(channel->keyframes.front().value.getRowColumn(3, 2)), 1e-6f);
}

/**
 * Test case: an absolutely positioned child placed by `right` / `bottom` has no `left` / `top`, so
 * the parent camera cannot be expressed in its local coordinates; that has to be reported instead of
 * silently pinning the vanishing point to the parent origin.
 */
PAG_TEST(PAGXHTML3DTest, RightPositionedChildUnderPerspectiveWarns) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:120px">
      <div style="position:absolute;left:0;top:0;width:200px;height:120px;perspective:600px">
        <div style="position:absolute;right:10px;top:10px;width:80px;height:60px;
             background-color:#38BDF8;transform:rotateY(30deg)"></div>
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
  EXPECT_EQ(warnings, 1);
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
  const float projection =
      std::fabs(card->matrix3D.getRowColumn(3, 0)) + std::fabs(card->matrix3D.getRowColumn(3, 2));
  EXPECT_GT(projection, 1e-5f) << "the body camera must reach the child pose";

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
 * Test case: a flat stop that is singular (`scale(0)`) cannot be lifted onto the `matrix3D` channel
 * when another stop of the same animation is 3D. PAGX requires every `matrix3D` keyframe to be
 * invertible, so the stop is dropped with one diagnostic instead of being written out invalid.
 */
PAG_TEST(PAGXHTML3DTest, SingularFlatStopIsNotLiftedIntoMatrix3D) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes k { 0% { transform: scale(0); } 100% { transform: rotateY(45deg); } }
    </style></head>
    <body style="width:200px;height:120px">
      <div id="card" style="position:absolute;left:20px;top:20px;width:80px;height:60px;
           background-color:#38BDF8;animation:k 1s linear infinite"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  EXPECT_TRUE(HasDiagnosticContaining(doc, "not invertible"));
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  auto* channel = FindMatrix3DChannel(animation);
  ASSERT_NE(channel, nullptr);
  ASSERT_FALSE(channel->keyframes.empty());
  for (const auto& key : channel->keyframes) {
    // The dropped stop would lift to an all-zero linear block.
    EXPECT_GT(std::fabs(key.value.getRowColumn(0, 0)), 1e-6f) << "time " << key.time;
    EXPECT_GT(std::fabs(key.value.getRowColumn(1, 1)), 1e-6f) << "time " << key.time;
  }
}

/**
 * Test case: a singular static pose (`scale(0)`) under a 3D keyframe track. The fill-mode holds
 * cannot carry an all-zero `matrix3D` (PAGX rejects non-invertible keyframes and a re-import would
 * read the identity), so they collapse to a tiny invertible scale instead of breaking the file.
 */
PAG_TEST(PAGXHTML3DTest, SingularStaticPoseKeepsTheFillHoldsInvertible) {
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
  // `scale(0)` renders nothing; the holds keep that by collapsing the linear block.
  EXPECT_LT(std::fabs(channel->keyframes.front().value.getRowColumn(0, 0)), 1e-3f);
  EXPECT_GT(std::fabs(channel->keyframes.front().value.getRowColumn(0, 0)), 1e-6f);
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
  const char* invalid[] = {"rotate(45px)", "rotate(45)", "skewX(1foo)", "rotate3d(0, 0, 1, 30px)"};
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
 * Test case: a `calc()` length argument resolves against the element box, including percentages and
 * nested parentheses. The X axis is relative to the width, the Y axis to the height.
 */
PAG_TEST(PAGXHTML3DTest, CalcLengthArgumentsResolveAgainstTheElementBox) {
  struct CalcCase {
    const char* transform;
    float tx;
    float ty;
  };
  // Box is 80x40.
  const CalcCase cases[] = {
      {"translateX(calc(100% - 10px))", 70.0f, 0.0f},
      {"translateY(calc(50% + 4px))", 0.0f, 24.0f},
      {"translate(calc((100% - 40px) / 2), calc(25% * 2))", 20.0f, 20.0f},
      {"translateX(calc(10px + 5px))", 15.0f, 0.0f},
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

}  // namespace pag
