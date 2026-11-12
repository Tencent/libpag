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
#include "pagx/nodes/PathData.h"
#include "tgfx/core/Path.h"

namespace pagx {

PathData PathDataFromSVGString(const std::string& d);

std::string PathDataToSVGString(const PathData& path);

/**
 * Serializes a tgfx::Path to SVG path data with the same full-float precision as
 * PathDataToSVGString. Used by exporters that bake path effects (e.g. adaptive dash) before
 * emitting the geometry.
 */
std::string TGFXPathToSVGString(const tgfx::Path& path);

}  // namespace pagx
