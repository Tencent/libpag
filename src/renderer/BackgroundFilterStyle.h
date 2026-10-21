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

#pragma once
#include <memory>
#include <vector>
#include "tgfx/layers/filters/LayerFilter.h"
#include "tgfx/layers/layerstyles/LayerStyle.h"

namespace pagx {
// Uses TGFX's existing background capture/root/transform machinery. Child filter
// properties are attached to the same owners so animation and mutation invalidate it.
class BackdropStyle : public tgfx::LayerStyle {
 public:
  explicit BackdropStyle(std::vector<std::shared_ptr<tgfx::LayerFilter>> filters, tgfx::Path clip);
  tgfx::LayerStylePosition position() const override {
    return tgfx::LayerStylePosition::Below;
  }
  tgfx::Rect filterBounds(const tgfx::Rect& bounds, float scale) override;
  tgfx::Rect filterSourceBounds(const tgfx::Rect& bounds, float scale) override;
  uint32_t extraSourceType() const override;

 protected:
  void attachToLayer(tgfx::Layer* layer) override;
  void detachFromLayer(tgfx::Layer* layer) override;
  tgfx::Rect filterBackgroundSharp(const tgfx::Rect& bounds, float scale) override;
  void onDraw(tgfx::Canvas* canvas, const tgfx::LayerStyleInput& input, float alpha,
              tgfx::BlendMode mode) override;

 private:
  std::vector<std::shared_ptr<tgfx::LayerFilter>> filters;
  tgfx::Path clip;
};
}  // namespace pagx
