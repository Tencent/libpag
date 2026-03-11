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

#include "cli/CommandDistribute.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "cli/CliUtils.h"
#include "cli/LayoutUtils.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "renderer/LayerBuilder.h"
#include "renderer/TextLayout.h"
#include "tgfx/layers/Layer.h"

namespace pagx::cli {

enum class DistributeAxis {
  X,
  Y,
};

struct DistributeOptions {
  std::string inputFile = {};
  std::string outputFile = {};
  std::vector<std::string> ids = {};
  std::string xpath = {};
  DistributeAxis axis = DistributeAxis::X;
  bool hasAxis = false;
};

static void PrintDistributeUsage() {
  std::cout << "Usage: pagx distribute [options] <file.pagx>\n"
            << "\n"
            << "Distribute selected Layer nodes with equal spacing along an axis.\n"
            << "The first and last Layers (by position) remain in place.\n"
            << "\n"
            << "Options:\n"
            << "  --id <id>              Select a Layer by its id attribute (can be repeated)\n"
            << "  --xpath <expr>         Select Layers by XPath expression\n"
            << "  --axis <value>         Distribution axis: x or y\n"
            << "  -o, --output <path>    Output file path (default: overwrite input)\n"
            << "  -h, --help             Show this help message\n"
            << "\n"
            << "At least 3 Layers must be selected. --id can be used multiple times and\n"
            << "combined with --xpath.\n"
            << "\n"
            << "Examples:\n"
            << "  pagx distribute --id a --id b --id c --axis x input.pagx\n"
            << "  pagx distribute --xpath \"//Layer[@name='item']\" --axis y input.pagx\n";
}

// Returns 0 on success, -1 if help was printed, 1 on error.
static int ParseDistributeOptions(int argc, char* argv[], DistributeOptions* options) {
  int i = 1;
  while (i < argc) {
    std::string arg = argv[i];
    if (arg == "--id" && i + 1 < argc) {
      options->ids.push_back(argv[++i]);
    } else if (arg == "--xpath" && i + 1 < argc) {
      options->xpath = argv[++i];
    } else if (arg == "--axis" && i + 1 < argc) {
      std::string value = argv[++i];
      if (value == "x") {
        options->axis = DistributeAxis::X;
      } else if (value == "y") {
        options->axis = DistributeAxis::Y;
      } else {
        std::cerr << "pagx distribute: invalid axis '" << value << "' (expected: x, y)\n";
        return 1;
      }
      options->hasAxis = true;
    } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
      options->outputFile = argv[++i];
    } else if (arg == "--help" || arg == "-h") {
      PrintDistributeUsage();
      return -1;
    } else if (arg[0] == '-') {
      std::cerr << "pagx distribute: unknown option '" << arg << "'\n";
      return 1;
    } else {
      options->inputFile = arg;
    }
    i++;
  }
  if (options->inputFile.empty()) {
    std::cerr << "pagx distribute: missing input file\n";
    return 1;
  }
  if (!options->hasAxis) {
    std::cerr << "pagx distribute: missing --axis option\n";
    return 1;
  }
  if (options->ids.empty() && options->xpath.empty()) {
    std::cerr << "pagx distribute: at least one --id or --xpath must be specified\n";
    return 1;
  }
  if (options->outputFile.empty()) {
    options->outputFile = options->inputFile;
  }
  return 0;
}

int RunDistribute(int argc, char* argv[]) {
  DistributeOptions options = {};
  auto parseResult = ParseDistributeOptions(argc, argv, &options);
  if (parseResult != 0) {
    return parseResult == -1 ? 0 : parseResult;
  }

  auto document = PAGXImporter::FromFile(options.inputFile);
  if (document == nullptr) {
    std::cerr << "pagx distribute: failed to load '" << options.inputFile << "'\n";
    return 1;
  }
  for (auto& error : document->errors) {
    std::cerr << "pagx distribute: warning: " << error << "\n";
  }

  // Build layer tree with mapping.
  TextLayout textLayout = {};
  SetupSystemFallbackFonts(textLayout);
  auto buildResult = LayerBuilder::BuildWithMap(document.get(), &textLayout);
  if (buildResult.root == nullptr) {
    std::cerr << "pagx distribute: failed to build layer tree\n";
    return 1;
  }

  // Select target Layers.
  auto targets =
      SelectLayers(document.get(), options.inputFile, options.ids, options.xpath, "distribute");
  if (targets.empty()) {
    return 1;
  }
  if (targets.size() < 3) {
    std::cerr << "pagx distribute: at least 3 Layers must be selected (got " << targets.size()
              << ")\n";
    return 1;
  }

  // Resolve selected Layers and compute global bounds.
  auto infos = ResolveLayerInfos(targets, buildResult, "distribute");
  if (infos.size() < 3) {
    std::cerr << "pagx distribute: not enough rendered Layers to distribute\n";
    return 1;
  }

  // Sort by position along the distribution axis.
  bool isX = options.axis == DistributeAxis::X;
  std::sort(infos.begin(), infos.end(), isX ? CompareByPositionX : CompareByPositionY);

  // Compute equal spacing.
  // Total available space = (last element's leading edge) - (first element's trailing edge)
  // Then subtract the sizes of all middle elements.
  auto count = infos.size();
  float firstEnd = isX ? infos[0].globalBounds.right : infos[0].globalBounds.bottom;
  float lastStart = isX ? infos[count - 1].globalBounds.left : infos[count - 1].globalBounds.top;
  float middleSizes = 0.0f;
  for (size_t i = 1; i < count - 1; i++) {
    if (isX) {
      middleSizes += infos[i].globalBounds.right - infos[i].globalBounds.left;
    } else {
      middleSizes += infos[i].globalBounds.bottom - infos[i].globalBounds.top;
    }
  }
  float totalGap = lastStart - firstEnd - middleSizes;
  float gap = totalGap / static_cast<float>(count - 1);

  // Reposition middle elements.
  float currentPos = firstEnd + gap;
  for (size_t i = 1; i < count - 1; i++) {
    float currentStart = isX ? infos[i].globalBounds.left : infos[i].globalBounds.top;
    float delta = currentPos - currentStart;
    float deltaGlobalX = isX ? delta : 0.0f;
    float deltaGlobalY = isX ? 0.0f : delta;
    ApplyGlobalOffset(infos[i].pagxLayer, infos[i].tgfxLayer, deltaGlobalX, deltaGlobalY);
    float size = isX ? (infos[i].globalBounds.right - infos[i].globalBounds.left)
                     : (infos[i].globalBounds.bottom - infos[i].globalBounds.top);
    currentPos += size + gap;
  }

  // Write back.
  auto xml = PAGXExporter::ToXML(*document);
  std::ofstream out(options.outputFile);
  if (!out.is_open()) {
    std::cerr << "pagx distribute: failed to write '" << options.outputFile << "'\n";
    return 1;
  }
  out << xml;
  out.close();
  if (out.fail()) {
    std::cerr << "pagx distribute: error writing to '" << options.outputFile << "'\n";
    return 1;
  }

  std::cout << "pagx distribute: distributed " << infos.size() << " layers along "
            << (isX ? "x" : "y") << " axis, wrote " << options.outputFile << "\n";
  return 0;
}

}  // namespace pagx::cli
