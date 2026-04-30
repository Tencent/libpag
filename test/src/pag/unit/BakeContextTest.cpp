// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 2 unit tests for BakeContext — diagnostics caps, depth budgets,
// and saturating exit semantics.
#include "gtest/gtest.h"
#include "pagx/pag/BakeContext.h"
#include "pagx/pag/limits.h"

namespace pagx::pag {

TEST(BakeContext, ErrorAndWarnSeparateBuckets) {
  BakeContext ctx;
  ctx.error(ErrorCode::LayoutNotApplied, "oops");
  ctx.warn(ErrorCode::ImageSourceMissing, "missing.png");
  EXPECT_EQ(ctx.errors.size(), 1u);
  EXPECT_EQ(ctx.warnings.size(), 1u);
  EXPECT_TRUE(ctx.hasFatal());
  EXPECT_EQ(ctx.errors[0].byteOffset, 0u);  // Baker has no byte stream
}

TEST(BakeContext, MaxDiagnosticsCap) {
  BakeContext ctx;
  for (uint32_t i = 0; i < limits::MAX_DIAGNOSTICS + 100; ++i) {
    ctx.warn(ErrorCode::ImageSourceMissing);
  }
  EXPECT_EQ(ctx.warnings.size(), limits::MAX_DIAGNOSTICS);
}

TEST(BakeContext, MessageTruncation) {
  BakeContext ctx;
  std::string huge(limits::MAX_DIAGNOSTIC_MESSAGE_BYTES + 1024, 'X');
  ctx.error(ErrorCode::LayoutNotApplied, huge);
  ctx.warn(ErrorCode::ImageSourceMissing, huge);
  EXPECT_EQ(ctx.errors[0].message.size(), limits::MAX_DIAGNOSTIC_MESSAGE_BYTES);
  EXPECT_EQ(ctx.warnings[0].message.size(), limits::MAX_DIAGNOSTIC_MESSAGE_BYTES);
}

TEST(BakeContext, EnterLayerHonoursDepthCap) {
  BakeContext ctx;
  for (uint32_t i = 0; i < limits::MAX_LAYER_DEPTH; ++i) {
    EXPECT_TRUE(ctx.enterLayer());
  }
  // Exceeding MAX_LAYER_DEPTH must produce a CompositionCycleDepth fatal.
  EXPECT_FALSE(ctx.enterLayer());
  EXPECT_TRUE(ctx.hasFatal());
  EXPECT_EQ(ctx.errors.back().code, ErrorCode::CompositionCycleDepth);
}

TEST(BakeContext, ExitLayerSaturatesAtZero) {
  BakeContext ctx;
  // exit at depth 0 must NOT underflow to UINT32_MAX.
  ctx.exitLayer();
  ctx.exitLayer();
  EXPECT_EQ(ctx.currentLayerDepth, 0u);
  // Subsequent enter still works normally.
  EXPECT_TRUE(ctx.enterLayer());
  EXPECT_EQ(ctx.currentLayerDepth, 1u);
}

TEST(BakeContext, EnterVectorGroupHonoursDepthCap) {
  BakeContext ctx;
  for (uint32_t i = 0; i < limits::MAX_VECTOR_ELEMENT_DEPTH; ++i) {
    EXPECT_TRUE(ctx.enterVectorGroup());
  }
  EXPECT_FALSE(ctx.enterVectorGroup());
  EXPECT_TRUE(ctx.hasFatal());
  EXPECT_EQ(ctx.errors.back().code, ErrorCode::StructureLimitExceeded);
}

TEST(BakeContext, RegisterCompositionHonoursTotalCap) {
  BakeContext ctx;
  for (uint32_t i = 0; i < limits::BAKE_MAX_TOTAL_COMPOSITIONS; ++i) {
    EXPECT_TRUE(ctx.registerComposition());
  }
  EXPECT_FALSE(ctx.registerComposition());
  EXPECT_TRUE(ctx.hasFatal());
  EXPECT_EQ(ctx.errors.back().code, ErrorCode::StructureLimitExceeded);
}

TEST(BakeContext, ContextIndexPropagates) {
  BakeContext ctx;
  ctx.warn(ErrorCode::ImageSourceMissing, "third image", /*contextIndex=*/2);
  ASSERT_EQ(ctx.warnings.size(), 1u);
  EXPECT_EQ(ctx.warnings[0].contextIndex, 2u);
}

}  // namespace pagx::pag
