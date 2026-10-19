// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
#pragma once

#include <map>
#include <string>
#include <vector>
#include "cli/CommandImport.h"
#include "cli/ImageStorage.h"

namespace pagx::cli {

struct ImportOptions {
  std::string input, batch, output, outputDir, outputName, format;
  std::string scenario, checkpoint, imageBaseDir;
  std::string htmlSource = "browser";
  std::string browserEngine = "puppeteer";
  ImportFormatOptions formatOptions = {};
  ImageStorageMode images = ImageStorageMode::External;
  bool resolve = true, render = false, preview = false, keepSnapshot = false;
  bool overwrite = false, dryRun = false, verbose = false, embedFonts = false;
  bool captureAnimations = false, scrollReveal = false, inlineIconFonts = true;
  bool downloadFonts = false;
  bool hasHTMLOptions = false, hasSVGOptions = false;
  double scale = 1;
  int previewColumns = 0;
  std::map<std::string, std::string> browserOptions;
  std::vector<std::string> cookies, headers;
};

struct ImportItem {
  std::string input, format, checkpoint, output, png, snapshot;
};

struct ImportPlan {
  std::vector<ImportItem> items;
  std::string manifest, previewDir;
};

bool IsImportURL(const std::string& input);
// Pure argument parsing. No browser, dependency installation or filesystem mutations.
// argv excludes the command name. An empty error with help=true means help was requested.
bool ParseImportOptions(const std::vector<std::string>& args, ImportOptions* options, bool* help,
                        std::string* error);
// checkpointNames come from the HTML adapter's scenario validator, in execution order.
bool BuildImportPlan(const ImportOptions& options, const std::vector<std::string>& checkpointNames,
                     ImportPlan* plan, std::string* error);
std::string ImportUsage();

}  // namespace pagx::cli
