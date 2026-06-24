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

#include "LayerBuilder.h"
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include "ToTGFX.h"
#include "base/utils/Log.h"
#include "pagx/PAGXDocument.h"
#include "pagx/TextLayout.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlendFilter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorMatrixFilter.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/Gradient.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/InnerShadowFilter.h"
#include "pagx/nodes/InnerShadowStyle.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/MergePath.h"
#include "pagx/nodes/Node.h"
#include "pagx/nodes/NoiseFilter.h"
#include "pagx/nodes/NoiseStyle.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/RangeSelector.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/RoundCorner.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/nodes/TrimPath.h"
#include "pagx/runtime/MixUtils.h"
#include "pagx/types/ColorSpace.h"
#include "pagx/types/Data.h"
#include "pagx/types/FillRule.h"
#include "pagx/types/LayerPlacement.h"
#include "pagx/types/MergePathMode.h"
#include "pagx/types/RepeaterOrder.h"
#include "pagx/types/SelectorTypes.h"
#include "pagx/types/TileMode.h"
#include "pagx/utils/Base64.h"
#include "renderer/GlyphRunRenderer.h"
#include "tgfx/core/ColorSpace.h"
#include "tgfx/core/CustomTypeface.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Font.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/TextBlob.h"
#include "tgfx/core/TextBlobBuilder.h"
#include "tgfx/layers/Layer.h"
#include "tgfx/layers/LayerMaskType.h"
#include "tgfx/layers/LayerPaint.h"
#include "tgfx/layers/LayerType.h"
#include "tgfx/layers/StrokeAlign.h"
#include "tgfx/layers/VectorLayer.h"
#include "tgfx/layers/filters/BlendFilter.h"
#include "tgfx/layers/filters/BlurFilter.h"
#include "tgfx/layers/filters/ColorMatrixFilter.h"
#include "tgfx/layers/filters/DropShadowFilter.h"
#include "tgfx/layers/filters/InnerShadowFilter.h"
#include "tgfx/layers/filters/LayerFilter.h"
#include "tgfx/layers/filters/NoiseFilter.h"
#include "tgfx/layers/layerstyles/BackgroundBlurStyle.h"
#include "tgfx/layers/layerstyles/DropShadowStyle.h"
#include "tgfx/layers/layerstyles/InnerShadowStyle.h"
#include "tgfx/layers/layerstyles/NoiseStyle.h"
#include "tgfx/layers/vectors/Ellipse.h"
#include "tgfx/layers/vectors/FillStyle.h"
#include "tgfx/layers/vectors/Gradient.h"
#include "tgfx/layers/vectors/ImagePattern.h"
#include "tgfx/layers/vectors/MergePath.h"
#include "tgfx/layers/vectors/Polystar.h"
#include "tgfx/layers/vectors/Rectangle.h"
#include "tgfx/layers/vectors/Repeater.h"
#include "tgfx/layers/vectors/RoundCorner.h"
#include "tgfx/layers/vectors/ShapePath.h"
#include "tgfx/layers/vectors/SolidColor.h"
#include "tgfx/layers/vectors/StrokeStyle.h"
#include "tgfx/layers/vectors/Text.h"
#include "tgfx/layers/vectors/TextModifier.h"
#include "tgfx/layers/vectors/TextPath.h"
#include "tgfx/layers/vectors/TextSelector.h"
#include "tgfx/layers/vectors/TrimPath.h"
#include "tgfx/layers/vectors/VectorGroup.h"

namespace pagx {

// Per-category switches to skip PAGX layer effects during conversion to the tgfx layer tree.
// Each effect normally forces tgfx to allocate an offscreen surface at render time and run a
// sampling pass over the affected layer; large designs can carry hundreds of such effects
// whose combined offscreen cost dominates the frame budget. Flip individual switches to 1 in
// debug builds to measure how much of the rendering load is attributable to each effect type.
// The visual result with an effect disabled is wrong (missing shadows / blur), but everything
// downstream still parses, lays out, and renders without crashing.
//
// PAG_DISABLE_BACKGROUND_BLUR_STYLE: skips BackgroundBlurStyle (offscreen background snapshot
//   plus blur pass; usually the heaviest single category in dense designs).
// PAG_DISABLE_SHADOW_STYLES:         skips DropShadowStyle and InnerShadowStyle.
// PAG_DISABLE_LAYER_FILTERS:         skips all LayerFilters (BlurFilter, DropShadow/InnerShadow
//   filters, BlendFilter, ColorMatrixFilter). BlurFilter with very large sigma dominates here.
#define PAG_DISABLE_BACKGROUND_BLUR_STYLE 0
#define PAG_DISABLE_SHADOW_STYLES 0
#define PAG_DISABLE_LAYER_FILTERS 0

// Runtime target for a Layer's tgfx::Layer that keeps the layer transform decomposed into its
// animatable sources (x, y translation and the 2D matrix). The final tgfx matrix is recomposed as
// Translate(x, y) * matrix, mirroring applyLayerAttributes so x/y and matrix animations stack
// correctly instead of overwriting each other's setMatrix. Channels other than the transform fall
// through to the base RuntimeTarget writer table.
struct LayerRuntimeTarget : RuntimeTarget {
  float animX = 0;
  float animY = 0;
  tgfx::Matrix animMatrix = tgfx::Matrix::I();

  bool apply(const std::string& channel, const KeyValue& value, float mix) override {
    if (channel == "x") {
      const auto* v = std::get_if<float>(&value);
      if (v == nullptr) {
        return false;
      }
      animX = MixFloat(animX, *v, mix);
      recompose();
      return true;
    }
    if (channel == "y") {
      const auto* v = std::get_if<float>(&value);
      if (v == nullptr) {
        return false;
      }
      animY = MixFloat(animY, *v, mix);
      recompose();
      return true;
    }
    if (channel == "matrix") {
      const auto* v = std::get_if<Matrix>(&value);
      if (v == nullptr) {
        return false;
      }
      auto target = ToTGFX(*v);
      animMatrix = MixTGFXMatrix(animMatrix, target, mix);
      recompose();
      return true;
    }
    return RuntimeTarget::apply(channel, value, mix);
  }

  bool hasWriter(const std::string& channel) const override {
    if (channel == "x" || channel == "y" || channel == "matrix") {
      return true;
    }
    return RuntimeTarget::hasWriter(channel);
  }

  // Seeds the decomposed transform from the node's authored values so the first animated frame
  // mixes against the static baseline rather than an identity.
  void initTransform(float x, float y, const tgfx::Matrix& matrix) {
    animX = x;
    animY = y;
    animMatrix = matrix;
  }

 private:
  void recompose() {
    auto* layer = static_cast<tgfx::Layer*>(const_cast<void*>(rawObject()));
    if (layer == nullptr) {
      return;
    }
    auto result = animMatrix;
    if (animX != 0 || animY != 0) {
      result = tgfx::Matrix::MakeTrans(animX, animY) * animMatrix;
    }
    layer->setMatrix(result);
  }
};

// Decode a data URI (e.g., "data:image/png;base64,...") to an Image.
static std::shared_ptr<tgfx::Image> ImageFromDataURI(const std::string& dataURI) {
  auto data = DecodeBase64DataURI(dataURI);
  if (!data) {
    return nullptr;
  }
  return tgfx::Image::MakeFromEncoded(ToTGFXData(data));
}

// Build context that maintains state during layer tree construction. Designed so the same
// instance can be reused: the session-based flow keeps the context alive after build() so that
// later rebuildForFilePath() calls can find the already-created tgfx layers and regenerate
// their contents in place.
class LayerBuilderContext {
 public:
  LayerBuilderContext() = default;

  void setNeedsRuntimeData(bool value) {
    _needsRuntimeData = value;
  }

  void setDocument(const PAGXDocument* document) {
    _document = document;
  }

  void setImageProvider(ImageResourceProvider* provider) {
    _imageProvider = provider;
  }

  // Builds a single Composition's subtree, exposed for PAGComposition runtime slots that need
  // their own independent layerMap. The returned LayerBuildResult.root is a fresh container layer
  // populated with the composition's child layers. Per-slot mask resolution still runs on the
  // returned tree so layer styles, filters, etc. behave the same as the document-wide build.
  LayerBuildResult buildSubtree(const Composition* comp) {
    auto root = tgfx::Layer::Make();
    if (comp != nullptr) {
      for (const auto& child : comp->layers) {
        auto childLayer = convertLayer(child);
        if (childLayer) {
          root->addChild(childLayer);
        }
      }
      resolvePendingMasks();
    }
    LayerBuildResult result = std::move(_result);
    _result = {};
    result.root = root;
    return result;
  }

  LayerBuildResult buildWithMap(const PAGXDocument& document) {
    auto root = build(document);
    LayerBuildResult result = {};
    result.root = root;
    // Transfer the RuntimeBinding populated during build()/convertLayer so callers (PAGScene,
    // BuildForRuntime) can drive animation channel writers. The layerMap below is rebuilt
    // separately and must not be clobbered, so we move only the binding rather than the whole
    // _result.
    result.binding = std::move(_result.binding);
    _result = {};
    // LayerBuildResult exposes a 1:1 pagx Layer -> tgfx Layer map for historical reasons. When
    // a Composition is referenced by multiple Layers a single pagx Layer node actually produces
    // several tgfx Layers; we only surface the first one here to keep the public contract
    // stable. Session users that need every copy go through getTgfxLayers() instead.
    result.layerMap.reserve(_tgfxLayersByPagxLayer.size());
    for (const auto& [pagxLayer, tgfxLayers] : _tgfxLayersByPagxLayer) {
      if (!tgfxLayers.empty()) {
        result.layerMap.emplace(pagxLayer, tgfxLayers.front());
      }
    }
    return result;
  }

  // Rewrites the current state of a single Layer node onto its existing tgfx::Layer, preserving the
  // tgfx::Layer object identity so handles holding it stay valid. Reuses the supplied binding both
  // to look up the existing layer and to refresh masks. Vector contents, layer attributes, styles
  // and filters are regenerated from the node's current fields. Returns false if the node has no
  // tgfx::Layer in the binding (e.g. it was never built or belongs to another composition slot).
  bool refreshLayerInPlace(const Layer* node, RuntimeBinding* binding) {
    if (node == nullptr || binding == nullptr) {
      return false;
    }
    auto layer = binding->get<tgfx::Layer>(node);
    if (layer == nullptr) {
      return false;
    }
    _result.binding = std::move(*binding);
    // applyLayerAttributes only assigns the mutable attributes when they differ from the default,
    // and the mask is only re-applied when node->mask is set, so reset them first; otherwise an
    // edit that cleared a matrix / blendMode / scrollRect / style / filter / mask would keep the
    // previously built value instead of reverting to the default.
    layer->setMatrix(tgfx::Matrix::I());
    layer->setMatrix3D(tgfx::Matrix3D::I());
    layer->setPreserve3D(false);
    layer->setBlendMode(ToTGFX(BlendMode::Normal));
    layer->setAllowsEdgeAntialiasing(true);
    layer->setPassThroughBackground(true);
    layer->setScrollRect(tgfx::Rect::MakeEmpty());
    // Collect style/filter nodes currently bound to this layer's styles and filters before
    // resetting them, so we can clean up entries for any that were removed from node->styles or
    // node->filters after the rebuild.
    std::vector<const Node*> oldStyles;
    for (const auto& tgfxStyle : layer->layerStyles()) {
      const auto* styleNode = _result.binding.findNode(tgfxStyle.get());
      if (styleNode != nullptr) {
        oldStyles.push_back(styleNode);
      }
    }
    std::vector<const Node*> oldFilters;
    for (const auto& tgfxFilter : layer->filters()) {
      const auto* filterNode = _result.binding.findNode(tgfxFilter.get());
      if (filterNode != nullptr) {
        oldFilters.push_back(filterNode);
      }
    }
    layer->setLayerStyles({});
    layer->setFilters({});
    layer->setMask(nullptr);
    // Regenerate vector contents in place; composition slot layers carry no contents and keep their
    // runtime-populated children untouched. Only a tgfx::VectorLayer can hold contents. A node that
    // gained contents was promoted above, and one whose contents were cleared (now empty, but still
    // a VectorLayer) drops its stale elements via setContents({}). The type guard keeps the cast
    // safe for plain layers that never had contents.
    // Collect old content elements that are currently bound so we can unbind those no longer present
    // in node->contents after the rebuild. Without this, stale binding entries for removed elements
    // (Fill/Stroke/Elements) remain in the RuntimeBinding, and shared ColorSource/Image unbind logic
    // never triggers for them.
    std::vector<Element*> oldElements;
    if (layer->type() == tgfx::LayerType::Vector) {
      auto* vecLayer = static_cast<tgfx::VectorLayer*>(layer.get());
      for (const auto& content : vecLayer->contents()) {
        collectElementTree(content.get(), &oldElements);
      }
    }
    // Untrack all old elements from the reverse index before rebuilding, so that convertXxx (which
    // calls trackColorSource/trackImage) starts from a clean slate. Surviving elements will be
    // re-tracked during convertVectorElement; removed elements stay un-tracked and are cleaned up
    // below. Without this, every refreshLayerInPlace call would append duplicate entries to the
    // colorSourceUsers / imageUsers maps.
    for (auto* element : oldElements) {
      untrackElementColorSource(element);
    }
    if (node->composition == nullptr && layer->type() == tgfx::LayerType::Vector) {
      auto* vectorLayer = static_cast<tgfx::VectorLayer*>(layer.get());
      std::vector<std::shared_ptr<tgfx::VectorElement>> contents = {};
      contents.reserve(node->contents.size());
      for (const auto& element : node->contents) {
        auto tgfxElement = convertVectorElement(element);
        if (tgfxElement) {
          contents.push_back(tgfxElement);
        }
      }
      vectorLayer->setContents(contents);
    }
    // Unbind content elements that were removed from node->contents. A Group/TextBox subtree
    // element is considered removed only when it cannot be reached from any top-level content.
    if (!oldElements.empty()) {
      std::vector<Element*> removed;
      for (auto* element : oldElements) {
        if (!isElementInContents(element, node->contents)) {
          removed.push_back(element);
        }
      }
      if (!removed.empty()) {
        unbindContentElements(removed);
      }
    }
    applyLayerAttributes(node, layer.get());
    // Unbind style/filter nodes that were removed from node->styles or node->filters.
    for (const auto* styleNode : oldStyles) {
      if (std::find(node->styles.begin(), node->styles.end(), styleNode) == node->styles.end()) {
        _result.binding.remove(styleNode);
      }
    }
    for (const auto* filterNode : oldFilters) {
      if (std::find(node->filters.begin(), node->filters.end(), filterNode) ==
          node->filters.end()) {
        _result.binding.remove(filterNode);
      }
    }
    // Re-seed the decomposed transform baseline from the node's current authored values, mirroring
    // the initial build (see convertLayer). The Layer's x / y / matrix channels are AnimLayout, so a
    // document edit to them must update the LayerRuntimeTarget baseline; otherwise a concurrent
    // transform animation would mix against the stale baseline. The target installed for a Layer
    // node is always a LayerRuntimeTarget (convertLayer), so the static_cast is safe.
    auto* layerTarget = static_cast<LayerRuntimeTarget*>(_result.binding.getTarget(node));
    if (layerTarget != nullptr) {
      auto layerPos = node->renderPosition();
      layerTarget->initTransform(layerPos.x, layerPos.y, ToTGFX(node->matrix));
    }
    if (node->mask != nullptr) {
      _pendingMasks.emplace_back(layer, node->mask, ToTGFXMaskType(node->maskType));
      resolvePendingMasks();
    }
    // Reconcile nested child layers (Layer.children) so additions and removals are reflected.
    // Composition slot layers are skipped: their children come from the runtime composition slot,
    // not from convertLayer's child recursion.
    if (node->composition == nullptr) {
      reconcileChildLayers(node, layer.get());
    }
    *binding = std::move(_result.binding);
    return true;
  }

  // Reconciles the direct child tgfx layers of a plain (non-composition) Layer node against its
  // current node->children list. Child nodes that still map to a tgfx layer are reused (handles stay
  // valid); newly added child nodes are built into the binding; removed children are detached and
  // unbound (recursively, including their descendants). Surviving children are reordered to match
  // the document order.
  void reconcileChildLayers(const Layer* node, tgfx::Layer* parentLayer) {
    std::unordered_set<const tgfx::Layer*> kept = {};
    for (const auto& child : node->children) {
      if (child == nullptr) {
        continue;
      }
      auto childLayer = _result.binding.get<tgfx::Layer>(child);
      if (childLayer == nullptr) {
        // Newly added: build its full subtree (and bindings) via the normal convertLayer path.
        childLayer = convertLayer(child);
      }
      if (childLayer != nullptr) {
        // addChild moves an existing child to the top, so appending in document order yields the
        // correct final z-order.
        parentLayer->addChild(childLayer);
        kept.insert(childLayer.get());
      }
    }
    // Detach and unbind children whose source node was removed from node->children. Iterate a copy
    // since removeFromParent mutates the parent's child list.
    auto tgfxChildren = parentLayer->children();
    for (const auto& tgfxChild : tgfxChildren) {
      if (kept.find(tgfxChild.get()) == kept.end()) {
        unbindSubtree(tgfxChild.get());
        tgfxChild->removeFromParent();
      }
    }
  }

  // Recursively drops binding entries for a detached tgfx layer subtree so removed nodes leave no
  // stale node->object mapping behind. A Layer's vector content elements are bound as their own
  // entries keyed by the Element node (not as tgfx layer children), so they are unbound explicitly
  // when the owning Layer node is found; otherwise removing a layer subtree would leak those
  // element bindings and leave dangling Node* keys.
  void unbindSubtree(const tgfx::Layer* layer) {
    if (layer == nullptr) {
      return;
    }
    const Node* owner = _result.binding.findNode(layer);
    if (owner != nullptr) {
      if (owner->nodeType() == NodeType::Layer) {
        unbindContentElements(static_cast<const Layer*>(owner)->contents);
      }
      _result.binding.remove(owner);
    }
    for (const auto& child : layer->children()) {
      unbindSubtree(child.get());
    }
  }

  // Drops binding entries for a list of vector content-element nodes, recursing into nested element
  // containers (Group / TextBox) so the whole element subtree of a removed layer is unbound. A
  // Fill/Stroke's color source (and a gradient's ColorStops) is bound separately and may be shared
  // via an "@id" reference, so it is unbound only when no surviving Fill/Stroke still references it.
  void unbindContentElements(const std::vector<Element*>& elements) {
    for (auto* element : elements) {
      if (element == nullptr) {
        continue;
      }
      auto type = element->nodeType();
      if (type == NodeType::Group || type == NodeType::TextBox) {
        unbindContentElements(static_cast<const Group*>(element)->elements);
      } else if (type == NodeType::Fill) {
        auto* fill = static_cast<const Fill*>(element);
        _result.binding.untrackColorSource(fill->color, element);
        unbindColorSourceIfUnreferenced(fill->color, element);
      } else if (type == NodeType::Stroke) {
        auto* stroke = static_cast<const Stroke*>(element);
        _result.binding.untrackColorSource(stroke->color, element);
        unbindColorSourceIfUnreferenced(stroke->color, element);
      }
      _result.binding.remove(element);
    }
  }

  // Recursively collects all descendant elements from a Group/TextBox tree by walking the tgfx
  // VectorElement hierarchy rather than the node tree. The node tree may have already been
  // modified (elements erased from Group->elements), but the tgfx tree still holds the full
  // pre-rebuild structure at the time oldElements are collected in refreshLayerInPlace. This
  // ensures removed nested elements (e.g., Fill inside a Group) are captured and later unbound.
  void collectElementTree(const tgfx::VectorElement* tgfxElement, std::vector<Element*>* out) {
    if (tgfxElement == nullptr) {
      return;
    }
    const auto* nodeFromTgfx = _result.binding.findNode(tgfxElement);
    if (nodeFromTgfx == nullptr || nodeFromTgfx->nodeType() == NodeType::Layer) {
      return;
    }
    auto* element = const_cast<Element*>(static_cast<const Element*>(nodeFromTgfx));
    out->push_back(element);
    auto nodeType = element->nodeType();
    if (nodeType == NodeType::Group || nodeType == NodeType::TextBox) {
      auto* tgfxGroup = static_cast<const tgfx::VectorGroup*>(tgfxElement);
      for (const auto& child : tgfxGroup->elements()) {
        collectElementTree(child.get(), out);
      }
    }
  }

  // Untracks the color source reverse-index entry for Fill/Stroke elements. If the color source
  // is an ImagePattern, also untracks its imageUsers entry. Called before rebuilding content during
  // refreshLayerInPlace so that surviving elements are re-tracked by convertXxx without accumulating
  // duplicate entries.
  void untrackElementColorSource(Element* element) {
    if (element == nullptr) {
      return;
    }
    auto type = element->nodeType();
    if (type == NodeType::Fill) {
      auto* fill = static_cast<const Fill*>(element);
      untrackColorSourceAndImage(fill->color, element);
    } else if (type == NodeType::Stroke) {
      auto* stroke = static_cast<const Stroke*>(element);
      untrackColorSourceAndImage(stroke->color, element);
    }
  }

  void untrackColorSourceAndImage(const ColorSource* colorSource, const Node* owner) {
    if (colorSource == nullptr) {
      return;
    }
    _result.binding.untrackColorSource(colorSource, owner);
    if (colorSource->nodeType() == NodeType::ImagePattern) {
      auto* pattern = static_cast<const ImagePattern*>(colorSource);
      if (pattern->image) {
        _result.binding.untrackImage(pattern->image, pattern);
      }
    }
  }

  // Returns true if the element is reachable from any of the top-level contents, recursing into
  // Group/TextBox containers.
  static bool isElementInContents(Element* target, const std::vector<Element*>& contents) {
    for (auto* element : contents) {
      if (element == target) {
        return true;
      }
      auto type = element->nodeType();
      if (type == NodeType::Group || type == NodeType::TextBox) {
        if (isElementInContents(target, static_cast<const Group*>(element)->elements)) {
          return true;
        }
      }
    }
    return false;
  }

  // Unbinds a Fill/Stroke color source, but only if no Fill/Stroke other than excludedOwner still
  // points at it. Shared color sources (referenced by several painters via "@id") stay bound as long
  // as any referencing painter survives. A gradient's ColorStop bindings are owned children of the
  // gradient and are dropped together with it; an ImagePattern's Image is an independently shareable
  // resource and is dropped only when no surviving ImagePattern still references it. excludedOwner is
  // the painter currently being removed; its own binding is dropped by the caller, so it must not
  // count as a surviving reference.
  void unbindColorSourceIfUnreferenced(const ColorSource* color, const Element* excludedOwner) {
    if (color == nullptr || !_result.binding.contains(color)) {
      return;
    }
    if (_result.binding.isColorSourceShared(color, excludedOwner)) {
      return;
    }
    if (color->nodeType() == NodeType::ImagePattern) {
      auto* pattern = static_cast<const ImagePattern*>(color);
      if (pattern->image) {
        _result.binding.untrackImage(pattern->image, pattern);
      }
      unbindImageIfUnreferenced(pattern);
    } else if (color->nodeType() != NodeType::SolidColor) {
      for (const auto* stop : static_cast<const Gradient*>(color)->colorStops) {
        _result.binding.remove(stop);
      }
    }
    _result.binding.remove(color);
  }

  // Unbinds the Image bound for an ImagePattern that is about to be removed, but only if no surviving
  // ImagePattern other than pattern still references the same Image node. An Image is a shareable
  // "@id" resource, so dropping it unconditionally would break other patterns still using it.
  void unbindImageIfUnreferenced(const ImagePattern* pattern) {
    const Image* image = pattern->image;
    if (image == nullptr || !_result.binding.contains(image)) {
      return;
    }
    if (_result.binding.isImageShared(image, pattern)) {
      return;
    }
    _result.binding.remove(image);
  }

  // Builds a single Layer node into the supplied binding and returns its new tgfx::Layer. Mirrors
  // the runtime convertLayer path (composition slots stay empty containers) so the produced layer
  // matches one created during the initial build, then hands the populated binding back.
  std::shared_ptr<tgfx::Layer> buildLayerInto(const Layer* node, RuntimeBinding* binding) {
    _result.binding = std::move(*binding);
    auto layer = convertLayer(node);
    resolvePendingMasks();
    *binding = std::move(_result.binding);
    return layer;
  }

  std::shared_ptr<tgfx::Layer> build(const PAGXDocument& document) {
    _document = &document;
    // Build layer tree.
    auto rootLayer = tgfx::Layer::Make();
    // Apply canvas clipping: the root layer clips to the canvas dimensions.
    rootLayer->setScrollRect(tgfx::Rect::MakeWH(document.width, document.height));
    for (const auto& layer : document.layers) {
      auto childLayer = convertLayer(layer);
      if (childLayer) {
        rootLayer->addChild(childLayer);
      }
    }
    resolvePendingMasks();
    return rootLayer;
  }

  void resolvePendingMasks() {
    for (const auto& [layer, maskPagx, maskType] : _pendingMasks) {
      auto it = _tgfxLayersByPagxLayer.find(maskPagx);
      if (it != _tgfxLayersByPagxLayer.end() && !it->second.empty()) {
        auto maskLayer = it->second.front();
        // tgfx requires mask layer to be visible for hasValidMask() check.
        // The mask layer won't be drawn because maskOwner is set.
        maskLayer->setVisible(true);
        layer->setMask(maskLayer);
        layer->setMaskType(maskType);
      }
    }
    _pendingMasks.clear();
  }

  // Returns every tgfx layer produced for the given pagx Layer. Order matches convertLayer()
  // invocation order; an empty vector means the pagx Layer was never visited.
  std::vector<std::shared_ptr<tgfx::Layer>> getTgfxLayers(const Layer* pagxLayer) const {
    auto it = _tgfxLayersByPagxLayer.find(pagxLayer);
    if (it == _tgfxLayersByPagxLayer.end()) {
      return {};
    }
    return it->second;
  }

  // Evicts any cached tgfx::Image whose backing Image node has the given filePath so the next
  // conversion goes through getOrCreateImage() again and re-queries the provider.
  void invalidateImagesByFilePath(const PAGXDocument& document, const std::string& filePath) {
    if (filePath.empty()) {
      return;
    }
    for (auto& node : document.nodes) {
      if (!node || node->nodeType() != NodeType::Image) {
        continue;
      }
      auto* imageNode = static_cast<const Image*>(node.get());
      if (imageNode->filePath != filePath) {
        continue;
      }
      _imageCache.erase(imageNode);
    }
  }

  // Drops the entire image cache. Must be called when the document's node tree undergoes
  // structural changes (e.g. notifyChange with node additions/removals) because the raw Image*
  // keys may become dangling after such edits.
  void invalidateAllImages() {
    _imageCache.clear();
  }

  // Builds the tgfx VectorElement list for a pagx Layer's contents, skipping elements that
  // convert to null. Shared by convertVectorLayer() (initial build) and rebuildVectorContents()
  // (progressive image refresh) so the two paths cannot drift.
  std::vector<std::shared_ptr<tgfx::VectorElement>> buildVectorContents(const Layer* node) {
    std::vector<std::shared_ptr<tgfx::VectorElement>> contents;
    contents.reserve(node->contents.size());
    for (const auto& element : node->contents) {
      auto tgfxElement = convertVectorElement(element);
      if (tgfxElement) {
        contents.push_back(tgfxElement);
      }
    }
    return contents;
  }

  // Regenerates the contents of a VectorLayer by walking the original pagx Layer's contents
  // list through convertVectorElement(). Non-VectorLayer targets are skipped because they do
  // not own a setContents()-style slot (Composition container layers etc).
  bool rebuildVectorContents(const Layer* pagxLayer, tgfx::Layer* tgfxLayer) {
    if (!pagxLayer || !tgfxLayer || tgfxLayer->type() != tgfx::LayerType::Vector) {
      return false;
    }
    auto* vectorLayer = static_cast<tgfx::VectorLayer*>(tgfxLayer);
    vectorLayer->setContents(buildVectorContents(pagxLayer));
    return true;
  }

 private:
  std::shared_ptr<tgfx::Layer> convertLayer(const Layer* node) {
    if (!node) {
      return nullptr;
    }

    std::shared_ptr<tgfx::Layer> layer = nullptr;

    if (node->composition != nullptr) {
      // Layer references a composition - render composition content
      layer = convertComposition(node->composition);
    } else if (!node->contents.empty()) {
      layer = convertVectorLayer(node);
    } else {
      layer = tgfx::VectorLayer::Make();
    }

    if (layer) {
      // Install a transform-aware target so the Layer's x / y / matrix channels share one
      // recomposed tgfx matrix. set() preserves any existing object, so install before set().
      auto target = std::unique_ptr<RuntimeTarget>(new LayerRuntimeTarget());
      auto* layerTarget =
          static_cast<LayerRuntimeTarget*>(_result.binding.setTarget(node, std::move(target)));
      // Register layer for mask lookups and later rebuildForFilePath(). A single pagx Layer
      // may appear here multiple times when its owning Composition is instanced more than
      // once, so we append rather than overwrite.
      _tgfxLayersByPagxLayer[node].push_back(layer);
      _result.binding.set(node, layer);
      bindLayerChannels(node);

      applyLayerAttributes(node, layer.get());
      // Seed the decomposed transform from the authored values so the first animated frame mixes
      // against the static baseline. applyLayerAttributes has already composed them into the layer.
      if (layerTarget != nullptr) {
        auto layerPos = node->renderPosition();
        layerTarget->initTransform(layerPos.x, layerPos.y, ToTGFX(node->matrix));
      }

      // Queue mask to be applied in second pass.
      if (node->mask != nullptr) {
        _pendingMasks.emplace_back(layer, node->mask, ToTGFXMaskType(node->maskType));
      }

      for (const auto& child : node->children) {
        auto childLayer = convertLayer(child);
        if (childLayer) {
          layer->addChild(childLayer);
        }
      }
    }

    return layer;
  }

  static void WriteLayerAlpha(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* layer = static_cast<tgfx::Layer*>(object);
    layer->setAlpha(MixFloat(layer->alpha(), *v, mix));
  }

  static void WriteLayerVisible(void* object, const KeyValue& value, float) {
    auto* v = std::get_if<bool>(&value);
    if (v == nullptr) {
      return;
    }
    static_cast<tgfx::Layer*>(object)->setVisible(*v);
  }

  static void WriteLayerBlendMode(void* object, const KeyValue& value, float) {
    auto* v = std::get_if<int>(&value);
    if (v == nullptr) {
      return;
    }
    auto mode = static_cast<BlendMode>(*v);
    static_cast<tgfx::Layer*>(object)->setBlendMode(ToTGFX(mode));
  }

  // x / y / matrix are handled by LayerRuntimeTarget::apply (they share one recomposed matrix), so
  // they are not registered as plain writers here.
  void bindLayerChannels(const Layer* node) {
    _result.binding.setWriter(node, "alpha", WriteLayerAlpha);
    _result.binding.setWriter(node, "visible", WriteLayerVisible);
    _result.binding.setWriter(node, "blendMode", WriteLayerBlendMode);
  }

  std::shared_ptr<tgfx::Layer> convertComposition(const Composition* comp) {
    if (!comp) {
      return nullptr;
    }
    // Composition slot: create an empty container layer. The runtime PAGComposition is
    // responsible for populating this slot with its own per-slot layer subtree, so multiple
    // Layers referencing the same Composition stay independent. When LayerBuilder is invoked
    // standalone (no PAGFile in play, e.g. from optimizer / cli rendering paths) the slot is
    // populated immediately with a flat expansion to preserve backward-compatible static
    // rendering. PAGFile sets _needsRuntimeData before calling Build() to opt out of this path.
    auto containerLayer = tgfx::Layer::Make();
    if (_needsRuntimeData) {
      return containerLayer;
    }
    for (const auto& compLayer : comp->layers) {
      auto childLayer = convertLayer(compLayer);
      if (childLayer) {
        containerLayer->addChild(childLayer);
      }
    }
    return containerLayer;
  }

  std::shared_ptr<tgfx::Layer> convertVectorLayer(const Layer* node) {
    auto layer = tgfx::VectorLayer::Make();
    layer->setContents(buildVectorContents(node));
    return layer;
  }

  // Generate TextBlob for a Text node using GlyphRunRenderer with the given inverse matrix.
  // For standalone Text (not inside TextBox), inverseMatrix is Identity.
  // For TextBox children, inverseMatrix cancels the cumulative Group transforms.
  static void PrepareTextBlob(Text* text, const tgfx::Matrix& inverseMatrix) {
    if (!text->glyphData->layoutRuns.empty()) {
      text->glyphData->textBlob =
          GlyphRunRenderer::BuildTextBlobFromLayoutRuns(text->glyphData->layoutRuns, inverseMatrix);
    } else if (!text->glyphRuns.empty()) {
      GlyphRunRenderer::BuildTextBlob(text, inverseMatrix);
    }
  }

  // FNV-1a 64-bit mixing step over a raw byte range. Kept as an explicit helper (no lambda)
  // so ComputeTextBlobFingerprint stays readable while honoring the no-lambda project rule.
  static void FnvMix(uint64_t* h, const void* data, size_t bytes) {
    constexpr uint64_t FNV_PRIME = 0x100000001b3ULL;
    const uint8_t* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < bytes; ++i) {
      *h ^= p[i];
      *h *= FNV_PRIME;
    }
  }

  // Mixes a contiguous container (size prefix + element bytes) into the running FNV hash.
  template <typename Vec>
  static void FnvMixVector(uint64_t* h, const Vec& vec) {
    size_t n = vec.size();
    FnvMix(h, &n, sizeof(n));
    if (n > 0) {
      FnvMix(h, vec.data(), n * sizeof(vec[0]));
    }
  }

  // Fingerprint over every input GlyphRunRenderer::BuildTextBlob consumes. When BuildTextBlob is
  // changed to read a new field, that field MUST be mixed in here — otherwise the cache may serve
  // stale glyphs.
  static uint64_t ComputeTextBlobFingerprint(const Text* text, const tgfx::Matrix& inverseMatrix) {
    // FNV-1a 64-bit. Collision probability for ~5K entries is ~10^-12, and the glyphSum secondary
    // check guards the rare miss.
    constexpr uint64_t FNV_OFFSET = 0xcbf29ce484222325ULL;
    uint64_t h = FNV_OFFSET;
    // 1. Inverse matrix (6 floats).
    float matrixValues[6] = {0};
    inverseMatrix.get6(matrixValues);
    FnvMix(&h, matrixValues, sizeof(matrixValues));
    // 2. Faux bold / italic flags
    bool fauxFlags[2] = {text->fauxBold, text->fauxItalic};
    FnvMix(&h, fauxFlags, sizeof(fauxFlags));
    // 3. GlyphRun count
    size_t runCount = text->glyphRuns.size();
    FnvMix(&h, &runCount, sizeof(runCount));
    // 4. Per-run geometry.
    for (const auto* run : text->glyphRuns) {
      if (!run) {
        uint8_t nullMarker = 0;
        FnvMix(&h, &nullMarker, sizeof(nullMarker));
        continue;
      }
      const Font* fontPtr = run->font;
      FnvMix(&h, &fontPtr, sizeof(fontPtr));
      FnvMix(&h, &run->fontSize, sizeof(run->fontSize));
      FnvMix(&h, &run->x, sizeof(run->x));
      FnvMix(&h, &run->y, sizeof(run->y));
      FnvMixVector(&h, run->glyphs);
      FnvMixVector(&h, run->positions);
      FnvMixVector(&h, run->xOffsets);
      FnvMixVector(&h, run->scales);
      FnvMixVector(&h, run->rotations);
      FnvMixVector(&h, run->skews);
      FnvMixVector(&h, run->anchors);
    }
    return h;
  }

  // Total glyph count across all GlyphRuns. Used as a cheap secondary key check so a 64-bit
  // hash collision (already < 1 in 10^12) cannot promote to a wrong-render bug — a mismatched
  // glyphSum forces a rebuild rather than reuse.
  static size_t SumGlyphCount(const Text* text) {
    size_t sum = 0;
    for (const auto* run : text->glyphRuns) {
      if (run) sum += run->glyphs.size();
    }
    return sum;
  }

  // Cached wrapper around PrepareTextBlob. Looks up the fingerprint first; on hit reuses the
  // TextBlob and per-glyph anchors from the cache. On miss builds via PrepareTextBlob and
  // stashes the result.
  void prepareTextBlobCached(Text* text, const tgfx::Matrix& inverseMatrix) {
    if (!text || !text->glyphData) return;
    // Already prepared in this build session (or a previous instance hit the same cache slot).
    if (text->glyphData->textBlob) return;
    // The runtime-shaping path uses layoutRuns rather than glyphRuns; the fingerprint scheme
    // here covers only the embedded glyphRuns path, which is what cocraft exports. Fall back
    // to the uncached path for layoutRuns to stay correct.
    if (!text->glyphData->layoutRuns.empty() || text->glyphRuns.empty()) {
      PrepareTextBlob(text, inverseMatrix);
      return;
    }
    auto fingerprint = ComputeTextBlobFingerprint(text, inverseMatrix);
    size_t glyphSum = SumGlyphCount(text);
    auto it = _textBlobCache.find(fingerprint);
    if (it != _textBlobCache.end() && it->second.glyphSum == glyphSum) {
      text->glyphData->textBlob = it->second.textBlob;
      text->glyphData->anchors = it->second.anchors;
      return;
    }
    PrepareTextBlob(text, inverseMatrix);
    if (text->glyphData->textBlob) {
      _textBlobCache[fingerprint] =
          TextBlobCacheEntry{glyphSum, text->glyphData->textBlob, text->glyphData->anchors};
    }
  }

  // Prepare TextBlobs for all Text children of a TextBox by applying inverse matrices.
  // This must happen at render time (not layout time) so that tgfx's StrokePainter can
  // detect the Group transform via geometry->matrix changes between apply() and draw().
  void prepareTextBoxTextBlobs(const TextBox* textBox) {
    std::vector<Text*> childText;
    std::vector<tgfx::Matrix> matrices;
    TextLayout::CollectTextElements(textBox->elements, childText, matrices);

    for (size_t i = 0; i < childText.size(); i++) {
      tgfx::Matrix inverse = {};
      if (!matrices[i].invert(&inverse)) {
        continue;
      }
      prepareTextBlobCached(childText[i], inverse);
    }
  }

  std::shared_ptr<tgfx::VectorElement> convertVectorElement(Element* node) {
    if (!node) {
      return nullptr;
    }

    std::shared_ptr<tgfx::VectorElement> result = nullptr;
    switch (node->nodeType()) {
      case NodeType::Rectangle:
        result = convertRectangle(static_cast<const Rectangle*>(node));
        break;
      case NodeType::Ellipse:
        result = convertEllipse(static_cast<const Ellipse*>(node));
        break;
      case NodeType::Polystar:
        result = convertPolystar(static_cast<const Polystar*>(node));
        break;
      case NodeType::Path:
        result = convertPath(static_cast<const Path*>(node));
        break;
      case NodeType::Text:
        result = convertText(static_cast<Text*>(node));
        break;
      case NodeType::Fill:
        result = convertFill(static_cast<const Fill*>(node));
        break;
      case NodeType::Stroke:
        result = convertStroke(static_cast<const Stroke*>(node));
        break;
      case NodeType::TrimPath:
        result = convertTrimPath(static_cast<const TrimPath*>(node));
        break;
      case NodeType::TextPath:
        result = convertTextPath(static_cast<const TextPath*>(node));
        break;
      case NodeType::RoundCorner:
        result = convertRoundCorner(static_cast<const RoundCorner*>(node));
        break;
      case NodeType::MergePath:
        result = convertMergePath(static_cast<const MergePath*>(node));
        break;
      case NodeType::Repeater:
        result = convertRepeater(static_cast<const Repeater*>(node));
        break;
      case NodeType::TextModifier:
        result = convertTextModifier(static_cast<const TextModifier*>(node));
        break;
      case NodeType::Group:
        result = convertGroup(static_cast<const Group*>(node));
        break;
      case NodeType::TextBox: {
        auto* textBox = static_cast<const TextBox*>(node);
        if (textBox->elements.empty()) {
          return nullptr;
        }
        prepareTextBoxTextBlobs(textBox);
        result = convertGroup(textBox);
        break;
      }
      default:
        break;
    }
    return result;
  }

  // Templated writer for the common "scalar float channel = setter(Mix(getter, value, mix))"
  // pattern, eliminating one near-identical writer per animatable float property.
  template <typename T, auto Getter, auto Setter>
  static void WriteMixedFloat(void* object, const KeyValue& value, float mix) {
    const auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* obj = static_cast<T*>(object);
    (obj->*Setter)(MixFloat((obj->*Getter)(), *v, mix));
  }

  // Templated writer for tgfx Color channels (setter takes const Color&).
  template <typename T, auto Getter, auto Setter>
  static void WriteMixedColor(void* object, const KeyValue& value, float mix) {
    const auto* v = std::get_if<Color>(&value);
    if (v == nullptr) {
      return;
    }
    auto* obj = static_cast<T*>(object);
    (obj->*Setter)(MixTGFXColor((obj->*Getter)(), ToTGFX(*v), mix));
  }

  // Templated writer for a width/height component of a tgfx Size-valued property. WidthAxis selects
  // which component the channel drives; the other component is preserved.
  template <typename T, auto Getter, auto Setter, bool WidthAxis>
  static void WriteSizeAxis(void* object, const KeyValue& value, float mix) {
    const auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* obj = static_cast<T*>(object);
    auto size = (obj->*Getter)();
    if (WidthAxis) {
      size.width = MixFloat(size.width, *v, mix);
    } else {
      size.height = MixFloat(size.height, *v, mix);
    }
    (obj->*Setter)(size);
  }

  // Templated writer for an x/y component of a tgfx Point-valued property.
  template <typename T, auto Getter, auto Setter, bool XAxis>
  static void WritePointAxis(void* object, const KeyValue& value, float mix) {
    const auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* obj = static_cast<T*>(object);
    auto point = (obj->*Getter)();
    if (XAxis) {
      point.x = MixFloat(point.x, *v, mix);
    } else {
      point.y = MixFloat(point.y, *v, mix);
    }
    (obj->*Setter)(point);
  }

  // Rectangle roundness: the PAGX node carries a single float but tgfx exposes a 4-corner array, so
  // this cannot use WriteMixedFloat. Mix against the first corner and apply uniformly.
  static void WriteRectangleRoundness(void* object, const KeyValue& value, float mix) {
    const auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* rect = static_cast<tgfx::Rectangle*>(object);
    float current = rect->roundness()[0];
    rect->setRoundness(MixFloat(current, *v, mix));
  }

  std::shared_ptr<tgfx::Rectangle> convertRectangle(const Rectangle* node) {
    auto rect = tgfx::Rectangle::Make();
    rect->setPosition(ToTGFX(node->renderPosition()));
    auto size = node->renderSize();
    rect->setSize({size.width, size.height});
    rect->setRoundness(node->roundness);
    rect->setReversed(node->reversed);
    _result.binding.set(node, rect);
    _result.binding.setWriter(
        node, "size.width",
        WriteSizeAxis<tgfx::Rectangle, &tgfx::Rectangle::size, &tgfx::Rectangle::setSize, true>);
    _result.binding.setWriter(
        node, "size.height",
        WriteSizeAxis<tgfx::Rectangle, &tgfx::Rectangle::size, &tgfx::Rectangle::setSize, false>);
    _result.binding.setWriter(node, "position.x",
                              WritePointAxis<tgfx::Rectangle, &tgfx::Rectangle::position,
                                             &tgfx::Rectangle::setPosition, true>);
    _result.binding.setWriter(node, "position.y",
                              WritePointAxis<tgfx::Rectangle, &tgfx::Rectangle::position,
                                             &tgfx::Rectangle::setPosition, false>);
    _result.binding.setWriter(node, "roundness", WriteRectangleRoundness);
    return rect;
  }

  std::shared_ptr<tgfx::Ellipse> convertEllipse(const Ellipse* node) {
    auto ellipse = tgfx::Ellipse::Make();
    ellipse->setPosition(ToTGFX(node->renderPosition()));
    auto size = node->renderSize();
    ellipse->setSize({size.width, size.height});
    ellipse->setReversed(node->reversed);
    _result.binding.set(node, ellipse);
    _result.binding.setWriter(
        node, "size.width",
        WriteSizeAxis<tgfx::Ellipse, &tgfx::Ellipse::size, &tgfx::Ellipse::setSize, true>);
    _result.binding.setWriter(
        node, "size.height",
        WriteSizeAxis<tgfx::Ellipse, &tgfx::Ellipse::size, &tgfx::Ellipse::setSize, false>);
    _result.binding.setWriter(
        node, "position.x",
        WritePointAxis<tgfx::Ellipse, &tgfx::Ellipse::position, &tgfx::Ellipse::setPosition, true>);
    _result.binding.setWriter(node, "position.y",
                              WritePointAxis<tgfx::Ellipse, &tgfx::Ellipse::position,
                                             &tgfx::Ellipse::setPosition, false>);
    return ellipse;
  }

  std::shared_ptr<tgfx::Polystar> convertPolystar(const Polystar* node) {
    auto polystar = tgfx::Polystar::Make();
    polystar->setPosition(ToTGFX(node->renderPosition()));
    polystar->setPointCount(node->pointCount);
    polystar->setOuterRadius(node->renderOuterRadius());
    polystar->setInnerRadius(node->renderInnerRadius());
    polystar->setOuterRoundness(node->outerRoundness);
    polystar->setInnerRoundness(node->innerRoundness);
    polystar->setRotation(node->rotation);
    polystar->setReversed(node->reversed);
    if (node->type == PolystarType::Polygon) {
      polystar->setPolystarType(tgfx::PolystarType::Polygon);
    } else {
      polystar->setPolystarType(tgfx::PolystarType::Star);
    }
    _result.binding.set(node, polystar);
    _result.binding.setWriter(node, "pointCount",
                              WriteMixedFloat<tgfx::Polystar, &tgfx::Polystar::pointCount,
                                              &tgfx::Polystar::setPointCount>);
    _result.binding.setWriter(node, "outerRadius",
                              WriteMixedFloat<tgfx::Polystar, &tgfx::Polystar::outerRadius,
                                              &tgfx::Polystar::setOuterRadius>);
    _result.binding.setWriter(node, "innerRadius",
                              WriteMixedFloat<tgfx::Polystar, &tgfx::Polystar::innerRadius,
                                              &tgfx::Polystar::setInnerRadius>);
    _result.binding.setWriter(node, "outerRoundness",
                              WriteMixedFloat<tgfx::Polystar, &tgfx::Polystar::outerRoundness,
                                              &tgfx::Polystar::setOuterRoundness>);
    _result.binding.setWriter(node, "innerRoundness",
                              WriteMixedFloat<tgfx::Polystar, &tgfx::Polystar::innerRoundness,
                                              &tgfx::Polystar::setInnerRoundness>);
    _result.binding.setWriter(
        node, "rotation",
        WriteMixedFloat<tgfx::Polystar, &tgfx::Polystar::rotation, &tgfx::Polystar::setRotation>);
    _result.binding.setWriter(node, "position.x",
                              WritePointAxis<tgfx::Polystar, &tgfx::Polystar::position,
                                             &tgfx::Polystar::setPosition, true>);
    _result.binding.setWriter(node, "position.y",
                              WritePointAxis<tgfx::Polystar, &tgfx::Polystar::position,
                                             &tgfx::Polystar::setPosition, false>);
    return polystar;
  }

  tgfx::Path getScaledPath(const PathData* pathData, float scale) {
    PathCacheKey key = {pathData, scale};
    auto it = _scaledPathCache.find(key);
    if (it != _scaledPathCache.end()) {
      return it->second;
    }
    auto path = ToTGFX(*pathData);
    if (scale != 1.0f) {
      path.transform(tgfx::Matrix::MakeScale(scale));
    }
    _scaledPathCache[key] = path;
    return path;
  }

  std::shared_ptr<tgfx::ShapePath> convertPath(const Path* node) {
    auto shapePath = tgfx::ShapePath::Make();
    shapePath->setPosition(ToTGFX(node->renderPosition()));
    if (node->data) {
      shapePath->setPath(getScaledPath(node->data, node->renderScale()));
    }
    shapePath->setReversed(node->reversed);
    return shapePath;
  }

  static void WriteTextX(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* text = static_cast<tgfx::Text*>(object);
    auto pos = text->position();
    pos.x = MixFloat(pos.x, *v, mix);
    text->setPosition(pos);
  }

  static void WriteTextY(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* text = static_cast<tgfx::Text*>(object);
    auto pos = text->position();
    pos.y = MixFloat(pos.y, *v, mix);
    text->setPosition(pos);
  }

  std::shared_ptr<tgfx::Text> convertText(Text* node) {
    prepareTextBlobCached(node, tgfx::Matrix::I());
    auto textBlob = node->glyphData->textBlob;
    if (textBlob == nullptr) {
      return nullptr;
    }
    auto tgfxText = tgfx::Text::Make(textBlob, node->glyphData->anchors);
    if (tgfxText) {
      auto pos = node->renderPosition();
      tgfxText->setPosition(tgfx::Point::Make(pos.x, pos.y));
      _result.binding.set(node, tgfxText);
      _result.binding.setWriter(node, "x", WriteTextX);
      _result.binding.setWriter(node, "y", WriteTextY);
    }
    return tgfxText;
  }

  std::shared_ptr<tgfx::FillStyle> convertFill(const Fill* node) {
    std::shared_ptr<tgfx::ColorSource> colorSource = nullptr;
    if (node->color) {
      colorSource = convertColorSource(node->color);
    }
    if (colorSource == nullptr) {
      // Image-backed fills drop out entirely until the underlying image arrives. Falling back to
      // an opaque SolidColor here would paint a black placeholder over every shape that uses an
      // ImagePattern before its image is resolved via the provider; leaving the fill empty keeps
      // those shapes transparent until the image is ready.
      if (node->color && node->color->nodeType() == NodeType::ImagePattern) {
        return nullptr;
      }
      colorSource = tgfx::SolidColor::Make();
    }

    auto fill = tgfx::FillStyle::Make(colorSource);
    if (fill) {
      _result.binding.set(node, fill);
      if (node->color) {
        _result.binding.trackColorSource(node->color, node);
      }
      fill->setAlpha(node->alpha);
      if (node->blendMode != BlendMode::Normal) {
        fill->setBlendMode(ToTGFX(node->blendMode));
      }
      if (node->fillRule != FillRule::Winding) {
        fill->setFillRule(ToTGFX(node->fillRule));
      }
      if (node->placement != LayerPlacement::Background) {
        fill->setPlacement(ToTGFX(node->placement));
      }
      if (node->color && node->color->nodeType() == NodeType::SolidColor) {
        _result.binding.setWriter(node, "color", WritePainterColor<tgfx::FillStyle>);
      }
      _result.binding.setWriter(
          node, "alpha",
          WriteMixedFloat<tgfx::FillStyle, &tgfx::FillStyle::alpha, &tgfx::FillStyle::setAlpha>);
    }
    return fill;
  }

  std::shared_ptr<tgfx::StrokeStyle> convertStroke(const Stroke* node) {
    std::shared_ptr<tgfx::ColorSource> colorSource = nullptr;
    if (node->color) {
      colorSource = convertColorSource(node->color);
    }
    if (colorSource == nullptr) {
      // Image-backed strokes drop out entirely until the underlying image arrives. Falling back
      // to an opaque SolidColor here would paint a black placeholder along every stroke that uses
      // an ImagePattern before its image is resolved via the provider; leaving the stroke empty
      // keeps those shapes transparent until the image is ready, matching convertFill.
      if (node->color && node->color->nodeType() == NodeType::ImagePattern) {
        return nullptr;
      }
      colorSource = tgfx::SolidColor::Make();
    }

    auto stroke = tgfx::StrokeStyle::Make(colorSource);
    if (stroke == nullptr) {
      return nullptr;
    }
    _result.binding.set(node, stroke);
    if (node->color) {
      _result.binding.trackColorSource(node->color, node);
    }
    stroke->setStrokeWidth(node->width);
    stroke->setAlpha(node->alpha);
    stroke->setLineCap(ToTGFX(node->cap));
    stroke->setLineJoin(ToTGFX(node->join));
    stroke->setMiterLimit(node->miterLimit);
    if (!node->dashes.empty()) {
      stroke->setDashes(node->dashes);
      stroke->setDashOffset(node->dashOffset);
      stroke->setDashAdaptive(node->dashAdaptive);
    }
    if (node->blendMode != BlendMode::Normal) {
      stroke->setBlendMode(ToTGFX(node->blendMode));
    }
    if (node->align != StrokeAlign::Center) {
      stroke->setStrokeAlign(ToTGFX(node->align));
    }
    if (node->placement != LayerPlacement::Background) {
      stroke->setPlacement(ToTGFX(node->placement));
    }
    if (node->color && node->color->nodeType() == NodeType::SolidColor) {
      _result.binding.setWriter(node, "color", WritePainterColor<tgfx::StrokeStyle>);
    }
    _result.binding.setWriter(node, "width",
                              WriteMixedFloat<tgfx::StrokeStyle, &tgfx::StrokeStyle::strokeWidth,
                                              &tgfx::StrokeStyle::setStrokeWidth>);
    _result.binding.setWriter(node, "alpha",
                              WriteMixedFloat<tgfx::StrokeStyle, &tgfx::StrokeStyle::alpha,
                                              &tgfx::StrokeStyle::setAlpha>);
    _result.binding.setWriter(node, "miterLimit",
                              WriteMixedFloat<tgfx::StrokeStyle, &tgfx::StrokeStyle::miterLimit,
                                              &tgfx::StrokeStyle::setMiterLimit>);
    _result.binding.setWriter(node, "dashOffset",
                              WriteMixedFloat<tgfx::StrokeStyle, &tgfx::StrokeStyle::dashOffset,
                                              &tgfx::StrokeStyle::setDashOffset>);

    return stroke;
  }

  static void WriteSolidColor(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<Color>(&value);
    if (v == nullptr) {
      return;
    }
    auto* solid = static_cast<tgfx::SolidColor*>(object);
    auto target = ToTGFX(*v);
    solid->setColor(MixTGFXColor(solid->color(), target, mix));
  }

  template <typename PainterType>
  static void WritePainterColor(void* object, const KeyValue& value, float mix) {
    auto* painter = static_cast<PainterType*>(object);
    auto colorSource = painter->colorSource();
    auto* solid = static_cast<tgfx::SolidColor*>(colorSource.get());
    if (solid == nullptr) {
      return;
    }
    WriteSolidColor(solid, value, mix);
  }

  std::shared_ptr<tgfx::ColorSource> convertColorSource(const ColorSource* node) {
    if (!node) {
      return nullptr;
    }

    switch (node->nodeType()) {
      case NodeType::SolidColor: {
        auto solid = static_cast<const SolidColor*>(node);
        auto tgfxSolid = tgfx::SolidColor::Make(ToTGFX(solid->color));
        if (tgfxSolid) {
          _result.binding.set(solid, tgfxSolid);
          _result.binding.setWriter(solid, "color", WriteSolidColor);
        }
        return tgfxSolid;
      }
      case NodeType::LinearGradient: {
        auto grad = static_cast<const LinearGradient*>(node);
        return convertLinearGradient(grad);
      }
      case NodeType::RadialGradient: {
        auto grad = static_cast<const RadialGradient*>(node);
        return convertRadialGradient(grad);
      }
      case NodeType::ConicGradient: {
        auto grad = static_cast<const ConicGradient*>(node);
        return convertConicGradient(grad);
      }
      case NodeType::DiamondGradient: {
        auto grad = static_cast<const DiamondGradient*>(node);
        return convertDiamondGradient(grad);
      }
      case NodeType::ImagePattern: {
        auto pattern = static_cast<const ImagePattern*>(node);
        return convertImagePattern(pattern);
      }
      default:
        return nullptr;
    }
  }

  static RuntimeColorStop* GetColorStopBinding(void* object) {
    return static_cast<RuntimeColorStop*>(object);
  }

  static void WriteColorStopColor(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<Color>(&value);
    if (v == nullptr) {
      return;
    }
    auto* stop = GetColorStopBinding(object);
    if (stop == nullptr || stop->gradient == nullptr) {
      return;
    }
    auto colors = stop->gradient->colors();
    if (stop->index >= colors.size()) {
      return;
    }
    auto target = ToTGFX(*v);
    colors[stop->index] = MixTGFXColor(colors[stop->index], target, mix);
    stop->gradient->setColors(std::move(colors));
  }

  static void WriteColorStopOffset(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* stop = GetColorStopBinding(object);
    if (stop == nullptr || stop->gradient == nullptr) {
      return;
    }
    auto positions = stop->gradient->positions();
    if (stop->index >= positions.size()) {
      return;
    }
    positions[stop->index] = MixFloat(positions[stop->index], *v, mix);
    stop->gradient->setPositions(std::move(positions));
  }

  void extractGradientStops(const std::vector<ColorStop*>& colorStops,
                            std::vector<tgfx::Color>* colors, std::vector<float>* positions) {
    colors->reserve(colorStops.size());
    positions->reserve(colorStops.size());
    for (const auto* stop : colorStops) {
      colors->push_back(ToTGFX(stop->color));
      positions->push_back(stop->offset);
    }
    if (colors->empty()) {
      *colors = {tgfx::Color::Black(), tgfx::Color::White()};
      *positions = {0.0f, 1.0f};
    }
  }

  std::shared_ptr<tgfx::ColorSource> applyGradientProperties(
      std::shared_ptr<tgfx::Gradient> gradient, const Gradient* node,
      const std::vector<ColorStop*>& colorStops) {
    if (gradient) {
      _result.binding.set(node, gradient);
      for (size_t index = 0; index < colorStops.size(); index++) {
        auto stopBinding = std::make_shared<RuntimeColorStop>();
        stopBinding->gradient = gradient;
        stopBinding->index = index;
        _result.binding.set(colorStops[index], stopBinding);
        _result.binding.setWriter(colorStops[index], "color", WriteColorStopColor);
        _result.binding.setWriter(colorStops[index], "offset", WriteColorStopOffset);
      }
      gradient->setFitsToGeometry(node->fitsToGeometry);
      if (!node->matrix.isIdentity()) {
        gradient->setMatrix(ToTGFX(node->matrix));
      }
    }

    return gradient;
  }

  std::shared_ptr<tgfx::ColorSource> convertLinearGradient(const LinearGradient* node) {
    std::vector<tgfx::Color> colors;
    std::vector<float> positions;
    extractGradientStops(node->colorStops, &colors, &positions);
    auto result = applyGradientProperties(
        tgfx::Gradient::MakeLinear(ToTGFX(node->startPoint), ToTGFX(node->endPoint), colors,
                                   positions),
        node, node->colorStops);
    if (result != nullptr) {
      _result.binding.setWriter(
          node, "startPoint.x",
          WritePointAxis<tgfx::LinearGradient, &tgfx::LinearGradient::startPoint,
                         &tgfx::LinearGradient::setStartPoint, true>);
      _result.binding.setWriter(
          node, "startPoint.y",
          WritePointAxis<tgfx::LinearGradient, &tgfx::LinearGradient::startPoint,
                         &tgfx::LinearGradient::setStartPoint, false>);
      _result.binding.setWriter(
          node, "endPoint.x",
          WritePointAxis<tgfx::LinearGradient, &tgfx::LinearGradient::endPoint,
                         &tgfx::LinearGradient::setEndPoint, true>);
      _result.binding.setWriter(
          node, "endPoint.y",
          WritePointAxis<tgfx::LinearGradient, &tgfx::LinearGradient::endPoint,
                         &tgfx::LinearGradient::setEndPoint, false>);
    }
    return result;
  }

  std::shared_ptr<tgfx::ColorSource> convertRadialGradient(const RadialGradient* node) {
    std::vector<tgfx::Color> colors;
    std::vector<float> positions;
    extractGradientStops(node->colorStops, &colors, &positions);
    auto result = applyGradientProperties(
        tgfx::Gradient::MakeRadial(ToTGFX(node->center), node->radius, colors, positions), node,
        node->colorStops);
    if (result != nullptr) {
      _result.binding.setWriter(node, "center.x",
                                WritePointAxis<tgfx::RadialGradient, &tgfx::RadialGradient::center,
                                               &tgfx::RadialGradient::setCenter, true>);
      _result.binding.setWriter(node, "center.y",
                                WritePointAxis<tgfx::RadialGradient, &tgfx::RadialGradient::center,
                                               &tgfx::RadialGradient::setCenter, false>);
      _result.binding.setWriter(node, "radius",
                                WriteMixedFloat<tgfx::RadialGradient, &tgfx::RadialGradient::radius,
                                                &tgfx::RadialGradient::setRadius>);
    }
    return result;
  }

  std::shared_ptr<tgfx::ColorSource> convertConicGradient(const ConicGradient* node) {
    std::vector<tgfx::Color> colors;
    std::vector<float> positions;
    extractGradientStops(node->colorStops, &colors, &positions);
    auto result =
        applyGradientProperties(tgfx::Gradient::MakeConic(ToTGFX(node->center), node->startAngle,
                                                          node->endAngle, colors, positions),
                                node, node->colorStops);
    if (result != nullptr) {
      _result.binding.setWriter(node, "center.x",
                                WritePointAxis<tgfx::ConicGradient, &tgfx::ConicGradient::center,
                                               &tgfx::ConicGradient::setCenter, true>);
      _result.binding.setWriter(node, "center.y",
                                WritePointAxis<tgfx::ConicGradient, &tgfx::ConicGradient::center,
                                               &tgfx::ConicGradient::setCenter, false>);
      _result.binding.setWriter(
          node, "startAngle",
          WriteMixedFloat<tgfx::ConicGradient, &tgfx::ConicGradient::startAngle,
                          &tgfx::ConicGradient::setStartAngle>);
      _result.binding.setWriter(node, "endAngle",
                                WriteMixedFloat<tgfx::ConicGradient, &tgfx::ConicGradient::endAngle,
                                                &tgfx::ConicGradient::setEndAngle>);
    }
    return result;
  }

  std::shared_ptr<tgfx::ColorSource> convertDiamondGradient(const DiamondGradient* node) {
    std::vector<tgfx::Color> colors;
    std::vector<float> positions;
    extractGradientStops(node->colorStops, &colors, &positions);
    auto result = applyGradientProperties(
        tgfx::Gradient::MakeDiamond(ToTGFX(node->center), node->radius, colors, positions), node,
        node->colorStops);
    if (result != nullptr) {
      _result.binding.setWriter(
          node, "center.x",
          WritePointAxis<tgfx::DiamondGradient, &tgfx::DiamondGradient::center,
                         &tgfx::DiamondGradient::setCenter, true>);
      _result.binding.setWriter(
          node, "center.y",
          WritePointAxis<tgfx::DiamondGradient, &tgfx::DiamondGradient::center,
                         &tgfx::DiamondGradient::setCenter, false>);
      _result.binding.setWriter(
          node, "radius",
          WriteMixedFloat<tgfx::DiamondGradient, &tgfx::DiamondGradient::radius,
                          &tgfx::DiamondGradient::setRadius>);
    }
    return result;
  }

  std::shared_ptr<tgfx::ColorSource> convertImagePattern(const ImagePattern* node) {
    if (!node || node->image == nullptr) {
      return nullptr;
    }

    auto imageNode = node->image;
    auto image = getOrCreateImage(imageNode);
    if (!image) {
      // Image data is missing for this node (never loaded or decode failed); paint nothing
      // rather than falling back to an opaque color that would be visibly wrong.
      return nullptr;
    }
    _result.binding.set(imageNode, image);

    auto sampling = tgfx::SamplingOptions(ToTGFX(node->filterMode), ToTGFX(node->mipmapMode));
    auto pattern =
        tgfx::ImagePattern::Make(image, ToTGFX(node->tileModeX), ToTGFX(node->tileModeY), sampling);
    if (pattern) {
      _result.binding.set(node, pattern);
      _result.binding.trackImage(imageNode, node);
      pattern->setScaleMode(ToTGFX(node->scaleMode));
      if (!node->matrix.isIdentity()) {
        pattern->setMatrix(ToTGFX(node->matrix));
      }
    }

    return pattern;
  }

  std::shared_ptr<tgfx::Image> getOrCreateImage(const Image* imageNode) {
    auto it = _imageCache.find(imageNode);
    if (it != _imageCache.end()) {
      return it->second;
    }
    // Resolution chain. Two camps of sources exist:
    //   1. Provider-resolved images (backend textures): already mipmapped at upload time by the
    //      host's createBackendTexture helper. Re-wrapping with makeMipmapped(true) would force
    //      tgfx to allocate a parallel mipmapped texture and copy the pixels. Use as-is.
    //   2. CPU-decoded images (encoded data, file path, data URI): produced lazily by tgfx
    //      codecs. Wrap with makeMipmapped(true) so subsequent sampling at non-1:1 scales does
    //      not re-decode at every zoom level.
    std::shared_ptr<tgfx::Image> image = nullptr;
    bool isBackendTexture = false;
    // Priority 1: query the provider for a platform-decoded image.
    if (_imageProvider && !imageNode->filePath.empty()) {
      image = _imageProvider->resolveImage(imageNode->filePath);
      if (image) {
        isBackendTexture = true;
      }
    }
    // Priority 2: fallback to standard decoding chain.
    if (!image) {
      if (imageNode->data) {
        image = tgfx::Image::MakeFromEncoded(ToTGFXData(imageNode->data));
      } else if (imageNode->filePath.find("data:") == 0) {
        image = ImageFromDataURI(imageNode->filePath);
      } else if (!imageNode->filePath.empty()) {
        image = tgfx::Image::MakeFromFile(imageNode->filePath);
      }
    }
    if (image && !isBackendTexture) {
      image = image->makeMipmapped(true);
    }
    // Only memoize successful results. A null entry would cache the absence of a provider-
    // resolved image forever, so a later provider update would never take effect after
    // invalidation.
    if (image) {
      _imageCache[imageNode] = image;
    }
    return image;
  }

  std::shared_ptr<tgfx::TrimPath> convertTrimPath(const TrimPath* node) {
    auto trim = tgfx::TrimPath::Make();
    trim->setStart(node->start);
    trim->setEnd(node->end);
    trim->setOffset(node->offset);
    if (node->type == TrimType::Continuous) {
      trim->setTrimType(tgfx::TrimPathType::Continuous);
    }
    _result.binding.set(node, trim);
    _result.binding.setWriter(
        node, "start",
        WriteMixedFloat<tgfx::TrimPath, &tgfx::TrimPath::start, &tgfx::TrimPath::setStart>);
    _result.binding.setWriter(
        node, "end",
        WriteMixedFloat<tgfx::TrimPath, &tgfx::TrimPath::end, &tgfx::TrimPath::setEnd>);
    _result.binding.setWriter(
        node, "offset",
        WriteMixedFloat<tgfx::TrimPath, &tgfx::TrimPath::offset, &tgfx::TrimPath::setOffset>);
    return trim;
  }

  std::shared_ptr<tgfx::TextPath> convertTextPath(const TextPath* node) {
    auto textPath = tgfx::TextPath::Make();
    if (node->path != nullptr) {
      auto path = getScaledPath(node->path, node->renderScale());
      auto pos = node->renderPosition();
      if (pos.x != 0 || pos.y != 0) {
        path.transform(tgfx::Matrix::MakeTrans(pos.x, pos.y));
      }
      textPath->setPath(std::move(path));
    }
    textPath->setBaselineOrigin(ToTGFX(node->renderBaselineOrigin()));
    textPath->setBaselineAngle(node->baselineAngle);
    textPath->setFirstMargin(node->firstMargin);
    textPath->setLastMargin(node->lastMargin);
    textPath->setPerpendicular(node->perpendicular);
    textPath->setReversed(node->reversed);
    textPath->setForceAlignment(node->forceAlignment);
    return textPath;
  }

  std::shared_ptr<tgfx::RoundCorner> convertRoundCorner(const RoundCorner* node) {
    auto round = tgfx::RoundCorner::Make();
    round->setRadius(node->radius);
    _result.binding.set(node, round);
    _result.binding.setWriter(node, "radius",
                              WriteMixedFloat<tgfx::RoundCorner, &tgfx::RoundCorner::radius,
                                              &tgfx::RoundCorner::setRadius>);
    return round;
  }

  std::shared_ptr<tgfx::MergePath> convertMergePath(const MergePath* node) {
    auto merge = tgfx::MergePath::Make();
    if (node->mode != MergePathMode::Append) {
      merge->setMode(ToTGFX(node->mode));
    }
    return merge;
  }

  std::shared_ptr<tgfx::Repeater> convertRepeater(const Repeater* node) {
    auto repeater = tgfx::Repeater::Make();
    repeater->setCopies(node->copies);
    repeater->setOffset(node->offset);
    repeater->setOrder(ToTGFX(node->order));
    repeater->setAnchor(ToTGFX(node->anchor));
    repeater->setPosition(ToTGFX(node->position));
    repeater->setRotation(node->rotation);
    repeater->setScale(ToTGFX(node->scale));
    repeater->setStartAlpha(node->startAlpha);
    repeater->setEndAlpha(node->endAlpha);
    _result.binding.set(node, repeater);
    _result.binding.setWriter(
        node, "copies",
        WriteMixedFloat<tgfx::Repeater, &tgfx::Repeater::copies, &tgfx::Repeater::setCopies>);
    _result.binding.setWriter(
        node, "offset",
        WriteMixedFloat<tgfx::Repeater, &tgfx::Repeater::offset, &tgfx::Repeater::setOffset>);
    _result.binding.setWriter(
        node, "rotation",
        WriteMixedFloat<tgfx::Repeater, &tgfx::Repeater::rotation, &tgfx::Repeater::setRotation>);
    _result.binding.setWriter(node, "startAlpha",
                              WriteMixedFloat<tgfx::Repeater, &tgfx::Repeater::startAlpha,
                                              &tgfx::Repeater::setStartAlpha>);
    _result.binding.setWriter(
        node, "endAlpha",
        WriteMixedFloat<tgfx::Repeater, &tgfx::Repeater::endAlpha, &tgfx::Repeater::setEndAlpha>);
    _result.binding.setWriter(
        node, "anchor.x",
        WritePointAxis<tgfx::Repeater, &tgfx::Repeater::anchor, &tgfx::Repeater::setAnchor, true>);
    _result.binding.setWriter(
        node, "anchor.y",
        WritePointAxis<tgfx::Repeater, &tgfx::Repeater::anchor, &tgfx::Repeater::setAnchor, false>);
    _result.binding.setWriter(node, "position.x",
                              WritePointAxis<tgfx::Repeater, &tgfx::Repeater::position,
                                             &tgfx::Repeater::setPosition, true>);
    _result.binding.setWriter(node, "position.y",
                              WritePointAxis<tgfx::Repeater, &tgfx::Repeater::position,
                                             &tgfx::Repeater::setPosition, false>);
    _result.binding.setWriter(
        node, "scale.x",
        WritePointAxis<tgfx::Repeater, &tgfx::Repeater::scale, &tgfx::Repeater::setScale, true>);
    _result.binding.setWriter(
        node, "scale.y",
        WritePointAxis<tgfx::Repeater, &tgfx::Repeater::scale, &tgfx::Repeater::setScale, false>);
    return repeater;
  }

  // TextModifier optional paint channels: animating sets the optional to the mixed value, blending
  // against the current value when present and against the target alone when unset.
  static void WriteTextModifierFillColor(void* object, const KeyValue& value, float mix) {
    const auto* v = std::get_if<Color>(&value);
    if (v == nullptr) {
      return;
    }
    auto* modifier = static_cast<tgfx::TextModifier*>(object);
    auto target = ToTGFX(*v);
    auto current = modifier->fillColor();
    modifier->setFillColor(current.has_value() ? MixTGFXColor(*current, target, mix) : target);
  }

  static void WriteTextModifierStrokeColor(void* object, const KeyValue& value, float mix) {
    const auto* v = std::get_if<Color>(&value);
    if (v == nullptr) {
      return;
    }
    auto* modifier = static_cast<tgfx::TextModifier*>(object);
    auto target = ToTGFX(*v);
    auto current = modifier->strokeColor();
    modifier->setStrokeColor(current.has_value() ? MixTGFXColor(*current, target, mix) : target);
  }

  static void WriteTextModifierStrokeWidth(void* object, const KeyValue& value, float mix) {
    const auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* modifier = static_cast<tgfx::TextModifier*>(object);
    auto current = modifier->strokeWidth();
    modifier->setStrokeWidth(current.has_value() ? MixFloat(*current, *v, mix) : *v);
  }

  std::shared_ptr<tgfx::TextModifier> convertTextModifier(const TextModifier* node) {
    auto modifier = tgfx::TextModifier::Make();

    // Convert transform properties
    modifier->setAnchor(ToTGFX(node->anchor));
    modifier->setPosition(ToTGFX(node->position));
    modifier->setRotation(node->rotation);
    modifier->setScale(ToTGFX(node->scale));
    modifier->setSkew(node->skew);
    modifier->setSkewAxis(node->skewAxis);
    modifier->setAlpha(node->alpha);

    // Convert paint properties
    if (node->fillColor.has_value()) {
      modifier->setFillColor(ToTGFX(node->fillColor.value()));
    }
    if (node->strokeColor.has_value()) {
      modifier->setStrokeColor(ToTGFX(node->strokeColor.value()));
    }
    if (node->strokeWidth.has_value()) {
      modifier->setStrokeWidth(node->strokeWidth.value());
    }

    _result.binding.set(node, modifier);
    _result.binding.setWriter(node, "rotation",
                              WriteMixedFloat<tgfx::TextModifier, &tgfx::TextModifier::rotation,
                                              &tgfx::TextModifier::setRotation>);
    _result.binding.setWriter(node, "skew",
                              WriteMixedFloat<tgfx::TextModifier, &tgfx::TextModifier::skew,
                                              &tgfx::TextModifier::setSkew>);
    _result.binding.setWriter(node, "skewAxis",
                              WriteMixedFloat<tgfx::TextModifier, &tgfx::TextModifier::skewAxis,
                                              &tgfx::TextModifier::setSkewAxis>);
    _result.binding.setWriter(node, "alpha",
                              WriteMixedFloat<tgfx::TextModifier, &tgfx::TextModifier::alpha,
                                              &tgfx::TextModifier::setAlpha>);
    _result.binding.setWriter(node, "anchor.x",
                              WritePointAxis<tgfx::TextModifier, &tgfx::TextModifier::anchor,
                                             &tgfx::TextModifier::setAnchor, true>);
    _result.binding.setWriter(node, "anchor.y",
                              WritePointAxis<tgfx::TextModifier, &tgfx::TextModifier::anchor,
                                             &tgfx::TextModifier::setAnchor, false>);
    _result.binding.setWriter(node, "position.x",
                              WritePointAxis<tgfx::TextModifier, &tgfx::TextModifier::position,
                                             &tgfx::TextModifier::setPosition, true>);
    _result.binding.setWriter(node, "position.y",
                              WritePointAxis<tgfx::TextModifier, &tgfx::TextModifier::position,
                                             &tgfx::TextModifier::setPosition, false>);
    _result.binding.setWriter(node, "scale.x",
                              WritePointAxis<tgfx::TextModifier, &tgfx::TextModifier::scale,
                                             &tgfx::TextModifier::setScale, true>);
    _result.binding.setWriter(node, "scale.y",
                              WritePointAxis<tgfx::TextModifier, &tgfx::TextModifier::scale,
                                             &tgfx::TextModifier::setScale, false>);
    _result.binding.setWriter(node, "fillColor", WriteTextModifierFillColor);
    _result.binding.setWriter(node, "strokeColor", WriteTextModifierStrokeColor);
    _result.binding.setWriter(node, "strokeWidth", WriteTextModifierStrokeWidth);

    // Convert selectors
    std::vector<std::shared_ptr<tgfx::TextSelector>> tgfxSelectors;
    tgfxSelectors.reserve(node->selectors.size());
    for (const auto* selector : node->selectors) {
      if (selector->nodeType() == NodeType::RangeSelector) {
        auto rangeSelector = static_cast<const RangeSelector*>(selector);
        auto tgfxSelector = std::make_shared<tgfx::RangeSelector>();
        tgfxSelector->setStart(rangeSelector->start);
        tgfxSelector->setEnd(rangeSelector->end);
        tgfxSelector->setOffset(rangeSelector->offset);
        tgfxSelector->setUnit(ToTGFX(rangeSelector->unit));
        tgfxSelector->setShape(ToTGFX(rangeSelector->shape));
        tgfxSelector->setEaseIn(rangeSelector->easeIn);
        tgfxSelector->setEaseOut(rangeSelector->easeOut);
        tgfxSelector->setMode(ToTGFX(rangeSelector->mode));
        tgfxSelector->setWeight(rangeSelector->weight);
        tgfxSelector->setRandomOrder(rangeSelector->randomOrder);
        tgfxSelector->setRandomSeed(static_cast<uint16_t>(rangeSelector->randomSeed));
        _result.binding.set(rangeSelector, tgfxSelector);
        _result.binding.setWriter(rangeSelector, "start",
                                  WriteMixedFloat<tgfx::RangeSelector, &tgfx::RangeSelector::start,
                                                  &tgfx::RangeSelector::setStart>);
        _result.binding.setWriter(rangeSelector, "end",
                                  WriteMixedFloat<tgfx::RangeSelector, &tgfx::RangeSelector::end,
                                                  &tgfx::RangeSelector::setEnd>);
        _result.binding.setWriter(rangeSelector, "offset",
                                  WriteMixedFloat<tgfx::RangeSelector, &tgfx::RangeSelector::offset,
                                                  &tgfx::RangeSelector::setOffset>);
        _result.binding.setWriter(rangeSelector, "easeIn",
                                  WriteMixedFloat<tgfx::RangeSelector, &tgfx::RangeSelector::easeIn,
                                                  &tgfx::RangeSelector::setEaseIn>);
        _result.binding.setWriter(
            rangeSelector, "easeOut",
            WriteMixedFloat<tgfx::RangeSelector, &tgfx::RangeSelector::easeOut,
                            &tgfx::RangeSelector::setEaseOut>);
        _result.binding.setWriter(rangeSelector, "weight",
                                  WriteMixedFloat<tgfx::RangeSelector, &tgfx::RangeSelector::weight,
                                                  &tgfx::RangeSelector::setWeight>);
        tgfxSelectors.push_back(tgfxSelector);
      }
    }
    modifier->setSelectors(std::move(tgfxSelectors));

    return modifier;
  }

  std::shared_ptr<tgfx::VectorGroup> convertGroup(const Group* node) {
    auto group = tgfx::VectorGroup::Make();
    std::vector<std::shared_ptr<tgfx::VectorElement>> elements;
    elements.reserve(node->elements.size());

    for (const auto& element : node->elements) {
      // Skip empty TextBox modifier (no children) - layout has been baked into GlyphRun positions
      // by TextLayout. TextBox with children is rendered as a Group.
      if (element->nodeType() == NodeType::TextBox) {
        auto* textBox = static_cast<const TextBox*>(element);
        if (textBox->elements.empty()) {
          continue;
        }
      }

      auto tgfxElement = convertVectorElement(element);
      if (tgfxElement) {
        elements.push_back(tgfxElement);
      }
    }

    group->setElements(std::move(elements));

    // Apply transform properties
    if (node->anchor.x != 0 || node->anchor.y != 0) {
      group->setAnchor(ToTGFX(node->anchor));
    }
    auto renderPos = node->renderPosition();
    if (renderPos.x != 0 || renderPos.y != 0) {
      group->setPosition(ToTGFX(renderPos));
    }
    if (node->scale.x != 1 || node->scale.y != 1) {
      group->setScale(ToTGFX(node->scale));
    }
    if (node->rotation != 0) {
      group->setRotation(node->rotation);
    }
    if (node->alpha != 1) {
      group->setAlpha(node->alpha);
    }
    if (node->skew != 0) {
      group->setSkew(node->skew);
    }
    if (node->skewAxis != 0) {
      group->setSkewAxis(node->skewAxis);
    }

    // Register transform channels. Group and TextBox share these; the node pointer is the source
    // node passed in (TextBox passes itself), so animation targets resolve to the right binding.
    _result.binding.set(node, group);
    _result.binding.setWriter(node, "rotation",
                              WriteMixedFloat<tgfx::VectorGroup, &tgfx::VectorGroup::rotation,
                                              &tgfx::VectorGroup::setRotation>);
    _result.binding.setWriter(node, "alpha",
                              WriteMixedFloat<tgfx::VectorGroup, &tgfx::VectorGroup::alpha,
                                              &tgfx::VectorGroup::setAlpha>);
    _result.binding.setWriter(
        node, "skew",
        WriteMixedFloat<tgfx::VectorGroup, &tgfx::VectorGroup::skew, &tgfx::VectorGroup::setSkew>);
    _result.binding.setWriter(node, "skewAxis",
                              WriteMixedFloat<tgfx::VectorGroup, &tgfx::VectorGroup::skewAxis,
                                              &tgfx::VectorGroup::setSkewAxis>);
    _result.binding.setWriter(node, "anchor.x",
                              WritePointAxis<tgfx::VectorGroup, &tgfx::VectorGroup::anchor,
                                             &tgfx::VectorGroup::setAnchor, true>);
    _result.binding.setWriter(node, "anchor.y",
                              WritePointAxis<tgfx::VectorGroup, &tgfx::VectorGroup::anchor,
                                             &tgfx::VectorGroup::setAnchor, false>);
    _result.binding.setWriter(node, "position.x",
                              WritePointAxis<tgfx::VectorGroup, &tgfx::VectorGroup::position,
                                             &tgfx::VectorGroup::setPosition, true>);
    _result.binding.setWriter(node, "position.y",
                              WritePointAxis<tgfx::VectorGroup, &tgfx::VectorGroup::position,
                                             &tgfx::VectorGroup::setPosition, false>);
    _result.binding.setWriter(node, "scale.x",
                              WritePointAxis<tgfx::VectorGroup, &tgfx::VectorGroup::scale,
                                             &tgfx::VectorGroup::setScale, true>);
    _result.binding.setWriter(node, "scale.y",
                              WritePointAxis<tgfx::VectorGroup, &tgfx::VectorGroup::scale,
                                             &tgfx::VectorGroup::setScale, false>);

    return group;
  }

  void applyLayerAttributes(const Layer* node, tgfx::Layer* layer) {
    layer->setVisible(node->visible);
    layer->setAlpha(node->alpha);
    if (node->blendMode != BlendMode::Normal) {
      layer->setBlendMode(ToTGFX(node->blendMode));
    }

    // Apply transformation: combine x/y translation with matrix
    auto matrix = ToTGFX(node->matrix);
    auto layerPos = node->renderPosition();
    if (layerPos.x != 0 || layerPos.y != 0) {
      matrix = tgfx::Matrix::MakeTrans(layerPos.x, layerPos.y) * matrix;
    }
    if (!matrix.isIdentity()) {
      layer->setMatrix(matrix);
    }

    // Apply 3D matrix if present (overrides 2D matrix)
    if (!node->matrix3D.isIdentity()) {
      layer->setMatrix3D(ToTGFX3D(node->matrix3D));
    }
    if (node->preserve3D) {
      layer->setPreserve3D(true);
    }

    // Apply layer rendering attributes
    if (!node->antiAlias) {
      layer->setAllowsEdgeAntialiasing(false);
    }
    layer->setAllowsGroupOpacity(node->groupOpacity);
    if (!node->passThroughBackground) {
      layer->setPassThroughBackground(false);
    }
    if (node->hasScrollRect) {
      layer->setScrollRect(ToTGFX(node->scrollRect));
    }

    // Layer styles
    std::vector<std::shared_ptr<tgfx::LayerStyle>> styles;
    styles.reserve(node->styles.size());
    for (const auto& style : node->styles) {
      auto tgfxStyle = convertLayerStyle(style);
      if (tgfxStyle) {
        if (style->excludeChildEffects) {
          tgfxStyle->setExcludeChildEffects(true);
        }
        styles.push_back(tgfxStyle);
      }
    }
    if (!styles.empty()) {
      layer->setLayerStyles(styles);
    }

    // Layer filters
#if !PAG_DISABLE_LAYER_FILTERS
    std::vector<std::shared_ptr<tgfx::LayerFilter>> filters;
    filters.reserve(node->filters.size());
    for (const auto& filter : node->filters) {
      auto tgfxFilter = convertLayerFilter(filter);
      if (tgfxFilter) {
        filters.push_back(tgfxFilter);
      }
    }
    if (!filters.empty()) {
      layer->setFilters(filters);
    }
#endif  // !PAG_DISABLE_LAYER_FILTERS
  }

  std::shared_ptr<tgfx::LayerStyle> convertLayerStyle(const LayerStyle* node) {
    if (!node) {
      return nullptr;
    }

    switch (node->nodeType()) {
      case NodeType::DropShadowStyle: {
#if PAG_DISABLE_SHADOW_STYLES
        return nullptr;
#else
        auto style = static_cast<const DropShadowStyle*>(node);
        auto tgfxStyle =
            tgfx::DropShadowStyle::Make(style->offsetX, style->offsetY, style->blurX, style->blurY,
                                        ToTGFX(style->color), style->showBehindLayer);
        if (node->blendMode != BlendMode::Normal) {
          tgfxStyle->setBlendMode(ToTGFX(node->blendMode));
        }
        _result.binding.set(style, tgfxStyle);
        bindDropShadowStyleChannels(style);
        return tgfxStyle;
#endif
      }
      case NodeType::InnerShadowStyle: {
#if PAG_DISABLE_SHADOW_STYLES
        return nullptr;
#else
        auto style = static_cast<const InnerShadowStyle*>(node);
        auto tgfxStyle = tgfx::InnerShadowStyle::Make(style->offsetX, style->offsetY, style->blurX,
                                                      style->blurY, ToTGFX(style->color));
        if (node->blendMode != BlendMode::Normal) {
          tgfxStyle->setBlendMode(ToTGFX(node->blendMode));
        }
        _result.binding.set(style, tgfxStyle);
        bindInnerShadowStyleChannels(style);
        return tgfxStyle;
#endif
      }
      case NodeType::BackgroundBlurStyle: {
#if PAG_DISABLE_BACKGROUND_BLUR_STYLE
        return nullptr;
#else
        auto style = static_cast<const pagx::BackgroundBlurStyle*>(node);
        auto tgfxStyle =
            tgfx::BackgroundBlurStyle::Make(style->blurX, style->blurY, ToTGFX(style->tileMode));
        if (node->blendMode != BlendMode::Normal) {
          tgfxStyle->setBlendMode(ToTGFX(node->blendMode));
        }
        _result.binding.set(style, tgfxStyle);
        bindBackgroundBlurStyleChannels(style);
        return tgfxStyle;
#endif
      }
      case NodeType::NoiseStyle: {
        auto style = static_cast<const pagx::NoiseStyle*>(node);
        if (style->size <= 0.0f) {
          return nullptr;
        }
        std::shared_ptr<tgfx::NoiseStyle> tgfxStyle;
        switch (style->mode) {
          case NoiseMode::Mono:
            tgfxStyle = tgfx::NoiseStyle::MakeMono(style->size, style->density,
                                                   ToTGFX(style->color), style->seed);
            break;
          case NoiseMode::Duo:
            tgfxStyle =
                tgfx::NoiseStyle::MakeDuo(style->size, style->density, ToTGFX(style->firstColor),
                                          ToTGFX(style->secondColor), style->seed);
            break;
          case NoiseMode::Multi:
            tgfxStyle = tgfx::NoiseStyle::MakeMulti(style->size, style->density, style->opacity,
                                                    style->seed);
            break;
        }
        if (tgfxStyle && node->blendMode != BlendMode::Normal) {
          tgfxStyle->setBlendMode(ToTGFX(node->blendMode));
        }
        _result.binding.set(style, tgfxStyle);
        bindNoiseStyleChannels(style);
        return tgfxStyle;
      }
      default:
        return nullptr;
    }
  }

  static void WriteDropShadowStyleOffsetX(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* style = static_cast<tgfx::DropShadowStyle*>(object);
    style->setOffsetX(MixFloat(style->offsetX(), *v, mix));
  }

  static void WriteDropShadowStyleOffsetY(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* style = static_cast<tgfx::DropShadowStyle*>(object);
    style->setOffsetY(MixFloat(style->offsetY(), *v, mix));
  }

  static void WriteDropShadowStyleBlurX(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* style = static_cast<tgfx::DropShadowStyle*>(object);
    style->setBlurrinessX(MixFloat(style->blurrinessX(), *v, mix));
  }

  static void WriteDropShadowStyleBlurY(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* style = static_cast<tgfx::DropShadowStyle*>(object);
    style->setBlurrinessY(MixFloat(style->blurrinessY(), *v, mix));
  }

  static void WriteDropShadowStyleColor(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<Color>(&value);
    if (v == nullptr) {
      return;
    }
    auto* style = static_cast<tgfx::DropShadowStyle*>(object);
    auto target = ToTGFX(*v);
    style->setColor(MixTGFXColor(style->color(), target, mix));
  }

  static void WriteDropShadowStyleShowBehindLayer(void* object, const KeyValue& value, float) {
    auto* v = std::get_if<bool>(&value);
    if (v == nullptr) {
      return;
    }
    static_cast<tgfx::DropShadowStyle*>(object)->setShowBehindLayer(*v);
  }

  void bindDropShadowStyleChannels(const DropShadowStyle* node) {
    _result.binding.setWriter(node, "offsetX", WriteDropShadowStyleOffsetX);
    _result.binding.setWriter(node, "offsetY", WriteDropShadowStyleOffsetY);
    _result.binding.setWriter(node, "blurX", WriteDropShadowStyleBlurX);
    _result.binding.setWriter(node, "blurY", WriteDropShadowStyleBlurY);
    _result.binding.setWriter(node, "color", WriteDropShadowStyleColor);
    _result.binding.setWriter(node, "showBehindLayer", WriteDropShadowStyleShowBehindLayer);
  }

  static void WriteInnerShadowStyleOffsetX(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* style = static_cast<tgfx::InnerShadowStyle*>(object);
    style->setOffsetX(MixFloat(style->offsetX(), *v, mix));
  }

  static void WriteInnerShadowStyleOffsetY(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* style = static_cast<tgfx::InnerShadowStyle*>(object);
    style->setOffsetY(MixFloat(style->offsetY(), *v, mix));
  }

  static void WriteInnerShadowStyleBlurX(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* style = static_cast<tgfx::InnerShadowStyle*>(object);
    style->setBlurrinessX(MixFloat(style->blurrinessX(), *v, mix));
  }

  static void WriteInnerShadowStyleBlurY(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* style = static_cast<tgfx::InnerShadowStyle*>(object);
    style->setBlurrinessY(MixFloat(style->blurrinessY(), *v, mix));
  }

  static void WriteInnerShadowStyleColor(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<Color>(&value);
    if (v == nullptr) {
      return;
    }
    auto* style = static_cast<tgfx::InnerShadowStyle*>(object);
    auto target = ToTGFX(*v);
    style->setColor(MixTGFXColor(style->color(), target, mix));
  }

  void bindInnerShadowStyleChannels(const InnerShadowStyle* node) {
    _result.binding.setWriter(node, "offsetX", WriteInnerShadowStyleOffsetX);
    _result.binding.setWriter(node, "offsetY", WriteInnerShadowStyleOffsetY);
    _result.binding.setWriter(node, "blurX", WriteInnerShadowStyleBlurX);
    _result.binding.setWriter(node, "blurY", WriteInnerShadowStyleBlurY);
    _result.binding.setWriter(node, "color", WriteInnerShadowStyleColor);
  }

  static void WriteBackgroundBlurStyleBlurX(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* style = static_cast<tgfx::BackgroundBlurStyle*>(object);
    style->setBlurrinessX(MixFloat(style->blurrinessX(), *v, mix));
  }

  static void WriteBackgroundBlurStyleBlurY(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* style = static_cast<tgfx::BackgroundBlurStyle*>(object);
    style->setBlurrinessY(MixFloat(style->blurrinessY(), *v, mix));
  }

  void bindBackgroundBlurStyleChannels(const BackgroundBlurStyle* node) {
    _result.binding.setWriter(node, "blurX", WriteBackgroundBlurStyleBlurX);
    _result.binding.setWriter(node, "blurY", WriteBackgroundBlurStyleBlurY);
  }

  static void WriteNoiseStyleSize(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* style = static_cast<tgfx::NoiseStyle*>(object);
    style->setSize(MixFloat(style->size(), *v, mix));
  }

  static void WriteNoiseStyleDensity(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* style = static_cast<tgfx::NoiseStyle*>(object);
    style->setDensity(MixFloat(style->density(), *v, mix));
  }

  static void WriteNoiseStyleSeed(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* style = static_cast<tgfx::NoiseStyle*>(object);
    style->setSeed(MixFloat(style->seed(), *v, mix));
  }

  static void WriteNoiseStyleColor(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<Color>(&value);
    if (v == nullptr) {
      return;
    }
    auto* style = static_cast<tgfx::MonoNoiseStyle*>(object);
    auto target = ToTGFX(*v);
    style->setColor(MixTGFXColor(style->color(), target, mix));
  }

  static void WriteNoiseStyleFirstColor(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<Color>(&value);
    if (v == nullptr) {
      return;
    }
    auto* style = static_cast<tgfx::DuoNoiseStyle*>(object);
    auto target = ToTGFX(*v);
    style->setFirstColor(MixTGFXColor(style->firstColor(), target, mix));
  }

  static void WriteNoiseStyleSecondColor(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<Color>(&value);
    if (v == nullptr) {
      return;
    }
    auto* style = static_cast<tgfx::DuoNoiseStyle*>(object);
    auto target = ToTGFX(*v);
    style->setSecondColor(MixTGFXColor(style->secondColor(), target, mix));
  }

  static void WriteNoiseStyleOpacity(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* style = static_cast<tgfx::MultiNoiseStyle*>(object);
    style->setOpacity(MixFloat(style->opacity(), *v, mix));
  }

  void bindNoiseStyleChannels(const pagx::NoiseStyle* node) {
    _result.binding.setWriter(node, "size", WriteNoiseStyleSize);
    _result.binding.setWriter(node, "density", WriteNoiseStyleDensity);
    _result.binding.setWriter(node, "seed", WriteNoiseStyleSeed);
    switch (node->mode) {
      case NoiseMode::Mono:
        _result.binding.setWriter(node, "color", WriteNoiseStyleColor);
        break;
      case NoiseMode::Duo:
        _result.binding.setWriter(node, "firstColor", WriteNoiseStyleFirstColor);
        _result.binding.setWriter(node, "secondColor", WriteNoiseStyleSecondColor);
        break;
      case NoiseMode::Multi:
        _result.binding.setWriter(node, "opacity", WriteNoiseStyleOpacity);
        break;
    }
  }

  static void WriteBlurFilterX(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* filter = static_cast<tgfx::BlurFilter*>(object);
    filter->setBlurrinessX(MixFloat(filter->blurrinessX(), *v, mix));
  }

  static void WriteBlurFilterY(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* filter = static_cast<tgfx::BlurFilter*>(object);
    filter->setBlurrinessY(MixFloat(filter->blurrinessY(), *v, mix));
  }

  void bindBlurFilterChannels(const BlurFilter* node) {
    _result.binding.setWriter(node, "blurX", WriteBlurFilterX);
    _result.binding.setWriter(node, "blurY", WriteBlurFilterY);
  }

  static void WriteDropShadowFilterOffsetX(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* filter = static_cast<tgfx::DropShadowFilter*>(object);
    filter->setOffsetX(MixFloat(filter->offsetX(), *v, mix));
  }

  static void WriteDropShadowFilterOffsetY(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* filter = static_cast<tgfx::DropShadowFilter*>(object);
    filter->setOffsetY(MixFloat(filter->offsetY(), *v, mix));
  }

  static void WriteDropShadowFilterBlurX(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* filter = static_cast<tgfx::DropShadowFilter*>(object);
    filter->setBlurrinessX(MixFloat(filter->blurrinessX(), *v, mix));
  }

  static void WriteDropShadowFilterBlurY(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* filter = static_cast<tgfx::DropShadowFilter*>(object);
    filter->setBlurrinessY(MixFloat(filter->blurrinessY(), *v, mix));
  }

  static void WriteDropShadowFilterColor(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<Color>(&value);
    if (v == nullptr) {
      return;
    }
    auto* filter = static_cast<tgfx::DropShadowFilter*>(object);
    auto target = ToTGFX(*v);
    filter->setColor(MixTGFXColor(filter->color(), target, mix));
  }

  static void WriteDropShadowFilterShadowOnly(void* object, const KeyValue& value, float) {
    auto* v = std::get_if<bool>(&value);
    if (v == nullptr) {
      return;
    }
    static_cast<tgfx::DropShadowFilter*>(object)->setDropsShadowOnly(*v);
  }

  void bindDropShadowFilterChannels(const DropShadowFilter* node) {
    _result.binding.setWriter(node, "offsetX", WriteDropShadowFilterOffsetX);
    _result.binding.setWriter(node, "offsetY", WriteDropShadowFilterOffsetY);
    _result.binding.setWriter(node, "blurX", WriteDropShadowFilterBlurX);
    _result.binding.setWriter(node, "blurY", WriteDropShadowFilterBlurY);
    _result.binding.setWriter(node, "color", WriteDropShadowFilterColor);
    _result.binding.setWriter(node, "shadowOnly", WriteDropShadowFilterShadowOnly);
  }

  static void WriteInnerShadowFilterOffsetX(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* filter = static_cast<tgfx::InnerShadowFilter*>(object);
    filter->setOffsetX(MixFloat(filter->offsetX(), *v, mix));
  }

  static void WriteInnerShadowFilterOffsetY(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* filter = static_cast<tgfx::InnerShadowFilter*>(object);
    filter->setOffsetY(MixFloat(filter->offsetY(), *v, mix));
  }

  static void WriteInnerShadowFilterBlurX(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* filter = static_cast<tgfx::InnerShadowFilter*>(object);
    filter->setBlurrinessX(MixFloat(filter->blurrinessX(), *v, mix));
  }

  static void WriteInnerShadowFilterBlurY(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* filter = static_cast<tgfx::InnerShadowFilter*>(object);
    filter->setBlurrinessY(MixFloat(filter->blurrinessY(), *v, mix));
  }

  static void WriteInnerShadowFilterColor(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<Color>(&value);
    if (v == nullptr) {
      return;
    }
    auto* filter = static_cast<tgfx::InnerShadowFilter*>(object);
    auto target = ToTGFX(*v);
    filter->setColor(MixTGFXColor(filter->color(), target, mix));
  }

  static void WriteInnerShadowFilterShadowOnly(void* object, const KeyValue& value, float) {
    auto* v = std::get_if<bool>(&value);
    if (v == nullptr) {
      return;
    }
    static_cast<tgfx::InnerShadowFilter*>(object)->setInnerShadowOnly(*v);
  }

  void bindInnerShadowFilterChannels(const InnerShadowFilter* node) {
    _result.binding.setWriter(node, "offsetX", WriteInnerShadowFilterOffsetX);
    _result.binding.setWriter(node, "offsetY", WriteInnerShadowFilterOffsetY);
    _result.binding.setWriter(node, "blurX", WriteInnerShadowFilterBlurX);
    _result.binding.setWriter(node, "blurY", WriteInnerShadowFilterBlurY);
    _result.binding.setWriter(node, "color", WriteInnerShadowFilterColor);
    _result.binding.setWriter(node, "shadowOnly", WriteInnerShadowFilterShadowOnly);
  }

  static void WriteBlendFilterColor(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<Color>(&value);
    if (v == nullptr) {
      return;
    }
    auto* filter = static_cast<tgfx::BlendFilter*>(object);
    auto target = ToTGFX(*v);
    filter->setColor(MixTGFXColor(filter->color(), target, mix));
  }

  void bindBlendFilterChannels(const BlendFilter* node) {
    _result.binding.setWriter(node, "color", WriteBlendFilterColor);
  }

  static void WriteNoiseFilterSize(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* filter = static_cast<tgfx::NoiseFilter*>(object);
    filter->setSize(MixFloat(filter->size(), *v, mix));
  }

  static void WriteNoiseFilterDensity(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* filter = static_cast<tgfx::NoiseFilter*>(object);
    filter->setDensity(MixFloat(filter->density(), *v, mix));
  }

  static void WriteNoiseFilterSeed(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* filter = static_cast<tgfx::NoiseFilter*>(object);
    filter->setSeed(MixFloat(filter->seed(), *v, mix));
  }

  static void WriteNoiseFilterColor(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<Color>(&value);
    if (v == nullptr) {
      return;
    }
    auto* filter = static_cast<tgfx::MonoNoiseFilter*>(object);
    auto target = ToTGFX(*v);
    filter->setColor(MixTGFXColor(filter->color(), target, mix));
  }

  static void WriteNoiseFilterFirstColor(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<Color>(&value);
    if (v == nullptr) {
      return;
    }
    auto* filter = static_cast<tgfx::DuoNoiseFilter*>(object);
    auto target = ToTGFX(*v);
    filter->setFirstColor(MixTGFXColor(filter->firstColor(), target, mix));
  }

  static void WriteNoiseFilterSecondColor(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<Color>(&value);
    if (v == nullptr) {
      return;
    }
    auto* filter = static_cast<tgfx::DuoNoiseFilter*>(object);
    auto target = ToTGFX(*v);
    filter->setSecondColor(MixTGFXColor(filter->secondColor(), target, mix));
  }

  static void WriteNoiseFilterOpacity(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* filter = static_cast<tgfx::MultiNoiseFilter*>(object);
    filter->setOpacity(MixFloat(filter->opacity(), *v, mix));
  }

  void bindNoiseFilterChannels(const pagx::NoiseFilter* node) {
    _result.binding.setWriter(node, "size", WriteNoiseFilterSize);
    _result.binding.setWriter(node, "density", WriteNoiseFilterDensity);
    _result.binding.setWriter(node, "seed", WriteNoiseFilterSeed);
    switch (node->mode) {
      case NoiseMode::Mono:
        _result.binding.setWriter(node, "color", WriteNoiseFilterColor);
        break;
      case NoiseMode::Duo:
        _result.binding.setWriter(node, "firstColor", WriteNoiseFilterFirstColor);
        _result.binding.setWriter(node, "secondColor", WriteNoiseFilterSecondColor);
        break;
      case NoiseMode::Multi:
        _result.binding.setWriter(node, "opacity", WriteNoiseFilterOpacity);
        break;
    }
  }

  std::shared_ptr<tgfx::LayerFilter> convertLayerFilter(const LayerFilter* node) {
    if (!node) {
      return nullptr;
    }

    switch (node->nodeType()) {
      case NodeType::BlurFilter: {
        auto filter = static_cast<const pagx::BlurFilter*>(node);
        auto tgfxFilter =
            tgfx::BlurFilter::Make(filter->blurX, filter->blurY, ToTGFX(filter->tileMode));
        _result.binding.set(filter, tgfxFilter);
        bindBlurFilterChannels(filter);
        return tgfxFilter;
      }
      case NodeType::DropShadowFilter: {
        auto filter = static_cast<const DropShadowFilter*>(node);
        auto tgfxFilter =
            tgfx::DropShadowFilter::Make(filter->offsetX, filter->offsetY, filter->blurX,
                                         filter->blurY, ToTGFX(filter->color), filter->shadowOnly);
        _result.binding.set(filter, tgfxFilter);
        bindDropShadowFilterChannels(filter);
        return tgfxFilter;
      }
      case NodeType::InnerShadowFilter: {
        auto filter = static_cast<const pagx::InnerShadowFilter*>(node);
        auto tgfxFilter =
            tgfx::InnerShadowFilter::Make(filter->offsetX, filter->offsetY, filter->blurX,
                                          filter->blurY, ToTGFX(filter->color), filter->shadowOnly);
        _result.binding.set(filter, tgfxFilter);
        bindInnerShadowFilterChannels(filter);
        return tgfxFilter;
      }
      case NodeType::BlendFilter: {
        auto filter = static_cast<const pagx::BlendFilter*>(node);
        auto tgfxFilter = tgfx::BlendFilter::Make(ToTGFX(filter->color), ToTGFX(filter->blendMode));
        _result.binding.set(filter, tgfxFilter);
        bindBlendFilterChannels(filter);
        return tgfxFilter;
      }
      case NodeType::ColorMatrixFilter: {
        auto filter = static_cast<const pagx::ColorMatrixFilter*>(node);
        return tgfx::ColorMatrixFilter::Make(filter->matrix);
      }
      case NodeType::NoiseFilter: {
        auto filter = static_cast<const pagx::NoiseFilter*>(node);
        if (filter->size <= 0.0f) {
          return nullptr;
        }
        auto tgfxBlendMode = ToTGFX(filter->blendMode);
        std::shared_ptr<tgfx::NoiseFilter> tgfxFilter;
        switch (filter->mode) {
          case NoiseMode::Mono:
            tgfxFilter = tgfx::NoiseFilter::MakeMono(
                filter->size, filter->density, ToTGFX(filter->color), filter->seed, tgfxBlendMode);
            break;
          case NoiseMode::Duo:
            tgfxFilter = tgfx::NoiseFilter::MakeDuo(
                filter->size, filter->density, ToTGFX(filter->firstColor),
                ToTGFX(filter->secondColor), filter->seed, tgfxBlendMode);
            break;
          case NoiseMode::Multi:
            tgfxFilter = tgfx::NoiseFilter::MakeMulti(filter->size, filter->density,
                                                      filter->opacity, filter->seed, tgfxBlendMode);
            break;
        }
        _result.binding.set(filter, tgfxFilter);
        bindNoiseFilterChannels(filter);
        return tgfxFilter;
      }
      default:
        return nullptr;
    }
  }

  std::unordered_map<const Layer*, std::vector<std::shared_ptr<tgfx::Layer>>>
      _tgfxLayersByPagxLayer = {};
  struct PathCacheKey {
    const PathData* data;
    float scale;
    bool operator==(const PathCacheKey& other) const {
      return data == other.data && scale == other.scale;
    }
  };
  struct PathCacheHash {
    size_t operator()(const PathCacheKey& key) const {
      auto h1 = std::hash<const void*>{}(key.data);
      auto h2 = std::hash<float>{}(key.scale);
      return h1 ^ (h2 << 1);
    }
  };
  std::unordered_map<PathCacheKey, tgfx::Path, PathCacheHash> _scaledPathCache = {};
  LayerBuildResult _result = {};
  std::vector<std::tuple<std::shared_ptr<tgfx::Layer>, const Layer*, tgfx::LayerMaskType>>
      _pendingMasks = {};
  // Maps pagx Image node pointers to their tgfx::Image wrappers. The raw pointer keys are stable
  // as long as the document's node vector is not structurally modified. If the document undergoes
  // structural changes (node additions/removals via notifyChange), callers must call
  // invalidateAllImages() to prevent dangling-pointer lookups.
  const PAGXDocument* _document = nullptr;
  ImageResourceProvider* _imageProvider = nullptr;
  std::unordered_map<const Image*, std::shared_ptr<tgfx::Image>> _imageCache = {};
  // TextBlob fingerprint cache. Keyed by FNV-1a 64-bit hash over every BuildTextBlob input.
  // glyphSum acts as a secondary check guarding against the (vanishingly rare) hash collision.
  // Sharing the same TextBlob shared_ptr across many Text nodes is what reclaims the bulk of
  // the per-Text glyph-path memory cost we observed at 0% reuse before the cache was added.
  struct TextBlobCacheEntry {
    size_t glyphSum = 0;
    std::shared_ptr<tgfx::TextBlob> textBlob;
    std::vector<tgfx::Point> anchors;
  };
  std::unordered_map<uint64_t, TextBlobCacheEntry> _textBlobCache = {};
  bool _needsRuntimeData = false;
};

// Public API implementation

std::shared_ptr<tgfx::Layer> LayerBuilder::Build(PAGXDocument* document) {
  if (document == nullptr) {
    return nullptr;
  }
  if (!document->isLayoutApplied()) {
    LOGE("LayerBuilder::Build() called before applyLayout(). Call document->applyLayout() first.");
    DEBUG_ASSERT(false);
    return nullptr;
  }

  LayerBuilderContext context;
  context.setImageProvider(document->imageResourceProvider());
  return context.build(*document);
}

LayerBuildResult LayerBuilder::BuildWithMap(PAGXDocument* document) {
  if (document == nullptr) {
    return {};
  }
  if (!document->isLayoutApplied()) {
    LOGE(
        "LayerBuilder::BuildWithMap() called before applyLayout(). Call document->applyLayout() "
        "first.");
    DEBUG_ASSERT(false);
    return {};
  }

  LayerBuilderContext context;
  context.setImageProvider(document->imageResourceProvider());
  return context.buildWithMap(*document);
}

// LayerBuilderSession PImpl: owns the LayerBuilderContext and a non-owning pointer back to the
// PAGXDocument so rebuildForFilePath() can re-enumerate Image nodes and affected layers.
// Lifetime guarantee: PAGXView always destroys builderSession before document (both in
// parsePAGX() and in the destructor's member destruction order), so this pointer never dangles.
struct LayerBuilderSession::Impl {
  LayerBuilderContext context;
  PAGXDocument* document = nullptr;
};

LayerBuilderSession::LayerBuilderSession() : impl(std::make_unique<Impl>()) {
}

LayerBuilderSession::~LayerBuilderSession() = default;

LayerBuildResult LayerBuilderSession::build(PAGXDocument* document) {
  if (document == nullptr) {
    return {};
  }
  if (!document->isLayoutApplied()) {
    LOGE(
        "LayerBuilderSession::build() called before applyLayout(). Call "
        "document->applyLayout() first.");
    DEBUG_ASSERT(false);
    return {};
  }
  impl->document = document;
  impl->context.setImageProvider(document->imageResourceProvider());
  return impl->context.buildWithMap(*document);
}

void LayerBuilderSession::invalidateAllImages() {
  impl->context.invalidateAllImages();
}

size_t LayerBuilderSession::rebuildForFilePath(const std::string& filePath) {
  if (!impl->document || filePath.empty()) {
    return 0;
  }
  // Evict the cached tgfx::Image for every Image node backing this filePath so the next call
  // into convertImagePattern() re-queries the provider for the updated image.
  impl->context.invalidateImagesByFilePath(*impl->document, filePath);

  const auto& affectedLayers = impl->document->findLayersByImageFilePath(filePath);
  if (affectedLayers.empty()) {
    return 0;
  }
  size_t rebuiltCount = 0;
  for (const auto* pagxLayer : affectedLayers) {
    auto tgfxLayers = impl->context.getTgfxLayers(pagxLayer);
    for (const auto& tgfxLayer : tgfxLayers) {
      if (impl->context.rebuildVectorContents(pagxLayer, tgfxLayer.get())) {
        ++rebuiltCount;
      }
    }
  }
  return rebuiltCount;
}

std::vector<std::shared_ptr<tgfx::Layer>> LayerBuilderSession::getTgfxLayersByImageFilePath(
    const std::string& filePath) const {
  if (!impl->document || filePath.empty()) {
    return {};
  }
  const auto& affectedLayers = impl->document->findLayersByImageFilePath(filePath);
  if (affectedLayers.empty()) {
    return {};
  }
  std::vector<std::shared_ptr<tgfx::Layer>> result;
  for (const auto* pagxLayer : affectedLayers) {
    auto tgfxLayers = impl->context.getTgfxLayers(pagxLayer);
    result.insert(result.end(), tgfxLayers.begin(), tgfxLayers.end());
  }
  return result;
}

std::vector<std::shared_ptr<tgfx::Layer>> LayerBuilderSession::getTgfxLayers(
    const Layer* pagxLayer) const {
  return impl->context.getTgfxLayers(pagxLayer);
}

LayerBuildResult LayerBuilder::BuildForRuntime(PAGXDocument* document) {
  if (document == nullptr) {
    return {};
  }
  if (!document->isLayoutApplied()) {
    LOGE(
        "LayerBuilder::BuildForRuntime() called before applyLayout(). Call "
        "document->applyLayout() first.");
    DEBUG_ASSERT(false);
    return {};
  }
  LayerBuilderContext context;
  context.setNeedsRuntimeData(true);
  context.setImageProvider(document->imageResourceProvider());
  return context.buildWithMap(*document);
}

LayerBuildResult LayerBuilder::BuildCompositionSubtree(const Composition* composition) {
  if (composition == nullptr) {
    return {};
  }
  LayerBuilderContext context;
  // Slot's recursive children build their own subtrees independently.
  context.setNeedsRuntimeData(true);
  return context.buildSubtree(composition);
}

bool LayerBuilder::RefreshLayerInPlace(const Layer* node, RuntimeBinding* binding,
                                       const PAGXDocument* document) {
  if (node == nullptr || binding == nullptr) {
    return false;
  }
  LayerBuilderContext context;
  context.setNeedsRuntimeData(true);
  context.setDocument(document);
  if (document) {
    context.setImageProvider(document->imageResourceProvider());
  }
  return context.refreshLayerInPlace(node, binding);
}

std::shared_ptr<tgfx::Layer> LayerBuilder::BuildLayerInto(const Layer* node,
                                                          RuntimeBinding* binding,
                                                          const PAGXDocument* document) {
  if (node == nullptr || binding == nullptr) {
    return nullptr;
  }
  LayerBuilderContext context;
  context.setNeedsRuntimeData(true);
  context.setDocument(document);
  if (document) {
    context.setImageProvider(document->imageResourceProvider());
  }
  return context.buildLayerInto(node, binding);
}

}  // namespace pagx
