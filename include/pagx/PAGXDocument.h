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

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "pagx/FontConfig.h"
#include "pagx/PAGFont.h"
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Node.h"
#include "pagx/types/Data.h"

namespace pagx {

class DataBind;
class PAGImage;
class LayoutContext;
class PAGScene;
class ViewModel;
class Image;
class ImagePattern;

/**
 * PAGXDocument is the root container for a PAGX document.
 * It contains resources and layers. This is a pure data structure class.
 * Use PAGXImporter to load documents and PAGXExporter to save documents.
 */
class PAGXDocument : public Node {
 public:
  /**
   * Creates an empty document with the specified size.
   * @param width the canvas width in pixels
   * @param height the canvas height in pixels
   */
  static std::shared_ptr<PAGXDocument> Make(float width, float height);

  /**
   * Canvas width.
   */
  float width = 0;

  /**
   * Canvas height.
   */
  float height = 0;

  /**
   * Top-level layers (raw pointers, owned by nodes).
   */
  std::vector<Layer*> layers = {};

  /**
   * Top-level animation and state-machine definitions from the document's &lt;Animations&gt; block,
   * in declaration order. Each entry is either an Animation or a StateMachine; check nodeType() to
   * dispatch.
   */
  std::vector<Node*> animations = {};

  /**
   * The ViewModel schema bound to this document (raw pointer, owned by nodes).
   */
  ViewModel* viewModel = nullptr;
  /**
   * DataBind nodes that bind ViewModel properties to this document's layers (raw pointers, owned
   * by nodes).
   */
  std::vector<DataBind*> dataBinds = {};

  /**
   * Creates a node of the specified type and adds it to the document management.
   * If an ID is provided, the node will be indexed for lookup.
   * If the ID already exists, an error will be logged and the new node will replace the old one in
   * the index.
   * @param id an optional unique identifier for the node, used for lookup via findNode(). If empty,
   * the node is not indexed.
   */
  template <typename T>
  T* makeNode(const std::string& id = "") {
    auto node = std::unique_ptr<T>(new T());
    auto* result = node.get();
    registerNode(result, id);
    nodeSet.insert(result);
    nodes.push_back(std::move(node));
    return result;
  }

  /**
   * Finds a node by ID within this document's own node index.
   * Returns nullptr if not found, or if the node belongs to an external composition document.
   * @param id The unique identifier of the node.
   * @return A pointer to the node, or nullptr if not found in this document.
   */
  Node* findNode(const std::string& id) const;

  /**
   * Finds a node of the specified type by ID within this document's own node index.
   * The caller must ensure T matches the actual node type, otherwise behavior is undefined.
   * Returns nullptr if not found, or if the node belongs to an external composition document.
   * @param id The unique identifier of the node.
   * @return A pointer to the node cast to type T, or nullptr if not found in this document.
   */
  template <typename T>
  T* findNode(const std::string& id) const {
    return static_cast<T*>(findNode(id));
  }

  /**
   * All nodes in the document (owned by the document).
   */
  std::vector<std::unique_ptr<Node>> nodes = {};

  /**
   * Errors collected during parsing. Non-empty errors indicate structural issues in the source
   * document but do not prevent the document from being returned. The parsed content may be
   * incomplete where errors occurred.
   */
  std::vector<std::string> errors = {};

  /**
   * Returns true if any layer in the document has unresolved import content (inline `<svg>` or
   * `import` attribute). These must be resolved via `pagx resolve` before layout or rendering.
   */
  bool hasUnresolvedImports() const;

  /**
   * Returns a list of external file paths referenced by Image nodes or external composition layers
   * that have no embedded data. Data URIs (paths starting with "data:") are excluded. The caller is
   * responsible for skipping paths it has already supplied (e.g. via PAGXDocument::loadFileData).
   */
  std::vector<std::string> getExternalFilePaths() const;

  /**
   * Loads external file data matching the given file path. Image data is embedded into matching
   * Image nodes, while PAGX data is parsed and attached to matching external composition layers.
   * For performance, load all external file data before creating any PAGScene from this document.
   * If called after a PAGScene exists, changes are reflected in existing scenes via notifyChange.
   * When structural nodes (e.g. composition layers) are loaded, applyLayout() is triggered as part
   * of the propagation, so the document is laid out even if no scene exists yet.
   * @param filePath the external file path to match against Image nodes or composition layers
   * @param data the file content to embed or parse
   * @return true if a matching node was found and its data was loaded successfully
   */
  bool loadFileData(const std::string& filePath, std::shared_ptr<Data> data);

  /**
   * Supplies a host-decoded image for the given external file path. Image nodes whose filePath
   * matches (in this document and its resolved external documents) render with this image instead
   * of decoding from their own data or file path; passing an empty shared_ptr clears a previous
   * one. The image is runtime-only state and is not serialized. Use this for resources the host
   * loads itself (e.g. asynchronously, or from a GPU texture via PAGImage::MakeFromTexture).
   * Existing scenes refresh the affected layers in place.
   *
   * Note: this affects ImagePattern rendering only. ViewModel image-valued properties resolve
   * images through their own decode chain (data → dataURI → filePath) and do not consult the
   * runtime image set here.
   * @param filePath the external file path as declared in the document's Image nodes
   * @param image the decoded image to use, or an empty shared_ptr to clear it
   * @return true if a matching Image node was found
   */
  bool loadFileData(const std::string& filePath, std::shared_ptr<PAGImage> image);

  /**
   * Executes auto layout on the document, positioning layers according to their layout
   * constraints. Must be called before rendering or font embedding. Re-running layout on an
   * already-laid-out document is supported (notifyChange relies on this to reflect edits): the
   * reset branch discards the cached layout outputs first so nodes are re-measured from their
   * current fields.
   * @param fontConfig Optional font config for text measurement and rendering. When provided,
   *                   updates the internal config before layout. Pass nullptr to use the
   *                   previously set config (or no config).
   */
  void applyLayout(const FontConfig* fontConfig = nullptr);

  /**
   * Returns true if applyLayout() has been called at least once.
   */
  bool isLayoutApplied() const {
    return layoutApplied;
  }

  /**
   * Embeds font data into the document by collecting layout glyph runs from all Text nodes.
   * The document must have had applyLayout() called first so that Text nodes contain valid
   * layout run data.
   * @return true if embedding succeeded, false if layout has not been applied.
   */
  bool embed();

  /**
   * Clears all embedded GlyphRuns from Text nodes in the document. Typically called before a
   * subsequent applyLayout() when re-embedding a file that already has embedded fonts, so that
   * layout performs runtime shaping instead of reusing stale embedded data.
   */
  void clearEmbed();

  /**
   * Returns the set of unique (fontFamily, fontStyle) pairs required by Text nodes in this
   * document and all loaded external composition documents (externalDoc). Call after all
   * external file data has been loaded via loadFileData() to get complete results. The caller
   * can use this list to register fonts with FontConfig before applyLayout() or embed().
   * Empty fontFamily entries are skipped.
   * @return deduplicated list of PAGFont. Results are sorted.
   */
  std::vector<PAGFont> getRequiredFonts() const;

  /**
   * Reflects post-build edits to the given nodes in every scene created from this document, reusing
   * Layer handles where possible. Accepts Layer, timeline (Animation / AnimationObject / Channel),
   * and content nodes; a content node refreshes its owning Layer. GlyphRun, Font, and Glyph are not
   * supported — edit the Text node with layoutChanged = true instead.
   *
   * When layoutChanged is true, only list nodes directly edited or whose child list changed: the
   * whole chain for an "@id" reference change, or the container Layer for a structural child-list
   * edit. Any other Layer that auto layout repositions as a side effect is refreshed automatically,
   * so callers do not need to list such siblings. For external compositions, notify the document
   * that owns the nodes; foreign nodes are skipped (see ownsNode()).
   *
   * @param dirtyNodes nodes whose fields or child lists changed. Must be owned by this document;
   * null and foreign entries are skipped; an empty list is a no-op.
   * @param layoutChanged true if any edit affects layout (size, fonts, text, geometry) or changes a
   * child list, which re-runs layout before refreshing; false skips layout for a cheaper
   * render-only refresh (safe only for edits that cannot move geometry, e.g. alpha, color). Adding
   * or removing content always requires true. SetNodeChannel callers can derive it from
   * RequiresLayout(NodeType, channel).
   *
   * Prefer passing the owning Layer when known: resolving a content node to its Layer scans all
   * nodes (O(N)).
   */
  void notifyChange(const std::vector<Node*>& dirtyNodes, bool layoutChanged);

  /**
   * Returns true if the node belongs to this document. A node belongs to exactly one document;
   * passing a node owned by a different document to notifyChange has no effect on this document's
   * scenes. Use this to dispatch a multi-document edit to the right owners, or to validate a node's
   * origin before notifying. Also returns true when the node is this document itself (used to
   * support notifyChange({doc}) for document-level changes such as width/height).
   * @param node the node to check; null returns false.
   */
  bool ownsNode(const Node* node) const;

  NodeType nodeType() const override {
    return NodeType::Document;
  }

 private:
  PAGXDocument() = default;

  const std::vector<const Layer*>& findLayersByImageFilePath(const std::string& imageFilePath);

  // Sets runtimeImage on every Image node matching filePath in this document and its resolved
  // external documents, collecting the touched Image nodes per owning document.
  static void LoadImageInChain(PAGXDocument* document, const std::string& filePath,
                               const std::shared_ptr<PAGImage>& image,
                               std::unordered_map<PAGXDocument*, std::vector<Node*>>& docDirtyImages,
                               std::unordered_set<const PAGXDocument*>& visited);

  // Recursive layout worker. visited holds the documents on the current ancestor path so an
  // externalDoc cycle built directly through the API (bypassing loadFileData's own chain guard)
  // is detected and stops the recursion instead of overflowing the stack. When changedOut is not
  // null and the document was already laid out, every Layer whose layoutBounds differ from before
  // the re-layout is appended to it, so notifyChange can auto-refresh siblings that auto layout
  // repositioned without the caller having to list them.
  void applyLayout(const FontConfig* config, std::unordered_set<const PAGXDocument*>& visited,
                   std::vector<Layer*>* changedOut);
  static void layoutLayers(const std::vector<Layer*>& layers, float containerW, float containerH,
                           LayoutContext* context);

  void registerNode(Node* node, const std::string& id);

  // PAGScene lifecycle hooks (called from PAGScene::Make / ~PAGScene).
  void registerLiveScene(const std::shared_ptr<PAGScene>& scene);
  void unregisterLiveScene(PAGScene* scene);

  FontConfig fontConfig;
  bool layoutApplied = false;
  std::unordered_map<std::string, Node*> nodeMap = {};
  // O(1) containment check for ownsNode(), maintained alongside nodes.
  std::unordered_set<const Node*> nodeSet = {};

  // Live PAGScene instances created from this document. Stored as weak_ptr so that the document
  // does not keep PAGScene alive; expired entries are pruned during notifyChange.
  std::vector<std::weak_ptr<PAGScene>> liveScenes = {};

  // Lazily built index of Image node filePath -> pagx Layer list, used by
  // findLayersByImageFilePath(). Built on first query; invalidated by notifyChange() since edits
  // may alter the tree topology or Image node filePath values. Also freed when the document is
  // destroyed (on the next parsePAGX() call).
  std::unordered_map<std::string, std::vector<const Layer*>> layersByImageFilePath = {};
  bool layersByImageFilePathBuilt = false;

  friend class PAGXImporter;
  friend class PAGXExporter;
  friend class TextLayoutContext;
  friend class PAGScene;
  friend class PAGComposition;
  friend class LayerBuilderContext;
  friend class LayerBuilderSession;
};

}  // namespace pagx
