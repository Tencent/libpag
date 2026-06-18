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
#include "pagx/nodes/Animation.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Node.h"
#include "pagx/types/Data.h"

namespace pagx {

class LayoutContext;
class PAGScene;

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
   * Top-level animations.
   */
  std::vector<Animation*> animations = {};

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
   * that have no embedded data. Data URIs (paths starting with "data:") are excluded.
   */
  std::vector<std::string> getExternalFilePaths() const;

  /**
   * Loads external file data matching the given file path. Image data is embedded into matching
   * Image nodes, while PAGX data is parsed and attached to matching external composition layers.
   * Must be called before creating any PAGScene from this document: PAGScene::Make() builds its
   * runtime tree once, so external compositions resolved after a scene exists will not appear in
   * that scene.
   * @param filePath the external file path to match against Image nodes or composition layers
   * @param data the file content to embed or parse
   * @return true if a matching node was found and its data was loaded successfully
   */
  bool loadFileData(const std::string& filePath, std::shared_ptr<Data> data);

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
   * Reflects post-build edits to the given nodes in every scene created from this document. Layer
   * handles are preserved wherever possible. Pass a container node to reflect changes to its child
   * list; an animation, animation object, or channel to apply new timeline data.
   *
   * Content nodes (Image, SolidColor, Gradient, ColorStop, Fill, Stroke, Group, TextBox, filters,
   * styles, etc.) are also accepted — the scene refreshes the Layer that contains the content
   * node. Filters and styles are matched by top-level pointer only (their color fields are value
   * types, not node references). You may still pass the Layer directly; both are equivalent.
   *
   * GlyphRun, Font, and Glyph nodes are NOT supported. They are generated by applyLayout() at
   * runtime. To update text content or font properties, mark the Text node dirty and re-apply
   * layout (layoutChanged = true) instead.
   *
   * When an edit changes a node's "@id" reference (e.g. AnimationObject.target, Fill.color), mark
   * every node on the affected reference chain dirty, not just the mutated one — notifyChange only
   * refreshes the nodes it is given.
   *
   * Editing an external composition: call notifyChange on the document that owns the edited nodes.
   * Scenes that embed this document as an external composition are refreshed automatically. A node
   * may only be notified through its owning document; foreign nodes are skipped. Use ownsNode() if
   * the caller does not statically know which document owns a given node.
   *
   * @param dirtyNodes the nodes whose fields (or child lists) were mutated. Must reference nodes
   * still owned by this document. Null entries and foreign nodes are skipped. Content nodes are
   * accepted alongside Layer and timeline nodes. An empty list is a no-op.
   * @param layoutChanged whether any mutated field affects layout (size, constraints, padding,
   * fonts, text, geometry) or a child list changed. Pass true to re-run layout before refreshing;
   * pass false for a cheaper render-only refresh, only safe for edits that do not affect layout
   * (e.g. alpha, color). Must pass true when adding or removing geometry content (Rectangle,
   * Ellipse, Path, etc.) from a Layer's contents list, even if individual elements keep their
   * sizes — new elements have no resolved layout bounds until a layout pass runs. Callers that
   * mutate via SetNodeChannel can derive the right value from RequiresLayout(NodeType, channel);
   * structural add/remove must pass true.
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
  // Recursive layout worker. visited holds the documents on the current ancestor path so an
  // externalDoc cycle built directly through the API (bypassing loadFileData's own chain guard)
  // is detected and stops the recursion instead of overflowing the stack.
  void applyLayout(const FontConfig* config, std::unordered_set<const PAGXDocument*>& visited);
  static void layoutLayers(const std::vector<Layer*>& layers, float containerW, float containerH,
                           LayoutContext* context);

  void registerNode(Node* node, const std::string& id);

  // PAGScene lifecycle hooks (called from PAGScene::Make / ~PAGScene).
  void registerLiveScene(const std::shared_ptr<PAGScene>& scene);
  void unregisterLiveScene(PAGScene* scene);

  FontConfig fontConfig;
  bool layoutApplied = false;
  std::unordered_map<std::string, Node*> nodeMap = {};

  // Live PAGScene instances created from this document. Stored as weak_ptr so that the document
  // does not keep PAGScene alive; expired entries are pruned during notifyChange.
  std::vector<std::weak_ptr<PAGScene>> liveScenes = {};

  friend class PAGXImporter;
  friend class PAGXExporter;
  friend class TextLayoutContext;
  friend class PAGScene;
  friend class PAGComposition;
};

}  // namespace pagx
