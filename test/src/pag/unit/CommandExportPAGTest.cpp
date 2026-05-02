// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 11 smoke tests for `pagx export --format pag`. Forks the pagx CLI
// binary (path injected via PAGX_CLI_BINARY_PATH at build time) with a
// hand-built .pagx input and verifies:
//   - exit code 0 on a clean document,
//   - output .pag file has the PAGv2 magic (`PAG\x02`),
//   - --pag-strict flips exit to non-zero when the document would have
//     produced warnings.
//
// POSIX only: uses std::system() + shell escaping. Windows smoke runs live
// in the CI harness separately.
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include "gtest/gtest.h"
#include "pag/support/PAGXBuilder.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/nodes/Layer.h"

#if defined(PAGX_CLI_BINARY_PATH)

namespace pagx {

namespace {

std::string UniqueTempPath(const std::string& leaf) {
  auto dir = std::filesystem::temp_directory_path();
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%ld_%d", static_cast<long>(std::time(nullptr)),
                std::rand() % 10000);
  return (dir / (std::string("pagx_cli_") + buf + "_" + leaf)).string();
}

// Writes a minimal valid .pagx (single empty layer) to `outPath`. Returns
// true on success. Using the public PAGXExporter::ToXML keeps the CLI
// smoke independent of internal XML layout decisions.
bool MakeMinimalPagxFile(const std::string& outPath) {
  auto builder = pagx::test::PAGXBuilder::Make(640, 480);
  builder.AddLayer().Name("smoke");
  auto doc = builder.Done();  // Done() already runs applyLayout.
  auto xml = PAGXExporter::ToXML(*doc);
  if (xml.empty()) {
    return false;
  }
  std::ofstream out(outPath);
  if (!out.is_open()) {
    return false;
  }
  out << xml;
  out.close();
  return !out.fail();
}

bool HasPAGMagic(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    return false;
  }
  char buf[4] = {};
  in.read(buf, 4);
  if (in.gcount() < 4) {
    return false;
  }
  return buf[0] == 'P' && buf[1] == 'A' && buf[2] == 'G' && static_cast<uint8_t>(buf[3]) == 0x02;
}

// Runs the pagx binary with the given argv (space-joined, no shell
// metacharacter handling — all paths in tests use alphanumeric leaves so
// no escaping needed).
int RunPagxCli(const std::string& args) {
  std::string cmd = std::string(PAGX_CLI_BINARY_PATH) + " " + args + " > /dev/null 2>&1";
  return std::system(cmd.c_str());
}

}  // namespace

TEST(CommandExportPAG, SmokeProducesPagFile) {
  const auto pagxPath = UniqueTempPath("in.pagx");
  const auto pagPath = UniqueTempPath("out.pag");
  ASSERT_TRUE(MakeMinimalPagxFile(pagxPath));

  const int rc = RunPagxCli("export --input " + pagxPath + " --format pag --output " + pagPath);
  EXPECT_EQ(rc, 0);
  EXPECT_TRUE(std::filesystem::exists(pagPath));
  EXPECT_GT(std::filesystem::file_size(pagPath), 0u);
  EXPECT_TRUE(HasPAGMagic(pagPath));

  std::error_code ec;
  std::filesystem::remove(pagxPath, ec);
  std::filesystem::remove(pagPath, ec);
}

TEST(CommandExportPAG, MissingInputFileFails) {
  const auto pagPath = UniqueTempPath("nope.pag");
  const int rc =
      RunPagxCli("export --input /nonexistent/path.pagx --format pag --output " + pagPath);
  EXPECT_NE(rc, 0);
  EXPECT_FALSE(std::filesystem::exists(pagPath));
}

// --pag-strict + empty-document input would normally surface
// EmptyDocument=207 as a warning; strict mode promotes it to error and
// CLI returns non-zero without writing the file.
TEST(CommandExportPAG, StrictFlagBlocksEmptyDocument) {
  // Build a PAGX file with NO top-level layers so Baker emits
  // EmptyDocument=207.
  const auto pagxPath = UniqueTempPath("empty.pagx");
  const auto pagPath = UniqueTempPath("empty.pag");

  auto builder = pagx::test::PAGXBuilder::Make(320, 240);
  auto doc = builder.Done();  // Empty doc — no AddLayer calls.
  auto xml = PAGXExporter::ToXML(*doc);
  ASSERT_FALSE(xml.empty());
  {
    std::ofstream out(pagxPath);
    ASSERT_TRUE(out.is_open());
    out << xml;
  }

  // Without --pag-strict: should succeed.
  const int rcRelaxed =
      RunPagxCli("export --input " + pagxPath + " --format pag --output " + pagPath);
  EXPECT_EQ(rcRelaxed, 0);
  EXPECT_TRUE(std::filesystem::exists(pagPath));

  std::error_code ec;
  std::filesystem::remove(pagPath, ec);

  // With --pag-strict: should fail and not write the file.
  const int rcStrict = RunPagxCli("export --input " + pagxPath + " --format pag --output " +
                                  pagPath + " --pag-strict");
  EXPECT_NE(rcStrict, 0);
  EXPECT_FALSE(std::filesystem::exists(pagPath));

  std::filesystem::remove(pagxPath, ec);
}

}  // namespace pagx

#else

namespace pagx {
TEST(CommandExportPAG, DISABLED_SmokeProducesPagFile) {
  GTEST_SKIP() << "PAGX_CLI_BINARY_PATH not defined — build with CMake to enable.";
}
}  // namespace pagx

#endif  // PAGX_CLI_BINARY_PATH
