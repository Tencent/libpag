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
#include "tgfx/layers/layerstyles/BackgroundBlurStyle.h"
#include "tgfx/layers/layerstyles/DropShadowStyle.h"
#include "tgfx/layers/layerstyles/InnerShadowStyle.h"
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

  std::shared_ptr<tgfx::Layer> build(const PAGXDocument& document) {
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
  // conversion goes through getOrCreateImage() again and picks up the updated decodedImage /
  // data pointer. Returns true when at least one cache entry was cleared.
  bool invalidateImagesByFilePath(const PAGXDocument& document, const std::string& filePath) {
    if (filePath.empty()) {
      return false;
    }
    bool cleared = false;
    for (auto& node : document.nodes) {
      if (!node || node->nodeType() != NodeType::Image) {
        continue;
      }
      auto* imageNode = static_cast<const Image*>(node.get());
      if (imageNode->filePath != filePath) {
        continue;
      }
      if (_imageCache.erase(imageNode) > 0) {
        cleared = true;
      }
    }
    return cleared;
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
      layer = tgfx::Layer::Make();
    }

    if (layer) {
      // Register layer for mask lookups and later rebuildForFilePath(). A single pagx Layer
      // may appear here multiple times when its owning Composition is instanced more than
      // once, so we append rather than overwrite.
      _tgfxLayersByPagxLayer[node].push_back(layer);
      _result.binding.set(node, layer);
      bindLayerChannels(node);

      applyLayerAttributes(node, layer.get());

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

  static void WriteLayerX(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* layer = static_cast<tgfx::Layer*>(object);
    auto matrix = layer->matrix();
    auto mixed = MixFloat(matrix.getTranslateX(), *v, mix);
    matrix.setTranslateX(mixed);
    layer->setMatrix(matrix);
  }

  static void WriteLayerY(void* object, const KeyValue& value, float mix) {
    auto* v = std::get_if<float>(&value);
    if (v == nullptr) {
      return;
    }
    auto* layer = static_cast<tgfx::Layer*>(object);
    auto matrix = layer->matrix();
    auto mixed = MixFloat(matrix.getTranslateY(), *v, mix);
    matrix.setTranslateY(mixed);
    layer->setMatrix(matrix);
  }

  void bindLayerChannels(const Layer* node) {
    _result.binding.setWriter(node, "alpha", WriteLayerAlpha);
    _result.binding.setWriter(node, "visible", WriteLayerVisible);
    _result.binding.setWriter(node, "blendMode", WriteLayerBlendMode);
    _result.binding.setWriter(node, "x", WriteLayerX);
    _result.binding.setWriter(node, "y", WriteLayerY);
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

  std::shared_ptr<tgfx::Rectangle> convertRectangle(const Rectangle* node) {
    auto rect = tgfx::Rectangle::Make();
    rect->setPosition(ToTGFX(node->renderPosition()));
    auto size = node->renderSize();
    rect->setSize({size.width, size.height});
    rect->setRoundness(node->roundness);
    rect->setReversed(node->reversed);
    return rect;
  }

  std::shared_ptr<tgfx::Ellipse> convertEllipse(const Ellipse* node) {
    auto ellipse = tgfx::Ellipse::Make();
    ellipse->setPosition(ToTGFX(node->renderPosition()));
    auto size = node->renderSize();
    ellipse->setSize({size.width, size.height});
    ellipse->setReversed(node->reversed);
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
      // ImagePattern before its decoded image is supplied via loadDecodedImage(); leaving the
      // fill empty keeps those shapes transparent until the image is ready.
      if (node->color && node->color->nodeType() == NodeType::ImagePattern) {
        return nullptr;
      }
      colorSource = tgfx::SolidColor::Make();
    }

    auto fill = tgfx::FillStyle::Make(colorSource);
    if (fill) {
      _result.binding.set(node, fill);
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
      // an ImagePattern before its decoded image is supplied via loadDecodedImage(); leaving the
      // stroke empty keeps those shapes transparent until the image is ready, matching convertFill.
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
    return applyGradientProperties(
        tgfx::Gradient::MakeLinear(ToTGFX(node->startPoint), ToTGFX(node->endPoint), colors,
                                   positions),
        node, node->colorStops);
  }

  std::shared_ptr<tgfx::ColorSource> convertRadialGradient(const RadialGradient* node) {
    std::vector<tgfx::Color> colors;
    std::vector<float> positions;
    extractGradientStops(node->colorStops, &colors, &positions);
    return applyGradientProperties(
        tgfx::Gradient::MakeRadial(ToTGFX(node->center), node->radius, colors, positions), node,
        node->colorStops);
  }

  std::shared_ptr<tgfx::ColorSource> convertConicGradient(const ConicGradient* node) {
    std::vector<tgfx::Color> colors;
    std::vector<float> positions;
    extractGradientStops(node->colorStops, &colors, &positions);
    return applyGradientProperties(tgfx::Gradient::MakeConic(ToTGFX(node->center), node->startAngle,
                                                             node->endAngle, colors, positions),
                                   node, node->colorStops);
  }

  std::shared_ptr<tgfx::ColorSource> convertDiamondGradient(const DiamondGradient* node) {
    std::vector<tgfx::Color> colors;
    std::vector<float> positions;
    extractGradientStops(node->colorStops, &colors, &positions);
    return applyGradientProperties(
        tgfx::Gradient::MakeDiamond(ToTGFX(node->center), node->radius, colors, positions), node,
        node->colorStops);
  }

  std::shared_ptr<tgfx::ColorSource> convertImagePattern(const ImagePattern* node) {
    if (!node || node->image == nullptr) {
      return nullptr;
    }

    auto image = getOrCreateImage(node->image);
    if (!image) {
      // Image data is missing for this node (never loaded or decode failed); paint nothing
      // rather than falling back to an opaque color that would be visibly wrong.
      return nullptr;
    }

    auto sampling = tgfx::SamplingOptions(ToTGFX(node->filterMode), ToTGFX(node->mipmapMode));
    auto pattern =
        tgfx::ImagePattern::Make(image, ToTGFX(node->tileModeX), ToTGFX(node->tileModeY), sampling);
    if (!pattern) {
      return nullptr;
    }
    _result.binding.set(node, pattern);
    pattern->setScaleMode(ToTGFX(node->scaleMode));
    if (!node->matrix.isIdentity()) {
      pattern->setMatrix(ToTGFX(node->matrix));
    }

    return pattern;
  }

  std::shared_ptr<tgfx::Image> getOrCreateImage(const Image* imageNode) {
    auto it = _imageCache.find(imageNode);
    if (it != _imageCache.end()) {
      return it->second;
    }
    // Fallback chain. Earlier sources have priority; the host runtime is responsible for
    // refreshing the chain via PAGXDocument::loadDecodedImage / loadDecodedImageAsThumbnail
    // (and the corresponding clear*) plus rebuildForFilePath when the renderer should pick up
    // a new asset. Sources fall into two camps:
    //   1. Backend-texture images (decodedImage / thumbnailImage): already mipmapped at upload
    //      time by the host's createBackendTexture helper. Re-wrapping with makeMipmapped(true)
    //      here would force tgfx to allocate a parallel mipmapped texture and copy the pixels,
    //      wasting GPU memory. Use as-is.
    //   2. CPU-decoded images (encoded data, file path, data URI): produced lazily by tgfx
    //      codecs. Wrap with makeMipmapped(true) so subsequent sampling at non-1:1 scales does
    //      not re-decode at every zoom level.
    std::shared_ptr<tgfx::Image> image = nullptr;
    bool isAdoptedBackendTexture = false;
    if (imageNode->decodedImage) {
      // Full-quality host-decoded image. Skip all codec paths and reuse directly.
      image = imageNode->decodedImage;
      isAdoptedBackendTexture = true;
    } else if (imageNode->thumbnailImage) {
      // Full-quality counterpart is missing (initial load not complete, or evicted under
      // memory pressure). Fall back to the low-resolution preview so the affected fill area
      // shows a blurry but non-empty image until the full texture arrives.
      image = imageNode->thumbnailImage;
      isAdoptedBackendTexture = true;
    } else if (imageNode->data) {
      image = tgfx::Image::MakeFromEncoded(ToTGFXData(imageNode->data));
    } else if (imageNode->filePath.find("data:") == 0) {
      image = ImageFromDataURI(imageNode->filePath);
    } else if (!imageNode->filePath.empty()) {
      image = tgfx::Image::MakeFromFile(imageNode->filePath);
    }
    if (image && !isAdoptedBackendTexture) {
      image = image->makeMipmapped(true);
    }
    // Only memoize successful results. A null entry would cache the absence of a decoded image
    // forever, so a later loadDecodedImage() on the same node would never take effect.
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
    return repeater;
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
  return context.buildWithMap(*document);
}

// LayerBuilderSession PImpl: owns the LayerBuilderContext and a non-owning pointer back to the
// PAGXDocument so rebuildForFilePath() can re-enumerate Image nodes and affected layers.
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
  return impl->context.buildWithMap(*document);
}

size_t LayerBuilderSession::rebuildForFilePath(const std::string& filePath) {
  if (!impl->document || filePath.empty()) {
    return 0;
  }
  // Evict the cached tgfx::Image for every Image node backing this filePath so the next call
  // into convertImagePattern() picks up the freshly attached decodedImage / data pointer.
  impl->context.invalidateImagesByFilePath(*impl->document, filePath);

  auto affectedLayers = impl->document->findLayersByImageFilePath(filePath);
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

}  // namespace pagx
