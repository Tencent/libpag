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
//  license is distributed on an "as is" BASIS, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

// HTML importer anchor coverage: CSS `transform-origin` must land on `Layer.anchor` while
// `Layer.matrix` keeps the raw CSS value (pure translation), so matrix keyframes never carry a
// baked pivot. See spec/pagx_spec.md 5.2 and Layer.h.

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

void CollectLayersWithMatrix(pagx::Layer* layer, std::vector<pagx::Layer*>* out) {
  if (layer == nullptr) {
    return;
  }
  if (!layer->matrix.isIdentity()) {
    out->push_back(layer);
  }
  for (auto* child : layer->children) {
    CollectLayersWithMatrix(child, out);
  }
}

std::vector<pagx::Layer*> LayersWithNonIdentityMatrix(
    const std::shared_ptr<pagx::PAGXDocument>& doc) {
  std::vector<pagx::Layer*> layers = {};
  if (doc == nullptr) {
    return layers;
  }
  for (auto* layer : doc->layers) {
    CollectLayersWithMatrix(layer, &layers);
  }
  return layers;
}

bool HasZeroAnchor(const pagx::Layer* layer) {
  return NearlyEqual(layer->anchor.x, 0.0f) && NearlyEqual(layer->anchor.y, 0.0f) &&
         NearlyEqual(layer->anchorZ, 0.0f);
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

pagx::TypedChannel<pagx::Matrix>* FindMatrixChannel(pagx::Animation* animation) {
  auto* channel = FindChannel(animation, "matrix");
  if (channel == nullptr || channel->valueType() != pagx::ChannelValueType::Matrix) {
    return nullptr;
  }
  return static_cast<pagx::TypedChannel<pagx::Matrix>*>(channel);
}

std::string MatrixChannelTarget(pagx::Animation* animation) {
  if (animation == nullptr) {
    return {};
  }
  for (auto* object : animation->objects) {
    for (auto* channel : object->channels) {
      if (channel != nullptr && channel->name == "matrix") {
        return object->target;
      }
    }
  }
  return {};
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

/**
 * Test case: the CSS default origin (box centre) becomes an anchor, and the matrix keeps the raw
 * CSS rotation without the T(centre) * R * T(-centre) translation.
 */
PAG_TEST(PAGXHTMLAnchorTest, RotateUsesCenterAnchor) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100px;height:60px;background-color:#6366F1;
           transform:rotate(90deg)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(NearlyEqual(layer->anchor.x, 50.0f));
  EXPECT_TRUE(NearlyEqual(layer->anchor.y, 30.0f));
  EXPECT_TRUE(NearlyEqual(layer->anchorZ, 0.0f));
  // Raw rotate(90deg): zero linear diagonal, unit skew terms, no translation.
  EXPECT_TRUE(NearlyEqual(layer->matrix.a, 0.0f));
  EXPECT_TRUE(NearlyEqual(layer->matrix.d, 0.0f));
  EXPECT_TRUE(NearlyEqual(std::fabs(layer->matrix.b), 1.0f));
  EXPECT_TRUE(NearlyEqual(std::fabs(layer->matrix.c), 1.0f));
  EXPECT_TRUE(NearlyEqual(layer->matrix.tx, 0.0f));
  EXPECT_TRUE(NearlyEqual(layer->matrix.ty, 0.0f));
}

/**
 * Test case: an explicit px origin is forwarded verbatim as the anchor.
 */
PAG_TEST(PAGXHTMLAnchorTest, ExplicitPxOriginBecomesAnchor) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100px;height:60px;background-color:#6366F1;
           transform:rotate(30deg);transform-origin:10px 20px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(NearlyEqual(layer->anchor.x, 10.0f));
  EXPECT_TRUE(NearlyEqual(layer->anchor.y, 20.0f));
  EXPECT_TRUE(NearlyEqual(layer->matrix.tx, 0.0f));
  EXPECT_TRUE(NearlyEqual(layer->matrix.ty, 0.0f));
  EXPECT_FALSE(HasDiagnosticContaining(doc, "transform-origin"));
}

/**
 * Test case: a top-left origin leaves the anchor at zero, so the legacy matrix-only form (which is
 * already the complete transform when the pivot is the local origin) is preserved.
 */
PAG_TEST(PAGXHTMLAnchorTest, TopLeftOriginKeepsZeroAnchor) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100px;height:60px;background-color:#6366F1;
           transform:rotate(30deg);transform-origin:0px 0px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(HasZeroAnchor(layer));
  EXPECT_TRUE(NearlyEqual(layer->matrix.tx, 0.0f));
  EXPECT_TRUE(NearlyEqual(layer->matrix.ty, 0.0f));
  EXPECT_TRUE(NearlyEqual(layer->matrix.a, std::cos(30.0f * 3.14159265f / 180.0f), 0.001f));
}

/**
 * Test case: a pure translation has an identity linear part, so the pivot cancels out and no
 * anchor is emitted; the matrix keeps the CSS translation.
 */
PAG_TEST(PAGXHTMLAnchorTest, PureTranslationSkipsAnchor) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100px;height:60px;background-color:#6366F1;
           transform:translate(20px, 10px);transform-origin:10px 8px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(HasZeroAnchor(layer));
  EXPECT_TRUE(NearlyEqual(layer->matrix.tx, 20.0f));
  EXPECT_TRUE(NearlyEqual(layer->matrix.ty, 10.0f));
}

/**
 * Test case: percentage origins resolve against the box size instead of warning.
 */
PAG_TEST(PAGXHTMLAnchorTest, PercentOriginResolvesToAnchor) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100px;height:60px;background-color:#6366F1;
           transform:scale(0.5);transform-origin:100% 50%"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(NearlyEqual(layer->anchor.x, 100.0f));
  EXPECT_TRUE(NearlyEqual(layer->anchor.y, 30.0f));
  EXPECT_FALSE(HasDiagnosticContaining(doc, "transform-origin"));
}

/**
 * Test case: keyword origins resolve against the box size instead of warning.
 */
PAG_TEST(PAGXHTMLAnchorTest, KeywordOriginResolvesToAnchor) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100px;height:60px;background-color:#6366F1;
           transform:rotate(-15deg);transform-origin:right bottom"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(NearlyEqual(layer->anchor.x, 100.0f));
  EXPECT_TRUE(NearlyEqual(layer->anchor.y, 60.0f));
  EXPECT_FALSE(HasDiagnosticContaining(doc, "transform-origin"));
}

/**
 * Test case: the third `transform-origin` value is the pivot depth and lands on anchorZ.
 */
PAG_TEST(PAGXHTMLAnchorTest, ThreeValueOriginWritesAnchorZ) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100px;height:60px;background-color:#6366F1;
           transform:rotate(10deg);transform-origin:20px 10px 25px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(NearlyEqual(layer->anchor.x, 20.0f));
  EXPECT_TRUE(NearlyEqual(layer->anchor.y, 10.0f));
  EXPECT_TRUE(NearlyEqual(layer->anchorZ, 25.0f));
}

/**
 * Test case: skews around a non-centre corner honour the requested pivot.
 */
PAG_TEST(PAGXHTMLAnchorTest, SkewCornerOriginBecomesAnchor) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:140px">
      <div id="box" style="width:90px;height:60px;background-color:#6366F1;
           transform:skewX(-12deg);transform-origin:0px 60px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(NearlyEqual(layer->anchor.x, 0.0f));
  EXPECT_TRUE(NearlyEqual(layer->anchor.y, 60.0f));
}

/**
 * Test case: the composed local transform of the anchor form equals the CSS conjugation
 * T(origin) * M * T(-origin) that the importer used to bake into the matrix.
 */
PAG_TEST(PAGXHTMLAnchorTest, AnchorMatchesCssBakedMatrix) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100px;height:60px;background-color:#6366F1;
           transform:rotate(30deg);transform-origin:10px 20px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  ASSERT_FALSE(HasZeroAnchor(layer)) << "The CSS origin must be carried by the anchor";
  const auto actual = pagx::BuildLayerMatrix(layer);
  const auto raw = layer->matrix;
  const auto expected = pagx::Matrix::Translate(layer->anchor.x, layer->anchor.y) * raw *
                        pagx::Matrix::Translate(-layer->anchor.x, -layer->anchor.y);
  EXPECT_TRUE(NearlyEqual(actual.a, expected.a, 0.001f));
  EXPECT_TRUE(NearlyEqual(actual.b, expected.b, 0.001f));
  EXPECT_TRUE(NearlyEqual(actual.c, expected.c, 0.001f));
  EXPECT_TRUE(NearlyEqual(actual.d, expected.d, 0.001f));
  EXPECT_TRUE(NearlyEqual(actual.tx, expected.tx, 0.001f));
  EXPECT_TRUE(NearlyEqual(actual.ty, expected.ty, 0.001f));
}

/**
 * Test case: an unresolved origin (relative box size) warns and keeps the top-left pivot instead of
 * silently emitting a wrong anchor.
 */
PAG_TEST(PAGXHTMLAnchorTest, UnknownBoxSizeWarnsAndKeepsZeroAnchor) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100%;height:60px;background-color:#6366F1;
           transform:rotate(10deg)"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(HasZeroAnchor(layer));
  EXPECT_TRUE(HasDiagnosticContaining(doc, "transform-origin"));
}

/**
 * Test case: replaced elements (img) follow the same origin-to-anchor mapping.
 */
PAG_TEST(PAGXHTMLAnchorTest, ImageTransformOriginBecomesAnchor) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <img id="logo" src="logo.png" style="width:60px;height:40px;
           transform:rotate(15deg);transform-origin:0px 40px"/>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "logo");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(NearlyEqual(layer->anchor.x, 0.0f));
  EXPECT_TRUE(NearlyEqual(layer->anchor.y, 40.0f));
}

/**
 * Test case: text leaves route their transform through the same host-layer path, so the anchor is
 * emitted on the text host rather than on the TextBox.
 */
PAG_TEST(PAGXHTMLAnchorTest, TextLeafTransformOriginBecomesAnchor) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:60px">
      <h1 id="title" style="width:200px;height:40px;
          transform:rotate(10deg);transform-origin:100px 20px">Title</h1>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto layers = LayersWithNonIdentityMatrix(doc);
  ASSERT_EQ(layers.size(), 1u);
  EXPECT_TRUE(NearlyEqual(layers.front()->anchor.x, 100.0f));
  EXPECT_TRUE(NearlyEqual(layers.front()->anchor.y, 20.0f));
}

/**
 * Test case: the box-shadow split moves the matrix to the wrapper, so the anchor must move with it.
 */
PAG_TEST(PAGXHTMLAnchorTest, BoxShadowSplitKeepsAnchorWithMatrix) {
  auto doc = ParseHtml(R"HTML(
    <html><body style="width:200px;height:100px">
      <div id="box" style="width:100px;height:60px;background-color:#6366F1;
           box-shadow:0 2px 8px #0008;
           transform:rotate(30deg);transform-origin:10px 20px"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto layers = LayersWithNonIdentityMatrix(doc);
  ASSERT_EQ(layers.size(), 1u);
  auto* matrixOwner = layers.front();
  EXPECT_TRUE(NearlyEqual(matrixOwner->anchor.x, 10.0f));
  EXPECT_TRUE(NearlyEqual(matrixOwner->anchor.y, 20.0f));
  for (auto* child : matrixOwner->children) {
    EXPECT_TRUE(HasZeroAnchor(child)) << "A child must not keep a dangling pivot";
  }
}

/**
 * Test case: an animated rotation keeps a static anchor and writes raw (un-pivoted) rotation
 * matrices into the `matrix` channel.
 */
PAG_TEST(PAGXHTMLAnchorTest, AnimatedRotationUsesStaticAnchor) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes spin {
        0%   { transform: rotate(0deg); }
        50%  { transform: rotate(180deg); }
        100% { transform: rotate(360deg); }
      }
    </style></head>
    <body style="width:200px;height:200px">
      <div id="box" style="position:absolute;left:40px;top:40px;width:20px;height:10px;
           background-color:#000;animation:spin 1s linear infinite"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(NearlyEqual(layer->anchor.x, 10.0f));
  EXPECT_TRUE(NearlyEqual(layer->anchor.y, 5.0f));
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  auto* matrixChannel = FindMatrixChannel(animation);
  ASSERT_NE(matrixChannel, nullptr);
  ASSERT_EQ(matrixChannel->keyframes.size(), 3u);
  for (const auto& keyframe : matrixChannel->keyframes) {
    EXPECT_TRUE(NearlyEqual(keyframe.value.tx, 0.0f)) << "Matrix keyframes must stay un-pivoted";
    EXPECT_TRUE(NearlyEqual(keyframe.value.ty, 0.0f)) << "Matrix keyframes must stay un-pivoted";
  }
}

/**
 * Test case: with the anchor carrying the pivot, a pure-rotation animation no longer needs the
 * structural pivot wrapper pair; the channel targets the element layer directly.
 */
PAG_TEST(PAGXHTMLAnchorTest, AnimatedRotationAddsNoPivotWrapper) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes spin {
        0%   { transform: rotate(0deg); }
        100% { transform: rotate(360deg); }
      }
    </style></head>
    <body style="width:200px;height:200px">
      <div id="box" style="position:absolute;left:40px;top:40px;width:20px;height:20px;
           background-color:#000;animation:spin 1s linear infinite"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  EXPECT_EQ(MatrixChannelTarget(animation), "box");
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(layer->children.empty()) << "The element must not be re-parented into a wrapper";
  EXPECT_FALSE(HasZeroAnchor(layer));
}

/**
 * Test case: a general affine animation (scale) also keeps its keyframes un-pivoted.
 */
PAG_TEST(PAGXHTMLAnchorTest, AnimatedScaleKeyframesCarryNoPivot) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes pulse {
        0%   { transform: scale(1); }
        100% { transform: scale(2); }
      }
    </style></head>
    <body style="width:200px;height:200px">
      <div id="box" style="width:50px;height:50px;background-color:#000;
           animation:pulse 1s linear forwards"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(NearlyEqual(layer->anchor.x, 25.0f));
  EXPECT_TRUE(NearlyEqual(layer->anchor.y, 25.0f));
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  auto* matrixChannel = FindMatrixChannel(animation);
  ASSERT_NE(matrixChannel, nullptr);
  ASSERT_GE(matrixChannel->keyframes.size(), 2u);
  EXPECT_TRUE(NearlyEqual(matrixChannel->keyframes.front().value.tx, 0.0f));
  EXPECT_TRUE(NearlyEqual(matrixChannel->keyframes.back().value.tx, 0.0f));
  EXPECT_TRUE(NearlyEqual(matrixChannel->keyframes.back().value.a, 2.0f));
  EXPECT_TRUE(NearlyEqual(matrixChannel->keyframes.back().value.d, 2.0f));
}

/**
 * Test case: a corner origin on an animated rotation keeps the zero anchor (pivot at the local
 * origin, the recommended `rotate(deg, cx, cy)` authoring form).
 */
PAG_TEST(PAGXHTMLAnchorTest, AnimatedRotationWithCornerOriginKeepsZeroAnchor) {
  auto doc = ParseHtml(R"HTML(
    <html><head><style>
      @keyframes spin {
        0%   { transform: rotate(0deg); }
        100% { transform: rotate(180deg); }
      }
    </style></head>
    <body style="width:200px;height:200px">
      <div id="box" style="position:absolute;left:40px;top:40px;width:20px;height:20px;
           background-color:#000;transform-origin:0px 0px;
           animation:spin 1s linear infinite"></div>
    </body></html>
  )HTML");
  ASSERT_NE(doc, nullptr);
  auto* layer = FindLayerById(doc, "box");
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(HasZeroAnchor(layer));
  auto* animation = FirstAnimation(doc);
  ASSERT_NE(animation, nullptr);
  auto* matrixChannel = FindMatrixChannel(animation);
  ASSERT_NE(matrixChannel, nullptr);
  for (const auto& keyframe : matrixChannel->keyframes) {
    EXPECT_TRUE(NearlyEqual(keyframe.value.tx, 0.0f));
    EXPECT_TRUE(NearlyEqual(keyframe.value.ty, 0.0f));
  }
}

/**
 * Test case: an auxiliary wrapper split (filter brightness) must leave the static matrix and its
 * anchor on the same layer; the generated child keeps no pivot.
 */
PAG_TEST(PAGXHTMLAnchorTest, AnimatedBrightnessSplitKeepsAnchorWithMatrix) {
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
    EXPECT_TRUE(HasZeroAnchor(child)) << "Generated wrappers must not inherit the pivot";
  }
}

/**
 * Test case: an imported anchor document renders exactly like the same document with the pivot
 * baked into the matrix, which pins the runtime application point of the anchor.
 */
PAGX_TEST(PAGXHTMLAnchorRenderTest, ImportedAnchorRendersLikeBakedMatrix) {
  const std::string html = R"HTML(
    <html><body style="width:200px;height:200px">
      <div id="box" style="position:absolute;left:60px;top:50px;width:80px;height:60px;
           background-color:#6366F1;transform:rotate(25deg);transform-origin:10px 45px"></div>
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
  ASSERT_FALSE(HasZeroAnchor(anchoredLayer)) << "The HTML origin must produce an anchor";

  bakedLayer->matrix3D =
      pagx::ApplyAnchorToMatrix(pagx::ComposeLayerTransform3DWithoutAnchor(bakedLayer),
                                bakedLayer->anchor, bakedLayer->anchorZ);
  bakedLayer->matrix = {};
  bakedLayer->anchor = {};
  bakedLayer->anchorZ = 0.0f;

  const auto anchoredMatrix = pagx::BuildLayerMatrix(anchoredLayer);
  const auto bakedMatrix = pagx::BuildLayerMatrix(bakedLayer);
  EXPECT_TRUE(NearlyEqual(anchoredMatrix.a, bakedMatrix.a, 0.001f));
  EXPECT_TRUE(NearlyEqual(anchoredMatrix.b, bakedMatrix.b, 0.001f));
  EXPECT_TRUE(NearlyEqual(anchoredMatrix.c, bakedMatrix.c, 0.001f));
  EXPECT_TRUE(NearlyEqual(anchoredMatrix.d, bakedMatrix.d, 0.001f));
  EXPECT_TRUE(NearlyEqual(anchoredMatrix.tx, bakedMatrix.tx, 0.001f));
  EXPECT_TRUE(NearlyEqual(anchoredMatrix.ty, bakedMatrix.ty, 0.001f));

  auto anchoredPixels = RenderFrame(anchored);
  auto bakedPixels = RenderFrame(baked);
  ASSERT_FALSE(anchoredPixels.empty());
  ASSERT_EQ(anchoredPixels.size(), bakedPixels.size());
  EXPECT_EQ(CountDifferentPixels(anchoredPixels, bakedPixels), 0u);
}

}  // namespace pag
