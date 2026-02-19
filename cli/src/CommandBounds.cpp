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

#include "CommandBounds.h"
#include <cstring>
#include <iostream>
#include <string>
#include "LayerBuilder.h"
#include "TextLayout.h"
#include "pagx/PAGXImporter.h"
#include "tgfx/layers/Layer.h"

namespace pagx::cli {

struct BoundsOptions {
  std::string inputFile = {};
  std::string nodeId = {};
  bool jsonOutput = false;
};

static void PrintBoundsUsage() {
  std::cerr << "Usage: pagx bounds [options] <file.pagx>\n"
            << "\n"
            << "Options:\n"
            << "  --id <nodeId>      Query bounds for a specific node\n"
            << "  --format json      Output in JSON format\n";
}

static bool ParseBoundsOptions(int argc, char* argv[], BoundsOptions* options) {
  int i = 1;
  while (i < argc) {
    std::string arg = argv[i];
    if (arg == "--id" && i + 1 < argc) {
      options->nodeId = argv[++i];
    } else if (arg == "--format" && i + 1 < argc) {
      std::string fmt = argv[++i];
      if (fmt == "json") {
        options->jsonOutput = true;
      } else {
        std::cerr << "pagx bounds: unsupported format '" << fmt << "'\n";
        return false;
      }
    } else if (arg == "--help" || arg == "-h") {
      PrintBoundsUsage();
      return false;
    } else if (arg[0] == '-') {
      std::cerr << "pagx bounds: unknown option '" << arg << "'\n";
      return false;
    } else {
      options->inputFile = arg;
    }
    i++;
  }
  if (options->inputFile.empty()) {
    std::cerr << "pagx bounds: no input file specified\n";
    return false;
  }
  return true;
}

static void PrintBoundsText(const std::string& label, float left, float top, float width,
                            float height) {
  std::cout << label << ": x=" << left << " y=" << top << " width=" << width
            << " height=" << height << "\n";
}

static void PrintBoundsJson(const std::string& label, float left, float top, float width,
                            float height) {
  std::cout << "{\"label\": \"" << label << "\", \"x\": " << left << ", \"y\": " << top
            << ", \"width\": " << width << ", \"height\": " << height << "}\n";
}

static void PrintLayerBoundsRecursive(tgfx::Layer* layer, bool jsonOutput, int depth) {
  auto bounds = layer->getBounds(nullptr, true);
  float width = bounds.right - bounds.left;
  float height = bounds.bottom - bounds.top;
  auto label = layer->name();
  if (label.empty()) {
    label = "(unnamed)";
  }
  if (jsonOutput) {
    PrintBoundsJson(label, bounds.left, bounds.top, width, height);
  } else {
    std::string indent(static_cast<size_t>(depth * 2), ' ');
    std::cout << indent;
    PrintBoundsText(label, bounds.left, bounds.top, width, height);
  }
  for (auto& child : layer->children()) {
    PrintLayerBoundsRecursive(child.get(), jsonOutput, depth + 1);
  }
}

int RunBounds(int argc, char* argv[]) {
  BoundsOptions options = {};
  if (!ParseBoundsOptions(argc, argv, &options)) {
    return 1;
  }

  // Load the PAGX document.
  auto document = PAGXImporter::FromFile(options.inputFile);
  if (document == nullptr) {
    std::cerr << "pagx bounds: failed to load '" << options.inputFile << "'\n";
    return 1;
  }
  if (!document->errors.empty()) {
    for (auto& error : document->errors) {
      std::cerr << "pagx bounds: warning: " << error << "\n";
    }
  }

  // If --id is specified, validate that the node exists.
  if (!options.nodeId.empty()) {
    auto node = document->findNode(options.nodeId);
    if (node == nullptr) {
      std::cerr << "pagx bounds: node '" << options.nodeId << "' not found\n";
      return 1;
    }
  }

  // Perform text layout and build the layer tree.
  TextLayout textLayout = {};
  textLayout.layout(document.get());
  auto rootLayer = LayerBuilder::Build(document.get(), &textLayout);
  if (rootLayer == nullptr) {
    std::cerr << "pagx bounds: failed to build layer tree\n";
    return 1;
  }

  if (!options.nodeId.empty()) {
    // Search the layer tree for a layer whose name matches the node id.
    std::shared_ptr<tgfx::Layer> targetLayer = nullptr;
    // Use a simple BFS to find the layer by name.
    std::vector<tgfx::Layer*> queue = {rootLayer.get()};
    size_t front = 0;
    while (front < queue.size()) {
      auto* current = queue[front++];
      if (current->name() == options.nodeId) {
        // Found the matching layer. Since we only have a raw pointer, we need
        // to report bounds directly.
        auto bounds = current->getBounds(nullptr, true);
        float width = bounds.right - bounds.left;
        float height = bounds.bottom - bounds.top;
        if (options.jsonOutput) {
          PrintBoundsJson(options.nodeId, bounds.left, bounds.top, width, height);
        } else {
          PrintBoundsText(options.nodeId, bounds.left, bounds.top, width, height);
        }
        return 0;
      }
      for (auto& child : current->children()) {
        queue.push_back(child.get());
      }
    }
    std::cerr << "pagx bounds: layer for node '" << options.nodeId
              << "' not found in layer tree\n";
    return 1;
  }

  // No --id specified: print bounds for the whole document and all layers.
  auto docBounds = rootLayer->getBounds(nullptr, true);
  float docWidth = docBounds.right - docBounds.left;
  float docHeight = docBounds.bottom - docBounds.top;
  if (options.jsonOutput) {
    PrintBoundsJson("document", docBounds.left, docBounds.top, docWidth, docHeight);
  } else {
    PrintBoundsText("document", docBounds.left, docBounds.top, docWidth, docHeight);
  }
  PrintLayerBoundsRecursive(rootLayer.get(), options.jsonOutput, 1);

  return 0;
}

}  // namespace pagx::cli
