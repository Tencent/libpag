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

#include "pagx/PAGXDocument.h"
#include "LayoutContext.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/LayoutNode.h"

namespace pagx {

void PAGXDocument::applyLayout(const FontConfig* config) {
  if (config != nullptr) {
    fontConfig = *config;
  }
  LayoutContext context(&fontConfig);
  // Composition layers are laid out first since they may be referenced by document layers.
  for (auto& node : nodes) {
    if (node->nodeType() == NodeType::Composition) {
      auto* comp = static_cast<Composition*>(node.get());
      layoutLayers(comp->layers, comp->width, comp->height, &context);
    }
  }
  layoutLayers(layers, width, height, &context);
  layoutApplied = true;
}

void PAGXDocument::layoutLayers(const std::vector<Layer*>& layers, float containerW,
                                float containerH, LayoutContext* context) {
  for (auto* layer : layers) {
    layer->updateSize(context);
  }
  std::vector<LayoutNode*> nodes(layers.begin(), layers.end());
  LayoutNode::PerformConstraintLayout(nodes, containerW, containerH, {}, context);
}

std::shared_ptr<PAGXDocument> PAGXDocument::Make(float docWidth, float docHeight) {
  auto doc = std::shared_ptr<PAGXDocument>(new PAGXDocument());
  doc->width = docWidth;
  doc->height = docHeight;
  return doc;
}

Node* PAGXDocument::findNode(const std::string& id) const {
  auto it = nodeMap.find(id);
  return it != nodeMap.end() ? it->second : nullptr;
}

void PAGXDocument::registerNode(Node* node, const std::string& id) {
  if (id.empty()) {
    return;
  }
  auto it = nodeMap.find(id);
  if (it != nodeMap.end()) {
    errors.push_back("Duplicate node id '" + id + "'.");
  }
  node->id = id;
  nodeMap[id] = node;
}

void PAGXDocument::removeNodes(const std::unordered_set<Node*>& toRemove) {
  for (auto it = nodeMap.begin(); it != nodeMap.end();) {
    if (toRemove.count(it->second) > 0) {
      it = nodeMap.erase(it);
    } else {
      ++it;
    }
  }
  size_t writeIdx = 0;
  for (size_t readIdx = 0; readIdx < nodes.size(); readIdx++) {
    if (toRemove.count(nodes[readIdx].get()) == 0) {
      nodes[writeIdx++] = std::move(nodes[readIdx]);
    }
  }
  nodes.resize(writeIdx);
}

void PAGXDocument::setNodeId(Node* node, const std::string& id) {
  if (node == nullptr) {
    return;
  }
  if (!node->id.empty()) {
    auto it = nodeMap.find(node->id);
    if (it != nodeMap.end() && it->second == node) {
      nodeMap.erase(it);
    }
  }
  registerNode(node, id);
}

void PAGXDocument::resetLayoutState() {
  layoutApplied = false;
}

static bool LayersHaveImports(const std::vector<Layer*>& layers) {
  for (auto* layer : layers) {
    if (!layer->importDirective.source.empty() || !layer->importDirective.content.empty()) {
      return true;
    }
    if (LayersHaveImports(layer->children)) {
      return true;
    }
  }
  return false;
}

bool PAGXDocument::hasUnresolvedImports() const {
  if (LayersHaveImports(layers)) {
    return true;
  }
  for (auto& node : nodes) {
    if (node->nodeType() == NodeType::Composition) {
      auto* comp = static_cast<Composition*>(node.get());
      if (LayersHaveImports(comp->layers)) {
        return true;
      }
    }
  }
  return false;
}

static bool IsUrlPath(const std::string& path) {
  // Match known URL schemes rather than generic :// to avoid false positives on Windows paths
  // like C://Users/file.png. data: is handled by PAGXImporter before this point.
  return path.find("http://") == 0 || path.find("https://") == 0 || path.find("file://") == 0;
}

std::vector<std::string> PAGXDocument::getExternalFilePaths() const {
  std::vector<std::string> paths = {};
  for (auto& node : nodes) {
    if (node->nodeType() != NodeType::Image) {
      continue;
    }
    auto* image = static_cast<Image*>(node.get());
    if (image->data != nullptr || image->filePath.empty()) {
      continue;
    }
    if (IsUrlPath(image->filePath)) {
      continue;
    }
    paths.push_back(image->filePath);
  }
  return paths;
}

bool PAGXDocument::loadFileData(const std::string& filePath, std::shared_ptr<Data> data) {
  if (filePath.empty() || data == nullptr) {
    return false;
  }
  bool found = false;
  for (auto& node : nodes) {
    if (node->nodeType() != NodeType::Image) {
      continue;
    }
    auto* image = static_cast<Image*>(node.get());
    if (image->filePath != filePath) {
      continue;
    }
    image->data = data;
    image->filePath = {};
    found = true;
  }
  return found;
}

void PAGXDocument::loadFileDataMap(
    const std::unordered_map<std::string, std::shared_ptr<Data>>& fileDataMap) {
  for (auto& node : nodes) {
    if (node->nodeType() != NodeType::Image) {
      continue;
    }
    auto* image = static_cast<Image*>(node.get());
    if (image->data != nullptr || image->filePath.empty()) {
      continue;
    }
    auto it = fileDataMap.find(image->filePath);
    if (it == fileDataMap.end()) {
      continue;
    }
    image->data = it->second;
    image->filePath = {};
  }
}

}  // namespace pagx
