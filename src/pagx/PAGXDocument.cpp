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
#include <unordered_set>
#include "LayoutContext.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/LayoutNode.h"
#include "pagx/nodes/Stroke.h"
#include "tgfx/core/Image.h"

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
    if (image->decodedImage != nullptr) {
      continue;
    }
    // Skip nodes that already carry a thumbnail (or any backend-texture preview). The host
    // typically responds to getExternalFilePaths() by downloading + attaching a thumbnail; once
    // that attachment lands, the path is considered "primed" and the host is expected to
    // schedule the full-quality upload through its own progressive logic, not through another
    // pass over getExternalFilePaths(). The eventual full-quality miss is signalled separately
    // via onTextureRequest, which only fires after eviction or for paths that never received a
    // thumbnail in the first place.
    if (image->thumbnailImage != nullptr) {
      continue;
    }
    if (image->filePath.find("data:") == 0) {
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

Image* PAGXDocument::loadDecodedImage(const std::string& filePath,
                                      std::shared_ptr<tgfx::Image> decodedImage) {
  if (filePath.empty() || decodedImage == nullptr) {
    return nullptr;
  }
  Image* firstMatch = nullptr;
  for (auto& node : nodes) {
    if (node->nodeType() != NodeType::Image) {
      continue;
    }
    auto* image = static_cast<Image*>(node.get());
    if (image->filePath != filePath) {
      continue;
    }
    image->decodedImage = decodedImage;
    if (firstMatch == nullptr) {
      firstMatch = image;
    }
  }
  return firstMatch;
}

Image* PAGXDocument::loadDecodedImageAsThumbnail(const std::string& filePath,
                                                 std::shared_ptr<tgfx::Image> thumbnailImage) {
  if (filePath.empty() || thumbnailImage == nullptr) {
    return nullptr;
  }
  Image* firstMatch = nullptr;
  for (auto& node : nodes) {
    if (node->nodeType() != NodeType::Image) {
      continue;
    }
    auto* image = static_cast<Image*>(node.get());
    if (image->filePath != filePath) {
      continue;
    }
    image->thumbnailImage = thumbnailImage;
    if (firstMatch == nullptr) {
      firstMatch = image;
    }
  }
  return firstMatch;
}

Image* PAGXDocument::clearDecodedImage(const std::string& filePath) {
  if (filePath.empty()) {
    return nullptr;
  }
  Image* firstMatch = nullptr;
  for (auto& node : nodes) {
    if (node->nodeType() != NodeType::Image) {
      continue;
    }
    auto* image = static_cast<Image*>(node.get());
    if (image->filePath != filePath) {
      continue;
    }
    image->decodedImage = nullptr;
    if (firstMatch == nullptr) {
      firstMatch = image;
    }
  }
  return firstMatch;
}

Image* PAGXDocument::clearThumbnailImage(const std::string& filePath) {
  if (filePath.empty()) {
    return nullptr;
  }
  Image* firstMatch = nullptr;
  for (auto& node : nodes) {
    if (node->nodeType() != NodeType::Image) {
      continue;
    }
    auto* image = static_cast<Image*>(node.get());
    if (image->filePath != filePath) {
      continue;
    }
    image->thumbnailImage = nullptr;
    if (firstMatch == nullptr) {
      firstMatch = image;
    }
  }
  return firstMatch;
}

// Records into `index` every Image node filePath referenced by an ImagePattern inside
// `element`, scoped to the owning `layer`. The inner unordered_set deduplicates layers that
// have multiple fills or strokes pointing at the same filePath so the resulting index keeps
// each Layer exactly once per path.
static void CollectImageFilePathsFromElement(
    const Element* element, const Layer* layer,
    std::unordered_map<std::string, std::unordered_set<const Layer*>>* index) {
  if (!element || !layer) {
    return;
  }
  auto recordPattern = [&](const ColorSource* color) {
    if (!color || color->nodeType() != NodeType::ImagePattern) {
      return;
    }
    const auto* pattern = static_cast<const ImagePattern*>(color);
    if (!pattern->image || pattern->image->filePath.empty()) {
      return;
    }
    (*index)[pattern->image->filePath].insert(layer);
  };

  switch (element->nodeType()) {
    case NodeType::Fill:
      recordPattern(static_cast<const Fill*>(element)->color);
      break;
    case NodeType::Stroke:
      recordPattern(static_cast<const Stroke*>(element)->color);
      break;
    case NodeType::Group:
    case NodeType::TextBox: {
      const auto* group = static_cast<const Group*>(element);
      for (const auto* child : group->elements) {
        CollectImageFilePathsFromElement(child, layer, index);
      }
      break;
    }
    default:
      break;
  }
}

static void CollectImageFilePathsFromLayers(
    const std::vector<Layer*>& layers,
    std::unordered_map<std::string, std::unordered_set<const Layer*>>* index,
    std::unordered_set<const Composition*>* visitedCompositions) {
  for (const auto* layer : layers) {
    if (!layer) {
      continue;
    }
    for (const auto* element : layer->contents) {
      CollectImageFilePathsFromElement(element, layer, index);
    }
    CollectImageFilePathsFromLayers(layer->children, index, visitedCompositions);
    // A Composition may be referenced by many Layers; skip re-entry once we have already
    // walked its inner layers to avoid quadratic blowups in documents that instance the
    // same Composition repeatedly.
    if (layer->composition && visitedCompositions->insert(layer->composition).second) {
      CollectImageFilePathsFromLayers(layer->composition->layers, index, visitedCompositions);
    }
  }
}

std::vector<const Layer*> PAGXDocument::findLayersByImageFilePath(
    const std::string& imageFilePath) {
  if (imageFilePath.empty()) {
    return {};
  }
  if (!layersByImageFilePathBuilt) {
    std::unordered_map<std::string, std::unordered_set<const Layer*>> tempIndex;
    std::unordered_set<const Composition*> visitedCompositions;
    CollectImageFilePathsFromLayers(layers, &tempIndex, &visitedCompositions);
    layersByImageFilePath.clear();
    layersByImageFilePath.reserve(tempIndex.size());
    for (auto& [path, layerSet] : tempIndex) {
      std::vector<const Layer*> layerList(layerSet.begin(), layerSet.end());
      layersByImageFilePath.emplace(path, std::move(layerList));
    }
    layersByImageFilePathBuilt = true;
  }
  auto it = layersByImageFilePath.find(imageFilePath);
  if (it == layersByImageFilePath.end()) {
    return {};
  }
  return it->second;
}

}  // namespace pagx
