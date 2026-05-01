// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 7 StyleFilterBaker — per-subtype bakers for PAGX LayerFilter /
// LayerStyle nodes.
//
// Each subtype baker touches only the fields its wire branch is going to
// emit (mirroring the ShapeStyleData pattern from PaintBaker). The
// remaining LayerFilter / LayerStyle fields stay at PAGDocument defaults
// so the ShapeStyleData / Property isDefault short-circuits still collapse
// them on the wire.
#include "pagx/pag/StyleFilterBaker.h"
#include <memory>
#include <utility>
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlendFilter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorMatrixFilter.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/InnerShadowFilter.h"
#include "pagx/nodes/InnerShadowStyle.h"
#include "pagx/nodes/LayerFilter.h"
#include "pagx/nodes/LayerStyle.h"
#include "pagx/nodes/Node.h"
#include "pagx/pag/BakeContext.h"
#include "pagx/pag/PAGDocument.h"
#include "renderer/ToTGFX.h"

namespace pagx::pag {

namespace {

tgfx::Color ToTgfxColor(const pagx::Color& c) {
  return tgfx::Color{c.red, c.green, c.blue, c.alpha};
}

std::unique_ptr<LayerFilter> BakeBlurFilter(const pagx::BlurFilter& src) {
  auto f = std::make_unique<LayerFilter>();
  f->type = LayerFilterType::Blur;
  f->blurX = MakeProp(src.blurX);
  f->blurY = MakeProp(src.blurY);
  f->tileMode = pagx::ToTGFX(src.tileMode);
  return f;
}

std::unique_ptr<LayerFilter> BakeDropShadowFilter(const pagx::DropShadowFilter& src) {
  auto f = std::make_unique<LayerFilter>();
  f->type = LayerFilterType::DropShadow;
  f->offsetX = MakeProp(src.offsetX);
  f->offsetY = MakeProp(src.offsetY);
  f->blurX = MakeProp(src.blurX);
  f->blurY = MakeProp(src.blurY);
  f->color = MakeProp(ToTgfxColor(src.color));
  f->shadowOnly = src.shadowOnly;
  return f;
}

std::unique_ptr<LayerFilter> BakeInnerShadowFilter(const pagx::InnerShadowFilter& src) {
  auto f = std::make_unique<LayerFilter>();
  f->type = LayerFilterType::InnerShadow;
  f->offsetX = MakeProp(src.offsetX);
  f->offsetY = MakeProp(src.offsetY);
  f->blurX = MakeProp(src.blurX);
  f->blurY = MakeProp(src.blurY);
  f->color = MakeProp(ToTgfxColor(src.color));
  f->shadowOnly = src.shadowOnly;
  return f;
}

std::unique_ptr<LayerFilter> BakeColorMatrixFilter(const pagx::ColorMatrixFilter& src) {
  auto f = std::make_unique<LayerFilter>();
  f->type = LayerFilterType::ColorMatrix;
  f->colorMatrix = MakeProp(src.matrix);
  return f;
}

std::unique_ptr<LayerFilter> BakeBlendFilter(const pagx::BlendFilter& src) {
  auto f = std::make_unique<LayerFilter>();
  f->type = LayerFilterType::Blend;
  f->blendColor = MakeProp(ToTgfxColor(src.color));
  f->blendFilterMode = pagx::ToTGFX(src.blendMode);
  return f;
}

// Common LayerStyle fields (blendMode + excludeChildEffects) are set for
// every variant; the per-subtype bakers layer the variant-specific fields
// on top.
void ApplyCommonStyleFields(const pagx::LayerStyle& src, LayerStyle* out) {
  out->blendMode = pagx::ToTGFX(src.blendMode);
  out->excludeChildEffects = src.excludeChildEffects;
}

std::unique_ptr<LayerStyle> BakeDropShadowStyle(const pagx::DropShadowStyle& src) {
  auto s = std::make_unique<LayerStyle>();
  s->type = LayerStyleType::DropShadow;
  ApplyCommonStyleFields(src, s.get());
  s->offsetX = MakeProp(src.offsetX);
  s->offsetY = MakeProp(src.offsetY);
  s->blurX = MakeProp(src.blurX);
  s->blurY = MakeProp(src.blurY);
  s->color = MakeProp(ToTgfxColor(src.color));
  s->showBehindLayer = src.showBehindLayer;
  return s;
}

std::unique_ptr<LayerStyle> BakeInnerShadowStyle(const pagx::InnerShadowStyle& src) {
  auto s = std::make_unique<LayerStyle>();
  s->type = LayerStyleType::InnerShadow;
  ApplyCommonStyleFields(src, s.get());
  s->offsetX = MakeProp(src.offsetX);
  s->offsetY = MakeProp(src.offsetY);
  s->blurX = MakeProp(src.blurX);
  s->blurY = MakeProp(src.blurY);
  s->color = MakeProp(ToTgfxColor(src.color));
  return s;
}

std::unique_ptr<LayerStyle> BakeBackgroundBlurStyle(const pagx::BackgroundBlurStyle& src) {
  auto s = std::make_unique<LayerStyle>();
  s->type = LayerStyleType::BackgroundBlur;
  ApplyCommonStyleFields(src, s.get());
  s->blurX = MakeProp(src.blurX);
  s->blurY = MakeProp(src.blurY);
  s->tileMode = pagx::ToTGFX(src.tileMode);
  return s;
}

}  // namespace

void BakeLayerFilters(BakeContext& ctx, const std::vector<pagx::LayerFilter*>& src,
                      std::vector<std::unique_ptr<LayerFilter>>* out) {
  out->reserve(src.size());
  for (const auto* node : src) {
    if (node == nullptr) {
      continue;
    }
    std::unique_ptr<LayerFilter> baked;
    switch (node->nodeType()) {
      case pagx::NodeType::BlurFilter:
        baked = BakeBlurFilter(*static_cast<const pagx::BlurFilter*>(node));
        break;
      case pagx::NodeType::DropShadowFilter:
        baked = BakeDropShadowFilter(*static_cast<const pagx::DropShadowFilter*>(node));
        break;
      case pagx::NodeType::InnerShadowFilter:
        baked = BakeInnerShadowFilter(*static_cast<const pagx::InnerShadowFilter*>(node));
        break;
      case pagx::NodeType::ColorMatrixFilter:
        baked = BakeColorMatrixFilter(*static_cast<const pagx::ColorMatrixFilter*>(node));
        break;
      case pagx::NodeType::BlendFilter:
        baked = BakeBlendFilter(*static_cast<const pagx::BlendFilter*>(node));
        break;
      default:
        // Unknown PAGX filter subtype — skip with a warning instead of
        // faking up a default LayerFilter, which would silently drop info.
        ctx.warn(ErrorCode::InvalidEnumValue,
                 "StyleFilterBaker encountered an unknown LayerFilter subtype");
        continue;
    }
    out->push_back(std::move(baked));
  }
}

void BakeLayerStyles(BakeContext& ctx, const std::vector<pagx::LayerStyle*>& src,
                     std::vector<std::unique_ptr<LayerStyle>>* out) {
  out->reserve(src.size());
  for (const auto* node : src) {
    if (node == nullptr) {
      continue;
    }
    std::unique_ptr<LayerStyle> baked;
    switch (node->nodeType()) {
      case pagx::NodeType::DropShadowStyle:
        baked = BakeDropShadowStyle(*static_cast<const pagx::DropShadowStyle*>(node));
        break;
      case pagx::NodeType::InnerShadowStyle:
        baked = BakeInnerShadowStyle(*static_cast<const pagx::InnerShadowStyle*>(node));
        break;
      case pagx::NodeType::BackgroundBlurStyle:
        baked = BakeBackgroundBlurStyle(*static_cast<const pagx::BackgroundBlurStyle*>(node));
        break;
      default:
        ctx.warn(ErrorCode::InvalidEnumValue,
                 "StyleFilterBaker encountered an unknown LayerStyle subtype");
        continue;
    }
    out->push_back(std::move(baked));
  }
}

}  // namespace pagx::pag
