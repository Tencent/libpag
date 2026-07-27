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

#include <cstdint>
#include <memory>
#include <vector>
#include "base/PAGTest.h"
#include "pagx/FontConfig.h"
#include "pagx/LayoutContext.h"
#include "pagx/PAGFont.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/TypefaceHolder.h"
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
#include "utils/TestUtils.h"

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
  fontConfig.registerFont(fontPath, 0, typeface->fontFamily(), typeface->fontStyle());
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

CLI_TEST(PAGTypefaceCacheTest, TypefaceHolderLoadsPathBytesAndPrebuiltTypeface) {
  auto fontPath = ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf");
  auto fontData = ReadFile("resources/font/NotoSansSC-Regular.otf");
  ASSERT_NE(fontData, nullptr);

  pagx::TypefaceHolder pathHolder(fontPath, 0, "Path Alias", "Regular");
  EXPECT_EQ(pathHolder.getFontFamily(), "Path Alias");
  EXPECT_EQ(pathHolder.getFontStyle(), "Regular");
  EXPECT_NE(pathHolder.getTypeface(), nullptr);
  // The second access returns the cached face instead of decoding the path again.
  EXPECT_EQ(pathHolder.getTypeface(), pathHolder.getTypeface());

  auto bytes = std::make_shared<const std::vector<uint8_t>>(
      fontData->bytes(), fontData->bytes() + fontData->size());
  pagx::TypefaceHolder bytesHolder(bytes, 0, "Bytes Alias", "Medium");
  EXPECT_NE(bytesHolder.getTypeface(), nullptr);

  auto prebuilt = Typeface::MakeFromPath(fontPath);
  ASSERT_NE(prebuilt, nullptr);
  pagx::TypefaceHolder prebuiltHolder(prebuilt, "Prebuilt Alias", "Regular");
  EXPECT_EQ(prebuiltHolder.getTypeface(), prebuilt);

  pagx::TypefaceHolder missingPath("/definitely/missing/font.otf", 0, "Missing", "Regular");
  EXPECT_EQ(missingPath.getTypeface(), nullptr);
  auto emptyBytes = std::make_shared<const std::vector<uint8_t>>();
  pagx::TypefaceHolder emptyBytesHolder(emptyBytes, 0, "Empty", "Regular");
  EXPECT_EQ(emptyBytesHolder.getTypeface(), nullptr);
}

CLI_TEST(PAGTypefaceCacheTest, FontConfigCoversRegistrationFallbackAndValueSemantics) {
  auto fontPath = ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf");
  auto fontData = ReadFile("resources/font/NotoSansSC-Regular.otf");
  auto sourceTypeface = Typeface::MakeFromPath(fontPath);
  ASSERT_NE(fontData, nullptr);
  ASSERT_NE(sourceTypeface, nullptr);

  const auto sourceFamily = sourceTypeface->fontFamily();
  const auto sourceStyle = sourceTypeface->fontStyle();
  pagx::PAGFont pathAlias("Path Alias", "");
  pagx::PAGFont bytesAlias("Bytes Alias", "Medium");

  pagx::FontConfig config;
  EXPECT_FALSE(config.containsFamily(""));
  EXPECT_FALSE(config.containsFamily("not registered"));

  // Exercise both PAGFont forwarding overloads and both lazy holder source kinds.
  config.registerFont(pathAlias, fontPath, 0);
  config.registerFont(bytesAlias, fontData->bytes(), fontData->size(), 0);
  EXPECT_TRUE(config.containsFamily("Path Alias"));
  EXPECT_TRUE(config.containsFamily("Bytes Alias"));
  pagx::LayoutContext lazyContext(&config);
  EXPECT_NE(lazyContext.findTypeface("Path Alias", ""), nullptr);
  EXPECT_NE(lazyContext.findTypeface("Bytes Alias", "Medium"), nullptr);

  // Empty families reverse-lookup the real family/style from the source.
  config.registerFont(fontPath, 0);
  config.registerFont(fontData->bytes(), fontData->size(), 0);
  EXPECT_TRUE(config.containsFamily(sourceFamily));
  config.registerFont("/definitely/missing/font.otf", 0);
  const uint8_t invalidFont[] = {0, 1, 2, 3};
  config.registerFont(invalidFont, sizeof(invalidFont), 0);

  config.addFallbackFont(pathAlias, fontPath, 0);
  config.addFallbackFont(bytesAlias, fontData->bytes(), fontData->size(), 0);
  config.addFallbackFont(fontPath, 0);
  config.addFallbackFont(fontData->bytes(), fontData->size(), 0);
  config.addFallbackFont("/definitely/missing/font.otf", 0);
  config.addFallbackFont(invalidFont, sizeof(invalidFont), 0);
  auto fallbackNames = config.fallbackFamilyNames();
  ASSERT_EQ(fallbackNames.size(), 4u);
  EXPECT_EQ(fallbackNames[0], "Path Alias");
  EXPECT_EQ(fallbackNames[1], "Bytes Alias");
  EXPECT_EQ(fallbackNames[2], sourceFamily);
  EXPECT_EQ(fallbackNames[3], sourceFamily);

  // Copying retains registrations independently; assignment handles both a different object and
  // self-assignment. Moving exercises the explicitly declared value semantics.
  pagx::FontConfig copied(config);
  EXPECT_TRUE(copied.containsFamily("Path Alias"));
  pagx::FontConfig assigned;
  assigned = config;
  auto* assignedPointer = &assigned;
  assigned = *assignedPointer;
  EXPECT_TRUE(assigned.containsFamily("Bytes Alias"));
  pagx::FontConfig moved(std::move(copied));
  EXPECT_TRUE(moved.containsFamily(sourceFamily));
  pagx::FontConfig moveAssigned;
  moveAssigned = std::move(assigned);
  EXPECT_TRUE(moveAssigned.containsFamily("Path Alias"));

  // Use an installed face to cover the eager system-font holder and PAGFont forwarders. The
  // resource's family may also be installed, so prefer it before the ubiquitous macOS families.
  std::vector<pagx::PAGFont> systemCandidates = {
      {sourceFamily, sourceStyle}, {"Helvetica", "Regular"}, {"Arial", "Regular"}};
  pagx::PAGFont installed;
  for (const auto& candidate : systemCandidates) {
    if (Typeface::MakeFromName(candidate.fontFamily, candidate.fontStyle) != nullptr) {
      installed = candidate;
      break;
    }
  }
  if (installed.fontFamily.empty()) {
    // Headless test environments can expose no system-name lookup at all. The same calls still
    // exercise the documented failure path without making the test platform-dependent.
    pagx::PAGFont unavailable("Definitely Missing Font Family", "Regular");
    EXPECT_FALSE(moveAssigned.registerSystemFont(unavailable));
    EXPECT_FALSE(moveAssigned.addFallbackSystemFont(unavailable));
  } else {
    EXPECT_TRUE(moveAssigned.registerSystemFont(installed));
    EXPECT_TRUE(moveAssigned.addFallbackSystemFont(installed));
    EXPECT_TRUE(moveAssigned.containsFamily(installed.fontFamily));
    EXPECT_EQ(moveAssigned.fallbackFamilyNames().back(), installed.fontFamily);
  }
}

}  // namespace pag
