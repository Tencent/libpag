// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 2 unit tests for the StructBuilders + PAGDocumentEquals fixtures.
#include <memory>
#include <string>
#include "gtest/gtest.h"
#include "pag/support/PAGDocumentEquals.h"
#include "pag/support/StructBuilders.h"
#include "pagx/pag/PAGDocument.h"

namespace pagx::test {

using namespace pagx::pag;

// ---------- DocumentsEqual identity ----------

TEST(DocumentsEqual, EmptyVsEmpty) {
  PAGDocument a;
  PAGDocument b;
  std::string diff;
  EXPECT_TRUE(DocumentsEqual(a, b, &diff));
  EXPECT_TRUE(diff.empty());
}

TEST(DocumentsEqual, NullPair) {
  std::string diff;
  EXPECT_TRUE(DocumentsEqual(nullptr, nullptr, &diff));
}

TEST(DocumentsEqual, NullVsNonNull) {
  PAGDocument a;
  std::string diff;
  EXPECT_FALSE(DocumentsEqual(&a, nullptr, &diff));
  EXPECT_FALSE(diff.empty());
}

TEST(DocumentsEqual, CanvasSizeDifference) {
  PAGDocument a;
  PAGDocument b;
  a.header.width = 1920.0f;
  b.header.width = 1280.0f;
  std::string diff;
  EXPECT_FALSE(DocumentsEqual(a, b, &diff));
  EXPECT_NE(diff.find("size"), std::string::npos);
}

TEST(DocumentsEqual, CompositionSizeMismatch) {
  auto a = MakeDeepLayerStack(3);
  auto b = MakeDeepLayerStack(4);
  std::string diff;
  EXPECT_FALSE(DocumentsEqual(*a, *b, &diff));
  EXPECT_FALSE(diff.empty());
}

// ---------- StructBuilders shape ----------

TEST(StructBuilders, MakeDeepLayerStack) {
  auto doc = MakeDeepLayerStack(5);
  ASSERT_NE(doc, nullptr);
  ASSERT_EQ(doc->compositions.size(), 1u);
  // Walk the chain: comp.layers[0] → child[0] → child[0] → ... five deep.
  Layer* current = doc->compositions[0]->layers[0].get();
  ASSERT_NE(current, nullptr);
  uint32_t depth = 1;
  while (!current->children.empty()) {
    current = current->children[0].get();
    ++depth;
  }
  EXPECT_EQ(depth, 5u);
}

TEST(StructBuilders, MakeWideSiblingTree) {
  auto doc = MakeWideSiblingTree(7);
  ASSERT_EQ(doc->compositions[0]->layers[0]->children.size(), 7u);
}

TEST(StructBuilders, MakeLayerWithKMasks) {
  auto doc = MakeLayerWithKMasks(4);
  ASSERT_EQ(doc->compositions[0]->layers.size(), 2u);
  EXPECT_EQ(doc->compositions[0]->layers[1]->children.size(), 4u);
  // Every host child must reference the target layer at chain {0}.
  for (const auto& sibling : doc->compositions[0]->layers[1]->children) {
    ASSERT_EQ(sibling->maskLayerPath.size(), 1u);
    EXPECT_EQ(sibling->maskLayerPath[0], 0u);
  }
}

TEST(StructBuilders, MakeCompositionRefCycle) {
  auto doc = MakeCompositionRefCycle();
  ASSERT_EQ(doc->compositions.size(), 2u);
  EXPECT_EQ(doc->compositions[0]->layers[0]->type, LayerType::CompositionRef);
  EXPECT_EQ(doc->compositions[0]->layers[0]->compositionIndex, 1u);
  EXPECT_EQ(doc->compositions[1]->layers[0]->compositionIndex, 0u);
}

// ---------- StructBuilders + DocumentsEqual integration ----------

TEST(StructBuilders, IdenticalBuildsAreEqual) {
  auto a = MakeDeepLayerStack(8);
  auto b = MakeDeepLayerStack(8);
  std::string diff;
  EXPECT_TRUE(DocumentsEqual(*a, *b, &diff)) << diff;
}

TEST(StructBuilders, DifferentBuildsAreUnequal) {
  auto a = MakeWideSiblingTree(3);
  auto b = MakeWideSiblingTree(5);
  std::string diff;
  EXPECT_FALSE(DocumentsEqual(*a, *b, &diff));
  EXPECT_FALSE(diff.empty());
}

}  // namespace pagx::test
