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

#include "cli/CommandOptimize.h"
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include "cli/CliUtils.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXOptimizer.h"

namespace pagx::cli {

namespace {

struct OptimizeOptions {
  std::string inputFile = {};
  std::string outputFile = {};
  PAGXOptimizer::Options optimizerOptions = {};
};

void PrintOptimizeUsage() {
  std::cout
      << "Usage: pagx optimize <file> [options]\n"
      << "\n"
      << "Simplifies the structure of a PAGX file without changing rendered output. Collapses\n"
      << "redundant Layers/Groups, merges adjacent shell Layers, rewrites Path nodes that\n"
      << "describe axis-aligned rectangles or ellipses, simplifies Path geometry below a\n"
      << "tolerance, and deduplicates shared PathData resources. Any Layer carrying id, name,\n"
      << "layout, or constraint attributes is left untouched.\n"
      << "\n"
      << "Options:\n"
      << "  -o, --output <path>       Output file path (default: overwrite input)\n"
      << "  --tolerance <float>       Path simplification tolerance in pixels (default: 0.25)\n"
      << "  --max-iterations <n>      Max rewrite iterations (default: 8)\n"
      << "  --no-prune-empty          Do not remove empty Layers/Groups\n"
      << "  --no-downgrade-shell      Do not downgrade shell children to Groups\n"
      << "  --no-merge-shell-layers   Do not merge adjacent shell Layers\n"
      << "  --no-unwrap-first-group   Do not unwrap leading redundant Groups\n"
      << "  --no-merge-groups         Do not merge adjacent Groups with identical painters\n"
      << "  --no-canonicalize-paths   Do not rewrite Paths as Rectangle/Ellipse when possible\n"
      << "  --no-rect-mask            Do not convert rect masks to scrollRect\n"
      << "  --no-simplify-paths       Do not simplify Path geometry\n"
      << "  --no-dedup-paths          Do not deduplicate PathData resources\n"
      << "  --no-prune-resources      Do not drop unreferenced resources\n"
      << "  -h, --help                Show this help message\n";
}

bool ParseFloatArg(const char* text, float* out) {
  char* endPtr = nullptr;
  errno = 0;
  double value = std::strtod(text, &endPtr);
  if (errno != 0 || endPtr == text || *endPtr != '\0') {
    return false;
  }
  *out = static_cast<float>(value);
  return true;
}

bool ParseIntArg(const char* text, int* out) {
  char* endPtr = nullptr;
  errno = 0;
  long value = std::strtol(text, &endPtr, 10);
  if (errno != 0 || endPtr == text || *endPtr != '\0' || value <= 0 || value > INT_MAX) {
    return false;
  }
  *out = static_cast<int>(value);
  return true;
}

int ParseOptimizeOptions(int argc, char* argv[], OptimizeOptions* options) {
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      PrintOptimizeUsage();
      return -1;
    }
    if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
      options->outputFile = argv[++i];
      continue;
    }
    if (arg == "--tolerance" && i + 1 < argc) {
      float value = 0.0f;
      if (!ParseFloatArg(argv[++i], &value) || value < 0.0f) {
        std::cerr << "pagx optimize: invalid --tolerance value '" << argv[i] << "'\n";
        return 1;
      }
      options->optimizerOptions.pathSimplifyTolerance = value;
      continue;
    }
    if (arg == "--max-iterations" && i + 1 < argc) {
      int value = 0;
      if (!ParseIntArg(argv[++i], &value)) {
        std::cerr << "pagx optimize: invalid --max-iterations value '" << argv[i] << "'\n";
        return 1;
      }
      options->optimizerOptions.maxIterations = value;
      continue;
    }
    if (arg == "--no-prune-empty") {
      options->optimizerOptions.pruneEmpty = false;
      continue;
    }
    if (arg == "--no-downgrade-shell") {
      options->optimizerOptions.downgradeShellChildren = false;
      continue;
    }
    if (arg == "--no-merge-shell-layers") {
      options->optimizerOptions.mergeAdjacentShellLayers = false;
      continue;
    }
    if (arg == "--no-unwrap-first-group") {
      options->optimizerOptions.unwrapRedundantFirstGroup = false;
      continue;
    }
    if (arg == "--no-merge-groups") {
      options->optimizerOptions.mergeAdjacentGroups = false;
      continue;
    }
    if (arg == "--no-canonicalize-paths") {
      options->optimizerOptions.canonicalizePaths = false;
      continue;
    }
    if (arg == "--no-rect-mask") {
      options->optimizerOptions.rectMaskToScrollRect = false;
      continue;
    }
    if (arg == "--no-simplify-paths") {
      options->optimizerOptions.simplifyPaths = false;
      continue;
    }
    if (arg == "--no-dedup-paths") {
      options->optimizerOptions.dedupPathData = false;
      continue;
    }
    if (arg == "--no-prune-resources") {
      options->optimizerOptions.pruneUnreferencedResources = false;
      continue;
    }
    if (!arg.empty() && arg[0] == '-') {
      std::cerr << "pagx optimize: unknown option '" << arg << "'\n";
      return 1;
    }
    if (options->inputFile.empty()) {
      options->inputFile = arg;
    } else {
      std::cerr << "pagx optimize: unexpected argument '" << arg << "'\n";
      return 1;
    }
  }

  if (options->inputFile.empty()) {
    std::cerr << "pagx optimize: missing input file\n";
    PrintOptimizeUsage();
    return 1;
  }
  if (options->outputFile.empty()) {
    options->outputFile = options->inputFile;
  }
  return 0;
}

}  // namespace

int RunOptimize(int argc, char* argv[]) {
  OptimizeOptions options = {};
  auto parseResult = ParseOptimizeOptions(argc, argv, &options);
  if (parseResult != 0) {
    return parseResult == -1 ? 0 : parseResult;
  }

  auto doc = LoadDocument(options.inputFile, "pagx optimize");
  if (doc == nullptr) {
    return 1;
  }

  PAGXOptimizer::Optimize(doc.get(), options.optimizerOptions);

  auto xml = PAGXExporter::ToXML(*doc);
  if (!WriteStringToFile(xml, options.outputFile, "pagx optimize")) {
    return 1;
  }
  return 0;
}

}  // namespace pagx::cli
