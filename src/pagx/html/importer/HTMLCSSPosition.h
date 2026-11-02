/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>

namespace pagx {
class HTMLValueParser;

struct HTMLLengthPercentage {
  float percent = 0;
  float pixels = 0;

  float resolve(float reference) const {
    return percent * reference + pixels;
  }
};

struct HTMLCSSPosition {
  HTMLLengthPercentage x = {0.5f, 0};
  HTMLLengthPercentage y = {0.5f, 0};

  bool isCenter() const {
    return x.percent == 0.5f && y.percent == 0.5f && x.pixels == 0 && y.pixels == 0;
  }
};

// Parses an entire length-percentage, including affine calc() arithmetic. Percentages stay
// unresolved until the positioning area's slack is known, which may be negative.
bool ParseHTMLLengthPercentage(const std::string& value, HTMLValueParser& parser,
                               HTMLLengthPercentage& result);

// Generic <position> accepts one, two or four components. Backgrounds additionally allow three.
// Relative length units must have been resolved by the CSS cascade or the browser snapshot.
bool ParseHTMLCSSPosition(const std::string& value, HTMLValueParser& parser,
                          HTMLCSSPosition& result, bool allowBackgroundSyntax = false);
}  // namespace pagx
