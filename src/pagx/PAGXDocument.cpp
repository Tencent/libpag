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
#include "pagx/PAGXNodeChannel.h"
#if defined(PAG_BUILD_SVG)
#include "pagx/SVGImporter.h"
#endif
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Element.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/Gradient.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/LayoutNode.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/utils/ImportFormat.h"
#include "pagx/utils/ImportResources.h"
#include "renderer/FontEmbedder.h"
#include "renderer/LayerBuilder.h"

namespace pagx {

static bool IsExternalFilePath(const std::string& filePath) {
  return !filePath.empty() && filePath.find("data:") != 0;
}

// A local file path is an external file path that can be opened via local I/O (ifstream). URL
// schemes (http/https/file/etc.) contain "://" and are not directly readable; the host is
// responsible for downloading them and supplying the data via loadFileData.
static bool IsLocalFilePath(const std::string& filePath) {
  return IsExternalFilePath(filePath) && filePath.find("://") == std::string::npos;
}

static bool IsExpiredScene(const std::weak_ptr<PAGScene>& scene) {
  return scene.expired();
}

static void PruneExpiredScenes(std::vector<std::weak_ptr<PAGScene>>* scenes) {
  scenes->erase(std::remove_if(scenes->begin(), scenes->end(), IsExpiredScene), scenes->end());
}

void PAGXDocument::appendExternalFilePaths(std::vector<std::string>* paths,
                                           std::unordered_set<const PAGXDocument*>& visited,
                                           std::unordered_set<std::string>& chain) const {
  if (paths == nullptr || !visited.insert(this).second) {
    return;
  }
  for (auto& node : nodes) {
    if (node->nodeType() == NodeType::Image) {
      auto* image = static_cast<Image*>(node.get());
      if (image->data != nullptr) {
        continue;
      }
      if (!IsExternalFilePath(image->filePath)) {
        continue;
      }
      paths->push_back(image->filePath);
    } else if (node->nodeType() == NodeType::Layer) {
      auto* layer = static_cast<Layer*>(node.get());
      auto mounted = mountedCompositions.find(layer->composition);
      bool supersededInline =
          mounted != mountedCompositions.end() && mounted->second == MountType::InlineImport &&
          (!layer->compositionFilePath.empty() || !layer->importDirective.source.empty());
      if ((layer->composition == nullptr || supersededInline) &&
          IsExternalFilePath(layer->compositionFilePath)) {
        if (chain.find(layer->compositionFilePath) == chain.end()) {
          paths->push_back(layer->compositionFilePath);
        }
      } else if (!supersededInline && layer->externalDoc != nullptr) {
        bool inserted =
            !layer->compositionFilePath.empty() && chain.insert(layer->compositionFilePath).second;
        layer->externalDoc->appendExternalFilePaths(paths, visited, chain);
        if (inserted) {
          chain.erase(layer->compositionFilePath);
        }
      }
    }
  }
  visited.erase(this);
}

static void AppendExternalImagePaths(const PAGXDocument* document, std::vector<std::string>* paths,
                                     std::unordered_set<const PAGXDocument*>& visited) {
  if (document == nullptr || paths == nullptr || !visited.insert(document).second) {
    return;
  }
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Image) {
      auto* image = static_cast<Image*>(node.get());
      if (image->data != nullptr) {
        continue;
      }
      if (!IsLocalFilePath(image->filePath)) {
        continue;
      }
      paths->push_back(image->filePath);
    } else if (node->nodeType() == NodeType::Layer) {
      auto* layer = static_cast<Layer*>(node.get());
      if (layer->externalDoc != nullptr) {
        AppendExternalImagePaths(layer->externalDoc.get(), paths, visited);
      }
    }
  }
}

static bool DetachSupersededInlineImports(PAGXDocument* document,
                                          std::unordered_set<const PAGXDocument*>& visited) {
  if (document == nullptr || !visited.insert(document).second) {
    return false;
  }
  std::vector<Layer*> layers = {};
  for (const auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Layer) {
      layers.push_back(static_cast<Layer*>(node.get()));
    }
  }
  bool detached = false;
  for (auto* layer : layers) {
    if (!layer->compositionFilePath.empty() || !layer->importDirective.source.empty()) {
      detached = document->detachInlineImport(layer) || detached;
    }
    detached = DetachSupersededInlineImports(layer->externalDoc.get(), visited) || detached;
  }
  return detached;
}

static void ReportExternalCompositionCycle(PAGXDocument* root, const std::string& filePath) {
  auto error = "Cyclic external composition reference detected: '" + filePath + "'.";
  if (std::find(root->errors.begin(), root->errors.end(), error) == root->errors.end()) {
    root->errors.push_back(error);
  }
}

static bool LoadExternalComposition(PAGXDocument* root, PAGXDocument* document, Layer* layer,
                                    const std::string& filePath, std::shared_ptr<Data> data,
                                    const std::unordered_set<std::string>& chain) {
  if (document == nullptr || layer == nullptr || data == nullptr ||
      layer->compositionFilePath != filePath || layer->composition != nullptr) {
    return false;
  }
  // Reject ancestor back-references without erasing the declared origin. Enumeration excludes
  // these back-edges, while the declaration still overrides inline content and survives export.
  if (chain.find(filePath) != chain.end()) {
    ReportExternalCompositionCycle(root, filePath);
    return false;
  }
  auto externalDoc = PAGXImporter::FromXML(data->bytes(), data->size());
  if (externalDoc == nullptr) {
    return false;
  }
  for (const auto& error : externalDoc->errors) {
    document->errors.push_back("[" + filePath + "] " + error);
  }
  for (const auto& node : externalDoc->nodes) {
    if (node->nodeType() == NodeType::Layer) {
      const auto* child = static_cast<const Layer*>(node.get());
      const auto& source = child->compositionFilePath;
      if (!source.empty() && (source == filePath || chain.find(source) != chain.end())) {
        ReportExternalCompositionCycle(root, source);
      }
    }
  }
  auto* wrapper = document->makeNode<Composition>();
  wrapper->width = externalDoc->width;
  wrapper->height = externalDoc->height;
  wrapper->layers = externalDoc->layers;
  wrapper->animations = externalDoc->animations;
  layer->composition = wrapper;
  layer->externalDoc = externalDoc;
  // Retain the origin path for load-chain cycle detection and round-trip serialization. Mounted
  // wrappers are runtime adapters, so export writes this path regardless of the wrapper's id.
  return true;
}

static bool LoadFileDataInChain(
    PAGXDocument* root, PAGXDocument* document, const std::string& filePath,
    std::shared_ptr<Data> data, std::unordered_set<std::string>& chain,
    std::unordered_map<PAGXDocument*, std::vector<Node*>>& docDirtyNodes,
    std::unordered_set<PAGXDocument*>& layoutDirtyDocs) {
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
      layoutDirtyDocs.insert(document);
      found = true;
    }
    if (!loadedComposition && layer->externalDoc != nullptr) {
      // Descend into an already-resolved externalDoc, recording its origin path so a reference back
      // to any ancestor origin (a cycle) is detected. Only erase the entry this frame inserted:
      // sibling externalDocs that legitimately share the same downstream file would otherwise drop
      // an ancestor's marker. insert().second is true exactly when this frame added the path.
      bool inserted = chain.insert(layer->compositionFilePath).second;
      found = LoadFileDataInChain(root, layer->externalDoc.get(), filePath, data, chain,
                                  docDirtyNodes, layoutDirtyDocs) ||
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
  applyLayout(config, visited, nullptr, true);
}

void PAGXDocument::applyLayout(const FontConfig* config,
                               std::unordered_set<const PAGXDocument*>& visited,
                               std::vector<Layer*>* changedOut, bool mountImports) {
  if (DocumentHasConflictingImportSources(this)) {
    errors.push_back("Layer cannot declare both 'composition' and 'import' sources.");
    layoutApplied = false;
    return;
  }
  if (mountImports) {
    std::unordered_set<const PAGXDocument*> sourceVisited = {};
    DetachSupersededInlineImports(this, sourceVisited);
  }
  invalidateImageFilePathIndex();
  if (!visited.insert(this).second) {
    errors.push_back("Cyclic external composition reference detected during layout.");
    return;
  }
  if (config != nullptr) {
    _fontConfig = *config;
  }
  // Mount inline import content as sub-documents before anything reads the tree: the wrapper
  // compositions join the node list here, so the bounds snapshot below and every layout pass
  // observe the mounted structure. Idempotent — already-mounted layers are skipped.
  if (mountImports) {
    attachInlineImportDocuments();
  }
  // Re-running layout on an already-laid-out document (e.g. from notifyChange after an edit) must
  // discard the cached layout outputs first; updateSize() skips re-measuring a node whose preferred
  // size is already set, so without this a size/constraint edit would keep the stale geometry.
  // When changedOut is requested, snapshot each Layer's layoutBounds here (before resetLayout()
  // clears them to NAN) so the post-layout pass can detect which Layers auto layout repositioned.
  std::unordered_map<Layer*, Rect> beforeBounds = {};
  if (layoutApplied && changedOut != nullptr) {
    for (const auto& node : nodes) {
      if (node->nodeType() == NodeType::Layer) {
        auto* layer = static_cast<Layer*>(node.get());
        beforeBounds.emplace(layer, layer->layoutBounds());
      }
    }
  }
  resetLayoutState();
  LayoutContext context(&_fontConfig);
  // Composition layers are laid out first since they may be referenced by document layers.
  for (auto& node : nodes) {
    if (node->nodeType() == NodeType::Composition && !isMountedComposition(node.get())) {
      auto* comp = static_cast<Composition*>(node.get());
      layoutLayers(comp->layers, comp->width, comp->height, &context);
    }
  }
  layoutLayers(layers, width, height, &context);
  // Bookkeeping for tests only (see LayoutContext::layoutMemoHits): the memo has to be observable,
  // otherwise a regression that skips fewer subtrees still passes every geometry assertion.
  lastLayoutMemoHits = context.layoutMemoHits;
  for (auto& node : nodes) {
    if (node->nodeType() == NodeType::Layer) {
      auto* layer = static_cast<Layer*>(node.get());
      if (layer->externalDoc != nullptr) {
        // Recurse so the externalDoc lays out its own layers (and any deeper externalDocs); it sets
        // its own layoutApplied flag internally. The host's fontConfig is passed down intentionally
        // so external compositions are measured and rendered with the host's fonts, keeping text
        // consistent across the embedded boundary rather than using the external doc's own config.
        // changedOut is intentionally not forwarded: an external document has its own notifyChange
        // chain, and runtime layers inside it are rebuilt by that document's scenes.
        layer->externalDoc->applyLayout(&_fontConfig, visited, nullptr, mountImports);
      }
    }
  }
  // Mark layout complete only after all external subtrees have finished. Setting it earlier would
  // leave the document flagged as laid out even when a downstream cycle aborts the recursion,
  // letting PAGScene::Make build from an inconsistent tree.
  layoutApplied = true;
  if (changedOut != nullptr) {
    // Layout is deterministic, so a Layer that auto layout did not move produces an identical
    // layoutBounds; only those whose bounds actually changed are appended, so notifyChange can
    // refresh the repositioned siblings the caller did not list.
    for (auto& [layer, oldBounds] : beforeBounds) {
      if (layer->layoutBounds() != oldBounds) {
        changedOut->push_back(layer);
      }
    }
  }
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

void PAGXDocument::adoptNodes(PAGXDocument& other) {
  if (&other == this) {
    // Self-adoption would append nodes to the very vector being iterated.
    return;
  }
  // A live scene built from `other` would keep rebuilding its runtime tree from a model this call
  // is about to drain, and would render nothing without reporting anything. Rejecting is the only
  // safe answer; the caller releases the scene and retries.
  if (other.hasLiveScenes()) {
    errors.push_back("Cannot adopt nodes from a document that still has live scenes.");
    return;
  }
  // Rename before taking the nodes over: importer auto-generated ids (image1, path1, ...) collide
  // as a matter of course when several inline SVGs merge into one document, and a caller that
  // forgot this step would merge duplicate ids (illegal PAGX on export) with only an error to show
  // for it. Doing it here makes the correct sequence the only one available.
  other.resetLayoutState();
  invalidateImageFilePathIndex();
  renameCollidingIds(&other);
  nodes.reserve(nodes.size() + other.nodes.size());
  for (auto& node : other.nodes) {
    nodeSet.insert(node.get());
    // Adopted content is part of this document after the transfer, so its ids join the id index
    // exactly like import-time registration: findNode(), data-bind "@id" targets, and deferred
    // mask resolution must see these nodes. A collision is reported and the HOST mapping is kept
    // — registerNode would overwrite it with the adopted node, hijacking every host "@id"
    // reference to it, which is worse than not registering the foreign id at all.
    if (!node->id.empty() && nodeMap.find(node->id) != nodeMap.end()) {
      errors.push_back("Duplicate node id '" + node->id + "'.");
    } else {
      registerNode(node.get(), node->id);
    }
    nodes.push_back(std::move(node));
  }
  mountedCompositions.insert(other.mountedCompositions.begin(), other.mountedCompositions.end());
  inlineImportMountFailures.insert(other.inlineImportMountFailures.begin(),
                                   other.inlineImportMountFailures.end());
  other.mountedCompositions.clear();
  other.inlineImportMountFailures.clear();
  // The nodes now belong to this document. Clearing the source's ownership set and id index
  // alongside its list keeps it from answering ownsNode()/findNode() for memory it no longer owns.
  other.nodes.clear();
  other.nodeSet.clear();
  other.nodeMap.clear();
  // Its layer and animation lists reference the same transferred nodes, so they go too: a drained
  // document that still lists them hands callers a way to reach memory it no longer owns.
  other.layers.clear();
  other.animations.clear();
  other.viewModel = nullptr;
  other.dataBinds.clear();
  other._fontConfig = FontConfig();
  other.liveScenes.clear();
}

void PAGXDocument::renameCollidingIds(PAGXDocument* other) {
  if (other == nullptr || other == this) {
    return;
  }
  std::unordered_set<std::string> usedIds = {};
  for (auto& node : nodes) {
    if (!node->id.empty()) {
      usedIds.insert(node->id);
    }
  }
  for (auto& node : other->nodes) {
    if (node->id.empty()) {
      continue;
    }
    if (usedIds.insert(node->id).second) {
      continue;
    }
    std::string base = node->id;
    std::string candidate = {};
    int suffix = 2;
    do {
      candidate = base + "_" + std::to_string(suffix++);
    } while (usedIds.count(candidate) > 0);
    node->id = candidate;
    usedIds.insert(candidate);
  }
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
  invalidateImageFilePathIndex();
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
    } else {
      auto* node = nodes[readIdx].get();
      nodeSet.erase(node);
      mountedCompositions.erase(node);
      if (node->nodeType() == NodeType::Layer) {
        inlineImportMountFailures.erase(static_cast<const Layer*>(node));
      }
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

void PAGXDocument::invalidateImageFilePathIndex() {
  layersByImageFilePath.clear();
  layersByImageFilePathBuilt = false;
}

void PAGXDocument::resetLayoutState() {
  for (const auto& node : nodes) {
    if (node->nodeType() == NodeType::Layer) {
      static_cast<Layer*>(node.get())->resetLayout();
    } else {
      switch (node->nodeType()) {
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
  layoutApplied = false;
  lastLayoutMemoHits = 0;
  invalidateImageFilePathIndex();
}

// Mounting inline imports needs a parser for the inline format. The SVG importer is an optional
// module (PAG_BUILD_SVG), so everything that parses content is compiled only when it is linked;
// without it a document keeps its inline directives unmounted, which hasUnresolvedImports() reports
// and 'pagx resolve' can still flatten.
#if defined(PAG_BUILD_SVG)

// True when the layer's inline import content is SVG. The format is resolved by the same function
// 'pagx resolve' uses, so a directive can never end up un-renderable here while resolvable there.
static bool IsInlineSvgImport(const Layer* layer) {
  if (layer->importDirective.content.empty()) {
    return false;
  }
  return ResolveImportFormat(layer->importDirective.format, {}, layer->importDirective.content) ==
         "svg";
}

// Collects layers whose inline import content is not mounted yet. Recurses into child layers;
// the SVG content itself never carries import directives, so mounted subtrees are not scanned.
//
// A layer that also declares an explicit source (import="a.svg") or composition file is skipped:
// that declaration is the author's stated origin, and mounting would swallow it silently — the
// file would never load, getExternalFilePaths() would stop returning the path, and
// hasUnresolvedImports() would call the layer resolved. Explicit declarations therefore win, the
// same precedence 'pagx resolve' applies (source ? ImportFile : ImportString).
static void CollectLayersWithInlineImports(const std::vector<Layer*>& layers,
                                           std::vector<Layer*>* out) {
  for (auto* layer : layers) {
    if (layer == nullptr) {
      continue;
    }
    if (layer->composition == nullptr && layer->externalDoc == nullptr &&
        layer->compositionFilePath.empty() && layer->importDirective.source.empty() &&
        IsInlineSvgImport(layer)) {
      out->push_back(layer);
    }
    CollectLayersWithInlineImports(layer->children, out);
  }
}

// Mounts the layer's inline import content as a sub-document, mirroring LoadExternalComposition:
// the SVG parses into its own document (own id space and node list), a synthetic wrapper
// Composition registers in the host so layout, binding and export walks find it, and the layer
// renders through the composition reference. The wrapper is not authored content — the caller
// records it in mountedCompositions so the exporter writes back the import directive instead of
// expanding the sub-document into the host's Resources. The import directive is kept — it is the
// serialization anchor the exporter writes back, and 'pagx resolve' still flattens it on demand.
// When the layer authors both dimensions, they are passed as the importer's target size so the
// content is uniformly scaled to fit — the same semantics 'pagx resolve' applies — and the
// sub-document's size equals the target, keeping the wrapper-size == sub-document-size invariant
// of external compositions. Content that parses but has no drawable layers (e.g. a defs-only SVG
// kept as a shared-resource carrier) mounts an empty wrapper, which renders as nothing — exactly
// what that content should produce. Returns the wrapper, or nullptr when the content cannot be
// parsed at all (leaving the directive unresolved).
static Composition* AttachInlineImport(PAGXDocument* host, Layer* layer,
                                       const std::string& directory) {
  SVGImporter::Options options = {};
  if (!std::isnan(layer->width) && !std::isnan(layer->height)) {
    options.targetWidth = layer->width;
    options.targetHeight = layer->height;
  }
  auto svgDoc = SVGImporter::ParseString(layer->importDirective.content, options);
  if (svgDoc == nullptr) {
    return nullptr;
  }
  ResolveImportResources(svgDoc.get(), directory);
  auto* wrapper = host->makeNode<Composition>();
  wrapper->width = svgDoc->width;
  wrapper->height = svgDoc->height;
  wrapper->layers = svgDoc->layers;
  wrapper->animations = svgDoc->animations;
  layer->composition = wrapper;
  layer->externalDoc = svgDoc;
  return wrapper;
}

#endif  // PAG_BUILD_SVG

bool PAGXDocument::hasUnresolvedImports() const {
  return DocumentHasUnresolvedImports(this);
}

void PAGXDocument::attachInlineImportDocuments() {
#if defined(PAG_BUILD_SVG)
  // Snapshot the layers to mount before mounting any of them: AttachInlineImport calls
  // makeNode<Composition>(), which push_back()s into nodes and would invalidate an active
  // iterator over the composition list.
  std::vector<Layer*> pending = {};
  CollectLayersWithInlineImports(layers, &pending);
  for (auto& node : nodes) {
    if (node->nodeType() == NodeType::Composition && !isMountedComposition(node.get())) {
      CollectLayersWithInlineImports(static_cast<Composition*>(node.get())->layers, &pending);
    }
  }
  for (auto* layer : pending) {
    // A layer whose content failed to parse once is not retried on every layout pass —
    // applyLayout runs on each notifyChange, and each retry would append the same error again.
    if (inlineImportMountFailures.find(layer) != inlineImportMountFailures.end()) {
      continue;
    }
    auto* wrapper = AttachInlineImport(this, layer, sourceDirectory);
    if (wrapper == nullptr) {
      inlineImportMountFailures.insert(layer);
      errors.push_back("Failed to parse inline import content" +
                       (layer->id.empty() ? std::string(".") : " on layer '" + layer->id + "'."));
      continue;
    }
    layer->externalDoc->sourceDirectory = sourceDirectory;
    mountedCompositions.emplace(wrapper, MountType::InlineImport);
  }
#endif
}

bool PAGXDocument::detachInlineImport(Layer* layer) {
  if (layer == nullptr || !ownsNode(layer)) {
    return false;
  }
  if (DocumentHasConflictingImportSources(this)) {
    errors.push_back("Layer cannot declare both 'composition' and 'import' sources.");
    return false;
  }
  // Clear any failure record up front, independently of whether a wrapper is really detached: a
  // failed attach records the layer while composition is still null (nothing was mounted), so
  // gating this erase behind the mount checks below would make it unreachable. That would break
  // the retry promise in the header - a host fixing importDirective.content and re-parsing would
  // still be skipped, leaving hasUnresolvedImports() permanently true.
  inlineImportMountFailures.erase(layer);
  auto mounted = mountedCompositions.find(layer->composition);
  if (mounted == mountedCompositions.end() || mounted->second != MountType::InlineImport ||
      layer->externalDoc == nullptr) {
    return false;
  }
  auto* wrapper = layer->composition;
  invalidateImageFilePathIndex();
  // Take the sub-document off the layer, but keep it alive in a local handle until nothing else
  // references it. A live scene's runtime tree holds nodes owned by the sub-document (PAGLayer::node
  // and PAGComposition::document), so dropping the last reference here would leave them dangling.
  auto subDocument = layer->externalDoc;
  layer->externalDoc = nullptr;
  layer->composition = nullptr;
  // Rebuild every live scene's runtime tree while the wrapper and the sub-document are both still
  // alive, so the tree being torn down can still read them. The rebuilt tree is generated from this
  // document's node list and no longer references either, which is what makes the two releases
  // below safe. Hosts therefore never have to tear a scene down before detaching.
  rebuildLiveScenes();
  // The wrapper's layers/animations point at nodes owned by the sub-document, so clear them before
  // the wrapper outlives its contents; both are still alive here, so this only drops the references
  // rather than rescuing them.
  wrapper->layers.clear();
  wrapper->animations.clear();
  removeNodes({wrapper});
  // `subDocument` releases the sub-document on return; the rebuild above already dropped every
  // runtime reference to it.
  return true;
}

std::vector<std::string> PAGXDocument::getExternalFilePaths() const {
  std::vector<std::string> paths = {};
  std::unordered_set<const PAGXDocument*> visited = {};
  std::unordered_set<std::string> chain = {};
  appendExternalFilePaths(&paths, visited, chain);
  return paths;
}

std::vector<std::string> PAGXDocument::getExternalImagePaths() const {
  std::vector<std::string> paths = {};
  std::unordered_set<const PAGXDocument*> visited = {};
  AppendExternalImagePaths(this, &paths, visited);
  return paths;
}

bool PAGXDocument::loadFileData(const std::string& filePath, std::shared_ptr<Data> data) {
  if (filePath.empty() || data == nullptr) {
    return false;
  }
  if (DocumentHasConflictingImportSources(this)) {
    errors.push_back("Layer cannot declare both 'composition' and 'import' sources.");
    return false;
  }
  std::unordered_set<const PAGXDocument*> sourceVisited = {};
  DetachSupersededInlineImports(this, sourceVisited);
  std::unordered_set<std::string> chain = {};
  std::unordered_map<PAGXDocument*, std::vector<Node*>> docDirtyNodes = {};
  std::unordered_set<PAGXDocument*> layoutDirtyDocs = {};
  bool found =
      LoadFileDataInChain(this, this, filePath, data, chain, docDirtyNodes, layoutDirtyDocs);
  if (found) {
    for (auto& entry : docDirtyNodes) {
      for (auto* node : entry.second) {
        if (node->nodeType() == NodeType::Layer) {
          auto* layer = static_cast<Layer*>(node);
          if (layer->composition != nullptr && layer->externalDoc != nullptr &&
              !entry.first->isMountedComposition(layer->composition)) {
            auto source =
                ResolveImportResourcePath(entry.first->sourceDirectory, layer->compositionFilePath);
            auto directory = GetImportSourceDirectory(source);
            layer->externalDoc->sourceDirectory = directory;
            ResolveImportResources(layer->externalDoc.get(), directory);
            entry.first->mountedCompositions.emplace(layer->composition, MountType::ExternalFile);
          }
        }
      }
    }
    for (auto& entry : docDirtyNodes) {
      bool structural = layoutDirtyDocs.find(entry.first) != layoutDirtyDocs.end();
      if (structural) {
        entry.second.push_back(entry.first);
      }
      entry.first->notifyChange(entry.second, structural);
    }
  }
  return found;
}

// Sets runtimeImage on every Image node matching filePath in this document and its resolved
// external documents, collecting the touched Image nodes per owning document for notifyChange.
void PAGXDocument::LoadImageInChain(
    PAGXDocument* document, const std::string& filePath, const std::shared_ptr<PAGImage>& image,
    std::unordered_map<PAGXDocument*, std::vector<Node*>>& docDirtyImages,
    std::unordered_set<const PAGXDocument*>& visited) {
  if (document == nullptr || !visited.insert(document).second) {
    return;
  }
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Image) {
      auto* imageNode = static_cast<Image*>(node.get());
      if (imageNode->filePath == filePath) {
        imageNode->runtimeImage = image;  // filePath is kept as the serialization anchor.
        docDirtyImages[document].push_back(imageNode);
      }
    } else if (node->nodeType() == NodeType::Layer) {
      auto* layer = static_cast<Layer*>(node.get());
      if (layer->externalDoc != nullptr) {
        LoadImageInChain(layer->externalDoc.get(), filePath, image, docDirtyImages, visited);
      }
    }
  }
}

bool PAGXDocument::loadFileData(const std::string& filePath, std::shared_ptr<PAGImage> image) {
  if (filePath.empty()) {
    return false;
  }
  std::unordered_map<PAGXDocument*, std::vector<Node*>> docDirtyImages = {};
  std::unordered_set<const PAGXDocument*> visited = {};
  LoadImageInChain(this, filePath, image, docDirtyImages, visited);
  if (docDirtyImages.empty()) {
    return false;
  }
  // A decoded image swap does not change geometry, so layout is not re-run (layoutChanged = false).
  for (auto& entry : docDirtyImages) {
    entry.first->notifyChange(entry.second, false);
  }
  return true;
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
  // Embedded Font nodes without a file are removed above. Drop cached typefaces from the surviving
  // file-backed Font nodes so their memory is reclaimed before the next embed/render cycle.
  for (auto& node : nodes) {
    if (node->nodeType() == NodeType::Font) {
      static_cast<Font*>(node.get())->resetRenderCache();
    }
  }
}

static void AppendRequiredFonts(const PAGXDocument* document, std::vector<PAGFont>* outFonts,
                                std::unordered_set<const PAGXDocument*>& visited) {
  if (document == nullptr || outFonts == nullptr || !visited.insert(document).second) {
    return;
  }
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Text) {
      auto* text = static_cast<Text*>(node.get());
      if (!text->fontFamily.empty()) {
        outFonts->emplace_back(text->fontFamily, text->fontStyle);
      }
    } else if (node->nodeType() == NodeType::Layer) {
      auto* layer = static_cast<Layer*>(node.get());
      if (layer->externalDoc != nullptr) {
        AppendRequiredFonts(layer->externalDoc.get(), outFonts, visited);
      }
    }
  }
}

std::vector<PAGFont> PAGXDocument::getRequiredFonts() const {
  std::vector<PAGFont> fonts = {};
  std::unordered_set<const PAGXDocument*> visited = {};
  AppendRequiredFonts(this, &fonts, visited);
  std::sort(fonts.begin(), fonts.end());
  fonts.erase(std::unique(fonts.begin(), fonts.end()), fonts.end());
  return fonts;
}

namespace {

// Checks whether a content node references (directly or indirectly through its fields) the given
// target node. Used by findLayerForContentNode to determine whether a content node belongs to a
// particular Layer.
bool contentReferencesNode(const Node* parent, const Node* target) {
  if (parent == nullptr || target == nullptr) {
    return false;
  }
  switch (parent->nodeType()) {
    case NodeType::Fill: {
      auto* fill = static_cast<const Fill*>(parent);
      return fill->color == target ||
             (fill->color != nullptr && contentReferencesNode(fill->color, target));
    }
    case NodeType::Stroke: {
      auto* stroke = static_cast<const Stroke*>(parent);
      return stroke->color == target ||
             (stroke->color != nullptr && contentReferencesNode(stroke->color, target));
    }
    case NodeType::ImagePattern: {
      auto* pattern = static_cast<const ImagePattern*>(parent);
      return pattern->image == target;
    }
    // Check Gradient.colorStops (LinearGradient, RadialGradient, ConicGradient, DiamondGradient).
    case NodeType::LinearGradient:
    case NodeType::RadialGradient:
    case NodeType::ConicGradient:
    case NodeType::DiamondGradient: {
      auto* gradient = static_cast<const Gradient*>(parent);
      for (auto* stop : gradient->colorStops) {
        if (stop == target) {
          return true;
        }
      }
      return false;
    }
    case NodeType::Group: {
      auto* group = static_cast<const Group*>(parent);
      for (auto* elem : group->elements) {
        if (elem == target || contentReferencesNode(elem, target)) {
          return true;
        }
      }
      return false;
    }
    case NodeType::TextBox: {
      auto* textBox = static_cast<const TextBox*>(parent);
      for (auto* elem : textBox->elements) {
        if (elem == target || contentReferencesNode(elem, target)) {
          return true;
        }
      }
      return false;
    }
    // Path and TextPath both reference a PathData resource node whose shape data may be mutated.
    case NodeType::Path: {
      auto* path = static_cast<const Path*>(parent);
      return path->data == target;
    }
    case NodeType::TextPath: {
      auto* textPath = static_cast<const TextPath*>(parent);
      return textPath->path == target;
    }
    default:
      return false;
  }
}

// Searches all Layer nodes in the document for any whose contents, filters, or styles reference
// the given content node. A node may be shared across multiple Layers (e.g. a SolidColor referenced
// by several Fills), so ALL matching Layers are returned. Returns an empty vector if not found.
std::vector<const Layer*> findLayerForContentNode(
    const std::vector<std::unique_ptr<Node>>& allNodes, const Node* target) {
  std::vector<const Layer*> result = {};
  for (auto& node : allNodes) {
    if (node->nodeType() != NodeType::Layer) {
      continue;
    }
    auto* layer = static_cast<const Layer*>(node.get());
    for (auto* elem : layer->contents) {
      if (elem == target || contentReferencesNode(elem, target)) {
        result.push_back(layer);
        break;
      }
    }
    for (auto* filter : layer->filters) {
      if (filter == target) {
        result.push_back(layer);
        break;
      }
    }
    for (auto* style : layer->styles) {
      if (style == target) {
        result.push_back(layer);
        break;
      }
    }
  }
  return result;
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
  std::unordered_set<const PAGXDocument*> sourceVisited = {};
  if (DetachSupersededInlineImports(this, sourceVisited)) {
    ownedDirty.push_back(this);
    layoutChanged = true;
  }
  invalidateImageFilePathIndex();
  // Resolve content nodes (Image, SolidColor, Gradient, Fill, Stroke, Group, Filter, Style, etc.)
  // to their owning Layers. refreshNodes only processes Layer nodes directly; non-Layer content
  // nodes are resolved here so callers can pass them to notifyChange without having to find the
  // owning Layer first. Timeline nodes (Animation, AnimationObject, Channel) and Layer nodes are
  // passed through as-is — they are handled by separate paths in onNodesChanged.
  //
  // Nodes that are NOT resolved (findLayerForContentNode returns empty) are logged as an error
  // and dropped:
  // - GlyphRun, Font, and Glyph nodes are layout-time data generated by applyLayout() and live
  //   outside the Layer tree; re-layout the owning Layer or mark its Text node dirty instead.
  // - Orphan nodes no longer attached to any Layer are dropped.
  // Collect dirty <Image> resource nodes before the Layer collapse below. An Image referenced by a
  // ViewModel image default (and not by any Layer's ImagePattern) is not reachable from the Layer
  // tree, so it would otherwise be dropped; ViewModel image values are refreshed off these nodes
  // directly via PAGScene::onImageResourcesChanged.
  std::vector<Image*> changedImages = {};
  for (auto* node : ownedDirty) {
    if (node != nullptr && node->nodeType() == NodeType::Image) {
      changedImages.push_back(static_cast<Image*>(node));
    }
  }
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
        auto owningLayers = findLayerForContentNode(nodes, node);
        if (owningLayers.empty()) {
          // An Image used only by a ViewModel default has no owning Layer; it is handled by the
          // image-resource refresh below, so skip it silently rather than logging an error.
          if (type == NodeType::Image) {
            continue;
          }
          LOGE(
              "PAGXDocument::notifyChange: content node (type=%d) not reachable from any Layer "
              "and dropped. GlyphRun, Font, and Glyph are layout-time data and must be resolved "
              "by re-laying out the owning Text node instead.",
              static_cast<int>(node->nodeType()));
          continue;
        }
        for (auto* owningLayer : owningLayers) {
          if (owningLayer != nullptr) {
            layerDirty.push_back(const_cast<Layer*>(owningLayer));
          }
        }
      }
    }
    std::sort(layerDirty.begin(), layerDirty.end());
    layerDirty.erase(std::unique(layerDirty.begin(), layerDirty.end()), layerDirty.end());
    ownedDirty = std::move(layerDirty);
    if (ownedDirty.empty() && changedImages.empty()) {
      return;
    }
  }
  PruneExpiredScenes(&liveScenes);
  // Layout-affecting edits (size, constraints, padding, fonts, text, geometry) and structural child
  // list changes require a full re-layout, since layout is resolved top-down and a single node
  // cannot be re-measured in isolation. applyLayout() discards the cached layout outputs first when
  // the document is already laid out (see its reset branch). Pure render edits skip this entirely.
  // Auto layout can reposition siblings the caller did not list (e.g. a flex container pushing
  // other children when one child is resized); applyLayout collects every Layer whose layoutBounds
  // changed, and those are merged into ownedDirty below so refreshNodes re-syncs their runtime
  // transform too.
  if (layoutChanged) {
    std::vector<Layer*> layoutRepositioned = {};
    std::unordered_set<const PAGXDocument*> visited = {};
    applyLayout(nullptr, visited, &layoutRepositioned, true);
    if (!layoutRepositioned.empty()) {
      ownedDirty.reserve(ownedDirty.size() + layoutRepositioned.size());
      for (auto* layer : layoutRepositioned) {
        ownedDirty.push_back(layer);
      }
      std::sort(ownedDirty.begin(), ownedDirty.end());
      ownedDirty.erase(std::unique(ownedDirty.begin(), ownedDirty.end()), ownedDirty.end());
    }
  }
  auto scenes = liveScenes;
  for (auto& weakScene : scenes) {
    auto scene = weakScene.lock();
    if (scene != nullptr) {
      if (!ownedDirty.empty()) {
        scene->onNodesChanged(ownedDirty);
      }
      if (!changedImages.empty()) {
        scene->onImageResourcesChanged(changedImages);
      }
    }
  }
}

bool PAGXDocument::ownsNode(const Node* node) const {
  if (node == this) {
    return true;
  }
  return nodeSet.find(node) != nodeSet.end();
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

bool PAGXDocument::hasLiveScenes() const {
  for (auto& weakScene : liveScenes) {
    if (!weakScene.expired()) {
      return true;
    }
  }
  return false;
}

bool PAGXDocument::isMountedComposition(const Node* node) const {
  return node != nullptr && ownsNode(node) &&
         mountedCompositions.find(node) != mountedCompositions.end();
}

void PAGXDocument::rebuildLiveScenes() {
  PruneExpiredScenes(&liveScenes);
  auto scenes = liveScenes;
  for (auto& weakScene : scenes) {
    auto scene = weakScene.lock();
    if (scene != nullptr) {
      // A full rebuild rather than an incremental refresh: the caller is about to destroy nodes the
      // current tree references, and those nodes are gone from the document by then, so patching the
      // tree in place would have to resolve references that no longer exist.
      scene->buildRuntimeTree();
    }
  }
}

// Embeds file data into every Image node whose filePath matches a key in fileDataMap, descending
// into resolved external composition documents so nested images are embedded in the same pass.
// Mirrors the recursion of AppendExternalImagePaths (read side) and LoadImageInChain (single-image
// side); without the descent, getExternalImagePaths() collects child-document paths that
// loadFileDataMap() would silently fail to match.
void PAGXDocument::loadFileDataMapInChain(
    const std::unordered_map<std::string, std::shared_ptr<Data>>& fileDataMap,
    std::unordered_set<const PAGXDocument*>& visited,
    std::unordered_map<PAGXDocument*, std::vector<Node*>>& dirtyImages) {
  if (!visited.insert(this).second) {
    return;
  }
  invalidateImageFilePathIndex();
  for (auto& node : nodes) {
    if (node->nodeType() == NodeType::Image) {
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
      dirtyImages[this].push_back(image);
    } else if (node->nodeType() == NodeType::Layer) {
      auto* layer = static_cast<Layer*>(node.get());
      if (layer->externalDoc != nullptr) {
        layer->externalDoc->loadFileDataMapInChain(fileDataMap, visited, dirtyImages);
      }
    }
  }
}

void PAGXDocument::loadFileDataMap(
    const std::unordered_map<std::string, std::shared_ptr<Data>>& fileDataMap) {
  std::unordered_set<const PAGXDocument*> visited = {};
  std::unordered_map<PAGXDocument*, std::vector<Node*>> dirtyImages = {};
  loadFileDataMapInChain(fileDataMap, visited, dirtyImages);
  for (auto& entry : dirtyImages) {
    entry.first->notifyChange(entry.second, false);
  }
}

// Records the Image node filePath referenced by `color` (when it is an ImagePattern) into
// `index`, scoped to the owning `layer`.
static void RecordImagePattern(
    const ColorSource* color, const Layer* layer,
    std::unordered_map<std::string, std::unordered_set<const Layer*>>* index) {
  if (!color || color->nodeType() != NodeType::ImagePattern) {
    return;
  }
  const auto* pattern = static_cast<const ImagePattern*>(color);
  if (!pattern->image || pattern->image->filePath.empty()) {
    return;
  }
  (*index)[pattern->image->filePath].insert(layer);
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
  switch (element->nodeType()) {
    case NodeType::Fill:
      RecordImagePattern(static_cast<const Fill*>(element)->color, layer, index);
      break;
    case NodeType::Stroke:
      RecordImagePattern(static_cast<const Stroke*>(element)->color, layer, index);
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

const std::vector<const Layer*>& PAGXDocument::findLayersByImageFilePath(
    const std::string& imageFilePath) {
  static const std::vector<const Layer*> empty;
  if (imageFilePath.empty()) {
    return empty;
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
    return empty;
  }
  return it->second;
}

}  // namespace pagx
