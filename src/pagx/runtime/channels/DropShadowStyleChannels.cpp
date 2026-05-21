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
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/runtime/ChannelRegistry.h"
#include "pagx/runtime/MixUtils.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/layers/layerstyles/DropShadowStyle.h"

namespace pagx {

static tgfx::DropShadowStyle* TgfxDropShadowStyleFor(const AnimContext& ctx) {
  if (ctx.layerTree == nullptr || ctx.targetNode == nullptr) {
    return nullptr;
  }
  auto* node = static_cast<const DropShadowStyle*>(ctx.targetNode);
  auto it = ctx.layerTree->dropShadowStyleMap.find(node);
  return it == ctx.layerTree->dropShadowStyleMap.end() ? nullptr : it->second.get();
}

void RegisterDropShadowStyleChannels(ChannelRegistry::Registrar& reg) {
  reg.add({NodeType::DropShadowStyle, "offsetX", ChannelValueType::Float,
           [](const AnimContext& ctx, const KeyValue& v) {
             auto* s = TgfxDropShadowStyleFor(ctx);
             if (s == nullptr) {
               return;
             }
             s->setOffsetX(MixFloat(s->offsetX(), std::get<float>(v), ctx.mix));
           }});

  reg.add({NodeType::DropShadowStyle, "offsetY", ChannelValueType::Float,
           [](const AnimContext& ctx, const KeyValue& v) {
             auto* s = TgfxDropShadowStyleFor(ctx);
             if (s == nullptr) {
               return;
             }
             s->setOffsetY(MixFloat(s->offsetY(), std::get<float>(v), ctx.mix));
           }});

  reg.add({NodeType::DropShadowStyle, "blurX", ChannelValueType::Float,
           [](const AnimContext& ctx, const KeyValue& v) {
             auto* s = TgfxDropShadowStyleFor(ctx);
             if (s == nullptr) {
               return;
             }
             s->setBlurrinessX(MixFloat(s->blurrinessX(), std::get<float>(v), ctx.mix));
           }});

  reg.add({NodeType::DropShadowStyle, "blurY", ChannelValueType::Float,
           [](const AnimContext& ctx, const KeyValue& v) {
             auto* s = TgfxDropShadowStyleFor(ctx);
             if (s == nullptr) {
               return;
             }
             s->setBlurrinessY(MixFloat(s->blurrinessY(), std::get<float>(v), ctx.mix));
           }});

  reg.add({NodeType::DropShadowStyle, "color", ChannelValueType::Color,
           [](const AnimContext& ctx, const KeyValue& v) {
             auto* s = TgfxDropShadowStyleFor(ctx);
             if (s == nullptr) {
               return;
             }
             auto target = ToTGFX(std::get<Color>(v));
             s->setColor(MixTGFXColor(s->color(), target, ctx.mix));
           }});

  reg.add({NodeType::DropShadowStyle, "showBehindLayer", ChannelValueType::Bool,
           [](const AnimContext& ctx, const KeyValue& v) {
             auto* s = TgfxDropShadowStyleFor(ctx);
             if (s == nullptr) {
               return;
             }
             s->setShowBehindLayer(std::get<bool>(v));
           }});
}

}  // namespace pagx
