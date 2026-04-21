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
#include "pagx/PAGXDocument.h"
#include "pagx/types/Rect.h"
#include "pagx/utils/ExporterUtils.h"
#include "renderer/LayerBuilder.h"

namespace tgfx {
class Data;
}

namespace pagx {

/**
 * PPTRasterizer wraps a GPUContext and a lazily-built tgfx layer tree to provide PNG fallback
 * rendering for individual PAGX layers. It is used when the layer (or its modifiers/effects/blend
 * mode) cannot be expressed as editable OOXML markup.
 *
 * The rasterizer keeps a cache of the document's tgfx::Layer build so masks, blend escalation,
 * and modifier fallback all share one expensive build.
 */
class PPTRasterizer {
 public:
  explicit PPTRasterizer(PAGXDocument* doc, int dpi = 192);

  /**
   * Returns the tgfx::Layer corresponding to the given PAGX Layer, or nullptr if the build hasn't
   * mapped it (e.g. invisible layers, composition instances).
   */
  std::shared_ptr<tgfx::Layer> getMappedLayer(const Layer* layer);

  /**
   * Rasterizes the entire layer subtree (with its mask, filters, and styles applied) into a PNG
   * data blob. The pixel bounds of the resulting image are returned via outBounds in document
   * coordinate space.
   *
   * Returns nullptr if the layer is not mapped, has zero bounds, or rendering fails.
   */
  std::shared_ptr<tgfx::Data> renderLayer(const Layer* layer, Rect* outBounds);

  /**
   * Returns the configured DPI.
   */
  int dpi() const {
    return _dpi;
  }

 private:
  PAGXDocument* _doc;
  int _dpi;
  GPUContext _gpu;
  LayerBuildResult _buildResult;
  bool _buildResultReady = false;

  const LayerBuildResult& ensureBuild();

  // Pulls the tgfx::Layer counterpart out of the build map, returning nullptr
  // when the layer has no mapping (invisible / composition instance / etc.).
  static std::shared_ptr<tgfx::Layer> lookupMappedLayer(const LayerBuildResult& build,
                                                        const Layer* layer);
};

}  // namespace pagx
