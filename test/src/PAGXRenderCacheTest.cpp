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
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Font.h"
#include "renderer/RenderCache.h"
#include "tgfx/core/CustomTypeface.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/Typeface.h"

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

}  // namespace pag
