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
#include <unordered_map>
#include <vector>
#include "pagx/PAGXDocument.h"
#include "tgfx/layers/Layer.h"

namespace tgfx {
class BlurFilter;
class DropShadowFilter;
class DropShadowStyle;
class FillStyle;
class Gradient;
class Image;
class ImagePattern;
class SolidColor;
class StrokeStyle;
class Text;
}  // namespace tgfx

namespace pagx {

class BlurFilter;
class ColorStop;
class DropShadowFilter;
class DropShadowStyle;
class Fill;
class Gradient;
class Image;
class ImagePattern;
class SolidColor;
class Stroke;
class Text;

/**
 * Runtime layer tree built from PAGX nodes, containing the root layer and mappings from PAGX nodes
 * to their corresponding tgfx objects.
 */
struct PAGLayerTree {
  std::shared_ptr<tgfx::Layer> root = nullptr;
  std::unordered_map<const Layer*, std::shared_ptr<tgfx::Layer>> layerMap = {};
  std::unordered_map<const SolidColor*, std::shared_ptr<tgfx::SolidColor>> solidMap = {};
  std::unordered_map<const Gradient*, std::shared_ptr<tgfx::Gradient>> gradientMap = {};
  std::unordered_map<const ColorStop*, std::pair<const Gradient*, size_t>> stopMap = {};
  std::unordered_map<const ImagePattern*, std::shared_ptr<tgfx::ImagePattern>> patternMap = {};
  std::unordered_map<const Image*, std::shared_ptr<tgfx::Image>> imageMap = {};
  std::unordered_map<const Text*, std::shared_ptr<tgfx::Text>> textMap = {};
  std::unordered_map<const Fill*, std::shared_ptr<tgfx::FillStyle>> fillMap = {};
  std::unordered_map<const Stroke*, std::shared_ptr<tgfx::StrokeStyle>> strokeMap = {};
  std::unordered_map<const BlurFilter*, std::shared_ptr<tgfx::BlurFilter>> blurFilterMap = {};
  std::unordered_map<const DropShadowFilter*, std::shared_ptr<tgfx::DropShadowFilter>>
      dropShadowFilterMap = {};
  std::unordered_map<const DropShadowStyle*, std::shared_ptr<tgfx::DropShadowStyle>>
      dropShadowStyleMap = {};
};

using LayerBuildResult = PAGLayerTree;

/**
 * LayerBuilder converts PAGXDocument to tgfx::Layer tree for rendering.
 * The document must have applyLayout() called before building.
 */
class LayerBuilder {
 public:
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
   * @return A LayerBuildResult containing the root layer and a mapping from PAGX Layer nodes to
   *         their corresponding tgfx::Layer objects. Returns empty result if document is null or
   *         layout was not applied.
   */
  static LayerBuildResult BuildWithMap(PAGXDocument* document);

  /**
   * Builds a top-level layer tree but stops at Composition slot boundaries — Layers whose
   * `composition` field is non-null produce empty container tgfx layers without recursing into
   * Composition.layers. The runtime PAGFile uses this entry point and then populates each slot
   * with an independent PAGComposition build via BuildCompositionSubtree(), keeping per-slot
   * layerMaps isolated.
   * @param document The document to build from. Must have had applyLayout() called.
   */
  static LayerBuildResult BuildWithSlotsHandedOff(PAGXDocument* document);

  /**
   * Builds a Composition's child layer subtree into a fresh PAGLayerTree. Used by the runtime
   * PAGComposition slot to obtain its own per-slot layerMap, masks, and tgfx layer instances.
   * @param composition The Composition to build. Must reference layers from a document that has
   *                    had applyLayout() called.
   */
  static LayerBuildResult BuildCompositionSubtree(const Composition* composition);
};

}  // namespace pagx
