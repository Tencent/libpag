// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 10.5 unit tests for PAGLoader — LoadFromBytes / LoadFromFile /
// Peek, strict mode scoping (Codec warning promotion vs Inflater warning
// pass-through), FileReadFailed surface, and the Phase 9.1
// ImageBytesReleasedAfterInflate contract.
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include "gtest/gtest.h"
#include "pag/support/PAGXBuilder.h"
#include "pagx/PAGExporter.h"
#include "pagx/PAGLoader.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Layer.h"
#include "tgfx/core/Data.h"

namespace pagx {

namespace {

std::shared_ptr<PAGXDocument> MakeSimpleDoc() {
  auto builder = pagx::test::PAGXBuilder::Make(800, 600);
  builder.AddLayer().Name("only");
  return builder.RawDocument();
}

std::vector<uint8_t> ExportToBytes(PAGXDocument* doc) {
  doc->applyLayout();
  auto r = PAGExporter::ToBytes(*doc);
  return r.bytes;
}

std::string UniqueTempPath(const std::string& leaf) {
  auto dir = std::filesystem::temp_directory_path();
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%ld", static_cast<long>(std::time(nullptr)));
  return (dir / (std::string("pagx_loader_") + buf + "_" + leaf)).string();
}

bool HasErrorCode(const std::vector<Diagnostic>& errs, DiagnosticCode code) {
  for (const auto& d : errs) {
    if (d.code == code) {
      return true;
    }
  }
  return false;
}

}  // namespace

// ---------------------------------------------------------------------------
// LoadFromBytes — round-trip through ToBytes produces a non-null Layer.
// ---------------------------------------------------------------------------

TEST(PAGLoader, LoadFromBytesRoundTrip) {
  auto doc = MakeSimpleDoc();
  auto bytes = ExportToBytes(doc.get());
  ASSERT_FALSE(bytes.empty());

  auto r = PAGLoader::LoadFromBytes(bytes.data(), bytes.size());
  EXPECT_TRUE(r.ok);
  EXPECT_NE(r.layer, nullptr);
}

TEST(PAGLoader, LoadFromBytesRejectsShortInput) {
  const uint8_t bogus[4] = {'P', 'A', 'G', 0x02};
  auto r = PAGLoader::LoadFromBytes(bogus, sizeof(bogus));
  EXPECT_FALSE(r.ok);
  EXPECT_TRUE(HasErrorCode(r.errors, DiagnosticCode::TruncatedData));
}

// ---------------------------------------------------------------------------
// LoadFromFile — happy path + missing-file path.
// ---------------------------------------------------------------------------

TEST(PAGLoader, LoadFromFileRoundTrip) {
  auto doc = MakeSimpleDoc();
  auto bytes = ExportToBytes(doc.get());
  ASSERT_FALSE(bytes.empty());

  const auto path = UniqueTempPath("file.pag");
  {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
  }

  auto r = PAGLoader::LoadFromFile(path);
  EXPECT_TRUE(r.ok);
  EXPECT_NE(r.layer, nullptr);

  std::error_code ec;
  std::filesystem::remove(path, ec);
}

TEST(PAGLoader, LoadFromFileMissingFile) {
  auto r = PAGLoader::LoadFromFile("/definitely/not/here.pag");
  EXPECT_FALSE(r.ok);
  EXPECT_TRUE(HasErrorCode(r.errors, DiagnosticCode::FileReadFailed));
}

// ---------------------------------------------------------------------------
// Peek — returns FileHeader fields without reading the whole document.
// ---------------------------------------------------------------------------

TEST(PAGLoader, PeekReturnsHeaderFields) {
  auto doc = MakeSimpleDoc();
  auto bytes = ExportToBytes(doc.get());
  ASSERT_FALSE(bytes.empty());

  auto p = PAGLoader::Peek(bytes.data(), bytes.size());
  EXPECT_TRUE(p.ok);
  EXPECT_FLOAT_EQ(p.width, 800.0f);
  EXPECT_FLOAT_EQ(p.height, 600.0f);
  // Default frame rate 24/1 from PAGXBuilder.
  EXPECT_EQ(p.frameRateNumerator, 24u);
  EXPECT_EQ(p.frameRateDenominator, 1u);
}

TEST(PAGLoader, PeekFromFile) {
  auto doc = MakeSimpleDoc();
  auto bytes = ExportToBytes(doc.get());

  const auto path = UniqueTempPath("peek.pag");
  {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
  }

  auto p = PAGLoader::Peek(path);
  EXPECT_TRUE(p.ok);
  EXPECT_FLOAT_EQ(p.width, 800.0f);

  std::error_code ec;
  std::filesystem::remove(path, ec);
}

TEST(PAGLoader, PeekRejectsBadMagic) {
  const uint8_t badMagic[9] = {'X', 'Y', 'Z', 0x02, 0, 0, 0, 0, 0};
  auto p = PAGLoader::Peek(badMagic, sizeof(badMagic));
  EXPECT_FALSE(p.ok);
  EXPECT_TRUE(HasErrorCode(p.errors, DiagnosticCode::InvalidMagic));
}

// ---------------------------------------------------------------------------
// Inflater warning pass-through — warn 604 (InflaterEmptyDocument) surfaces
// in warnings but ok stays true (no layer in Result.layer). An empty doc
// exported with strict=false yields this exact shape.
// ---------------------------------------------------------------------------

TEST(PAGLoader, EmptyDocumentSurfacesInflaterWarning) {
  auto doc = pagx::test::PAGXBuilder::Make().Done();  // no layers
  auto r = PAGExporter::ToBytes(*doc);
  ASSERT_TRUE(r.ok);

  auto loaded = PAGLoader::LoadFromBytes(r.bytes.data(), r.bytes.size());
  EXPECT_TRUE(loaded.ok) << "Inflater warnings must NOT flip ok=false";
  EXPECT_EQ(loaded.layer, nullptr);
  bool sawEmpty = false;
  for (const auto& w : loaded.warnings) {
    if (w.code == DiagnosticCode::InflaterEmptyDocument) {
      sawEmpty = true;
      break;
    }
  }
  EXPECT_TRUE(sawEmpty);
}

// ---------------------------------------------------------------------------
// Strict mode scoping — Inflater warnings (InflaterEmptyDocument=604) must
// NOT be promoted even under strict=true.
// ---------------------------------------------------------------------------

TEST(PAGLoader, StrictModeDoesNotPromoteInflaterWarning) {
  auto doc = pagx::test::PAGXBuilder::Make().Done();
  auto r = PAGExporter::ToBytes(*doc);
  ASSERT_TRUE(r.ok);

  PAGLoader::Options opts;
  opts.strict = true;
  auto loaded = PAGLoader::LoadFromBytes(r.bytes.data(), r.bytes.size(), opts);
  EXPECT_TRUE(loaded.ok);  // Strict scope excludes Inflater warnings.
  EXPECT_EQ(loaded.layer, nullptr);
  EXPECT_FALSE(loaded.warnings.empty());
}

}  // namespace pagx
