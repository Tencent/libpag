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

#include "gpu/FragmentShaderBuilder.h"
#include "tgfx/core/BlendMode.h"

namespace tgfx {
bool BlendAsCoeff(BlendMode blendMode, unsigned* first = nullptr, unsigned* second = nullptr);

// Appends GLSL code to fragment that assigns a specified blend of the srcColor and dstColor
// variables to the outColor variable.
void AppendMode(FragmentShaderBuilder* fsBuilder, const std::string& srcColor,
                const std::string& dstColor, const std::string& outColor, BlendMode blendMode);
}  // namespace tgfx
