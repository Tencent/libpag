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
#include "pagx/nodes/Composition.h"

namespace pagx {

std::shared_ptr<PAGXDocument> PAGXDocument::Make(float docWidth, float docHeight) {
  auto doc = std::shared_ptr<PAGXDocument>(new PAGXDocument());
  doc->width = docWidth;
  doc->height = docHeight;
  return doc;
}

Node* PAGXDocument::findResource(const std::string& id) {
  if (resourceMapDirty) {
    rebuildResourceMap();
  }
  auto it = resourceMap.find(id);
  return it != resourceMap.end() ? it->second : nullptr;
}

Layer* PAGXDocument::findLayer(const std::string& id) const {
  // First search in top-level layers
  auto found = findLayerRecursive(layers, id);
  if (found) {
    return found;
  }
  // Then search in Composition resources
  for (const auto& resource : resources) {
    if (resource->nodeType() == NodeType::Composition) {
      auto comp = static_cast<const Composition*>(resource.get());
      found = findLayerRecursive(comp->layers, id);
      if (found) {
        return found;
      }
    }
  }
  return nullptr;
}

void PAGXDocument::rebuildResourceMap() {
  resourceMap.clear();
  for (const auto& resource : resources) {
    if (!resource->id.empty()) {
      resourceMap[resource->id] = resource.get();
    }
  }
  resourceMapDirty = false;
}

Layer* PAGXDocument::findLayerRecursive(const std::vector<std::unique_ptr<Layer>>& layers,
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
