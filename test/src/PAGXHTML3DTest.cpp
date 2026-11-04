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

// HTML importer coverage for the 3D transform and anchor work. The importer must stop projecting
// `matrix3d(...)` onto its 2D affine subset, compose the whole `transform` function list into a
// 4x4, and keep `transform-origin` on `Layer.anchor` instead of baking it into the matrix, so
// matrix / matrix3D keyframes stay un-pivoted and the runtime applies the pivot once.
//
// Static 2D anchoring has no observable effect on its own (a baked matrix is an exact substitute),
// so the anchor cases here are animation-focused; the few static cases cover the 3D output only.

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
#include "pagx/types/Matrix.h"
#include "pagx/types/Matrix3D.h"
#include "pagx/types/Point.h"
#include "pagx/utils/ExporterUtils.h"
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
std::vector<uint8_t> RenderFrame(const std::shared_ptr<pagx::PAGXDocument>& doc) {
  std::vector<uint8_t> pixels = {};
  if (doc == nullptr) {
    return pixels;
  }
  auto scene = pagx::PAGScene::Make(doc);
  if (scene == nullptr) {
    return pixels;
  }
  scene->advanceAndApply(0);
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
// Static 3D output. Only a few cases: the runtime already pins the anchor math (PAGXAnchorTest),
// so the importer side only has to prove it stops projecting 3D onto the 2D plane.
// ---------------------------------------------------------------------------------------------

/**
 * Test case: a 3D rotation reaches `matrix3D` instead of being flattened onto `matrix`.
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
  ASSERT_FALSE(layer->matrix3D.isIdentity());
  EXPECT_TRUE(layer->matrix.isIdentity()) << "3D must not also fill the 2D matrix";
  EXPECT_TRUE(NearlyEqual(layer->matrix3D.getRowColumn(1, 1), 1.0f));
  EXPECT_TRUE(NearlyEqual(layer->matrix3D.getRowColumn(0, 0), 0.866025f, 0.001f));
  EXPECT_TRUE(NearlyEqual(std::fabs(layer->matrix3D.getRowColumn(0, 2)), 0.5f, 0.001f));
  EXPECT_TRUE(NearlyEqual(std::fabs(layer->matrix3D.getRowColumn(2, 0)), 0.5f, 0.001f));
}

/**
 * Test case: `perspective()` inside the function list lands on the projection row (row 3), which is
 * the row `ApplyAnchorToMatrix` leaves outside the pivot.
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
  ASSERT_FALSE(layer->matrix3D.isIdentity());
  EXPECT_TRUE(NearlyEqual(layer->matrix3D.getRowColumn(3, 2), -0.002f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(layer->matrix3D.getRowColumn(3, 0), 0.0f, 1e-5f));
  EXPECT_TRUE(NearlyEqual(layer->matrix3D.getRowColumn(3, 3), 1.0f, 1e-5f));
}

/**
 * Test case: `matrix3d(...)` is written through verbatim; the orthographic projection is gone, so
 * the depth column and the perspective row survive.
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
 * Test case: `transform-style: preserve-3d` maps onto the layer flag.
 */
PAG_TEST(PAGXHTML3DTest, TransformStylePreserve3DMapsToLayer) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100px;height:60px;background-color:#6366F1;
           transform-style:preserve-3d;transform:rotateY(10deg)">
        <div style="width:20px;height:20px;background-color:#F472B6;transform:translateZ(30px)"></div>
      </div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(layer->preserve3D);
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
  EXPECT_TRUE(NearlyEqual(layers.front()->matrix3D.getRowColumn(3, 2), -0.0025f, 1e-5f));
}

// ---------------------------------------------------------------------------------------------
// Animated transforms. This is where the pivot matters: baked into the keyframes the pivot
// translation is interpolated on its own (drift), while a static anchor keeps the pivot fixed.
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
 * Test case: 3D keyframes route through the `matrix3D` channel and still leave the pivot on the
 * layer, so the runtime can apply it once per frame.
 */
PAG_TEST(PAGXHTML3DTest, Animated3DRotationKeepsAnchor) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes flip {
        0%   { transform: perspective(500px) rotateY(0deg); }
        100% { transform: perspective(500px) rotateY(60deg); }
      }
    </style></head>
    <body style="width:200px;height:200px">
      <div id="card" style="width:100px;height:60px;background-color:#000;
           animation:flip 1s linear infinite"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "card");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(NearlyEqual(layer->anchor.x, 50.0f));
  EXPECT_TRUE(NearlyEqual(layer->anchor.y, 30.0f));
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  EXPECT_EQ(FindChannel(animation, "matrix"), nullptr) << "3D keys must not collapse to matrix";
  auto* channel = FindMatrix3DChannel(animation);
  ASSERT_NE(channel, nullptr);
  ASSERT_EQ(channel->keyframes.size(), 2u);
  for (const auto& keyframe : channel->keyframes) {
    EXPECT_TRUE(NearlyEqual(keyframe.value.getRowColumn(3, 2), -0.002f, 1e-5f));
    EXPECT_TRUE(NearlyEqual(keyframe.value.getRowColumn(0, 3), 0.0f, 1e-5f));
    EXPECT_TRUE(NearlyEqual(keyframe.value.getRowColumn(1, 3), 0.0f, 1e-5f));
  }
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
// Rendering
// ---------------------------------------------------------------------------------------------

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
