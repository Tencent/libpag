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

#include "renderer/ToTGFX.h"
#include "pagx/nodes/Layer.h"
#include "pagx/runtime/ChannelRegistry.h"
#include "pagx/runtime/MixUtils.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/layers/Layer.h"

namespace pagx {

static tgfx::Layer* TgfxLayerFor(const AnimContext& ctx) {
  if (ctx.layerTree == nullptr || ctx.targetNode == nullptr) {
    return nullptr;
  }
  auto* layer = static_cast<const Layer*>(ctx.targetNode);
  auto it = ctx.layerTree->layerMap.find(layer);
  if (it == ctx.layerTree->layerMap.end()) {
    return nullptr;
  }
  return it->second.get();
}

void RegisterLayerChannels(ChannelRegistry::Registrar& reg) {
  reg.add({NodeType::Layer, "alpha", ChannelValueType::Float,
           [](const AnimContext& ctx, const KeyValue& v) {
             auto* layer = TgfxLayerFor(ctx);
             if (layer == nullptr) {
               return;
             }
             auto target = std::get<float>(v);
             layer->setAlpha(MixFloat(layer->alpha(), target, ctx.mix));
           }});

  reg.add({NodeType::Layer, "visible", ChannelValueType::Bool,
           [](const AnimContext& ctx, const KeyValue& v) {
             auto* layer = TgfxLayerFor(ctx);
             if (layer == nullptr) {
               return;
             }
             layer->setVisible(std::get<bool>(v));
           }});

  reg.add({NodeType::Layer, "blendMode", ChannelValueType::Enum,
           [](const AnimContext& ctx, const KeyValue& v) {
             auto* layer = TgfxLayerFor(ctx);
             if (layer == nullptr) {
               return;
             }
             auto mode = static_cast<BlendMode>(std::get<int>(v));
             layer->setBlendMode(ToTGFX(mode));
           }});

  reg.add({NodeType::Layer, "x", ChannelValueType::Float,
           [](const AnimContext& ctx, const KeyValue& v) {
             auto* layer = TgfxLayerFor(ctx);
             if (layer == nullptr) {
               return;
             }
             auto matrix = layer->matrix();
             auto target = std::get<float>(v);
             auto mixed = MixFloat(matrix.getTranslateX(), target, ctx.mix);
             matrix.setTranslateX(mixed);
             layer->setMatrix(matrix);
           }});

  reg.add({NodeType::Layer, "y", ChannelValueType::Float,
           [](const AnimContext& ctx, const KeyValue& v) {
             auto* layer = TgfxLayerFor(ctx);
             if (layer == nullptr) {
               return;
             }
             auto matrix = layer->matrix();
             auto target = std::get<float>(v);
             auto mixed = MixFloat(matrix.getTranslateY(), target, ctx.mix);
             matrix.setTranslateY(mixed);
             layer->setMatrix(matrix);
           }});
}

}  // namespace pagx
