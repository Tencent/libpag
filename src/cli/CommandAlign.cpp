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

#include "cli/CommandAlign.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
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

enum class AlignAnchor {
  Left,
  Right,
  Top,
  Bottom,
  CenterX,
  CenterY,
};

struct AlignOptions {
  std::string inputFile = {};
  std::string outputFile = {};
  std::vector<std::string> ids = {};
  std::string xpath = {};
  AlignAnchor anchor = AlignAnchor::Left;
  bool hasAnchor = false;
};

static void PrintAlignUsage() {
  std::cout << "Usage: pagx align [options] <file.pagx>\n"
            << "\n"
            << "Align selected Layer nodes along a specified edge or center line.\n"
            << "\n"
            << "Options:\n"
            << "  --id <id>              Select a Layer by its id attribute (can be repeated)\n"
            << "  --xpath <expr>         Select Layers by XPath expression\n"
            << "  --anchor <value>       Alignment anchor: left, right, top, bottom, centerX,\n"
            << "                         centerY\n"
            << "  -o, --output <path>    Output file path (default: overwrite input)\n"
            << "  -h, --help             Show this help message\n"
            << "\n"
            << "At least 2 Layers must be selected. --id can be used multiple times and\n"
            << "combined with --xpath.\n"
            << "\n"
            << "Examples:\n"
            << "  pagx align --id btn1 --id btn2 --anchor left input.pagx\n"
            << "  pagx align --xpath \"//Layer[@name='icon']\" --anchor centerY input.pagx\n";
}

static bool ParseAlignAnchor(const std::string& value, AlignAnchor* anchor) {
  if (value == "left") {
    *anchor = AlignAnchor::Left;
  } else if (value == "right") {
    *anchor = AlignAnchor::Right;
  } else if (value == "top") {
    *anchor = AlignAnchor::Top;
  } else if (value == "bottom") {
    *anchor = AlignAnchor::Bottom;
  } else if (value == "centerX") {
    *anchor = AlignAnchor::CenterX;
  } else if (value == "centerY") {
    *anchor = AlignAnchor::CenterY;
  } else {
    return false;
  }
  return true;
}

// Returns 0 on success, -1 if help was printed, 1 on error.
static int ParseAlignOptions(int argc, char* argv[], AlignOptions* options) {
  int i = 1;
  while (i < argc) {
    std::string arg = argv[i];
    if (arg == "--id" && i + 1 < argc) {
      options->ids.push_back(argv[++i]);
    } else if (arg == "--xpath" && i + 1 < argc) {
      options->xpath = argv[++i];
    } else if (arg == "--anchor" && i + 1 < argc) {
      if (!ParseAlignAnchor(argv[++i], &options->anchor)) {
        std::cerr << "pagx align: invalid anchor '" << argv[i]
                  << "' (expected: left, right, top, bottom, centerX, centerY)\n";
        return 1;
      }
      options->hasAnchor = true;
    } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
      options->outputFile = argv[++i];
    } else if (arg == "--help" || arg == "-h") {
      PrintAlignUsage();
      return -1;
    } else if (arg[0] == '-') {
      std::cerr << "pagx align: unknown option '" << arg << "'\n";
      return 1;
    } else {
      options->inputFile = arg;
    }
    i++;
  }
  if (options->inputFile.empty()) {
    std::cerr << "pagx align: missing input file\n";
    return 1;
  }
  if (!options->hasAnchor) {
    std::cerr << "pagx align: missing --anchor option\n";
    return 1;
  }
  if (options->ids.empty() && options->xpath.empty()) {
    std::cerr << "pagx align: at least one --id or --xpath must be specified\n";
    return 1;
  }
  if (options->outputFile.empty()) {
    options->outputFile = options->inputFile;
  }
  return 0;
}

int RunAlign(int argc, char* argv[]) {
  AlignOptions options = {};
  auto parseResult = ParseAlignOptions(argc, argv, &options);
  if (parseResult != 0) {
    return parseResult == -1 ? 0 : parseResult;
  }

  auto document = PAGXImporter::FromFile(options.inputFile);
  if (document == nullptr) {
    std::cerr << "pagx align: failed to load '" << options.inputFile << "'\n";
    return 1;
  }
  for (auto& error : document->errors) {
    std::cerr << "pagx align: warning: " << error << "\n";
  }

  // Build layer tree with mapping.
  TextLayout textLayout = {};
  SetupSystemFallbackFonts(textLayout);
  auto buildResult = LayerBuilder::BuildWithMap(document.get(), &textLayout);
  if (buildResult.root == nullptr) {
    std::cerr << "pagx align: failed to build layer tree\n";
    return 1;
  }

  // Select target Layers.
  auto targets =
      SelectLayers(document.get(), options.inputFile, options.ids, options.xpath, "align");
  if (targets.empty()) {
    return 1;
  }
  if (targets.size() < 2) {
    std::cerr << "pagx align: at least 2 Layers must be selected (got " << targets.size() << ")\n";
    return 1;
  }

  // Resolve selected Layers to tgfx::Layer pointers and compute global bounds.
  auto infos = ResolveLayerInfos(targets, buildResult, "align");
  if (infos.size() < 2) {
    std::cerr << "pagx align: not enough rendered Layers to align\n";
    return 1;
  }

  // Compute the alignment target value.
  float targetValue = 0.0f;
  switch (options.anchor) {
    case AlignAnchor::Left: {
      targetValue = std::numeric_limits<float>::max();
      for (auto& info : infos) {
        targetValue = std::min(targetValue, info.globalBounds.left);
      }
      break;
    }
    case AlignAnchor::Right: {
      targetValue = std::numeric_limits<float>::lowest();
      for (auto& info : infos) {
        targetValue = std::max(targetValue, info.globalBounds.right);
      }
      break;
    }
    case AlignAnchor::Top: {
      targetValue = std::numeric_limits<float>::max();
      for (auto& info : infos) {
        targetValue = std::min(targetValue, info.globalBounds.top);
      }
      break;
    }
    case AlignAnchor::Bottom: {
      targetValue = std::numeric_limits<float>::lowest();
      for (auto& info : infos) {
        targetValue = std::max(targetValue, info.globalBounds.bottom);
      }
      break;
    }
    case AlignAnchor::CenterX: {
      float minLeft = std::numeric_limits<float>::max();
      float maxRight = std::numeric_limits<float>::lowest();
      for (auto& info : infos) {
        minLeft = std::min(minLeft, info.globalBounds.left);
        maxRight = std::max(maxRight, info.globalBounds.right);
      }
      targetValue = (minLeft + maxRight) / 2.0f;
      break;
    }
    case AlignAnchor::CenterY: {
      float minTop = std::numeric_limits<float>::max();
      float maxBottom = std::numeric_limits<float>::lowest();
      for (auto& info : infos) {
        minTop = std::min(minTop, info.globalBounds.top);
        maxBottom = std::max(maxBottom, info.globalBounds.bottom);
      }
      targetValue = (minTop + maxBottom) / 2.0f;
      break;
    }
  }

  // Apply alignment offsets.
  for (auto& info : infos) {
    float deltaGlobalX = 0.0f;
    float deltaGlobalY = 0.0f;
    switch (options.anchor) {
      case AlignAnchor::Left:
        deltaGlobalX = targetValue - info.globalBounds.left;
        break;
      case AlignAnchor::Right:
        deltaGlobalX = targetValue - info.globalBounds.right;
        break;
      case AlignAnchor::Top:
        deltaGlobalY = targetValue - info.globalBounds.top;
        break;
      case AlignAnchor::Bottom:
        deltaGlobalY = targetValue - info.globalBounds.bottom;
        break;
      case AlignAnchor::CenterX: {
        float currentCenter = (info.globalBounds.left + info.globalBounds.right) / 2.0f;
        deltaGlobalX = targetValue - currentCenter;
        break;
      }
      case AlignAnchor::CenterY: {
        float currentCenter = (info.globalBounds.top + info.globalBounds.bottom) / 2.0f;
        deltaGlobalY = targetValue - currentCenter;
        break;
      }
    }

    ApplyGlobalOffset(info.pagxLayer, info.tgfxLayer, deltaGlobalX, deltaGlobalY);
  }

  // Write back.
  auto xml = PAGXExporter::ToXML(*document);
  std::ofstream out(options.outputFile);
  if (!out.is_open()) {
    std::cerr << "pagx align: failed to write '" << options.outputFile << "'\n";
    return 1;
  }
  out << xml;
  out.close();
  if (out.fail()) {
    std::cerr << "pagx align: error writing to '" << options.outputFile << "'\n";
    return 1;
  }

  std::cout << "pagx align: aligned " << infos.size() << " layers, wrote " << options.outputFile
            << "\n";
  return 0;
}

}  // namespace pagx::cli
