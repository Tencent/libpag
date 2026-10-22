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
//  or required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>
#include "base/PAGTest.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/pag/exporter/LayoutFlattener.h"
#include "renderer/LayerBuilder.h"
#include "rendering/gpu/Devices.h"
#include "tgfx/core/Surface.h"
#include "tgfx/layers/DisplayList.h"
#include "tgfx/layers/Layer.h"
#include "utils/ProjectPath.h"

namespace pagx {
namespace {
using pag::DeviceLockScope;
using pag::ProjectPath;

struct SurfacePixels {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> pixels = {};
};

bool RenderDocument(PAGXDocument* document, tgfx::Context* context, SurfacePixels* out) {
  auto layer = LayerBuilder::Build(document);
  if (layer == nullptr) {
    return false;
  }
  auto width = std::max(1, static_cast<int>(std::round(document->width)));
  auto height = std::max(1, static_cast<int>(std::round(document->height)));
  auto surface = tgfx::Surface::Make(context, width, height);
  if (surface == nullptr) {
    return false;
  }
  tgfx::DisplayList displayList;
  auto container = tgfx::Layer::Make();
  container->addChild(layer);
  displayList.root()->addChild(container);
  displayList.render(surface.get(), false);
  tgfx::Bitmap bitmap(static_cast<size_t>(width), static_cast<size_t>(height), false, false);
  tgfx::Pixmap pixmap(bitmap);
  if (!surface->readPixels(pixmap.info(), pixmap.writablePixels())) {
    return false;
  }
  auto byteCount = pixmap.info().byteSize();
  out->width = width;
  out->height = height;
  out->pixels.resize(byteCount);
  std::memcpy(out->pixels.data(), pixmap.writablePixels(), byteCount);
  return true;
}

void CollectPagxFiles(const std::string& directory, std::vector<std::string>* files) {
  std::error_code ec;
  for (auto it = std::filesystem::recursive_directory_iterator(directory, ec); it != end(it);
       it.increment(ec)) {
    if (ec) {
      break;
    }
    if (it->is_regular_file() && it->path().extension() == ".pagx") {
      files->push_back(it->path().string());
    }
  }
  std::sort(files->begin(), files->end());
}

template <typename T>
T* FirstNodeOfType(PAGXDocument* document, NodeType type) {
  for (auto& node : document->nodes) {
    if (node->nodeType() == type) {
      return static_cast<T*>(node.get());
    }
  }
  return nullptr;
}

struct HookRecord {
  Node* node = nullptr;
  float scale = 0.0f;
};

std::vector<HookRecord> gGeometryHooks = {};

void RecordGeometryScale(Node* node, float scale) {
  gGeometryHooks.push_back(HookRecord{node, scale});
}
}  // namespace

PAGX_TEST(PAGFlattenTest, FlattenPreservesRendering) {
  std::vector<std::string> directories = {
      ProjectPath::Absolute("resources/pagx_to_pag/layout_flatten"),
      ProjectPath::Absolute("resources/layout"),
      ProjectPath::Absolute("spec/samples"),
  };
  std::vector<std::string> files = {};
  for (const auto& directory : directories) {
    CollectPagxFiles(directory, &files);
  }
  ASSERT_GT(files.size(), 60u);

  DeviceLockScope deviceLock;
  ASSERT_TRUE(static_cast<bool>(deviceLock));
  auto* context = deviceLock.context();

  for (const auto& path : files) {
    auto document = PAGXImporter::FromFile(path);
    ASSERT_TRUE(document != nullptr) << path;
    document->applyLayout();

    SurfacePixels before = {};
    ASSERT_TRUE(RenderDocument(document.get(), context, &before)) << path;

    LayoutFlattener::Flatten(document.get());
    // The load-time simulation: the binary decoder hands back the same float32 values, so
    // re-running applyLayout on the flattened tree reproduces decode + re-measure exactly.
    // (The XML round trip below only checks that the flattened document stays a valid PAGX
    // file: PAGXExporter's %g float formatting can lose sub-1e-3 px fidelity, which the v4
    // binary does not.)
    document->applyLayout();
    auto xml = PAGXExporter::ToXML(*document);
    auto reloaded = PAGXImporter::FromXML(xml);
    ASSERT_TRUE(reloaded != nullptr) << path;
    for (auto& error : reloaded->errors) {
      ADD_FAILURE() << "reloaded doc has errors in " << path << ": " << error;
    }

    SurfacePixels after = {};
    ASSERT_TRUE(RenderDocument(document.get(), context, &after)) << path;
    ASSERT_EQ(after.width, before.width) << path;
    ASSERT_EQ(after.height, before.height) << path;
    ASSERT_EQ(after.pixels.size(), before.pixels.size()) << path;
    size_t differing = 0;
    double diffSum = 0;
    uint8_t maxDiff = 0;
    size_t firstDiff = 0;
    for (size_t i = 0; i < before.pixels.size(); i++) {
      auto delta = static_cast<uint8_t>(
          std::abs(static_cast<int>(before.pixels[i]) - static_cast<int>(after.pixels[i])));
      if (delta != 0) {
        differing++;
        diffSum += delta;
        if (delta > maxDiff) {
          maxDiff = delta;
        }
        if (firstDiff == 0) {
          firstDiff = i;
        }
      }
    }
    double meanDiff =
        before.pixels.empty() ? 0 : diffSum / static_cast<double>(before.pixels.size());
    EXPECT_LE(meanDiff, 1.0) << path << " differing bytes " << differing;
    EXPECT_LE(maxDiff, 3) << path << " first diff at byte " << firstDiff;
    EXPECT_LT(differing, before.pixels.size() / 1000) << path;
  }
}

PAGX_TEST(PAGFlattenTest, StructuralCleanUp) {
  auto path = ProjectPath::Absolute("resources/pagx_to_pag/layout_flatten/clip_to_bounds.pagx");
  auto document = PAGXImporter::FromFile(path);
  ASSERT_TRUE(document != nullptr);
  document->applyLayout();
  auto* layer = document->layers[0];
  ASSERT_TRUE(layer != nullptr);
  EXPECT_TRUE(layer->clipToBounds);
  LayoutFlattener::Flatten(document.get());
  EXPECT_FALSE(layer->clipToBounds);
  EXPECT_GT(layer->scrollRect.width, 0);
  EXPECT_GT(layer->scrollRect.height, 0);
  EXPECT_EQ(layer->width, 160);
  EXPECT_EQ(layer->height, 120);
  EXPECT_TRUE(std::isnan(layer->left));
  EXPECT_TRUE(std::isnan(layer->top));
  EXPECT_EQ(layer->layout, LayoutMode::None);
  EXPECT_EQ(layer->padding.top, 0);

  auto textSample = PAGXImporter::FromFile(
      ProjectPath::Absolute("resources/pagx_to_pag/layout_flatten/text_anchor_modes.pagx"));
  ASSERT_TRUE(textSample != nullptr);
  textSample->applyLayout();
  LayoutFlattener::Flatten(textSample.get());
  std::vector<Text*> texts = {};
  for (auto& node : textSample->nodes) {
    if (node->nodeType() == NodeType::Text) {
      texts.push_back(static_cast<Text*>(node.get()));
    }
  }
  ASSERT_EQ(texts.size(), 3u);
  for (auto* text : texts) {
    EXPECT_TRUE(std::isnan(text->left));
    EXPECT_TRUE(std::isnan(text->top));
    EXPECT_TRUE(std::isnan(text->width));
    EXPECT_FALSE(std::isnan(text->position.x));
  }
}

PAGX_TEST(PAGFlattenTest, ScaledTextKeepsFitScale) {
  auto path =
      ProjectPath::Absolute("resources/pagx_to_pag/layout_flatten/text_scaled_by_layout.pagx");
  auto document = PAGXImporter::FromFile(path);
  ASSERT_TRUE(document != nullptr);
  document->applyLayout();
  auto* text = FirstNodeOfType<Text>(document.get(), NodeType::Text);
  ASSERT_TRUE(text != nullptr);
  auto originalFontSize = text->fontSize;
  LayoutFlattener::Flatten(document.get());
  EXPECT_EQ(text->fontSize, originalFontSize);
  EXPECT_FALSE(std::isnan(text->width));
  EXPECT_FALSE(std::isnan(text->height));
}

PAGX_TEST(PAGFlattenTest, GeometryScaleHook) {
  auto path =
      ProjectPath::Absolute("resources/pagx_to_pag/layout_flatten/path_scaled_by_layout.pagx");
  auto document = PAGXImporter::FromFile(path);
  ASSERT_TRUE(document != nullptr);
  document->applyLayout();
  auto* shapePath = FirstNodeOfType<Path>(document.get(), NodeType::Path);
  ASSERT_TRUE(shapePath != nullptr);
  auto originalBounds = shapePath->data->getBounds();

  gGeometryHooks.clear();
  FlattenOptions options = {};
  options.geometryScaled = RecordGeometryScale;
  LayoutFlattener::Flatten(document.get(), options);
  ASSERT_EQ(gGeometryHooks.size(), 1u);
  EXPECT_EQ(gGeometryHooks[0].node, shapePath);
  EXPECT_NEAR(gGeometryHooks[0].scale, 0.5f, 1e-6f);
  auto scaledBounds = shapePath->data->getBounds();
  EXPECT_NEAR(scaledBounds.width, originalBounds.width * 0.5f, 0.05f);
}

PAGX_TEST(PAGFlattenTest, LayoutOnlyChannelClassification) {
  EXPECT_TRUE(LayoutFlattener::IsLayoutOnlyChannel(NodeType::Layer, "flex"));
  EXPECT_TRUE(LayoutFlattener::IsLayoutOnlyChannel(NodeType::Layer, "gap"));
  EXPECT_TRUE(LayoutFlattener::IsLayoutOnlyChannel(NodeType::Layer, "left"));
  EXPECT_FALSE(LayoutFlattener::IsLayoutOnlyChannel(NodeType::Rectangle, "size.width"));
  EXPECT_FALSE(LayoutFlattener::IsLayoutOnlyChannel(NodeType::Layer, "noSuchChannel"));

  auto path =
      ProjectPath::Absolute("resources/pagx_to_pag/layout_flatten/layout_channel_targets.pagx");
  auto document = PAGXImporter::FromFile(path);
  ASSERT_TRUE(document != nullptr);
  std::vector<PAGExportIssue> issues = {};
  LayoutFlattener::CollectLayoutChannelIssues(document.get(), "main", &issues);
  ASSERT_EQ(issues.size(), 1u);
  EXPECT_EQ(issues[0].code, PAGExportWarning::LayoutChannelDropped);
  EXPECT_EQ(issues[0].scope, "main");
  EXPECT_TRUE(issues[0].message.find("flex") != std::string::npos);
}
}  // namespace pagx
