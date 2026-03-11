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

#include "cli/XPathQuery.h"
#include <libxml/xpath.h>
#include <iostream>
#include <string>
#include <vector>

namespace pagx::cli {

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
    bool isParentPagx =
        current->parent->name != nullptr && xmlStrEqual(current->parent->name, BAD_CAST "pagx");
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

std::vector<const Layer*> EvaluateXPath(xmlDocPtr xmlDoc, const std::string& xpathExpr,
                                        const PAGXDocument* document) {
  std::vector<const Layer*> results = {};

  xmlXPathContextPtr xpathCtx = xmlXPathNewContext(xmlDoc);
  if (xpathCtx == nullptr) {
    std::cerr << "pagx: failed to create XPath context\n";
    return results;
  }

  auto* xpathBuf = reinterpret_cast<const xmlChar*>(xpathExpr.c_str());
  xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xpathBuf, xpathCtx);
  if (xpathObj == nullptr) {
    std::cerr << "pagx: invalid XPath expression '" << xpathExpr << "'\n";
    xmlXPathFreeContext(xpathCtx);
    return results;
  }

  if (xpathObj->nodesetval != nullptr) {
    int count = xpathObj->nodesetval->nodeNr;
    results.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; i++) {
      xmlNodePtr xmlNode = xpathObj->nodesetval->nodeTab[i];
      std::vector<int> layerPath = {};
      if (!ComputeLayerPath(xmlNode, &layerPath)) {
        auto name =
            xmlNode->name ? std::string(reinterpret_cast<const char*>(xmlNode->name)) : "unknown";
        std::cerr << "pagx: XPath matched a <" << name
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

std::vector<Layer*> EvaluateXPath(xmlDocPtr xmlDoc, const std::string& xpathExpr,
                                  PAGXDocument* document) {
  auto constLayers = EvaluateXPath(xmlDoc, xpathExpr, static_cast<const PAGXDocument*>(document));
  std::vector<Layer*> results = {};
  results.reserve(constLayers.size());
  for (auto* layer : constLayers) {
    results.push_back(const_cast<Layer*>(layer));
  }
  return results;
}

const Layer* EvaluateSingleXPath(xmlDocPtr xmlDoc, const std::string& xpathExpr,
                                 const PAGXDocument* document, const std::string& commandName) {
  auto layers = EvaluateXPath(xmlDoc, xpathExpr, document);
  if (layers.empty()) {
    return nullptr;
  }
  if (layers.size() > 1) {
    std::cerr << "pagx " << commandName << ": --xpath matched " << layers.size()
              << " nodes, expected exactly 1\n";
    return nullptr;
  }
  return layers[0];
}

}  // namespace pagx::cli
