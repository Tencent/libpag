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

#include <string>
#include <vector>
#include "pagx/PAGXDocument.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/layers/Layer.h"

namespace pagx::cli {

/**
 * Holds a pagx::Layer, its corresponding tgfx::Layer, and the global bounds computed from the
 * layer tree root. Used by align and distribute commands.
 */
struct LayerInfo {
  Layer* pagxLayer = nullptr;
  tgfx::Layer* tgfxLayer = nullptr;
  tgfx::Rect globalBounds = {};
};

/**
 * Selects Layers from a PAGXDocument by a combination of --id values and an --xpath expression.
 * Returns a deduplicated list of matching Layers. Prints errors to stderr and returns an empty
 * vector on failure.
 */
std::vector<Layer*> SelectLayers(const PAGXDocument* document, const std::string& inputFile,
                                 const std::vector<std::string>& ids, const std::string& xpath,
                                 const std::string& commandName);

/**
 * Resolves selected Layers to LayerInfo entries by looking up each Layer in the build result's
 * layer map and computing global bounds. Layers not found in the map are skipped with a warning.
 */
std::vector<LayerInfo> ResolveLayerInfos(const std::vector<Layer*>& targets,
                                         const LayerBuildResult& buildResult,
                                         const std::string& commandName);

/**
 * Applies a global-coordinate offset to a pagx::Layer by converting the offset into the Layer's
 * parent coordinate space. This handles cross-hierarchy alignment correctly by using the tgfx
 * Layer's parent globalToLocal conversion.
 */
void ApplyGlobalOffset(Layer* pagxLayer, tgfx::Layer* tgfxLayer, float deltaGlobalX,
                       float deltaGlobalY);

/**
 * Returns a display label for a Layer (id, name, or "(unnamed)").
 */
std::string GetLayerLabel(const Layer* layer);

/**
 * Compares two LayerInfo entries by their global bounds left edge for sorting along the X axis.
 */
bool CompareByPositionX(const LayerInfo& a, const LayerInfo& b);

/**
 * Compares two LayerInfo entries by their global bounds top edge for sorting along the Y axis.
 */
bool CompareByPositionY(const LayerInfo& a, const LayerInfo& b);

}  // namespace pagx::cli
