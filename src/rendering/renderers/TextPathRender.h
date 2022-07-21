/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "pag/file.h"
#include "rendering/graphics/Glyph.h"

namespace pag {

class TextPathRender {
 public:
  static std::shared_ptr<TextPathRender> MakeFrom(const TextDocument* textDocument,
                                                  const TextPathOptions* pathOptions);

  void applyForceAlignmentToGlyphs(const std::vector<std::vector<GlyphHandle>>& lines,
                                   Frame layerFrame);

  void applyToGlyphs(const std::vector<std::vector<GlyphHandle>>& glyphLines, Frame layerFrame);

 private:
  TextPathRender(const TextDocument* textDocument, const TextPathOptions* pathOptions);

  const TextDocument* textDocument = nullptr;
  const TextPathOptions* pathOptions = nullptr;
};
}  // namespace pag
