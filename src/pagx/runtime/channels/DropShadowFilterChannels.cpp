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
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/runtime/ChannelRegistry.h"
#include "pagx/runtime/MixUtils.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/layers/filters/DropShadowFilter.h"

namespace pagx {

static tgfx::DropShadowFilter* TgfxDropShadowFilterFor(const AnimContext& ctx) {
  if (ctx.layerTree == nullptr || ctx.targetNode == nullptr) {
    return nullptr;
  }
  auto* node = static_cast<const DropShadowFilter*>(ctx.targetNode);
  auto it = ctx.layerTree->dropShadowFilterMap.find(node);
  return it == ctx.layerTree->dropShadowFilterMap.end() ? nullptr : it->second.get();
}

void RegisterDropShadowFilterChannels(ChannelRegistry::Registrar& reg) {
  reg.add({NodeType::DropShadowFilter, "offsetX", ChannelValueType::Float,
           [](const AnimContext& ctx, const KeyValue& v) {
             auto* f = TgfxDropShadowFilterFor(ctx);
             if (f == nullptr) {
               return;
             }
             f->setOffsetX(MixFloat(f->offsetX(), std::get<float>(v), ctx.mix));
           }});

  reg.add({NodeType::DropShadowFilter, "offsetY", ChannelValueType::Float,
           [](const AnimContext& ctx, const KeyValue& v) {
             auto* f = TgfxDropShadowFilterFor(ctx);
             if (f == nullptr) {
               return;
             }
             f->setOffsetY(MixFloat(f->offsetY(), std::get<float>(v), ctx.mix));
           }});

  reg.add({NodeType::DropShadowFilter, "blurX", ChannelValueType::Float,
           [](const AnimContext& ctx, const KeyValue& v) {
             auto* f = TgfxDropShadowFilterFor(ctx);
             if (f == nullptr) {
               return;
             }
             f->setBlurrinessX(MixFloat(f->blurrinessX(), std::get<float>(v), ctx.mix));
           }});

  reg.add({NodeType::DropShadowFilter, "blurY", ChannelValueType::Float,
           [](const AnimContext& ctx, const KeyValue& v) {
             auto* f = TgfxDropShadowFilterFor(ctx);
             if (f == nullptr) {
               return;
             }
             f->setBlurrinessY(MixFloat(f->blurrinessY(), std::get<float>(v), ctx.mix));
           }});

  reg.add({NodeType::DropShadowFilter, "color", ChannelValueType::Color,
           [](const AnimContext& ctx, const KeyValue& v) {
             auto* f = TgfxDropShadowFilterFor(ctx);
             if (f == nullptr) {
               return;
             }
             auto target = ToTGFX(std::get<Color>(v));
             f->setColor(MixTGFXColor(f->color(), target, ctx.mix));
           }});

  reg.add({NodeType::DropShadowFilter, "shadowOnly", ChannelValueType::Bool,
           [](const AnimContext& ctx, const KeyValue& v) {
             auto* f = TgfxDropShadowFilterFor(ctx);
             if (f == nullptr) {
               return;
             }
             f->setDropsShadowOnly(std::get<bool>(v));
           }});
}

}  // namespace pagx
