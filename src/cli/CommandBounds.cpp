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
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include "renderer/LayerBuilder.h"
#include "renderer/TextLayout.h"
#include "pagx/PAGXImporter.h"
#include "tgfx/layers/Layer.h"

namespace pagx::cli {

struct BoundsOptions {
  std::string inputFile = {};
  std::string xpath = {};
  std::string relativeTo = {};
  bool jsonOutput = false;
};

static void PrintBoundsUsage() {
  std::cerr << "Usage: pagx bounds [options] <file.pagx>\n"
            << "\n"
            << "Query precise rendered bounds of Layer nodes.\n"
            << "\n"
            << "Options:\n"
            << "  --xpath <expr>         Select nodes by XPath expression\n"
            << "  --relative <xpath>     Output bounds relative to another Layer\n"
            << "  --json                 Output in JSON format\n"
            << "\n"
            << "Without --xpath, outputs bounds for all layers.\n"
            << "\n"
            << "XPath examples:\n"
            << "  //Layer[@id='btn']     Layer with id 'btn'\n"
            << "  /pagx/Layer[1]         First top-level Layer\n"
            << "  /pagx/Layer[2]/Layer   Child Layers of second top-level Layer\n";
}

static bool ParseBoundsOptions(int argc, char* argv[], BoundsOptions* options) {
  int i = 1;
  while (i < argc) {
    std::string arg = argv[i];
    if (arg == "--xpath" && i + 1 < argc) {
      options->xpath = argv[++i];
    } else if (arg == "--relative" && i + 1 < argc) {
      options->relativeTo = argv[++i];
    } else if (arg == "--json") {
      options->jsonOutput = true;
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

// Compute the "Layer path" from <pagx> root to a given xmlNode. The path is a list of 0-based
// child Layer indices at each level. Returns false if the node is not a <Layer> element.
static bool ComputeLayerPath(xmlNodePtr node, std::vector<int>* path) {
  if (node == nullptr || node->type != XML_ELEMENT_NODE) {
    return false;
  }
  if (!xmlStrEqual(node->name, BAD_CAST "Layer")) {
    return false;
  }

  // Walk up to build the path from root to node.
  std::vector<int> reversePath = {};
  xmlNodePtr current = node;
  while (current != nullptr && current->parent != nullptr) {
    bool isParentPagx = current->parent->name != nullptr &&
                        xmlStrEqual(current->parent->name, BAD_CAST "pagx");
    // Count the index of current among sibling <Layer> elements.
    int index = 0;
    for (xmlNodePtr sibling = current->parent->children; sibling != nullptr;
         sibling = sibling->next) {
      if (sibling == current) {
        break;
      }
      if (sibling->type == XML_ELEMENT_NODE && sibling->name != nullptr &&
          xmlStrEqual(sibling->name, BAD_CAST "Layer")) {
        index++;
      }
    }
    reversePath.push_back(index);

    if (isParentPagx) {
      break;
    }
    current = current->parent;
  }

  // Reverse to get root-to-node order.
  path->assign(reversePath.rbegin(), reversePath.rend());
  return true;
}

// Resolve a Layer path to a pagx::Layer pointer in the document.
static const Layer* ResolveLayerPath(const PAGXDocument* document, const std::vector<int>& path) {
  if (path.empty()) {
    return nullptr;
  }
  // First index selects from document->layers.
  int topIndex = path[0];
  if (topIndex < 0 || topIndex >= static_cast<int>(document->layers.size())) {
    return nullptr;
  }
  const Layer* current = document->layers[static_cast<size_t>(topIndex)];

  // Subsequent indices descend through children.
  for (size_t i = 1; i < path.size(); i++) {
    int childIndex = path[i];
    if (childIndex < 0 || childIndex >= static_cast<int>(current->children.size())) {
      return nullptr;
    }
    current = current->children[static_cast<size_t>(childIndex)];
  }
  return current;
}

// Evaluate an XPath expression and return matching Layer nodes.
static std::vector<const Layer*> EvaluateXPath(xmlDocPtr xmlDoc, const std::string& xpathExpr,
                                               const PAGXDocument* document) {
  std::vector<const Layer*> results = {};

  xmlXPathContextPtr xpathCtx = xmlXPathNewContext(xmlDoc);
  if (xpathCtx == nullptr) {
    std::cerr << "pagx bounds: failed to create XPath context\n";
    return results;
  }

  auto* xpathBuf = reinterpret_cast<const xmlChar*>(xpathExpr.c_str());
  xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xpathBuf, xpathCtx);
  if (xpathObj == nullptr) {
    std::cerr << "pagx bounds: invalid XPath expression '" << xpathExpr << "'\n";
    xmlXPathFreeContext(xpathCtx);
    return results;
  }

  if (xpathObj->nodesetval != nullptr) {
    int count = xpathObj->nodesetval->nodeNr;
    for (int i = 0; i < count; i++) {
      xmlNodePtr xmlNode = xpathObj->nodesetval->nodeTab[i];
      std::vector<int> layerPath = {};
      if (!ComputeLayerPath(xmlNode, &layerPath)) {
        auto name =
            xmlNode->name ? std::string(reinterpret_cast<const char*>(xmlNode->name)) : "unknown";
        std::cerr << "pagx bounds: XPath matched a <" << name
                  << "> node, but only <Layer> nodes are supported\n";
        continue;
      }
      const Layer* pagxLayer = ResolveLayerPath(document, layerPath);
      if (pagxLayer != nullptr) {
        results.push_back(pagxLayer);
      }
    }
  }

  xmlXPathFreeObject(xpathObj);
  xmlXPathFreeContext(xpathCtx);
  return results;
}

// Evaluate XPath and return a single Layer (for --relative).
static const Layer* EvaluateSingleXPath(xmlDocPtr xmlDoc, const std::string& xpathExpr,
                                        const PAGXDocument* document) {
  auto layers = EvaluateXPath(xmlDoc, xpathExpr, document);
  if (layers.empty()) {
    return nullptr;
  }
  if (layers.size() > 1) {
    std::cerr << "pagx bounds: --relative XPath matched " << layers.size()
              << " nodes, expected exactly 1\n";
    return nullptr;
  }
  return layers[0];
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

static std::string GetLayerLabel(const Layer* layer) {
  if (!layer->id.empty()) {
    return layer->id;
  }
  if (!layer->name.empty()) {
    return layer->name;
  }
  return "(unnamed)";
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
  if (!ParseBoundsOptions(argc, argv, &options)) {
    return 1;
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
  TextLayout textLayout = {};
  textLayout.layout(document.get());
  auto buildResult = LayerBuilder::BuildWithMap(document.get(), &textLayout);
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
        EvaluateSingleXPath(xmlDoc, options.relativeTo, document.get());
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

  if (!options.xpath.empty()) {
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
