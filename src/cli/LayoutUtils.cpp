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

#include "cli/LayoutUtils.h"
#include <libxml/parser.h>
#include <iostream>
#include <unordered_set>
#include "cli/XPathQuery.h"

namespace pagx::cli {

std::vector<Layer*> SelectLayers(PAGXDocument* document, const std::string& inputFile,
                                 const std::vector<std::string>& ids, const std::string& xpath,
                                 const std::string& commandName) {
  std::vector<Layer*> result = {};
  std::unordered_set<Layer*> seen = {};

  // Collect Layers by --id.
  for (auto& id : ids) {
    auto* node = document->findNode(id);
    if (node == nullptr) {
      std::cerr << "pagx " << commandName << ": no node found with id '" << id << "'\n";
      return {};
    }
    if (node->nodeType() != NodeType::Layer) {
      std::cerr << "pagx " << commandName << ": node '" << id << "' is not a Layer\n";
      return {};
    }
    auto* layer = static_cast<Layer*>(node);
    if (seen.insert(layer).second) {
      result.push_back(layer);
    }
  }

  // Collect Layers by --xpath.
  if (!xpath.empty()) {
    auto xmlDoc = xmlReadFile(inputFile.c_str(), nullptr, XML_PARSE_NONET);
    if (xmlDoc == nullptr) {
      std::cerr << "pagx " << commandName << ": failed to parse XML from '" << inputFile << "'\n";
      return {};
    }
    auto xpathLayers = EvaluateXPath(xmlDoc, xpath, document);
    xmlFreeDoc(xmlDoc);
    if (xpathLayers.empty()) {
      std::cerr << "pagx " << commandName << ": --xpath matched no Layer nodes\n";
      return {};
    }
    for (auto* matchedLayer : xpathLayers) {
      if (seen.insert(matchedLayer).second) {
        result.push_back(matchedLayer);
      }
    }
  }

  return result;
}

std::vector<LayerInfo> ResolveLayerInfos(const std::vector<Layer*>& targets,
                                         const LayerBuildResult& buildResult,
                                         const std::string& commandName) {
  std::vector<LayerInfo> infos = {};
  infos.reserve(targets.size());
  for (auto* layer : targets) {
    auto it = buildResult.layerMap.find(layer);
    if (it == buildResult.layerMap.end()) {
      std::cerr << "pagx " << commandName << ": Layer '" << GetLayerLabel(layer)
                << "' has no rendered layer\n";
      continue;
    }
    LayerInfo info = {};
    info.pagxLayer = layer;
    info.tgfxLayer = it->second.get();
    info.globalBounds = info.tgfxLayer->getBounds(buildResult.root.get(), true);
    infos.push_back(info);
  }
  return infos;
}

void ApplyGlobalOffset(Layer* pagxLayer, tgfx::Layer* tgfxLayer, float deltaGlobalX,
                       float deltaGlobalY) {
  if (deltaGlobalX == 0.0f && deltaGlobalY == 0.0f) {
    return;
  }

  auto* parentTgfx = tgfxLayer->parent();
  if (parentTgfx == nullptr) {
    // Top-level Layer: global and local coordinates are the same.
    pagxLayer->x += deltaGlobalX;
    pagxLayer->y += deltaGlobalY;
    return;
  }

  // Convert global offset to parent's local coordinate space.
  // globalToLocal converts a global point to the Layer's local space. To get the offset in the
  // parent's coordinate system, we convert two global points through the parent and take the
  // difference. This correctly handles any rotation, scaling, or skewing in the parent chain.
  auto origin = parentTgfx->globalToLocal({0.0f, 0.0f});
  auto offset = parentTgfx->globalToLocal({deltaGlobalX, deltaGlobalY});
  float deltaLocalX = offset.x - origin.x;
  float deltaLocalY = offset.y - origin.y;

  pagxLayer->x += deltaLocalX;
  pagxLayer->y += deltaLocalY;
}

std::string GetLayerLabel(const Layer* layer) {
  if (!layer->id.empty()) {
    return layer->id;
  }
  if (!layer->name.empty()) {
    return layer->name;
  }
  return "(unnamed)";
}

bool CompareByPositionX(const LayerInfo& a, const LayerInfo& b) {
  return a.globalBounds.left < b.globalBounds.left;
}

bool CompareByPositionY(const LayerInfo& a, const LayerInfo& b) {
  return a.globalBounds.top < b.globalBounds.top;
}

}  // namespace pagx::cli
