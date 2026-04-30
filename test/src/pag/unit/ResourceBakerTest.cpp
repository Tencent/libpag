// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 2 unit tests for ResourceBaker — the three-tier intern chain on
// BakeContext.imageIndex* / fontIndex* maps.
#include <memory>
#include <string>
#include "gtest/gtest.h"
#include "pagx/pag/BakeContext.h"
#include "pagx/pag/PAGDocument.h"
#include "pagx/pag/ResourceBaker.h"
#include "tgfx/core/Data.h"

namespace pagx::pag {

namespace {

std::unique_ptr<ImageAsset> MakeImage(uint8_t fillByte, size_t bytes,
                                      std::shared_ptr<const tgfx::Data>* outData = nullptr) {
  auto buf = std::make_unique<uint8_t[]>(bytes);
  for (size_t i = 0; i < bytes; ++i) {
    buf[i] = fillByte;
  }
  auto data = tgfx::Data::MakeAdopted(buf.release(), bytes, tgfx::Data::DeleteProc);
  auto asset = std::make_unique<ImageAsset>();
  asset->data = data;
  asset->width = 16;
  asset->height = 16;
  if (outData != nullptr) {
    *outData = data;
  }
  return asset;
}

std::unique_ptr<FontAsset> MakeFont(std::string family, std::string style,
                                    FontSourceKind kind = FontSourceKind::System) {
  auto f = std::make_unique<FontAsset>();
  f->kind = kind;
  f->family = std::move(family);
  f->style = std::move(style);
  return f;
}

}  // namespace

TEST(ResourceBaker, ImageNodePtrShortCircuit) {
  BakeContext ctx;
  PAGDocument doc;

  int sentinel = 0;
  uint32_t first = RegisterImage(ctx, doc, MakeImage(0xAA, 64), &sentinel, "key/a");
  uint32_t second = RegisterImage(ctx, doc, MakeImage(0xBB, 64), &sentinel, "key/b");
  // Same nodePtr → same slot, fresh asset is dropped.
  EXPECT_EQ(first, second);
  EXPECT_EQ(doc.images.size(), 1u);
}

TEST(ResourceBaker, ImageDataPtrCollapse) {
  BakeContext ctx;
  PAGDocument doc;

  // Two distinct ImageAsset wrappers but the same underlying tgfx::Data ptr —
  // tier 2 must collapse.
  std::shared_ptr<const tgfx::Data> sharedData;
  auto first = MakeImage(0xCC, 64, &sharedData);
  auto second = std::make_unique<ImageAsset>();
  second->data = sharedData;  // pointer-equal to first->data
  second->width = 16;
  second->height = 16;

  uint32_t firstIndex = RegisterImage(ctx, doc, std::move(first), nullptr, "key/x");
  uint32_t secondIndex = RegisterImage(ctx, doc, std::move(second), nullptr, "key/y");
  EXPECT_EQ(firstIndex, secondIndex);
  EXPECT_EQ(doc.images.size(), 1u);
}

TEST(ResourceBaker, ImageSemanticKeyCollapse) {
  BakeContext ctx;
  PAGDocument doc;
  // Different node pointers, different Data ptrs, but identical semantic key.
  uint32_t firstIndex = RegisterImage(ctx, doc, MakeImage(0xDD, 32), nullptr, "/abs/path/a.png");
  uint32_t secondIndex = RegisterImage(ctx, doc, MakeImage(0xEE, 48), nullptr, "/abs/path/a.png");
  EXPECT_EQ(firstIndex, secondIndex);
  EXPECT_EQ(doc.images.size(), 1u);
}

TEST(ResourceBaker, ImageDistinctAssetsTakeFreshSlots) {
  BakeContext ctx;
  PAGDocument doc;
  uint32_t a = RegisterImage(ctx, doc, MakeImage(0x11, 16), nullptr, "/key/a");
  uint32_t b = RegisterImage(ctx, doc, MakeImage(0x22, 16), nullptr, "/key/b");
  uint32_t c = RegisterImage(ctx, doc, MakeImage(0x33, 16), nullptr, "/key/c");
  EXPECT_EQ(a, 0u);
  EXPECT_EQ(b, 1u);
  EXPECT_EQ(c, 2u);
  EXPECT_EQ(doc.images.size(), 3u);
}

TEST(ResourceBaker, FontSemanticKeyCollapse) {
  BakeContext ctx;
  PAGDocument doc;
  std::string semantic = std::string("system") + '\0' + "PingFang" + '\0' + "Regular";
  uint32_t firstIndex = RegisterFont(ctx, doc, MakeFont("PingFang", "Regular"), nullptr, semantic);
  uint32_t secondIndex = RegisterFont(ctx, doc, MakeFont("PingFang", "Regular"), nullptr, semantic);
  EXPECT_EQ(firstIndex, secondIndex);
  EXPECT_EQ(doc.fonts.size(), 1u);
}

TEST(ResourceBaker, FontDistinctFamiliesTakeFreshSlots) {
  BakeContext ctx;
  PAGDocument doc;
  std::string keyA = std::string("system") + '\0' + "PingFang" + '\0' + "Regular";
  std::string keyB = std::string("system") + '\0' + "Helvetica" + '\0' + "Bold";
  uint32_t a = RegisterFont(ctx, doc, MakeFont("PingFang", "Regular"), nullptr, keyA);
  uint32_t b = RegisterFont(ctx, doc, MakeFont("Helvetica", "Bold"), nullptr, keyB);
  EXPECT_NE(a, b);
  EXPECT_EQ(doc.fonts.size(), 2u);
}

TEST(ResourceBaker, NullAssetDoesNotCrash) {
  BakeContext ctx;
  PAGDocument doc;
  // Null asset triggers a fresh slot at index 0 (Baker hits this when an
  // image source is missing — we still want a placeholder index back).
  uint32_t idx = RegisterImage(ctx, doc, nullptr, nullptr, "missing");
  EXPECT_EQ(idx, 0u);
  EXPECT_EQ(doc.images.size(), 1u);
  EXPECT_EQ(doc.images[0], nullptr);
}

}  // namespace pagx::pag
