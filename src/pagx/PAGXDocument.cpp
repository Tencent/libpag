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
#include <unordered_set>
#include "LayoutContext.h"
#include "base/utils/Log.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/LayoutNode.h"
#include "renderer/FontEmbedder.h"
#include "renderer/LayerBuilder.h"

namespace pagx {

static bool IsExternalFilePath(const std::string& filePath) {
  return !filePath.empty() && filePath.find("data:") != 0;
}

static bool IsExpiredScene(const std::weak_ptr<PAGScene>& scene) {
  return scene.expired();
}

static void PruneExpiredScenes(std::vector<std::weak_ptr<PAGScene>>* scenes) {
  scenes->erase(std::remove_if(scenes->begin(), scenes->end(), IsExpiredScene), scenes->end());
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

static bool LoadExternalComposition(PAGXDocument* root, PAGXDocument* document, Layer* layer,
                                    const std::string& filePath, std::shared_ptr<Data> data,
                                    const std::unordered_set<std::string>& chain) {
  if (document == nullptr || layer == nullptr || data == nullptr ||
      layer->compositionFilePath != filePath || layer->composition != nullptr) {
    return false;
  }
  // Cycle guard: if this file path already appears among the ancestor origins on the load chain,
  // resolving it would nest externalDocs without end across repeated host load passes. Clear the
  // layer's compositionFilePath so getExternalFilePaths() stops returning it and the host loop
  // converges, then report the cycle on the root document.
  if (chain.find(filePath) != chain.end()) {
    root->errors.push_back("Cyclic external composition reference detected: '" + filePath + "'.");
    layer->compositionFilePath = {};
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
  // compositionFilePath is retained as the origin path of this externalDoc: it identifies the
  // ancestor source on the load chain for cycle detection, and lets the exporter round-trip the
  // external reference when the wrapper composition has no id.
  return true;
}

static bool LoadFileDataInChain(PAGXDocument* root, PAGXDocument* document,
                                const std::string& filePath, std::shared_ptr<Data> data,
                                std::unordered_set<std::string>& chain) {
  bool found = false;
  // First pass is read-only over nodes: handle Image nodes inline (they never append to nodes) and
  // snapshot Layer pointers. Layer resolution must be deferred because LoadExternalComposition calls
  // makeNode<Composition>(), which push_back()s into nodes and would invalidate this iterator.
  std::vector<Layer*> layers = {};
  for (auto& node : document->nodes) {
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
    bool loadedComposition = LoadExternalComposition(root, document, layer, filePath, data, chain);
    found = loadedComposition || found;
    if (!loadedComposition && layer->externalDoc != nullptr) {
      // Descend into an already-resolved externalDoc, recording its origin path so a reference back
      // to any ancestor origin (a cycle) is detected. Only erase the entry this frame inserted:
      // sibling externalDocs that legitimately share the same downstream file would otherwise drop
      // an ancestor's marker. insert().second is true exactly when this frame added the path.
      bool inserted = chain.insert(layer->compositionFilePath).second;
      found = LoadFileDataInChain(root, layer->externalDoc.get(), filePath, data, chain) || found;
      if (inserted) {
        chain.erase(layer->compositionFilePath);
      }
    }
  }
  return found;
}

void PAGXDocument::applyLayout(const FontConfig* config) {
  std::unordered_set<const PAGXDocument*> visited = {};
  applyLayout(config, visited);
}

void PAGXDocument::applyLayout(const FontConfig* config,
                               std::unordered_set<const PAGXDocument*>& visited) {
  if (!visited.insert(this).second) {
    errors.push_back("Cyclic external composition reference detected during layout.");
    return;
  }
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
  for (auto& node : nodes) {
    if (node->nodeType() == NodeType::Layer) {
      auto* layer = static_cast<Layer*>(node.get());
      if (layer->externalDoc != nullptr) {
        // Recurse so the externalDoc lays out its own layers (and any deeper externalDocs); it sets
        // its own layoutApplied flag internally. The host's fontConfig is passed down intentionally
        // so external compositions are measured and rendered with the host's fonts, keeping text
        // consistent across the embedded boundary rather than using the external doc's own config.
        layer->externalDoc->applyLayout(&fontConfig, visited);
      }
    }
  }
  // Mark layout complete only after all external subtrees have finished. Setting it earlier would
  // leave the document flagged as laid out even when a downstream cycle aborts the recursion,
  // letting PAGScene::Make build from an inconsistent tree.
  layoutApplied = true;
  visited.erase(this);
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
  // Match known URL schemes (case-insensitive per RFC 3986 Section 3.1) rather than generic ://
  // to avoid false positives on Windows paths like C://Users/file.png.
  // data: URIs have no "://" so they are checked separately.
  if (path.size() >= 5 && (path[0] == 'd' || path[0] == 'D') &&
      (path[1] == 'a' || path[1] == 'A') && (path[2] == 't' || path[2] == 'T') &&
      (path[3] == 'a' || path[3] == 'A') && path[4] == ':') {
    return true;
  }
  auto schemeEnd = path.find("://");
  if (schemeEnd == std::string::npos) {
    return false;
  }
  auto scheme = path.substr(0, schemeEnd);
  if (scheme.size() == 4) {
    if ((scheme[0] == 'h' || scheme[0] == 'H') && (scheme[1] == 't' || scheme[1] == 'T') &&
        (scheme[2] == 't' || scheme[2] == 'T') && (scheme[3] == 'p' || scheme[3] == 'P')) {
      return true;
    }
    if ((scheme[0] == 'f' || scheme[0] == 'F') && (scheme[1] == 'i' || scheme[1] == 'I') &&
        (scheme[2] == 'l' || scheme[2] == 'L') && (scheme[3] == 'e' || scheme[3] == 'E')) {
      return true;
    }
  } else if (scheme.size() == 5) {
    if ((scheme[0] == 'h' || scheme[0] == 'H') && (scheme[1] == 't' || scheme[1] == 'T') &&
        (scheme[2] == 't' || scheme[2] == 'T') && (scheme[3] == 'p' || scheme[3] == 'P') &&
        (scheme[4] == 's' || scheme[4] == 'S')) {
      return true;
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
  // loadFileData must run before any PAGScene is built from this document: PAGScene::Make()
  // snapshots the layer/composition tree once, so external compositions resolved here afterwards
  // would never reach the already-built runtime tree. A non-empty liveScenes means at least one
  // scene already exists, so warn that the freshly loaded content will not be visible.
  if (!liveScenes.empty()) {
    LOGE(
        "PAGXDocument::loadFileData() called after a PAGScene was created from this document. "
        "Load all external file data before PAGScene::Make(); the loaded content will not appear "
        "in existing scenes.");
  }
  std::unordered_set<std::string> chain = {};
  return LoadFileDataInChain(this, this, filePath, data, chain);
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
  // The Font nodes themselves remain in `nodes`, but they are no longer reachable from any
  // Text->glyphRuns and will not be touched by the next embed/render cycle. Drop their cached
  // tgfx typefaces here so the typeface memory is reclaimed promptly instead of being held
  // until the document is destroyed.
  for (auto& node : nodes) {
    if (node->nodeType() == NodeType::Font) {
      static_cast<Font*>(node.get())->resetRenderCache();
    }
  }
}

void PAGXDocument::notifyChange(const std::vector<Node*>& dirtyNodes) {
  if (dirtyNodes.empty()) {
    return;
  }
  // Prune expired weak_ptr entries to keep liveScenes bounded.
  PruneExpiredScenes(&liveScenes);
  // Dispatch to PAGScene::onNodesChanged by iterating liveScenes; each scene decides which dirty
  // nodes are relevant using its own runtime binding. Implementation lives in PAGScene.cpp to avoid
  // pulling PAGScene.h into the document header.
  // TODO(PR11): wire to PAGScene::onNodesChanged once that method is implemented.
  (void)dirtyNodes;
}

void PAGXDocument::registerLiveScene(const std::shared_ptr<PAGScene>& scene) {
  if (scene == nullptr) {
    return;
  }
  liveScenes.emplace_back(scene);
}

void PAGXDocument::unregisterLiveScene(PAGScene* scene) {
  if (scene == nullptr) {
    return;
  }
  PruneExpiredScenes(&liveScenes);
  for (auto it = liveScenes.begin(); it != liveScenes.end();) {
    if (it->lock().get() == scene) {
      it = liveScenes.erase(it);
    } else {
      ++it;
    }
  }
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
