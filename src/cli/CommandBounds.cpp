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

#include "cli/CommandBounds.h"
#include <libxml/parser.h>
#include <cstring>
#include <iostream>
#include <string>
#include "cli/CliUtils.h"
#include "cli/LayoutUtils.h"
#include "cli/XPathQuery.h"
#include "pagx/FontConfig.h"
#include "pagx/PAGXImporter.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/layers/Layer.h"

namespace pagx::cli {

struct BoundsOptions {
  std::string inputFile = {};
  std::string id = {};
  std::string xpath = {};
  std::string relativeTo = {};
  bool jsonOutput = false;
};

static void PrintBoundsUsage() {
  std::cout << "Usage: pagx bounds [options] <file.pagx>\n"
            << "\n"
            << "Query precise rendered bounds of Layer nodes.\n"
            << "\n"
            << "Options:\n"
            << "  --id <id>              Select a Layer by its id attribute\n"
            << "  --xpath <expr>         Select nodes by XPath expression\n"
            << "  --relative <xpath>     Output bounds relative to another Layer\n"
            << "  --json                 Output in JSON format\n"
            << "\n"
            << "Without --id or --xpath, outputs bounds for all layers.\n"
            << "\n"
            << "XPath examples:\n"
            << "  //Layer[@id='btn']     Layer with id 'btn'\n"
            << "  /pagx/Layer[1]         First top-level Layer\n"
            << "  /pagx/Layer[2]/Layer   Child Layers of second top-level Layer\n";
}

// Returns 0 on success, -1 if help was printed, 1 on error.
static int ParseBoundsOptions(int argc, char* argv[], BoundsOptions* options) {
  int i = 1;
  while (i < argc) {
    std::string arg = argv[i];
    if (arg == "--id" && i + 1 < argc) {
      options->id = argv[++i];
    } else if (arg == "--xpath" && i + 1 < argc) {
      options->xpath = argv[++i];
    } else if (arg == "--relative" && i + 1 < argc) {
      options->relativeTo = argv[++i];
    } else if (arg == "--json") {
      options->jsonOutput = true;
    } else if (arg == "--help" || arg == "-h") {
      PrintBoundsUsage();
      return -1;
    } else if (arg[0] == '-') {
      std::cerr << "pagx bounds: unknown option '" << arg << "'\n";
      return 1;
    } else {
      options->inputFile = arg;
    }
    i++;
  }
  if (!options->id.empty() && !options->xpath.empty()) {
    std::cerr << "pagx bounds: --id and --xpath are mutually exclusive\n";
    return 1;
  }
  if (options->inputFile.empty()) {
    std::cerr << "pagx bounds: missing input file\n";
    return 1;
  }
  return 0;
}

static void PrintBoundsText(const std::string& label, float left, float top, float width,
                            float height) {
  std::cout << label << ": x=" << left << " y=" << top << " width=" << width << " height=" << height
            << "\n";
}

static void PrintBoundsJson(const std::string& label, float left, float top, float width,
                            float height) {
  std::cout << "{\"label\": \"" << EscapeJson(label) << "\", \"x\": " << left << ", \"y\": " << top
            << ", \"width\": " << width << ", \"height\": " << height << "}\n";
}

static void PrintLayerBounds(const Layer* pagxLayer, tgfx::Layer* tgfxLayer,
                             tgfx::Layer* relativeLayer, bool jsonOutput) {
  auto bounds = tgfxLayer->getBounds(relativeLayer, true);
  float width = bounds.right - bounds.left;
  float height = bounds.bottom - bounds.top;
  auto label = GetLayerLabel(pagxLayer);
  if (jsonOutput) {
    PrintBoundsJson(label, bounds.left, bounds.top, width, height);
  } else {
    PrintBoundsText(label, bounds.left, bounds.top, width, height);
  }
}

static void PrintLayerBoundsRecursive(const Layer* pagxLayer, tgfx::Layer* tgfxLayer,
                                      tgfx::Layer* relativeLayer, bool jsonOutput, int depth) {
  auto bounds = tgfxLayer->getBounds(relativeLayer, true);
  float width = bounds.right - bounds.left;
  float height = bounds.bottom - bounds.top;
  auto label = GetLayerLabel(pagxLayer);
  if (jsonOutput) {
    PrintBoundsJson(label, bounds.left, bounds.top, width, height);
  } else {
    std::string indent(static_cast<size_t>(depth * 2), ' ');
    std::cout << indent;
    PrintBoundsText(label, bounds.left, bounds.top, width, height);
  }

  // Recurse into child layers. Match PAGX children with tgfx children by index.
  auto& tgfxChildren = tgfxLayer->children();
  for (size_t i = 0; i < pagxLayer->children.size() && i < tgfxChildren.size(); i++) {
    PrintLayerBoundsRecursive(pagxLayer->children[i], tgfxChildren[i].get(), relativeLayer,
                              jsonOutput, depth + 1);
  }
}

int RunBounds(int argc, char* argv[]) {
  BoundsOptions options = {};
  auto parseResult = ParseBoundsOptions(argc, argv, &options);
  if (parseResult != 0) {
    return parseResult == -1 ? 0 : parseResult;
  }

  auto document = PAGXImporter::FromFile(options.inputFile);
  if (document == nullptr) {
    std::cerr << "pagx bounds: failed to load '" << options.inputFile << "'\n";
    return 1;
  }
  for (auto& error : document->errors) {
    std::cerr << "pagx bounds: warning: " << error << "\n";
  }

  // Build layer tree with mapping.
  FontConfig fontProvider = {};
  auto buildResult = LayerBuilder::BuildWithMap(document.get(), &fontProvider);
  if (buildResult.root == nullptr) {
    std::cerr << "pagx bounds: failed to build layer tree\n";
    return 1;
  }

  // Parse the XML document once for XPath queries.
  bool needsXPath = !options.xpath.empty() || !options.relativeTo.empty();
  xmlDocPtr xmlDoc = nullptr;
  if (needsXPath) {
    xmlDoc = xmlReadFile(options.inputFile.c_str(), nullptr, XML_PARSE_NONET);
    if (xmlDoc == nullptr) {
      std::cerr << "pagx bounds: failed to parse XML from '" << options.inputFile << "'\n";
      return 1;
    }
  }

  // Resolve --relative Layer.
  tgfx::Layer* relativeLayer = nullptr;
  if (!options.relativeTo.empty()) {
    const Layer* relativePagx =
        EvaluateSingleXPath(xmlDoc, options.relativeTo, document.get(), "bounds");
    if (relativePagx == nullptr) {
      xmlFreeDoc(xmlDoc);
      std::cerr << "pagx bounds: --relative target not found\n";
      return 1;
    }
    auto it = buildResult.layerMap.find(relativePagx);
    if (it == buildResult.layerMap.end()) {
      xmlFreeDoc(xmlDoc);
      std::cerr << "pagx bounds: --relative target has no rendered layer\n";
      return 1;
    }
    relativeLayer = it->second.get();
  }

  if (!options.id.empty()) {
    // ID mode: look up a single Layer by document node ID.
    auto* node = document->findNode(options.id);
    if (node == nullptr) {
      std::cerr << "pagx bounds: no node found with id '" << options.id << "'\n";
      return 1;
    }
    if (node->nodeType() != NodeType::Layer) {
      std::cerr << "pagx bounds: node '" << options.id << "' is not a Layer\n";
      return 1;
    }
    auto* pagxLayer = static_cast<const Layer*>(node);
    auto it = buildResult.layerMap.find(pagxLayer);
    if (it == buildResult.layerMap.end()) {
      std::cerr << "pagx bounds: Layer '" << options.id << "' has no rendered layer\n";
      return 1;
    }
    PrintLayerBounds(pagxLayer, it->second.get(), relativeLayer, options.jsonOutput);
  } else if (!options.xpath.empty()) {
    // XPath mode: query specific nodes.
    auto matchedLayers = EvaluateXPath(xmlDoc, options.xpath, document.get());
    if (matchedLayers.empty()) {
      xmlFreeDoc(xmlDoc);
      std::cerr << "pagx bounds: no matching Layer nodes found\n";
      return 1;
    }
    for (const Layer* pagxLayer : matchedLayers) {
      auto it = buildResult.layerMap.find(pagxLayer);
      if (it == buildResult.layerMap.end()) {
        std::cerr << "pagx bounds: Layer '" << GetLayerLabel(pagxLayer)
                  << "' has no rendered layer\n";
        continue;
      }
      PrintLayerBounds(pagxLayer, it->second.get(), relativeLayer, options.jsonOutput);
    }
  } else {
    // No XPath: print bounds for the whole document and all layers.
    auto docBounds = buildResult.root->getBounds(relativeLayer, true);
    float docWidth = docBounds.right - docBounds.left;
    float docHeight = docBounds.bottom - docBounds.top;
    if (options.jsonOutput) {
      PrintBoundsJson("document", docBounds.left, docBounds.top, docWidth, docHeight);
    } else {
      PrintBoundsText("document", docBounds.left, docBounds.top, docWidth, docHeight);
    }

    auto& tgfxChildren = buildResult.root->children();
    for (size_t i = 0; i < document->layers.size() && i < tgfxChildren.size(); i++) {
      PrintLayerBoundsRecursive(document->layers[i], tgfxChildren[i].get(), relativeLayer,
                                options.jsonOutput, 1);
    }
  }

  if (xmlDoc != nullptr) {
    xmlFreeDoc(xmlDoc);
  }

  return 0;
}

}  // namespace pagx::cli
