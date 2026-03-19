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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "Layout.h"
#include <cmath>
#include "AutoLayout.h"
#include "TextLayout.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Layer.h"

namespace pagx {

Layout::~Layout() = default;

FontConfig* Layout::getFontProvider() {
  if (externalConfig_ != nullptr) {
    return externalConfig_;
  }
  return fontProvider_.get();
}

void Layout::apply(PAGXDocument* document) {
  if (document == nullptr) {
    return;
  }

  auto* fontConfig = getFontProvider();

  // Perform auto layout (container + constraint) with FontConfig for precise text measurement.
  AutoLayout::Apply(document, fontConfig);

  // Perform text layout (shaping, glyph positioning) with the same FontConfig.
  auto layoutResult = TextLayout::Layout(document, fontConfig);

  // Cache the shaped text map for LayerBuilder
  shapedTextMap_ = std::move(layoutResult.shapedTextMap);
}

const ShapedTextMap& Layout::shapedTextMap() const {
  return shapedTextMap_;
}

}  // namespace pagx
