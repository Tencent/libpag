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

#include <filesystem>
#include <fstream>
#include <string>
#include "base/PAGTest.h"
#include "cli/ImageStorage.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/types/Data.h"
#include "utils/TestDir.h"

namespace pag {
using namespace pagx;
using namespace pagx::cli;

namespace {

// A per-test scratch directory under test/out. Recreated fresh so leftovers from a previous run
// never leak into assertions about copied/embedded files.
std::string ScratchDir(const std::string& name) {
  auto dir = TestDir::GetRoot() + "/out/ImageStorageTest/" + name;
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  return dir;
}

void WriteFile(const std::string& path, const std::string& contents) {
  std::filesystem::create_directories(std::filesystem::path(path).parent_path());
  std::ofstream out(path, std::ios::binary);
  out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
}

std::string WithSlash(const std::string& dir) {
  return dir.empty() || dir.back() == '/' ? dir : dir + "/";
}

Image* AddImage(PAGXDocument* doc, const std::string& filePath) {
  auto* image = doc->makeNode<Image>();
  image->filePath = filePath;
  return image;
}

}  // namespace

// -------------------------------------------------------------------------------------------------
// ParseImageStorageMode
// -------------------------------------------------------------------------------------------------

CLI_TEST(ImageStorageTest, ParseModeExternal) {
  ImageStorageMode mode = ImageStorageMode::Embed;
  EXPECT_TRUE(ParseImageStorageMode("external", &mode));
  EXPECT_EQ(mode, ImageStorageMode::External);
  // Case-insensitive.
  mode = ImageStorageMode::Embed;
  EXPECT_TRUE(ParseImageStorageMode("ExTeRnAl", &mode));
  EXPECT_EQ(mode, ImageStorageMode::External);
}

CLI_TEST(ImageStorageTest, ParseModeEmbedAliases) {
  for (const char* name : {"embed", "embedded", "inline", "EMBED", "Inline"}) {
    ImageStorageMode mode = ImageStorageMode::External;
    EXPECT_TRUE(ParseImageStorageMode(name, &mode)) << name;
    EXPECT_EQ(mode, ImageStorageMode::Embed) << name;
  }
}

CLI_TEST(ImageStorageTest, ParseModeUnknownLeavesModeUntouched) {
  ImageStorageMode mode = ImageStorageMode::External;
  EXPECT_FALSE(ParseImageStorageMode("bogus", &mode));
  EXPECT_EQ(mode, ImageStorageMode::External);
  EXPECT_FALSE(ParseImageStorageMode("", &mode));
  EXPECT_EQ(mode, ImageStorageMode::External);
}

CLI_TEST(ImageStorageTest, ParseModeNullOutPointer) {
  // A null out-pointer must be tolerated for both a valid and an invalid name.
  EXPECT_TRUE(ParseImageStorageMode("external", nullptr));
  EXPECT_TRUE(ParseImageStorageMode("embed", nullptr));
  EXPECT_FALSE(ParseImageStorageMode("nope", nullptr));
}

// -------------------------------------------------------------------------------------------------
// ApplyImageStorage — general
// -------------------------------------------------------------------------------------------------

CLI_TEST(ImageStorageTest, ApplyNullDocumentIsNoOp) {
  ImageStorageOptions options = {};
  ApplyImageStorage(nullptr, options, "test");  // Must not crash.
}

// -------------------------------------------------------------------------------------------------
// ApplyImageStorage — Embed mode
// -------------------------------------------------------------------------------------------------

CLI_TEST(ImageStorageTest, EmbedLeavesExistingDataAndDataUriUntouched) {
  auto doc = PAGXDocument::Make(100, 100);
  const char bytes[] = {1, 2, 3, 4};
  auto* withData = AddImage(doc.get(), "ignored.png");
  withData->data = Data::MakeWithCopy(bytes, sizeof(bytes));
  auto* dataUri = AddImage(doc.get(), "data:image/png;base64,AAAA");

  ImageStorageOptions options = {};
  options.mode = ImageStorageMode::Embed;
  ApplyImageStorage(doc.get(), options, "test");

  EXPECT_NE(withData->data, nullptr);
  EXPECT_EQ(withData->filePath, "ignored.png");
  EXPECT_EQ(dataUri->data, nullptr);
  EXPECT_EQ(dataUri->filePath, "data:image/png;base64,AAAA");
}

CLI_TEST(ImageStorageTest, EmbedLeavesRemoteUrlsUntouched) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* http = AddImage(doc.get(), "http://example.com/a.png");
  auto* https = AddImage(doc.get(), "https://example.com/b.png");

  ImageStorageOptions options = {};
  options.mode = ImageStorageMode::Embed;
  ApplyImageStorage(doc.get(), options, "test");

  EXPECT_EQ(http->data, nullptr);
  EXPECT_EQ(http->filePath, "http://example.com/a.png");
  EXPECT_EQ(https->data, nullptr);
  EXPECT_EQ(https->filePath, "https://example.com/b.png");
}

CLI_TEST(ImageStorageTest, EmbedRelativePathResolvedViaBaseDir) {
  auto dir = ScratchDir("EmbedRelativeBaseDir");
  WriteFile(dir + "/pics/logo.png", "PNGDATA");
  auto doc = PAGXDocument::Make(100, 100);
  auto* image = AddImage(doc.get(), "pics/logo.png");

  ImageStorageOptions options = {};
  options.mode = ImageStorageMode::Embed;
  options.baseDir = WithSlash(dir);
  ApplyImageStorage(doc.get(), options, "test");

  ASSERT_NE(image->data, nullptr);
  EXPECT_EQ(image->data->size(), 7u);
  EXPECT_TRUE(image->filePath.empty());
}

CLI_TEST(ImageStorageTest, EmbedFallsBackToOutputDirWhenNoBaseDir) {
  auto dir = ScratchDir("EmbedOutputDirFallback");
  WriteFile(dir + "/logo.png", "DATA");
  auto doc = PAGXDocument::Make(100, 100);
  auto* image = AddImage(doc.get(), "logo.png");

  ImageStorageOptions options = {};
  options.mode = ImageStorageMode::Embed;
  // baseDir empty -> helper falls back to outputDir for resolution.
  options.outputDir = WithSlash(dir);
  ApplyImageStorage(doc.get(), options, "test");

  ASSERT_NE(image->data, nullptr);
  EXPECT_TRUE(image->filePath.empty());
}

CLI_TEST(ImageStorageTest, EmbedAbsolutePathReadsFileDirectly) {
  auto dir = ScratchDir("EmbedAbsolute");
  auto abs = dir + "/pic.png";
  WriteFile(abs, "ABSDATA");
  auto doc = PAGXDocument::Make(100, 100);
  auto* image = AddImage(doc.get(), abs);

  ImageStorageOptions options = {};
  options.mode = ImageStorageMode::Embed;
  ApplyImageStorage(doc.get(), options, "test");

  ASSERT_NE(image->data, nullptr);
  EXPECT_EQ(image->data->size(), 7u);
  EXPECT_TRUE(image->filePath.empty());
}

CLI_TEST(ImageStorageTest, EmbedEmptyFilePathIsLeftAlone) {
  auto doc = PAGXDocument::Make(100, 100);
  // An empty source is neither a data URI, a URL, nor a relative path: it resolves to nothing.
  auto* image = AddImage(doc.get(), "");

  ImageStorageOptions options = {};
  options.mode = ImageStorageMode::Embed;
  ApplyImageStorage(doc.get(), options, "test");

  EXPECT_EQ(image->data, nullptr);
  EXPECT_TRUE(image->filePath.empty());
}

CLI_TEST(ImageStorageTest, EmbedSchemeAndDriveLetterPathsAreNotRelative) {
  auto doc = PAGXDocument::Make(100, 100);
  // A colon before the first slash marks a scheme/drive reference, treated as non-relative and
  // read verbatim (which fails to open, leaving the reference in place).
  auto* driveLetter = AddImage(doc.get(), "C:/nope.png");
  auto* scheme = AddImage(doc.get(), "ftp://host/x.png");

  ImageStorageOptions options = {};
  options.mode = ImageStorageMode::Embed;
  ApplyImageStorage(doc.get(), options, "test");

  EXPECT_EQ(driveLetter->data, nullptr);
  EXPECT_EQ(driveLetter->filePath, "C:/nope.png");
  EXPECT_EQ(scheme->data, nullptr);
  EXPECT_EQ(scheme->filePath, "ftp://host/x.png");
}

CLI_TEST(ImageStorageTest, EmbedColonAfterSlashIsRelative) {
  auto dir = ScratchDir("EmbedColonAfterSlash");
  // A colon appearing only after a slash is still a plain relative path.
  WriteFile(dir + "/dir/a:b.png", "COLON");
  auto doc = PAGXDocument::Make(100, 100);
  auto* image = AddImage(doc.get(), "dir/a:b.png");

  ImageStorageOptions options = {};
  options.mode = ImageStorageMode::Embed;
  options.baseDir = WithSlash(dir);
  ApplyImageStorage(doc.get(), options, "test");

  ASSERT_NE(image->data, nullptr);
  EXPECT_TRUE(image->filePath.empty());
}

CLI_TEST(ImageStorageTest, EmbedAbsoluteMissingFileReadsAndFails) {
  auto dir = ScratchDir("EmbedAbsoluteMissing");
  auto doc = PAGXDocument::Make(100, 100);
  // Absolute paths skip existence resolution and are read directly, so a missing one exercises the
  // unreadable-file path inside the reader.
  auto* image = AddImage(doc.get(), dir + "/missing.png");

  ImageStorageOptions options = {};
  options.mode = ImageStorageMode::Embed;
  ApplyImageStorage(doc.get(), options, "test");

  EXPECT_EQ(image->data, nullptr);
  EXPECT_EQ(image->filePath, dir + "/missing.png");
}

CLI_TEST(ImageStorageTest, ExternalFilePathEqualToDocumentDirIsLeftAlone) {
  auto dir = ScratchDir("ExternalPrefixEqualsPath");
  auto out = ScratchDir("ExternalPrefixEqualsPathOut");
  auto doc = PAGXDocument::Make(100, 100);
  // filePath exactly equal to the stripped prefix recovers an empty authored source, so there is
  // nothing to relocate.
  auto* image = AddImage(doc.get(), WithSlash(dir));

  ImageStorageOptions options = {};
  options.mode = ImageStorageMode::External;
  options.documentDir = WithSlash(dir);
  options.outputDir = WithSlash(out);
  ApplyImageStorage(doc.get(), options, "test");

  EXPECT_EQ(image->filePath, WithSlash(dir));
}

CLI_TEST(ImageStorageTest, EmbedMissingFileEmitsWarningAndKeepsReference) {
  auto dir = ScratchDir("EmbedMissing");
  auto doc = PAGXDocument::Make(100, 100);
  auto* image = AddImage(doc.get(), "does-not-exist.png");

  ImageStorageOptions options = {};
  options.mode = ImageStorageMode::Embed;
  options.baseDir = WithSlash(dir);
  ApplyImageStorage(doc.get(), options, "test");

  EXPECT_EQ(image->data, nullptr);
  EXPECT_EQ(image->filePath, "does-not-exist.png");
}

CLI_TEST(ImageStorageTest, EmbedEmptyFileIsTreatedAsUnreadable) {
  auto dir = ScratchDir("EmbedEmptyFile");
  WriteFile(dir + "/empty.png", "");
  auto doc = PAGXDocument::Make(100, 100);
  auto* image = AddImage(doc.get(), "empty.png");

  ImageStorageOptions options = {};
  options.mode = ImageStorageMode::Embed;
  options.baseDir = WithSlash(dir);
  ApplyImageStorage(doc.get(), options, "test");

  // Zero-length files cannot be embedded; the reference is left as a warning.
  EXPECT_EQ(image->data, nullptr);
  EXPECT_EQ(image->filePath, "empty.png");
}

// -------------------------------------------------------------------------------------------------
// ApplyImageStorage — External mode
// -------------------------------------------------------------------------------------------------

CLI_TEST(ImageStorageTest, ExternalLeavesInlineImagesUntouched) {
  auto dir = ScratchDir("ExternalInline");
  auto doc = PAGXDocument::Make(100, 100);
  const char bytes[] = {9, 8, 7};
  auto* withData = AddImage(doc.get(), "");
  withData->data = Data::MakeWithCopy(bytes, sizeof(bytes));
  auto* dataUri = AddImage(doc.get(), "data:image/png;base64,BBBB");

  ImageStorageOptions options = {};
  options.mode = ImageStorageMode::External;
  options.outputDir = WithSlash(dir);
  ApplyImageStorage(doc.get(), options, "test");

  EXPECT_NE(withData->data, nullptr);
  EXPECT_EQ(dataUri->filePath, "data:image/png;base64,BBBB");
}

CLI_TEST(ImageStorageTest, ExternalWithoutOutputDirLeavesReference) {
  auto doc = PAGXDocument::Make(100, 100);
  auto* image = AddImage(doc.get(), "assets/pic.png");

  ImageStorageOptions options = {};
  options.mode = ImageStorageMode::External;
  // No outputDir -> nowhere to write, reference untouched.
  ApplyImageStorage(doc.get(), options, "test");

  EXPECT_EQ(image->filePath, "assets/pic.png");
}

CLI_TEST(ImageStorageTest, ExternalLeavesAbsolutePathUntouched) {
  auto dir = ScratchDir("ExternalAbsolute");
  auto abs = dir + "/pic.png";
  WriteFile(abs, "DATA");
  auto doc = PAGXDocument::Make(100, 100);
  auto* image = AddImage(doc.get(), abs);

  ImageStorageOptions options = {};
  options.mode = ImageStorageMode::External;
  options.outputDir = WithSlash(ScratchDir("ExternalAbsoluteOut"));
  ApplyImageStorage(doc.get(), options, "test");

  // An absolute authored path is not relocatable without changing the reference; left as-is.
  EXPECT_EQ(image->filePath, abs);
}

CLI_TEST(ImageStorageTest, ExternalCopiesRelativeFileToOutputDir) {
  auto base = ScratchDir("ExternalCopyBase");
  auto out = ScratchDir("ExternalCopyOut");
  WriteFile(base + "/assets/pic.png", "PICDATA");
  auto doc = PAGXDocument::Make(100, 100);
  auto* image = AddImage(doc.get(), "assets/pic.png");

  ImageStorageOptions options = {};
  options.mode = ImageStorageMode::External;
  options.baseDir = WithSlash(base);
  options.outputDir = WithSlash(out);
  ApplyImageStorage(doc.get(), options, "test");

  EXPECT_EQ(image->filePath, "assets/pic.png");
  EXPECT_TRUE(std::filesystem::exists(out + "/assets/pic.png"));
}

CLI_TEST(ImageStorageTest, ExternalStripsDocumentDirPrefix) {
  auto docDir = ScratchDir("ExternalDocDir");
  auto out = ScratchDir("ExternalDocDirOut");
  // The importer prefixes relative sources with the document directory; that prefix must be
  // stripped so the copy keeps the authored relative name.
  WriteFile(docDir + "/assets/pic.png", "PREFIXED");
  auto doc = PAGXDocument::Make(100, 100);
  auto* image = AddImage(doc.get(), WithSlash(docDir) + "assets/pic.png");

  ImageStorageOptions options = {};
  options.mode = ImageStorageMode::External;
  options.documentDir = WithSlash(docDir);
  options.outputDir = WithSlash(out);
  ApplyImageStorage(doc.get(), options, "test");

  EXPECT_EQ(image->filePath, "assets/pic.png");
  EXPECT_TRUE(std::filesystem::exists(out + "/assets/pic.png"));
}

CLI_TEST(ImageStorageTest, ExternalColocatedFileJustCleansReference) {
  auto dir = ScratchDir("ExternalColocated");
  WriteFile(dir + "/pic.png", "SAME");
  auto doc = PAGXDocument::Make(100, 100);
  // Source already lives in the output directory: no copy, only the reference is normalised.
  auto* image = AddImage(doc.get(), WithSlash(dir) + "pic.png");

  ImageStorageOptions options = {};
  options.mode = ImageStorageMode::External;
  options.documentDir = WithSlash(dir);
  options.outputDir = WithSlash(dir);
  ApplyImageStorage(doc.get(), options, "test");

  EXPECT_EQ(image->filePath, "pic.png");
  EXPECT_TRUE(std::filesystem::exists(dir + "/pic.png"));
}

CLI_TEST(ImageStorageTest, ExternalMissingSourceEmitsWarningAndKeepsReference) {
  auto base = ScratchDir("ExternalMissingBase");
  auto out = ScratchDir("ExternalMissingOut");
  auto doc = PAGXDocument::Make(100, 100);
  auto* image = AddImage(doc.get(), "assets/gone.png");

  ImageStorageOptions options = {};
  options.mode = ImageStorageMode::External;
  options.baseDir = WithSlash(base);
  options.outputDir = WithSlash(out);
  ApplyImageStorage(doc.get(), options, "test");

  // The authored relative path is recovered but the file does not exist anywhere: left untouched.
  EXPECT_EQ(image->filePath, "assets/gone.png");
  EXPECT_FALSE(std::filesystem::exists(out + "/assets/gone.png"));
}

CLI_TEST(ImageStorageTest, ExternalResolvesViaFilePathDirectly) {
  auto out = ScratchDir("ExternalDirectResolveOut");
  auto src = ScratchDir("ExternalDirectResolveSrc");
  // filePath itself resolves on disk (prefixed with documentDir), exercising the primary
  // exists(filePath) branch rather than the baseDir fallback.
  WriteFile(src + "/pic.png", "DIRECT");
  auto doc = PAGXDocument::Make(100, 100);
  auto* image = AddImage(doc.get(), WithSlash(src) + "pic.png");

  ImageStorageOptions options = {};
  options.mode = ImageStorageMode::External;
  options.documentDir = WithSlash(src);
  options.outputDir = WithSlash(out);
  ApplyImageStorage(doc.get(), options, "test");

  EXPECT_EQ(image->filePath, "pic.png");
  EXPECT_TRUE(std::filesystem::exists(out + "/pic.png"));
}

CLI_TEST(ImageStorageTest, EmbedResolvesRelativePathAgainstCwd) {
  auto dir = ScratchDir("EmbedCwdRelative");
  auto abs = dir + "/logo.png";
  WriteFile(abs, "CWDDATA");
  // Express the file as a path relative to the current working directory with no baseDir, so the
  // resolver finds it directly rather than via a base join.
  auto rel = std::filesystem::relative(abs, std::filesystem::current_path()).generic_string();
  ASSERT_FALSE(rel.empty());
  auto doc = PAGXDocument::Make(100, 100);
  auto* image = AddImage(doc.get(), rel);

  ImageStorageOptions options = {};
  options.mode = ImageStorageMode::Embed;
  ApplyImageStorage(doc.get(), options, "test");

  ASSERT_NE(image->data, nullptr);
  EXPECT_TRUE(image->filePath.empty());
}

CLI_TEST(ImageStorageTest, ExternalCreateDirectoryFailureKeepsReference) {
  auto base = ScratchDir("ExternalCreateDirFailBase");
  auto out = ScratchDir("ExternalCreateDirFailOut");
  WriteFile(base + "/assets/pic.png", "PIC");
  // A regular file sitting where the destination sub-directory needs to be makes directory
  // creation fail; the reference must then be left unchanged with a warning.
  WriteFile(out + "/assets", "not a directory");
  auto doc = PAGXDocument::Make(100, 100);
  auto* image = AddImage(doc.get(), "assets/pic.png");

  ImageStorageOptions options = {};
  options.mode = ImageStorageMode::External;
  options.baseDir = WithSlash(base);
  options.outputDir = WithSlash(out);
  ApplyImageStorage(doc.get(), options, "test");

  EXPECT_EQ(image->filePath, "assets/pic.png");
}

CLI_TEST(ImageStorageTest, ExternalCopyFailureKeepsReference) {
  auto base = ScratchDir("ExternalCopyFailBase");
  auto out = ScratchDir("ExternalCopyFailOut");
  // The "source" is actually a directory, so copy_file fails even though it exists on disk.
  std::filesystem::create_directories(base + "/assets/pic.png");
  auto doc = PAGXDocument::Make(100, 100);
  auto* image = AddImage(doc.get(), "assets/pic.png");

  ImageStorageOptions options = {};
  options.mode = ImageStorageMode::External;
  options.baseDir = WithSlash(base);
  options.outputDir = WithSlash(out);
  ApplyImageStorage(doc.get(), options, "test");

  EXPECT_EQ(image->filePath, "assets/pic.png");
}

CLI_TEST(ImageStorageTest, NonImageNodesAreSkipped) {
  auto dir = ScratchDir("NonImageNodes");
  WriteFile(dir + "/logo.png", "DATA");
  auto doc = PAGXDocument::Make(100, 100);
  // A non-Image node must be skipped by the loop; the Image following it is still processed.
  doc->makeNode<Rectangle>();
  auto* image = AddImage(doc.get(), "logo.png");

  ImageStorageOptions options = {};
  options.mode = ImageStorageMode::Embed;
  options.baseDir = WithSlash(dir);
  ApplyImageStorage(doc.get(), options, "test");

  EXPECT_NE(image->data, nullptr);
}

}  // namespace pag
