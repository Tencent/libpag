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
#include "pagx/nodes/ColorStop.h"
#include "pagx/runtime/ChannelRegistry.h"
#include "pagx/runtime/MixUtils.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/layers/vectors/Gradient.h"

namespace pagx {

// Resolves the parent tgfx::Gradient and stop index for the ColorStop in ctx.targetNode.
// Returns nullptr if the stop is not present in the runtime layer tree.
static tgfx::Gradient* GradientForStop(const AnimContext& ctx, size_t* outIndex) {
  if (ctx.layerTree == nullptr || ctx.targetNode == nullptr) {
    return nullptr;
  }
  auto* stop = static_cast<const ColorStop*>(ctx.targetNode);
  auto stopIt = ctx.layerTree->stopMap.find(stop);
  if (stopIt == ctx.layerTree->stopMap.end()) {
    return nullptr;
  }
  auto gradIt = ctx.layerTree->gradientMap.find(stopIt->second.first);
  if (gradIt == ctx.layerTree->gradientMap.end()) {
    return nullptr;
  }
  *outIndex = stopIt->second.second;
  return gradIt->second.get();
}

void RegisterColorStopChannels(ChannelRegistry::Registrar& reg) {
  reg.add({NodeType::ColorStop, "color", ChannelValueType::Color,
           [](const AnimContext& ctx, const KeyValue& v) {
             size_t index = 0;
             auto* gradient = GradientForStop(ctx, &index);
             if (gradient == nullptr) {
               return;
             }
             auto colors = gradient->colors();
             if (index >= colors.size()) {
               return;
             }
             auto target = ToTGFX(std::get<Color>(v));
             colors[index] = MixTGFXColor(colors[index], target, ctx.mix);
             gradient->setColors(std::move(colors));
           }});

  reg.add({NodeType::ColorStop, "offset", ChannelValueType::Float,
           [](const AnimContext& ctx, const KeyValue& v) {
             size_t index = 0;
             auto* gradient = GradientForStop(ctx, &index);
             if (gradient == nullptr) {
               return;
             }
             auto positions = gradient->positions();
             if (index >= positions.size()) {
               return;
             }
             auto target = std::get<float>(v);
             positions[index] = MixFloat(positions[index], target, ctx.mix);
             gradient->setPositions(std::move(positions));
           }});
}

}  // namespace pagx
