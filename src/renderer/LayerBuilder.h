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

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "pagx/PAGImage.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Channel.h"
#include "tgfx/layers/Layer.h"

namespace tgfx {
class Gradient;
}  // namespace tgfx

namespace pagx {

class ColorSource;
class ImagePattern;

// Maps an Image node's external file path to a host-supplied decoded image that overrides the
// document's own decoding. Owned by the runtime PAGScene and passed into the builder so a build can
// resolve those paths to the host's images instead of decoding from embedded data or file paths.
using ImageOverrideMap = std::unordered_map<std::string, std::shared_ptr<tgfx::Image>>;

/**
 * Runtime color stop binding keeps the parent gradient and stop index for a ColorStop node.
 */
struct RuntimeColorStop {
  std::shared_ptr<tgfx::Gradient> gradient = nullptr;
  size_t index = 0;
};

/**
 * Runtime binding maps PAGX document nodes to the tgfx runtime objects created for one concrete
 * layer tree instance. Animation channel writers use it to update the right runtime object without
 * storing runtime state on document nodes.
 */
using RuntimeWriter = void (*)(void* object, const KeyValue& value, float mix);

// Reads the current value of a channel back from a runtime object into *out. Returns false when the
// channel currently has no value to sync back (e.g. an unset optional property), in which case the
// channel is skipped by DataBind syncBack. Symmetric to RuntimeWriter; used to flow target changes
// back into ViewModel properties.
using RuntimeReader = bool (*)(const void* object, KeyValue* out);

struct RuntimeTarget {
  virtual ~RuntimeTarget() = default;

  void setObject(std::shared_ptr<void> object) {
    this->object = std::move(object);
  }

  template <typename T>
  std::shared_ptr<T> getObject() const {
    return std::static_pointer_cast<T>(object);
  }

  // Raw bound object pointer for reverse lookup, without changing ownership.
  const void* rawObject() const {
    return object.get();
  }

  void setWriter(const std::string& channel, RuntimeWriter writer) {
    if (!channel.empty() && writer != nullptr) {
      writers[channel] = writer;
    }
  }

  void setReader(const std::string& channel, RuntimeReader reader) {
    if (!channel.empty() && reader != nullptr) {
      readers[channel] = reader;
    }
  }

  // Returns true if this target can apply the given channel. Virtual so a subclass that intercepts
  // channels in apply() (LayerRuntimeTarget's x / y / matrix) reports them as handled even though
  // they are not in the writers map.
  virtual bool hasWriter(const std::string& channel) const {
    return writers.find(channel) != writers.end();
  }

  // Applies an evaluated channel value. Virtual so a subclass (LayerRuntimeTarget) can intercept
  // channels that need shared state across writers (the Layer transform: x / y / matrix).
  virtual bool apply(const std::string& channel, const KeyValue& value, float mix) {
    auto it = writers.find(channel);
    if (it == writers.end() || object == nullptr) {
      return false;
    }
    it->second(object.get(), value, mix);
    return true;
  }

  // Reads the current value of a channel back from the bound object. Virtual so a subclass
  // (LayerRuntimeTarget) can intercept channels that hold shared transform state (x / y). Returns
  // false if no reader is registered for the channel, the object is null, or the reader reports the
  // channel currently has no value to sync back.
  virtual bool read(const std::string& channel, KeyValue* out) const {
    auto it = readers.find(channel);
    if (it == readers.end() || object == nullptr || out == nullptr) {
      return false;
    }
    return it->second(object.get(), out);
  }

 protected:
  std::shared_ptr<void> object = nullptr;
  std::unordered_map<std::string, RuntimeWriter> writers = {};
  std::unordered_map<std::string, RuntimeReader> readers = {};
};

struct RuntimeBinding {
  template <typename T>
  void set(const Node* node, std::shared_ptr<T> object) {
    if (node == nullptr || object == nullptr) {
      return;
    }
    ensureTarget(node)->setObject(std::move(object));
  }

  // Registers a writer and its symmetric reader for a channel in one call, declaring the channel as
  // two-way capable (ViewModel -> target via the writer, target -> ViewModel via the reader).
  void setAccessor(const Node* node, const std::string& channel, RuntimeWriter writer,
                   RuntimeReader reader) {
    if (node == nullptr) {
      return;
    }
    auto* target = ensureTarget(node);
    target->setWriter(channel, std::move(writer));
    target->setReader(channel, std::move(reader));
  }

  template <typename T>
  std::shared_ptr<T> get(const Node* node) const {
    auto it = targets.find(node);
    if (it == targets.end()) {
      return nullptr;
    }
    return it->second->getObject<T>();
  }

  // Drops the mapping for the given node, including its tgfx object and channel writers. Used when
  // a node is removed from the document so the binding does not keep a stale entry alive.
  void remove(const Node* node) {
    targets.erase(node);
  }

  // Returns the node whose bound tgfx object is the given pointer, or nullptr if none. Linear scan;
  // used by in-place refresh to map a tgfx child layer back to its source node when reconciling
  // child lists.
  const Node* findNode(const void* object) const {
    for (const auto& entry : targets) {
      if (entry.second->rawObject() == object) {
        return entry.first;
      }
    }
    return nullptr;
  }

  // Returns true if the node currently has a binding entry.
  bool contains(const Node* node) const {
    return targets.find(node) != targets.end();
  }

  // Tracks a Fill/Stroke in the reverse index for its color source. Called by LayerBuilder after
  // an element (Fill or Stroke) is bound to a tgfx object during tree construction.
  void trackColorSource(const Node* colorSource, const Node* element) {
    if (colorSource == nullptr || element == nullptr) {
      return;
    }
    colorSourceUsers[colorSource].push_back(element);
  }

  // Tracks an ImagePattern in the reverse index for its image. Called by LayerBuilder after an
  // ImagePattern is bound to a tgfx object during tree construction.
  void trackImage(const Node* image, const Node* pattern) {
    if (image == nullptr || pattern == nullptr) {
      return;
    }
    imageUsers[image].push_back(pattern);
  }

  // Untrack an element from its color source's reverse index. Called when a Fill/Stroke is
  // about to be removed from the binding.
  void untrackColorSource(const Node* colorSource, const Node* element) {
    if (colorSource == nullptr || element == nullptr) {
      return;
    }
    auto it = colorSourceUsers.find(colorSource);
    if (it != colorSourceUsers.end()) {
      auto& vec = it->second;
      vec.erase(std::remove(vec.begin(), vec.end(), element), vec.end());
      if (vec.empty()) {
        colorSourceUsers.erase(it);
      }
    }
  }

  // Untrack an ImagePattern from its image's reverse index. Called when an ImagePattern is
  // about to be removed from the binding.
  void untrackImage(const Node* image, const Node* pattern) {
    if (image == nullptr || pattern == nullptr) {
      return;
    }
    auto it = imageUsers.find(image);
    if (it != imageUsers.end()) {
      auto& vec = it->second;
      vec.erase(std::remove(vec.begin(), vec.end(), pattern), vec.end());
      if (vec.empty()) {
        imageUsers.erase(it);
      }
    }
  }

  // Returns true if any element other than excludedOwner references the given color source. O(1)
  // via the reverse index maintained during build.
  bool isColorSourceShared(const Node* colorSource, const Node* excludedOwner) const {
    auto it = colorSourceUsers.find(colorSource);
    if (it == colorSourceUsers.end()) {
      return false;
    }
    for (const auto* painter : it->second) {
      if (painter != excludedOwner && targets.find(painter) != targets.end()) {
        return true;
      }
    }
    return false;
  }

  // Returns true if any ImagePattern other than excludedPattern references the given image. O(1)
  // via the reverse index maintained during build.
  bool isImageShared(const Node* image, const Node* excludedPattern) const {
    auto it = imageUsers.find(image);
    if (it == imageUsers.end()) {
      return false;
    }
    for (const auto* pattern : it->second) {
      if (pattern != excludedPattern && targets.find(pattern) != targets.end()) {
        return true;
      }
    }
    return false;
  }

  bool apply(const Node* node, const std::string& channel, const KeyValue& value, float mix) const {
    auto it = targets.find(node);
    if (it == targets.end()) {
      return false;
    }
    return it->second->apply(channel, value, mix);
  }

  // Reads the current value of a node's channel back into out. Returns false if the node has no
  // target or no reader for the channel. Used by DataBind syncBack.
  bool read(const Node* node, const std::string& channel, KeyValue* out) const {
    auto it = targets.find(node);
    if (it == targets.end()) {
      return false;
    }
    return it->second->read(channel, out);
  }

  // Returns true if the node has a target that can apply the given channel. Used by tests to verify
  // every Animatable channel in the reflection registry has a matching runtime writer.
  bool hasWriter(const Node* node, const std::string& channel) const {
    auto it = targets.find(node);
    if (it == targets.end()) {
      return false;
    }
    return it->second->hasWriter(channel);
  }

  // Installs a specific RuntimeTarget subclass for a node (e.g. LayerRuntimeTarget). Replaces any
  // existing target for the node. Returns the installed target for further setup.
  RuntimeTarget* setTarget(const Node* node, std::unique_ptr<RuntimeTarget> target) {
    if (node == nullptr || target == nullptr) {
      return nullptr;
    }
    auto* raw = target.get();
    targets[node] = std::move(target);
    return raw;
  }

  // Returns the node's installed RuntimeTarget, or nullptr if none. Used to re-seed a layer's
  // transform baseline on an in-place refresh.
  RuntimeTarget* getTarget(const Node* node) const {
    auto it = targets.find(node);
    return it != targets.end() ? it->second.get() : nullptr;
  }

 private:
  // Returns the existing target for the node, creating a plain RuntimeTarget if none exists yet.
  RuntimeTarget* ensureTarget(const Node* node) {
    auto it = targets.find(node);
    if (it != targets.end()) {
      return it->second.get();
    }
    auto target = std::unique_ptr<RuntimeTarget>(new RuntimeTarget());
    auto* raw = target.get();
    targets[node] = std::move(target);
    return raw;
  }

  std::unordered_map<const Node*, std::unique_ptr<RuntimeTarget>> targets = {};

  // Reverse index: for each ColorSource bound to any Fill/Stroke, the set of Elements
  // (Fill/Stroke) that reference it. Maintained incrementally by set/remove so
  // unbindColorSourceIfUnreferenced can check for surviving references in O(1) instead of
  // scanning all bound nodes.
  std::unordered_map<const Node*, std::vector<const Node*>> colorSourceUsers = {};

  // Reverse index: for each Image bound to any ImagePattern, the set of ImagePattern nodes that
  // reference it. Maintained incrementally so unbindImageIfUnreferenced can check for surviving
  // references in O(1).
  std::unordered_map<const Node*, std::vector<const Node*>> imageUsers = {};
};

/**
 * Result of building PAGX nodes into a tgfx layer tree.
 */
struct LayerBuildResult {
  std::shared_ptr<tgfx::Layer> root = nullptr;
  RuntimeBinding binding = {};
  // 1:1 pagx Layer -> tgfx Layer map, exposing only the first copy when a Composition is
  // referenced by multiple Layers. Session users that need every copy use getTgfxLayers().
  std::unordered_map<const Layer*, std::shared_ptr<tgfx::Layer>> layerMap = {};

  /**
   * Returns the tgfx layer corresponding to the specified PAGX Layer node, or nullptr if the node
   * was not rendered into this build result.
   */
  std::shared_ptr<tgfx::Layer> getLayer(const Layer* layer) const {
    return binding.get<tgfx::Layer>(layer);
  }
};

/**
 * LayerBuilder converts PAGXDocument to tgfx::Layer tree for rendering.
 * The document must have applyLayout() called before building.
 *
 * Thread safety: not safe to call Build / BuildWithMap concurrently on the same PAGXDocument.
 * The renderer lazily caches a built tgfx::Typeface on each Font node referenced by the document
 * on the first call, and the cache is updated without synchronization. Callers that share a
 * PAGXDocument across threads must serialize Build / BuildWithMap calls themselves (e.g. by
 * mutex or by rendering on a single dedicated thread).
 */
class LayerBuilder {
 public:
  /**
   * Returns the tgfx::Image wrapped by a PAGImage. For internal use by channel writers that need
   * to apply the image to a runtime object.
   */
  static std::shared_ptr<tgfx::Image> GetTGFXImage(const std::shared_ptr<PAGImage>& image);

  /**
   * Wraps a tgfx::Image into a new PAGImage with an empty source string. Used by DataBind syncBack
   * to read an image-valued channel back into the ViewModel. The returned PAGImage carries only the
   * decoded bitmap (no originating path or data URI); syncBack compares the underlying tgfx::Image
   * for change detection, so the empty source does not cause spurious writebacks.
   */
  static std::shared_ptr<PAGImage> WrapTGFXImage(const std::shared_ptr<tgfx::Image>& image);

  /**
   * Builds a layer tree from a PAGXDocument.
   * @param document The document to build from. Must have had applyLayout() called.
   * @return The root layer of the built layer tree, or nullptr if document is null or layout was
   *         not applied.
   */
  static std::shared_ptr<tgfx::Layer> Build(PAGXDocument* document);

  /**
   * Builds a layer tree and returns a mapping from PAGX Layer nodes to tgfx::Layer objects. This
   * mapping allows callers to look up the rendered layer for any PAGX Layer node.
   * @param document The document to build from. Must have had applyLayout() called.
   * @return A LayerBuildResult containing the root layer and runtime binding maps. Returns empty
   *         result if document is null or
   *         layout was not applied.
   */
  static LayerBuildResult BuildWithMap(PAGXDocument* document);

  /**
   * Builds a top-level layer tree for runtime use, stopping at Composition slot boundaries —
   * Layers whose `composition` field is non-null produce empty container tgfx layers without
   * recursing into Composition.layers. The runtime PAGFile uses this entry point and then
   * populates each slot with an independent PAGComposition build via BuildCompositionSubtree(),
   * keeping per-slot layerMaps isolated.
   * @param document The document to build from. Must have had applyLayout() called.
   */
  static LayerBuildResult BuildForRuntime(PAGXDocument* document,
                                          const ImageOverrideMap* imageOverrides = nullptr);

  /**
   * Builds a Composition's child layer subtree into a fresh LayerBuildResult. Used by the runtime
   * PAGComposition slot to obtain its own per-slot binding, masks, and tgfx layer instances.
   * @param composition The Composition to build. Must reference layers from a document that has
   *                    had applyLayout() called.
   */
  static LayerBuildResult BuildCompositionSubtree(const Composition* composition,
                                                  const ImageOverrideMap* imageOverrides = nullptr);

  /**
   * Re-applies the current state of a single Layer node onto its existing tgfx::Layer in place,
   * reusing the supplied binding. The tgfx::Layer object identity is preserved so handles that
   * hold it stay valid; only its vector contents, transform/render attributes, styles and filters
   * are regenerated from the node's current fields. The document must have had applyLayout() called
   * (re-run it first when layout-affecting fields changed). Used by PAGScene to reflect post-build
   * edits without rebuilding the layer tree.
   * @param node The Layer node to refresh.
   * @param binding The runtime binding that maps the node to its tgfx::Layer.
   * @param document The owning document, used to decode image resources.
   * @param imageOverrides Optional host-supplied decoded images keyed by Image filePath.
   * @return true if the node had a tgfx::Layer in the binding and was refreshed, false otherwise.
   */
  static bool RefreshLayerInPlace(const Layer* node, RuntimeBinding* binding,
                                  const PAGXDocument* document,
                                  const ImageOverrideMap* imageOverrides = nullptr);

  /**
   * Builds a single Layer node (and its vector contents and recursive sub-layers) into the supplied
   * existing binding, returning the new tgfx::Layer. Layers referencing a composition produce an
   * empty container layer (the runtime PAGComposition slot is populated separately), matching the
   * runtime build path. Used to add a newly inserted child layer to a live scene without rebuilding
   * the whole tree.
   * @param node The Layer node to build.
   * @param binding The runtime binding to populate with the node's mapping.
   * @param document The owning document, used to decode image resources.
   * @param imageOverrides Optional host-supplied decoded images keyed by Image filePath.
   * @return The new tgfx::Layer for the node, or nullptr if node or binding is null.
   */
  static std::shared_ptr<tgfx::Layer> BuildLayerInto(
      const Layer* node, RuntimeBinding* binding, const PAGXDocument* document,
      const ImageOverrideMap* imageOverrides = nullptr);
};

/**
 * LayerBuilderSession wraps LayerBuilder with stateful behavior: it retains the build context
 * after build() returns so callers can later re-generate a layer's contents when an underlying
 * Image resource changes (e.g. progressive upgrade from a low-resolution thumbnail to a full
 * version). The class is intended for the progressive image loading flow on WeChat; other
 * platforms should keep using the static LayerBuilder::Build / BuildWithMap entry points.
 *
   * Lifecycle: create a session, call build() once per document (matching parsePAGX+buildLayers
   * cycle), and then call rebuildForFilePath() whenever the decoded image for a given filePath
   * changes (new image attached or evicted). Destroying the session releases all
 * cached layer/image state.
 *
 * IMPORTANT: The caller must guarantee that the PAGXDocument passed to build() remains valid
 * for the entire lifetime of this session. The session stores a non-owning pointer to the
 * document and dereferences it on every rebuildForFilePath() call. Destroy the session before
 * (or together with) the document.
 */
class LayerBuilderSession {
 public:
  LayerBuilderSession();
  ~LayerBuilderSession();

  LayerBuilderSession(const LayerBuilderSession&) = delete;
  LayerBuilderSession& operator=(const LayerBuilderSession&) = delete;

  /**
   * Builds the layer tree for the given document. Behaves identically to
   * LayerBuilder::BuildWithMap() but keeps the internal context alive for later rebuilds.
   * @return The LayerBuildResult (root layer + pagx Layer to tgfx Layer mapping). The mapping
   *         reflects the first tgfx layer produced for each pagx Layer; callers that need all
   *         copies (for example to refresh every Composition instance that shares a Layer)
   *         should use getTgfxLayers() instead.
   */
  LayerBuildResult build(PAGXDocument* document);

  /**
   * Rebuilds the tgfx vector contents of every layer whose fill/stroke references an
   * ImagePattern backed by an Image node whose filePath matches the given value. Call this
   * after the decoded image for the path changes so the renderer re-resolves it and picks up
   * the new tgfx::Image. A single filePath may match multiple Image
   * nodes and each Image node may be referenced by multiple Layers, which in turn may have
   * been duplicated by Composition instancing; all such copies are refreshed in one call.
   * @return The number of tgfx layers whose contents were regenerated. Zero means no layer
   *         currently references the given filePath (or the document has not been built yet).
   */
  size_t rebuildForFilePath(const std::string& filePath);

  /**
   * Drops the entire internal image cache. Call this after the document undergoes structural
   * node changes (e.g. after notifyChange with node additions or removals) because the
   * cache keys are raw Image* pointers that become dangling when nodes are replaced.
   */
  void invalidateAllImages();

  /**
   * Returns every tgfx::Layer that was produced for layers referencing the given image file
   * path. Combines findLayersByImageFilePath() and getTgfxLayers() into a single call so
   * callers do not need to handle internal pagx::Layer pointers.
   */
  std::vector<std::shared_ptr<tgfx::Layer>> getTgfxLayersByImageFilePath(
      const std::string& filePath) const;

  /**
   * Returns every tgfx::Layer that was produced for the given pagx Layer during build(). A
   * pagx Layer may map to several tgfx layers when its owning Composition is instanced more
   * than once; the returned vector preserves build order.
   * @return An empty vector when the pagx Layer was not seen during build() (e.g. nullptr,
   *         a mask-only layer that got skipped, or no build() call has been made yet).
   */
  std::vector<std::shared_ptr<tgfx::Layer>> getTgfxLayers(const Layer* pagxLayer) const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl;
};

}  // namespace pagx
