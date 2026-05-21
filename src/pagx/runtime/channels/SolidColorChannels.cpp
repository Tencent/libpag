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
#include "pagx/nodes/SolidColor.h"
#include "pagx/runtime/ChannelRegistry.h"
#include "pagx/runtime/MixUtils.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/layers/vectors/SolidColor.h"

namespace pagx {

void RegisterSolidColorChannels(ChannelRegistry::Registrar& reg) {
  reg.add({NodeType::SolidColor, "color", ChannelValueType::Color,
           [](const AnimContext& ctx, const KeyValue& v) {
             if (ctx.layerTree == nullptr || ctx.targetNode == nullptr) {
               return;
             }
             auto* node = static_cast<const SolidColor*>(ctx.targetNode);
             auto it = ctx.layerTree->solidMap.find(node);
             if (it == ctx.layerTree->solidMap.end()) {
               return;
             }
             auto target = ToTGFX(std::get<Color>(v));
             it->second->setColor(MixTGFXColor(it->second->color(), target, ctx.mix));
           }});
}

}  // namespace pagx
