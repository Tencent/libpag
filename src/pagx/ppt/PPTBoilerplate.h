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

#include <cstddef>
#include <string>

namespace pagx {

class PPTWriterContext;

// `hasPNG` / `hasJPEG` are aggregated across every slide's context so the deck
// declares each media default extension exactly once. `slideCount` controls how
// many `/ppt/slides/slideN.xml` overrides are emitted.
std::string GenerateContentTypes(bool hasPNG, bool hasJPEG, size_t slideCount);
std::string GenerateRootRels();
// `w` / `h` are the deck's slide size (taken from the first document). `slideCount`
// controls the number of <p:sldId> entries in the slide id list.
std::string GeneratePresentation(float w, float h, size_t slideCount);
// Emits the slideMaster relationship followed by one relationship per slide, then
// the presProps / viewProps / theme / tableStyles relationships.
std::string GeneratePresentationRels(size_t slideCount);
std::string GenerateSlideRels(const PPTWriterContext& ctx);
std::string GenerateSlideMaster();
std::string GenerateSlideMasterRels();
std::string GenerateSlideLayout();
std::string GenerateSlideLayoutRels();
std::string GenerateTheme();
std::string GeneratePresProps();
std::string GenerateViewProps();
std::string GenerateTableStyles();
std::string GenerateCoreProps();
std::string GenerateAppProps(size_t slideCount);

}  // namespace pagx
