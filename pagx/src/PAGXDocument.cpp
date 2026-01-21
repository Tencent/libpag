/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "pagx/PAGXDocument.h"
#include <fstream>
#include <sstream>
#include "PAGXXMLParser.h"
#include "PAGXXMLWriter.h"

namespace pagx {

std::shared_ptr<PAGXDocument> PAGXDocument::Make(float docWidth, float docHeight) {
  auto doc = std::shared_ptr<PAGXDocument>(new PAGXDocument());
  doc->width = docWidth;
  doc->height = docHeight;
  return doc;
}

std::shared_ptr<PAGXDocument> PAGXDocument::FromFile(const std::string& filePath) {
  std::ifstream file(filePath, std::ios::binary);
  if (!file.is_open()) {
    return nullptr;
  }
  std::stringstream buffer = {};
  buffer << file.rdbuf();
  auto doc = FromXML(buffer.str());
  if (doc) {
    auto lastSlash = filePath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
      doc->basePath = filePath.substr(0, lastSlash + 1);
    }
  }
  return doc;
}

std::shared_ptr<PAGXDocument> PAGXDocument::FromXML(const std::string& xmlContent) {
  return FromXML(reinterpret_cast<const uint8_t*>(xmlContent.data()), xmlContent.size());
}

std::shared_ptr<PAGXDocument> PAGXDocument::FromXML(const uint8_t* data, size_t length) {
  return PAGXXMLParser::Parse(data, length);
}

std::string PAGXDocument::toXML() const {
  return PAGXXMLWriter::Write(*this);
}

std::shared_ptr<PAGXDocument> PAGXDocument::clone() const {
  auto doc = std::shared_ptr<PAGXDocument>(new PAGXDocument());
  doc->version = version;
  doc->width = width;
  doc->height = height;
  doc->basePath = basePath;
  for (const auto& resource : resources) {
    doc->resources.push_back(
        std::unique_ptr<ResourceNode>(static_cast<ResourceNode*>(resource->clone().release())));
  }
  for (const auto& layer : layers) {
    doc->layers.push_back(
        std::unique_ptr<LayerNode>(static_cast<LayerNode*>(layer->clone().release())));
  }
  doc->resourceMapDirty = true;
  return doc;
}

ResourceNode* PAGXDocument::findResource(const std::string& id) const {
  if (resourceMapDirty) {
    rebuildResourceMap();
  }
  auto it = resourceMap.find(id);
  return it != resourceMap.end() ? it->second : nullptr;
}

LayerNode* PAGXDocument::findLayer(const std::string& id) const {
  // First search in top-level layers
  auto found = findLayerRecursive(layers, id);
  if (found) {
    return found;
  }
  // Then search in Composition resources
  for (const auto& resource : resources) {
    if (resource->type() == NodeType::Composition) {
      auto comp = static_cast<const CompositionNode*>(resource.get());
      found = findLayerRecursive(comp->layers, id);
      if (found) {
        return found;
      }
    }
  }
  return nullptr;
}

void PAGXDocument::rebuildResourceMap() const {
  resourceMap.clear();
  for (const auto& resource : resources) {
    if (!resource->id.empty()) {
      resourceMap[resource->id] = resource.get();
    }
  }
  resourceMapDirty = false;
}

LayerNode* PAGXDocument::findLayerRecursive(const std::vector<std::unique_ptr<LayerNode>>& layers,
                                            const std::string& id) {
  for (const auto& layer : layers) {
    if (layer->id == id) {
      return layer.get();
    }
    auto found = findLayerRecursive(layer->children, id);
    if (found) {
      return found;
    }
  }
  return nullptr;
}

}  // namespace pagx
