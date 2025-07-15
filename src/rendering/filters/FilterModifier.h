/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "base/utils/UniqueID.h"
#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/graphics/Modifier.h"

namespace pag {
class FilterModifier : public Modifier {
 public:
  /**
   * Returns nullptr if trackMatteLayer is nullptr.
   */
  static std::shared_ptr<FilterModifier> Make(PAGLayer* pagLayer);

  /**
   * Returns nullptr if trackMatteLayer is nullptr.
   */
  static std::shared_ptr<FilterModifier> Make(Layer* layer, Frame layerFrame);

  ID type() const override {
    static const auto TypeID = UniqueID::Next();
    return TypeID;
  }

  bool isEmpty() const override {
    return false;
  }

  bool hitTest(RenderCache*, float, float) const override {
    return true;
  }

  void prepare(RenderCache*) const override;

  void applyToBounds(tgfx::Rect* bounds) const override;

  bool applyToPath(tgfx::Path*) const override {
    return false;
  }

  void applyToGraphic(Canvas* canvas, std::shared_ptr<Graphic> graphic) const override;

  std::shared_ptr<Modifier> mergeWith(const Modifier*) const override {
    return nullptr;
  }

  Layer* layer = nullptr;
  Frame layerFrame = 0;
};
}  // namespace pag
