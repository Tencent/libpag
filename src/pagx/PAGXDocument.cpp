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
#include "pagx/nodes/Element.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/Gradient.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/LayoutNode.h"
#include "pagx/nodes/Stroke.h"
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
                                std::unordered_set<std::string>& chain,
                                std::unordered_map<PAGXDocument*, std::vector<Node*>>&
                                    docDirtyNodes) {
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
        docDirtyNodes[document].push_back(image);
        found = true;
      }
    } else if (node->nodeType() == NodeType::Layer) {
      layers.push_back(static_cast<Layer*>(node.get()));
    }
  }
  for (auto* layer : layers) {
    bool loadedComposition = LoadExternalComposition(root, document, layer, filePath, data, chain);
    if (loadedComposition) {
      docDirtyNodes[document].push_back(layer);
      docDirtyNodes[document].push_back(layer->composition);
      found = true;
    }
    if (!loadedComposition && layer->externalDoc != nullptr) {
      // Descend into an already-resolved externalDoc, recording its origin path so a reference back
      // to any ancestor origin (a cycle) is detected. Only erase the entry this frame inserted:
      // sibling externalDocs that legitimately share the same downstream file would otherwise drop
      // an ancestor's marker. insert().second is true exactly when this frame added the path.
      bool inserted = chain.insert(layer->compositionFilePath).second;
      found = LoadFileDataInChain(root, layer->externalDoc.get(), filePath, data, chain,
                                  docDirtyNodes) ||
              found;
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
  // Re-running layout on an already-laid-out document (e.g. from notifyChange after an edit) must
  // discard the cached layout outputs first; updateSize() skips re-measuring a node whose preferred
  // size is already set, so without this a size/constraint edit would keep the stale geometry.
  if (layoutApplied) {
    for (auto& node : nodes) {
      switch (node->nodeType()) {
        case NodeType::Layer:
          static_cast<Layer*>(node.get())->resetLayout();
          break;
        case NodeType::Rectangle:
        case NodeType::Ellipse:
        case NodeType::Path:
        case NodeType::Polystar:
        case NodeType::Text:
        case NodeType::TextPath:
        case NodeType::Group:
        case NodeType::TextBox: {
          auto* layoutNode = LayoutNode::AsLayoutNode(static_cast<Element*>(node.get()));
          if (layoutNode != nullptr) {
            layoutNode->resetLayout();
          }
          break;
        }
        default:
          break;
      }
    }
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
  std::unordered_set<std::string> chain = {};
  std::unordered_map<PAGXDocument*, std::vector<Node*>> docDirtyNodes = {};
  bool found = LoadFileDataInChain(this, this, filePath, data, chain, docDirtyNodes);
  if (found) {
    for (auto& entry : docDirtyNodes) {
      entry.first->notifyChange(entry.second, /*layoutChanged=*/true);
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

namespace {

// Checks whether a Fill, Stroke, Group, or ImagePattern node references (directly or indirectly
// through its color/image fields) the given target node. Used by findLayerForContentNode to
// determine whether a content node belongs to a particular Layer.
bool contentReferencesNode(const Node* parent, const Node* target) {
  if (parent == nullptr || target == nullptr) {
    return false;
  }
  if (parent->nodeType() == NodeType::Fill) {
    auto* fill = static_cast<const Fill*>(parent);
    if (fill->color == target) {
      return true;
    }
    if (fill->color != nullptr && contentReferencesNode(fill->color, target)) {
      return true;
    }
  }
  if (parent->nodeType() == NodeType::Stroke) {
    auto* stroke = static_cast<const Stroke*>(parent);
    if (stroke->color == target) {
      return true;
    }
    if (stroke->color != nullptr && contentReferencesNode(stroke->color, target)) {
      return true;
    }
  }
  if (parent->nodeType() == NodeType::ImagePattern) {
    auto* pattern = static_cast<const ImagePattern*>(parent);
    return pattern->image == target;
  }
  // Check Gradient.colorStops (LinearGradient, RadialGradient, ConicGradient, DiamondGradient).
  if (parent->nodeType() == NodeType::LinearGradient ||
      parent->nodeType() == NodeType::RadialGradient ||
      parent->nodeType() == NodeType::ConicGradient ||
      parent->nodeType() == NodeType::DiamondGradient) {
    auto* gradient = static_cast<const Gradient*>(parent);
    for (auto* stop : gradient->colorStops) {
      if (stop == target) {
        return true;
      }
    }
    return false;
  }
  if (parent->nodeType() == NodeType::Group) {
    auto* group = static_cast<const Group*>(parent);
    for (auto* elem : group->elements) {
      if (elem == target || contentReferencesNode(elem, target)) {
        return true;
      }
    }
  }
  return false;
}

// Searches all Layer nodes in the document for one whose contents, filters, or styles reference
// the given content node. Returns a pointer to the first Layer found, or nullptr if not found.
const Layer* findLayerForContentNode(const std::vector<std::unique_ptr<Node>>& allNodes,
                                     const Node* target) {
  for (auto& node : allNodes) {
    if (node->nodeType() != NodeType::Layer) {
      continue;
    }
    auto* layer = static_cast<const Layer*>(node.get());
    for (auto* elem : layer->contents) {
      if (elem == target || contentReferencesNode(elem, target)) {
        return layer;
      }
    }
    for (auto* filter : layer->filters) {
      if (filter == target) {
        return layer;
      }
    }
    for (auto* style : layer->styles) {
      if (style == target) {
        return layer;
      }
    }
  }
  return nullptr;
}

}  // namespace

void PAGXDocument::notifyChange(const std::vector<Node*>& dirtyNodes, bool layoutChanged) {
  if (dirtyNodes.empty()) {
    return;
  }
  // Partition the input into nodes this document owns (forwarded to live scenes) and foreign nodes
  // (dropped, with one aggregate LOGE). A node belongs to exactly one document, so a single
  // mis-routed entry must not stop the rest of the batch from refreshing — the editor scenario where
  // a multi-select dirty list mixes nodes from a parent and an embedded child document is common,
  // and silent batch rejection would translate every such typo into "nothing updates". Foreign-node
  // edits still need to be re-issued through the owning document by the caller (use ownsNode() to
  // predicate the call).
  std::vector<Node*> ownedDirty = {};
  ownedDirty.reserve(dirtyNodes.size());
  size_t foreignCount = 0;
  for (auto* node : dirtyNodes) {
    if (node == nullptr) {
      continue;
    }
    if (ownsNode(node)) {
      ownedDirty.push_back(node);
    } else {
      ++foreignCount;
    }
  }
  if (foreignCount > 0) {
    LOGE(
        "PAGXDocument::notifyChange: %zu node(s) not owned by this document were dropped; notify "
        "them through their own document.",
        foreignCount);
  }
  if (ownedDirty.empty()) {
    return;
  }
  // Resolve content nodes (Image, SolidColor, Gradient, Fill, Stroke, Group, Filter, Style, etc.)
  // to their owning Layers. refreshNodes only processes Layer nodes directly; non-Layer content
  // nodes are resolved here so callers can pass them to notifyChange without having to find the
  // owning Layer first. Timeline nodes (Animation, AnimationObject, Channel) and Layer nodes are
  // passed through as-is — they are handled by separate paths in onNodesChanged.
  //
  // Nodes that are NOT resolved (findLayerForContentNode returns nullptr) are silently dropped:
  // - GlyphRun, Font, and Glyph nodes are layout-time data generated by applyLayout() and live
  //   outside the Layer tree; re-layout the owning Layer or mark its Text node dirty instead.
  // - Orphan nodes no longer attached to any Layer are dropped.
  {
    std::vector<Node*> layerDirty = {};
    for (auto* node : ownedDirty) {
      if (node == nullptr) {
        continue;
      }
      auto type = node->nodeType();
      if (type == NodeType::Layer || type == NodeType::Document || type == NodeType::Animation ||
          type == NodeType::AnimationObject || type == NodeType::Channel) {
        layerDirty.push_back(node);
      } else {
        auto* owningLayer = findLayerForContentNode(nodes, node);
        if (owningLayer != nullptr) {
          layerDirty.push_back(const_cast<Layer*>(owningLayer));
        }
      }
    }
    std::sort(layerDirty.begin(), layerDirty.end());
    layerDirty.erase(std::unique(layerDirty.begin(), layerDirty.end()), layerDirty.end());
    ownedDirty = std::move(layerDirty);
    if (ownedDirty.empty()) {
      return;
    }
  }
  PruneExpiredScenes(&liveScenes);
  // Layout-affecting edits (size, constraints, padding, fonts, text, geometry) and structural child
  // list changes require a full re-layout, since layout is resolved top-down and a single node
  // cannot be re-measured in isolation. applyLayout() discards the cached layout outputs first when
  // the document is already laid out (see its reset branch). Pure render edits skip this entirely.
  if (layoutChanged) {
    applyLayout();
  }
  for (auto& weakScene : liveScenes) {
    auto scene = weakScene.lock();
    if (scene != nullptr) {
      scene->onNodesChanged(ownedDirty);
    }
  }
}

bool PAGXDocument::ownsNode(const Node* node) const {
  if (node == this) {
    return true;
  }
  for (auto& ownedNode : nodes) {
    if (ownedNode.get() == node) {
      return true;
    }
  }
  return false;
}

void PAGXDocument::registerLiveScene(const std::shared_ptr<PAGScene>& scene) {
  if (scene == nullptr) {
    return;
  }
  // Avoid duplicate entries: a scene re-registers with its external documents on every rebuild.
  PruneExpiredScenes(&liveScenes);
  for (auto& weakScene : liveScenes) {
    if (weakScene.lock() == scene) {
      return;
    }
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

}  // namespace pagx
