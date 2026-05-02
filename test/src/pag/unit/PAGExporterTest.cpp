// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 10 unit tests for PAGExporter — public ToBytes / ToFile entry
// points, strict-mode warning promotion, atomic file write.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include "gtest/gtest.h"
#include "pag/support/PAGXBuilder.h"
#include "pagx/PAGExporter.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Layer.h"

namespace pagx {

namespace {

std::shared_ptr<PAGXDocument> MakeSimpleDoc() {
  auto builder = pagx::test::PAGXBuilder::Make(640, 480);
  builder.AddLayer().Name("solo");
  return builder.RawDocument();
}

std::string UniqueTempPath(const std::string& leaf) {
  auto dir = std::filesystem::temp_directory_path();
  char pidBuf[32];
  std::snprintf(pidBuf, sizeof(pidBuf), "%d", static_cast<int>(std::time(nullptr)));
  return (dir / (std::string("pagx_exporter_") + pidBuf + "_" + leaf)).string();
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
// ToBytes — happy path.
// ---------------------------------------------------------------------------

TEST(PAGExporter, ToBytesSimpleDocument) {
  auto doc = MakeSimpleDoc();
  doc->applyLayout();
  auto r = PAGExporter::ToBytes(*doc);
  EXPECT_TRUE(r.ok);
  EXPECT_FALSE(r.bytes.empty());
  EXPECT_TRUE(r.errors.empty());
}

// ---------------------------------------------------------------------------
// ToBytes — missing applyLayout triggers Baker fatal; Result.ok=false and
// bytes cleared.
// ---------------------------------------------------------------------------

TEST(PAGExporter, ToBytesWithoutLayoutFailsFatal) {
  auto doc = MakeSimpleDoc();
  // Deliberately skip applyLayout.
  auto r = PAGExporter::ToBytes(*doc);
  EXPECT_FALSE(r.ok);
  EXPECT_TRUE(r.bytes.empty());
  EXPECT_TRUE(HasErrorCode(r.errors, DiagnosticCode::LayoutNotApplied));
}

// ---------------------------------------------------------------------------
// ToBytes — strict mode promotes Baker warnings. Build an empty doc so
// Baker emits EmptyDocument (207) warning, then flip strict=true.
// ---------------------------------------------------------------------------

TEST(PAGExporter, ToBytesStrictPromotesEmptyDocumentWarning) {
  auto doc = pagx::test::PAGXBuilder::Make().Done();  // no layers → EmptyDocument warning

  PAGExporter::Options opts;
  opts.strict = false;
  auto relaxed = PAGExporter::ToBytes(*doc, opts);
  EXPECT_TRUE(relaxed.ok);
  EXPECT_FALSE(relaxed.warnings.empty());
  EXPECT_FALSE(relaxed.bytes.empty());

  opts.strict = true;
  auto strict = PAGExporter::ToBytes(*doc, opts);
  EXPECT_FALSE(strict.ok);
  EXPECT_TRUE(strict.bytes.empty());
  EXPECT_FALSE(strict.errors.empty());
  EXPECT_TRUE(strict.warnings.empty())
      << "strict mode should drain warnings vector after promotion";
}

// ---------------------------------------------------------------------------
// ToFile — happy path writes the file atomically.
// ---------------------------------------------------------------------------

TEST(PAGExporter, ToFileWritesFile) {
  auto doc = MakeSimpleDoc();
  doc->applyLayout();

  const auto outPath = UniqueTempPath("ok.pag");
  auto r = PAGExporter::ToFile(*doc, outPath);
  EXPECT_TRUE(r.ok) << (r.errors.empty() ? "" : r.errors[0].message);
  EXPECT_TRUE(std::filesystem::exists(outPath));
  EXPECT_GT(std::filesystem::file_size(outPath), 0u);
  // ToFile leaves bytes empty.
  EXPECT_TRUE(r.bytes.empty());

  // The file starts with 'PAG' magic + version 0x02.
  std::ifstream in(outPath, std::ios::binary);
  char buf[4] = {};
  in.read(buf, 4);
  EXPECT_EQ(buf[0], 'P');
  EXPECT_EQ(buf[1], 'A');
  EXPECT_EQ(buf[2], 'G');
  EXPECT_EQ(static_cast<uint8_t>(buf[3]), 0x02);

  std::error_code ec;
  std::filesystem::remove(outPath, ec);
}

// ---------------------------------------------------------------------------
// ToFile — parent directory missing → ProducerFatal=106 and no file.
// ---------------------------------------------------------------------------

TEST(PAGExporter, ToFileMissingParentDirectoryFails) {
  auto doc = MakeSimpleDoc();
  doc->applyLayout();

  const std::string badPath = "/definitely/does/not/exist/output.pag";
  auto r = PAGExporter::ToFile(*doc, badPath);
  EXPECT_FALSE(r.ok);
  EXPECT_TRUE(HasErrorCode(r.errors, DiagnosticCode::ProducerFatal));
  EXPECT_FALSE(std::filesystem::exists(badPath));
}

// ---------------------------------------------------------------------------
// ToFile — strict warning promotion prevents file creation.
// ---------------------------------------------------------------------------

TEST(PAGExporter, ToFileStrictBlocksWriteOnWarning) {
  auto doc = pagx::test::PAGXBuilder::Make().Done();  // no layers → EmptyDocument warning

  const auto outPath = UniqueTempPath("strict.pag");
  PAGExporter::Options opts;
  opts.strict = true;
  auto r = PAGExporter::ToFile(*doc, outPath, opts);
  EXPECT_FALSE(r.ok);
  EXPECT_FALSE(std::filesystem::exists(outPath))
      << "strict-blocked ToFile must NOT leave a file behind";
}

}  // namespace pagx
