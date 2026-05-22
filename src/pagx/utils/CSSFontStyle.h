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

// Resolves a CSS font-weight string and a CSS font-style string into the PAGX `fontStyle`
// attribute value. Supported font-weight inputs are numeric values (1-1000) and the
// keywords `normal`, `bold`, `bolder`, `lighter`. Non-standard numeric weights are
// rounded to the nearest multiple of 100. Regular weight (400) and `normal` produce an
// empty weight portion. Italic and oblique styles are folded onto the result.
//
// Examples:
//   ("900", "italic")  -> "Black Italic"
//   ("700", "")        -> "Bold"
//   ("500", "italic")  -> "Medium Italic"
//   ("400", "")        -> ""
//   ("normal", "italic") -> "Italic"
std::string ResolveFontStyleName(const std::string& cssFontWeight, const std::string& cssFontStyle);

}  // namespace pagx
