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

#include <string>

namespace pagx {

class PPTWriterContext;

std::string GenerateContentTypes(const PPTWriterContext& ctx);
std::string GenerateRootRels();
std::string GeneratePresentation(float w, float h);
std::string GeneratePresentationRels();
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
std::string GenerateAppProps();

}  // namespace pagx
