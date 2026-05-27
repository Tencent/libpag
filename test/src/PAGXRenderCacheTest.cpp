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
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Text.h"
#include "renderer/FontEmbedder.h"
#include "renderer/LayerBuilder.h"
#include "renderer/RenderCache.h"
#include "tgfx/core/CustomTypeface.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/Typeface.h"
#include "utils/ProjectPath.h"

namespace pag {
using namespace tgfx;

static std::shared_ptr<Typeface> MakeStubTypeface() {
  PathTypefaceBuilder builder;
  Path glyphPath = {};
  glyphPath.addRect(Rect::MakeWH(10, 10));
  builder.addGlyph(glyphPath, 10);
  return builder.detach();
}

CLI_TEST(PAGXRenderCacheTest, MissReturnsNull) {
  pagx::RenderCache cache;
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* fontNode = doc->makeNode<pagx::Font>();
  EXPECT_EQ(cache.getTypeface(fontNode), nullptr);
}

CLI_TEST(PAGXRenderCacheTest, HitReturnsSameInstance) {
  pagx::RenderCache cache;
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* fontNode = doc->makeNode<pagx::Font>();
  auto typeface = MakeStubTypeface();
  ASSERT_TRUE(typeface != nullptr);

  cache.setTypeface(fontNode, typeface);
  auto cached = cache.getTypeface(fontNode);
  EXPECT_EQ(cached.get(), typeface.get());
}

CLI_TEST(PAGXRenderCacheTest, DistinctFontKeysAreIndependent) {
  pagx::RenderCache cache;
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* fontA = doc->makeNode<pagx::Font>();
  auto* fontB = doc->makeNode<pagx::Font>();
  ASSERT_NE(fontA, fontB);

  auto typefaceA = MakeStubTypeface();
  cache.setTypeface(fontA, typefaceA);

  EXPECT_EQ(cache.getTypeface(fontA).get(), typefaceA.get());
  EXPECT_EQ(cache.getTypeface(fontB), nullptr);
}

CLI_TEST(PAGXRenderCacheTest, RepeatedSetOverwrites) {
  pagx::RenderCache cache;
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* fontNode = doc->makeNode<pagx::Font>();

  auto first = MakeStubTypeface();
  auto second = MakeStubTypeface();
  ASSERT_TRUE(first != nullptr && second != nullptr);
  ASSERT_NE(first.get(), second.get());

  cache.setTypeface(fontNode, first);
  cache.setTypeface(fontNode, second);
  EXPECT_EQ(cache.getTypeface(fontNode).get(), second.get());
}

CLI_TEST(PAGXRenderCacheTest, PerDocumentCachesAreIsolated) {
  auto docA = pagx::PAGXDocument::Make(100, 100);
  auto docB = pagx::PAGXDocument::Make(100, 100);
  auto* fontA = docA->makeNode<pagx::Font>();

  auto typeface = MakeStubTypeface();
  ASSERT_TRUE(typeface != nullptr);
  docA->getOrCreateRenderCache()->setTypeface(fontA, typeface);

  // docB has its own cache; docA's entry must not leak across documents.
  EXPECT_EQ(docA->getOrCreateRenderCache()->getTypeface(fontA).get(), typeface.get());
  EXPECT_EQ(docB->getOrCreateRenderCache()->getTypeface(fontA), nullptr);
}

CLI_TEST(PAGXRenderCacheTest, DocumentDestructionReleasesCachedTypefaces) {
  auto typeface = MakeStubTypeface();
  ASSERT_TRUE(typeface != nullptr);
  std::weak_ptr<Typeface> weak = typeface;

  {
    auto doc = pagx::PAGXDocument::Make(100, 100);
    auto* fontNode = doc->makeNode<pagx::Font>();
    doc->getOrCreateRenderCache()->setTypeface(fontNode, typeface);
    typeface.reset();
    // Cache still holds the only strong reference at this point.
    EXPECT_FALSE(weak.expired());
  }

  // Document destruction tears down the RenderCache, which drops its cached strong references.
  EXPECT_TRUE(weak.expired());
}

// Builds a minimal text document, embeds fonts, and reloads it from XML so the embedded glyphRun
// path (the one that consults RenderCache) is exercised by LayerBuilder. Returns nullptr if the
// fallback font asset is missing on disk so the test is skipped gracefully.
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
  fontConfig.registerTypeface(typeface);
  authoredDoc->applyLayout(&fontConfig);
  if (!pagx::FontEmbedder().embed(authoredDoc.get())) {
    return nullptr;
  }

  auto xml = pagx::PAGXExporter::ToXML(*authoredDoc);
  if (xml.empty()) {
    return nullptr;
  }
  // Re-import a fresh doc so glyphData->layoutRuns is empty and prepareTextBlob falls into the
  // embedded glyphRun branch (the only branch that consults RenderCache).
  auto reloadedDoc = pagx::PAGXImporter::FromXML(xml);
  if (reloadedDoc == nullptr) {
    return nullptr;
  }
  reloadedDoc->applyLayout();
  return reloadedDoc;
}

CLI_TEST(PAGXRenderCacheTest, BuildPopulatesPerDocumentCache) {
  auto doc = MakeReloadedEmbeddedTextDocument();
  if (doc == nullptr) {
    GTEST_SKIP() << "Fallback font asset missing; skipping integration check.";
  }

  // Locate the embedded Font node via the first GlyphRun on the first Text in the document.
  pagx::Font* fontNode = nullptr;
  for (auto& node : doc->nodes) {
    if (node->nodeType() == pagx::NodeType::Text) {
      auto* t = static_cast<pagx::Text*>(node.get());
      if (!t->glyphRuns.empty() && t->glyphRuns[0]->font != nullptr) {
        fontNode = t->glyphRuns[0]->font;
        break;
      }
    }
  }
  ASSERT_NE(fontNode, nullptr);

  auto* cache = doc->getOrCreateRenderCache();
  EXPECT_EQ(cache->getTypeface(fontNode), nullptr);

  auto rootLayer = pagx::LayerBuilder::Build(doc.get());
  ASSERT_TRUE(rootLayer != nullptr);

  auto firstTypeface = cache->getTypeface(fontNode);
  ASSERT_TRUE(firstTypeface != nullptr);
}

CLI_TEST(PAGXRenderCacheTest, RepeatedBuildReusesCachedTypeface) {
  auto doc = MakeReloadedEmbeddedTextDocument();
  if (doc == nullptr) {
    GTEST_SKIP() << "Fallback font asset missing; skipping integration check.";
  }

  pagx::Font* fontNode = nullptr;
  for (auto& node : doc->nodes) {
    if (node->nodeType() == pagx::NodeType::Text) {
      auto* t = static_cast<pagx::Text*>(node.get());
      if (!t->glyphRuns.empty() && t->glyphRuns[0]->font != nullptr) {
        fontNode = t->glyphRuns[0]->font;
        break;
      }
    }
  }
  ASSERT_NE(fontNode, nullptr);

  ASSERT_TRUE(pagx::LayerBuilder::Build(doc.get()) != nullptr);
  auto firstTypeface = doc->getOrCreateRenderCache()->getTypeface(fontNode);
  ASSERT_TRUE(firstTypeface != nullptr);

  ASSERT_TRUE(pagx::LayerBuilder::Build(doc.get()) != nullptr);
  auto secondTypeface = doc->getOrCreateRenderCache()->getTypeface(fontNode);
  // Repeated Build calls on the same document must hit the cache rather than rebuilding the
  // typeface, so both lookups must return the very same shared_ptr instance.
  EXPECT_EQ(firstTypeface.get(), secondTypeface.get());
}

}  // namespace pag
