/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "PAGFilterUtil.h"
#include "src/utils/CommonMethod.h"

// 从总合成开始，深度遍历每个layer，并对指定类型的layer执行自定义的函数。
void PAGFilterUtil::TraversalLayers(pagexporter::Context& context,
                                    pag::Composition* composition,
                                    pag::LayerType layerType,
                                    ProcessLayerFP processLayerFP,
                                    void* ctx) {
  if (composition->type() != pag::CompositionType::Vector) {
    return;
  }
  AssignRecover<pag::ID> arCI(context.curCompId, composition->id);
  for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
    AssignRecover<pag::ID> arLI(context.curLayerId, layer->id);
    if (layer->type() == pag::LayerType::PreCompose) {
      TraversalLayers(context, static_cast<pag::PreComposeLayer*>(layer)->composition,
                      layerType, processLayerFP, ctx);
    }
    if (layerType == pag::LayerType::Unknown || layer->type() == layerType) {
      processLayerFP(context, layer, ctx);
    }
  }
}

// 从总合成开始，深度遍历每个layer，并对指定类型的layer执行自定义的函数。
void PAGFilterUtil::TraversalLayers(pagexporter::Context& context,
                                    pag::Composition* composition,
                                    pag::LayerType layerType,
                                    pag::Frame compositionStartTime,
                                    ProcessLayerWithTimeFP processLayerWithTimeFP,
                                    void* ctx) {
  if (composition->type() != pag::CompositionType::Vector) {
    return;
  }
  AssignRecover<pag::ID> arCI(context.curCompId, composition->id);
  for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
    AssignRecover<pag::ID> arLI(context.curLayerId, layer->id);
    if (layer->type() == pag::LayerType::PreCompose) {
      auto preComposeLayer = static_cast<pag::PreComposeLayer*>(layer);
      TraversalLayers(context, preComposeLayer->composition, layerType,
                      compositionStartTime + preComposeLayer->compositionStartTime,
                      processLayerWithTimeFP, ctx);
    }
    if (layerType == pag::LayerType::Unknown || layer->type() == layerType) {
      processLayerWithTimeFP(context, layer, compositionStartTime, ctx);
    }
  }
}

// 遍历每个合成的每个layer，并对指定类型的layer执行自定义的函数。
void PAGFilterUtil::TraversalLayers(pagexporter::Context& context,
                                    std::vector<pag::Composition*>& compositions,
                                    pag::LayerType layerType,
                                    ProcessLayerFP processLayerFP,
                                    void* ctx) {
  for (auto composition : compositions) {
    AssignRecover<pag::ID> arCI(context.curCompId, composition->id);
    if (composition->type() == pag::CompositionType::Vector) {
      for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
        AssignRecover<pag::ID> arLI(context.curLayerId, layer->id);
        if (layerType == pag::LayerType::Unknown || layer->type() == layerType) {
          processLayerFP(context, layer, ctx);
        }
      }
    }
  }
}
