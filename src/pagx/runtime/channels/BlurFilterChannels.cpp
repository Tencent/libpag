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

#include "pagx/nodes/BlurFilter.h"
#include "pagx/runtime/ChannelRegistry.h"
#include "pagx/runtime/MixUtils.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/layers/filters/BlurFilter.h"

namespace pagx {

static tgfx::BlurFilter* TgfxBlurFor(const AnimContext& ctx) {
  if (ctx.layerTree == nullptr || ctx.targetNode == nullptr) {
    return nullptr;
  }
  auto* node = static_cast<const BlurFilter*>(ctx.targetNode);
  auto it = ctx.layerTree->blurFilterMap.find(node);
  return it == ctx.layerTree->blurFilterMap.end() ? nullptr : it->second.get();
}

void RegisterBlurFilterChannels(ChannelRegistry::Registrar& reg) {
  reg.add({NodeType::BlurFilter, "blurX", ChannelValueType::Float,
           [](const AnimContext& ctx, const KeyValue& v) {
             auto* filter = TgfxBlurFor(ctx);
             if (filter == nullptr) {
               return;
             }
             filter->setBlurrinessX(MixFloat(filter->blurrinessX(), std::get<float>(v), ctx.mix));
           }});

  reg.add({NodeType::BlurFilter, "blurY", ChannelValueType::Float,
           [](const AnimContext& ctx, const KeyValue& v) {
             auto* filter = TgfxBlurFor(ctx);
             if (filter == nullptr) {
               return;
             }
             filter->setBlurrinessY(MixFloat(filter->blurrinessY(), std::get<float>(v), ctx.mix));
           }});
}

}  // namespace pagx
