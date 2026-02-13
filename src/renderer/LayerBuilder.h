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
#include "pagx/PAGXDocument.h"
#include "Typesetter.h"
#include "tgfx/layers/Layer.h"

namespace pagx {

/**
 * LayerBuilder converts PAGXDocument to tgfx::Layer tree for rendering.
 * Text elements are rendered using the Typesetter to create ShapedText.
 */
class LayerBuilder {
 public:
  /**
   * Builds a layer tree from a PAGXDocument.
   * @param document The document to build from.
   * @param typesetter Optional typesetter for text rendering. If nullptr, a default Typesetter is
   *                   created internally. Pass a custom Typesetter to use registered typefaces
   *                   and fallback fonts.
   * @return The root layer of the built layer tree.
   */
  static std::shared_ptr<tgfx::Layer> Build(PAGXDocument* document,
                                            Typesetter* typesetter = nullptr);
};

}  // namespace pagx
