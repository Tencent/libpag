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
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <memory>
#include "base/PAGTest.h"
#include "pagx/FontConfig.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/PAGTypeface.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/FontRenderCache.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Text.h"
#include "renderer/FontEmbedder.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/core/Typeface.h"
#include "utils/ProjectPath.h"

namespace pag {
using namespace tgfx;

// Returns the cached tgfx typeface on a Font node, or nullptr if no cache has been built yet.
static std::shared_ptr<Typeface> CachedTypeface(const pagx::Font* fontNode) {
  if (fontNode == nullptr || fontNode->renderCache == nullptr) {
    return nullptr;
  }
  return fontNode->renderCache->typeface;
}

// Builds a minimal text document, embeds fonts, and reloads it from XML so the embedded glyphRun
// path (the one that lazily builds and caches a tgfx typeface on each Font node) is exercised by
// LayerBuilder. Returns nullptr if the fallback font asset is missing on disk so the test is
// skipped gracefully.
static std::shared_ptr<pagx::PAGXDocument> MakeReloadedEmbeddedTextDocument() {
  auto fontPath = ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf");
  auto typeface = Typeface::MakeFromPath(fontPath);
  if (typeface == nullptr) {
    return nullptr;
  }

  auto authoredDoc = pagx::PAGXDocument::Make(100, 60);
  auto* layer = authoredDoc->makeNode<pagx::Layer>();
  auto* group = authoredDoc->makeNode<pagx::Group>();
  auto* text = authoredDoc->makeNode<pagx::Text>();
  text->text = "Hi";
  text->fontSize = 24;
  auto* fill = authoredDoc->makeNode<pagx::Fill>();
  auto* solid = authoredDoc->makeNode<pagx::SolidColor>();
  solid->color = {0, 0, 0, 1};
  fill->color = solid;
  group->elements.push_back(text);
  group->elements.push_back(fill);
  layer->contents.push_back(group);
  authoredDoc->layers.push_back(layer);

  pagx::FontConfig fontConfig;
  fontConfig.registerTypeface(pagx::PAGTypeface::MakeFromTypeface(typeface));
  authoredDoc->applyLayout(&fontConfig);
  if (!pagx::FontEmbedder().embed(authoredDoc.get())) {
    return nullptr;
  }

  auto xml = pagx::PAGXExporter::ToXML(*authoredDoc);
  if (xml.empty()) {
    return nullptr;
  }
  auto reloadedDoc = pagx::PAGXImporter::FromXML(xml);
  if (reloadedDoc == nullptr) {
    return nullptr;
  }
  reloadedDoc->applyLayout();
  return reloadedDoc;
}

static pagx::Font* FindFirstEmbeddedFont(pagx::PAGXDocument* doc) {
  for (auto& node : doc->nodes) {
    if (node->nodeType() != pagx::NodeType::Text) {
      continue;
    }
    auto* t = static_cast<pagx::Text*>(node.get());
    if (!t->glyphRuns.empty() && t->glyphRuns[0]->font != nullptr) {
      return t->glyphRuns[0]->font;
    }
  }
  return nullptr;
}

CLI_TEST(PAGTypefaceCacheTest, BuildPopulatesFontTypeface) {
  auto doc = MakeReloadedEmbeddedTextDocument();
  if (doc == nullptr) {
    GTEST_SKIP() << "Fallback font asset missing; skipping integration check.";
  }

  auto* fontNode = FindFirstEmbeddedFont(doc.get());
  ASSERT_NE(fontNode, nullptr);
  EXPECT_EQ(CachedTypeface(fontNode), nullptr);

  auto rootLayer = pagx::LayerBuilder::Build(doc.get());
  ASSERT_TRUE(rootLayer != nullptr);

  EXPECT_TRUE(CachedTypeface(fontNode) != nullptr);
}

CLI_TEST(PAGTypefaceCacheTest, RepeatedBuildReusesCachedTypeface) {
  auto doc = MakeReloadedEmbeddedTextDocument();
  if (doc == nullptr) {
    GTEST_SKIP() << "Fallback font asset missing; skipping integration check.";
  }

  auto* fontNode = FindFirstEmbeddedFont(doc.get());
  ASSERT_NE(fontNode, nullptr);

  ASSERT_TRUE(pagx::LayerBuilder::Build(doc.get()) != nullptr);
  auto firstTypeface = CachedTypeface(fontNode);
  ASSERT_TRUE(firstTypeface != nullptr);

  ASSERT_TRUE(pagx::LayerBuilder::Build(doc.get()) != nullptr);
  auto secondTypeface = CachedTypeface(fontNode);
  EXPECT_EQ(firstTypeface.get(), secondTypeface.get())
      << "second LayerBuilder::Build should hit the cache, not rebuild the typeface";
}

CLI_TEST(PAGTypefaceCacheTest, PerDocumentTypefacesAreIsolated) {
  auto docA = MakeReloadedEmbeddedTextDocument();
  auto docB = MakeReloadedEmbeddedTextDocument();
  if (docA == nullptr || docB == nullptr) {
    GTEST_SKIP() << "Fallback font asset missing; skipping integration check.";
  }

  ASSERT_TRUE(pagx::LayerBuilder::Build(docA.get()) != nullptr);
  auto* fontA = FindFirstEmbeddedFont(docA.get());
  auto* fontB = FindFirstEmbeddedFont(docB.get());
  ASSERT_NE(fontA, nullptr);
  ASSERT_NE(fontB, nullptr);
  EXPECT_TRUE(CachedTypeface(fontA) != nullptr);
  EXPECT_EQ(CachedTypeface(fontB), nullptr);
}

CLI_TEST(PAGTypefaceCacheTest, DocumentDestructionReleasesTypeface) {
  auto doc = MakeReloadedEmbeddedTextDocument();
  if (doc == nullptr) {
    GTEST_SKIP() << "Fallback font asset missing; skipping integration check.";
  }

  auto* fontNode = FindFirstEmbeddedFont(doc.get());
  ASSERT_NE(fontNode, nullptr);

  ASSERT_TRUE(pagx::LayerBuilder::Build(doc.get()) != nullptr);
  std::weak_ptr<Typeface> weak = CachedTypeface(fontNode);
  ASSERT_FALSE(weak.expired());
  fontNode = nullptr;

  doc.reset();
  EXPECT_TRUE(weak.expired());
}

CLI_TEST(PAGTypefaceCacheTest, ClearEmbedResetsFontTypeface) {
  auto doc = MakeReloadedEmbeddedTextDocument();
  if (doc == nullptr) {
    GTEST_SKIP() << "Fallback font asset missing; skipping integration check.";
  }

  ASSERT_TRUE(pagx::LayerBuilder::Build(doc.get()) != nullptr);
  auto* fontNode = FindFirstEmbeddedFont(doc.get());
  ASSERT_NE(fontNode, nullptr);
  ASSERT_TRUE(CachedTypeface(fontNode) != nullptr);

  doc->clearEmbed();
  EXPECT_EQ(CachedTypeface(fontNode), nullptr);
}

}  // namespace pag
