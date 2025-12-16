/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "pag/c/pag_composition.h"
#include "pag_types_priv.h"

int pag_composition_get_width(pag_composition* composition) {
  auto p = ToPAGComposition(composition);
  if (p == nullptr) {
    return 0;
  }
  return p->width();
}

int pag_composition_get_height(pag_composition* composition) {
  auto p = ToPAGComposition(composition);
  if (p == nullptr) {
    return 0;
  }
  return p->height();
}

pag_layer** pag_composition_get_layers_by_name(pag_composition* composition, const char* layerName,
                                               size_t* count) {
  auto p = ToPAGComposition(composition);
  if (p == nullptr) {
    return nullptr;
  }
  auto pagLayers = p->getLayersByName(layerName);
  if (pagLayers.empty()) {
    return nullptr;
  }
  auto** layers = static_cast<pag_layer**>(malloc(sizeof(pag_layer*) * pagLayers.size()));
  if (layers == nullptr) {
    return nullptr;
  }
  for (size_t i = 0; i < pagLayers.size(); ++i) {
    layers[i] = new pag_layer(std::move(pagLayers[i]));
  }
  *count = pagLayers.size();
  return layers;
}

static void PAGCompositionFindLayers(const std::function<bool(pag::PAGLayer* pagLayer)>& filterFunc,
                                     std::vector<std::shared_ptr<pag::PAGLayer>>* result,
                                     std::shared_ptr<pag::PAGLayer> pagLayer) {
  if (filterFunc(pagLayer.get())) {
    result->push_back(pagLayer);
  }
  if (pagLayer->trackMatteLayer()) {
    PAGCompositionFindLayers(filterFunc, result, pagLayer->trackMatteLayer());
  }
  if (pagLayer->layerType() == pag::LayerType::PreCompose) {
    auto* composition = static_cast<pag::PAGComposition*>(pagLayer.get());
    auto count = composition->numChildren();
    for (int i = 0; i < count; ++i) {
      auto childLayer = composition->getLayerAt(i);
      PAGCompositionFindLayers(filterFunc, result, childLayer);
    }
  }
}

static std::vector<std::shared_ptr<pag::PAGLayer>> PAGCompositionGetLayersBy(
    const std::function<bool(pag::PAGLayer* pagLayer)>& filterFunc,
    std::shared_ptr<pag::PAGLayer> pagLayer) {
  std::vector<std::shared_ptr<pag::PAGLayer>> result = {};
  PAGCompositionFindLayers(filterFunc, &result, pagLayer);
  return result;
}

pag_layer** pag_composition_get_layers_by_type(pag_composition* composition,
                                               pag_layer_type layerType, size_t* count) {
  auto p = ToPAGComposition(composition);
  if (p == nullptr) {
    return nullptr;
  }

  pag::LayerType pagLayerType;
  FromCLayerType(layerType, &pagLayerType);

  auto pagLayers = PAGCompositionGetLayersBy(
      [=](pag::PAGLayer* pagLayer) -> bool { return pagLayer->layerType() == pagLayerType; }, p);
  if (pagLayers.empty()) {
    return nullptr;
  }
  auto** layers = static_cast<pag_layer**>(malloc(sizeof(pag_layer*) * pagLayers.size()));
  if (layers == nullptr) {
    return nullptr;
  }
  for (size_t i = 0; i < pagLayers.size(); ++i) {
    layers[i] = new pag_layer(std::move(pagLayers[i]));
  }
  *count = pagLayers.size();
  return layers;
}
