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
#include <algorithm>
#include "LayoutContext.h"
#include "base/utils/Log.h"
#include "pagx/PAGFile.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/LayoutNode.h"
#include "renderer/FontEmbedder.h"

namespace pagx {

static bool IsExternalFilePath(const std::string& filePath) {
  return !filePath.empty() && filePath.find("data:") != 0;
}

static void AppendExternalFilePaths(const PAGXDocument* document, std::vector<std::string>* paths) {
  if (document == nullptr || paths == nullptr) {
    return;
  }
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Image) {
      auto* image = static_cast<Image*>(node.get());
      if (image->data == nullptr && IsExternalFilePath(image->filePath)) {
        paths->push_back(image->filePath);
      }
    } else if (node->nodeType() == NodeType::Layer) {
      auto* layer = static_cast<Layer*>(node.get());
      if (layer->composition == nullptr && IsExternalFilePath(layer->compositionFilePath)) {
        paths->push_back(layer->compositionFilePath);
      } else if (layer->externalDoc != nullptr) {
        AppendExternalFilePaths(layer->externalDoc.get(), paths);
      }
    }
  }
}

static bool LoadExternalComposition(PAGXDocument* document, Layer* layer,
                                    const std::string& filePath, std::shared_ptr<Data> data) {
  if (document == nullptr || layer == nullptr || data == nullptr ||
      layer->compositionFilePath != filePath || layer->composition != nullptr) {
    return false;
  }
  auto externalDoc = PAGXImporter::FromXML(data->bytes(), data->size());
  if (externalDoc == nullptr) {
    return false;
  }
  for (const auto& error : externalDoc->errors) {
    document->errors.push_back("[" + filePath + "] " + error);
  }
  auto* wrapper = document->makeNode<Composition>();
  wrapper->width = externalDoc->width;
  wrapper->height = externalDoc->height;
  wrapper->layers = externalDoc->layers;
  wrapper->animations = externalDoc->animations;
  layer->composition = wrapper;
  layer->externalDoc = externalDoc;
  layer->compositionFilePath = {};
  return true;
}

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
  AppendExternalFilePaths(this, &paths);
  return paths;
}

bool PAGXDocument::loadFileData(const std::string& filePath, std::shared_ptr<Data> data) {
  if (filePath.empty() || data == nullptr) {
    return false;
  }
  bool found = false;
  // First pass is read-only over nodes: handle Image nodes inline (they never append to nodes) and
  // snapshot Layer pointers. Layer resolution must be deferred because LoadExternalComposition calls
  // makeNode<Composition>(), which push_back()s into nodes and would invalidate this iterator.
  std::vector<Layer*> layers = {};
  for (auto& node : nodes) {
    if (node->nodeType() == NodeType::Image) {
      auto* image = static_cast<Image*>(node.get());
      if (image->filePath == filePath) {
        image->data = data;
        image->filePath = {};
        found = true;
      }
    } else if (node->nodeType() == NodeType::Layer) {
      layers.push_back(static_cast<Layer*>(node.get()));
    }
  }
  for (auto* layer : layers) {
    bool loadedComposition = LoadExternalComposition(this, layer, filePath, data);
    found = loadedComposition || found;
    if (!loadedComposition && layer->externalDoc != nullptr) {
      found = layer->externalDoc->loadFileData(filePath, data) || found;
    }
  }
  return found;
}

bool PAGXDocument::embed() {
  if (!isLayoutApplied()) {
    LOGE("PAGXDocument::embed() called before applyLayout(). Call applyLayout() first.");
    return false;
  }
  FontEmbedder embedder;
  return embedder.embed(this);
}

void PAGXDocument::clearEmbed() {
  FontEmbedder::ClearEmbeddedGlyphRuns(this);
}

void PAGXDocument::notifyChange(const std::vector<Node*>& dirtyNodes) {
  if (dirtyNodes.empty()) {
    return;
  }
  // Prune expired weak_ptr entries to keep liveFiles bounded.
  liveFiles.erase(std::remove_if(liveFiles.begin(), liveFiles.end(),
                                 [](const std::weak_ptr<PAGFile>& weak) { return weak.expired(); }),
                  liveFiles.end());
  // Dispatch to PAGFile::onNodesChanged by iterating liveFiles; each file decides which dirty nodes
  // are relevant using its own runtime binding. Implementation lives in PAGFile.cpp to avoid pulling
  // PAGFile.h into the document header.
  // TODO(PR11): wire to PAGFile::onNodesChanged once that method is implemented.
  (void)dirtyNodes;
}

void PAGXDocument::registerLiveFile(const std::shared_ptr<PAGFile>& file) {
  if (file == nullptr) {
    return;
  }
  liveFiles.emplace_back(file);
}

void PAGXDocument::unregisterLiveFile(PAGFile* file) {
  if (file == nullptr) {
    return;
  }
  liveFiles.erase(std::remove_if(liveFiles.begin(), liveFiles.end(),
                                 [file](const std::weak_ptr<PAGFile>& weak) {
                                   auto locked = weak.lock();
                                   return weak.expired() || locked.get() == file;
                                 }),
                  liveFiles.end());
}

}  // namespace pagx
