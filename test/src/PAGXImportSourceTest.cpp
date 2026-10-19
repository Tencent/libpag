// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
#include <filesystem>
#include <fstream>
#include "base/PAGTest.h"
#include "cli/ImportRuntime.h"
#include "cli/ImportSource.h"
#include "pagx/nodes/Image.h"
#include "pagx/utils/Base64.h"

namespace pag {
using namespace pagx::cli;
namespace fs = std::filesystem;

CLI_TEST(PAGXImportSourceTest, OriginalImageBaseSurvivesTemporarySnapshot) {
  ImportTempDirectory source, temporary;
  std::ofstream(source.path / "pixel.png", std::ios::binary) << "image bytes";
  auto subset = temporary.path / "page.html";
  std::ofstream(subset) << "<html><body style='width:100px;height:100px'><img src='pixel.png' "
                           "width='1' height='1'/></body></html>";
  ImportItem item;
  item.input = (source.path / "original.html").u8string();
  item.format = "html";
  ImportRequest options;
  options.resolve = false;
  auto result = ImportPreparedSource(item, subset.u8string(), source.path.u8string(), options);
  ASSERT_TRUE(result.error.empty()) << result.error;
  bool found = false;
  for (const auto& node : result.document->nodes)
    if (node->nodeType() == pagx::NodeType::Image) {
      auto image = static_cast<pagx::Image*>(node.get());
      ASSERT_NE(image->data, nullptr);
      EXPECT_EQ(image->data->size(), 11u);
      EXPECT_TRUE(image->filePath.empty());
      found = true;
    }
  EXPECT_TRUE(found);
}

CLI_TEST(PAGXImportSourceTest, ResolveFailureIsNotSilentlyDropped) {
  ImportTempDirectory temp;
  auto subset = temp.path / "page.html";
  std::ofstream(subset) << "<html><body style='width:100px;height:100px'><img src='missing.svg' "
                           "width='10' height='10'/></body></html>";
  ImportItem item;
  item.input = subset.u8string();
  item.format = "html";
  ImportRequest options;
  auto result = ImportPreparedSource(item, subset.u8string(), temp.path.u8string(), options);
  EXPECT_FALSE(result.error.empty());
  options.resolve = false;
  result = ImportPreparedSource(item, subset.u8string(), temp.path.u8string(), options);
  ASSERT_TRUE(result.error.empty()) << result.error;
  EXPECT_TRUE(result.document->hasUnresolvedImports());
}

CLI_TEST(PAGXImportSourceTest, RelativeImageBaseOverridePreservesAbsoluteReferences) {
  ImportTempDirectory source, resources, fixed;
  std::ofstream(source.path / "pixel.png", std::ios::binary) << "original";
  std::ofstream(resources.path / "pixel.png", std::ios::binary) << "override";
  std::ofstream(fixed.path / "pixel.png", std::ios::binary) << "absolute";
  const auto absoluteImage = (fixed.path / "pixel.png").generic_u8string();
  for (const auto& format : {"html", "svg"}) {
    SCOPED_TRACE(format);
    const auto file = source.path / (std::string("page.") + format);
    if (std::string(format) == "html") {
      std::ofstream(file) << "<html><body style='width:20px;height:20px'>"
                             "<img src='pixel.png' width='1' height='1'/><img src='"
                          << absoluteImage << "' width='1' height='1'/></body></html>";
    } else {
      std::ofstream(file) << "<svg width='20' height='20'><image href='pixel.png' width='1' "
                             "height='1'/><image href='"
                          << absoluteImage << "' width='1' height='1'/></svg>";
    }
    ImportItem item;
    item.input = file.u8string();
    item.format = format;
    ImportRequest options;
    options.resolve = false;
    options.imageBaseDir = fs::relative(resources.path, fs::current_path()).u8string();
    auto result = ImportPreparedSource(item, item.input, source.path.u8string(), options);
    ASSERT_TRUE(result.error.empty()) << result.error;
    bool foundOverride = false, foundAbsolute = false;
    for (const auto& node : result.document->nodes) {
      if (node->nodeType() != pagx::NodeType::Image) continue;
      const auto image = static_cast<pagx::Image*>(node.get());
      ASSERT_NE(image->data, nullptr);
      const std::string bytes(static_cast<const char*>(image->data->data()), image->data->size());
      EXPECT_NE(bytes, "original");
      foundOverride |= bytes == "override";
      foundAbsolute |= bytes == "absolute";
    }
    EXPECT_TRUE(foundOverride);
    EXPECT_TRUE(foundAbsolute);
  }
}

CLI_TEST(PAGXImportSourceTest, MissingImageFailsEvenWithoutResolve) {
  ImportTempDirectory temp;
  auto subset = temp.path / "page.html";
  std::ofstream(subset) << "<html><body style='width:100px;height:100px'><img src='missing.png' "
                           "width='10' height='10'/></body></html>";
  ImportItem item;
  item.input = subset.u8string();
  item.format = "html";
  ImportRequest options;
  options.resolve = false;
  auto result = ImportPreparedSource(item, subset.u8string(), temp.path.u8string(), options);
  EXPECT_NE(result.error.find("image resource"), std::string::npos);
}

CLI_TEST(PAGXImportSourceTest, AtomicPublicationDoesNotReplaceExistingFile) {
  ImportTempDirectory temp;
  auto target = temp.path / "output.pagx";
  WriteImportFile(target, "old", false);
  EXPECT_THROW(WriteImportFile(target, "new", false), std::exception);
  std::ifstream old(target);
  std::string value;
  old >> value;
  EXPECT_EQ(value, "old");
  old.close();
  EXPECT_NO_THROW(WriteImportFile(target, "new", true));
  std::ifstream updated(target);
  updated >> value;
  EXPECT_EQ(value, "new");
}
}  // namespace pag
