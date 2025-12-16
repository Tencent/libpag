/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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

#include "LayerHelper.h"
#include <cmath>
#include "ScopedHelper.h"

namespace exporter {

void TraversalLayers(std::shared_ptr<exporter::PAGExportSession> session,
                     pag::Composition* composition, pag::LayerType layerType, pag::Frame startTime,
                     LayerHandlerWithTime handler, void* ctx) {
  if (composition->type() != pag::CompositionType::Vector) {
    return;
  }
  ScopedAssign<pag::ID> compID(session->compID, composition->id);
  for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
    ScopedAssign<pag::ID> layerID(session->layerID, layer->id);
    if (layer->type() == pag::LayerType::PreCompose) {
      auto preComposeLayer = static_cast<pag::PreComposeLayer*>(layer);
      TraversalLayers(session, preComposeLayer->composition, layerType,
                      startTime + preComposeLayer->compositionStartTime, handler, ctx);
    }
    if (layerType == pag::LayerType::Unknown || layer->type() == layerType) {
      handler(session, layer, startTime, ctx);
    }
  }
}

void TraversalLayers(std::shared_ptr<exporter::PAGExportSession> session,
                     pag::Composition* composition, pag::LayerType layerType, LayerHandler handler,
                     void* ctx) {
  if (composition->type() != pag::CompositionType::Vector) {
    return;
  }
  ScopedAssign<pag::ID> arCI(session->compID, composition->id);
  for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
    ScopedAssign<pag::ID> arLI(session->layerID, layer->id);
    if (layer->type() == pag::LayerType::PreCompose) {
      TraversalLayers(session, static_cast<pag::PreComposeLayer*>(layer)->composition, layerType,
                      handler, ctx);
    }
    if (layerType == pag::LayerType::Unknown || layer->type() == layerType) {
      handler(session, layer, ctx);
    }
  }
}

void TraversalLayers(std::shared_ptr<exporter::PAGExportSession> session,
                     std::vector<pag::Composition*>& compositions, pag::LayerType layerType,
                     LayerHandler handler, void* ctx) {
  for (auto composition : compositions) {
    ScopedAssign<pag::ID> compID(session->compID, composition->id);
    if (composition->type() == pag::CompositionType::Vector) {
      for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
        ScopedAssign<pag::ID> layerID(session->layerID, layer->id);
        if (layerType == pag::LayerType::Unknown || layer->type() == layerType) {
          handler(session, layer, ctx);
        }
      }
    }
  }
}

}  // namespace exporter
