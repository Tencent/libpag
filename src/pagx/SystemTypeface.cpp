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

#include <cctype>
#include <string>
#include <vector>
#include "SystemFonts.h"
#include "pagx/utils/CSSFontStyle.h"
#include "tgfx/core/Typeface.h"

namespace pagx {

std::string SystemFonts::NormalizeFontName(const std::string& name) {
  std::string result = {};
  result.reserve(name.size());
  bool pendingSpace = false;
  for (unsigned char character : name) {
    if (std::isspace(character)) {
      pendingSpace = !result.empty();
      continue;
    }
    if (pendingSpace) {
      result.push_back(' ');
      pendingSpace = false;
    }
    result.push_back(static_cast<char>(std::tolower(character)));
  }
  return result;
}

bool SystemFonts::FontNamesMatch(const std::string& requested, const std::string& resolved) {
  return !resolved.empty() && NormalizeFontName(requested) == NormalizeFontName(resolved);
}

static bool TypefaceMatches(const std::shared_ptr<tgfx::Typeface>& typeface,
                            const std::string& family, const std::string& style,
                            bool requireStyle) {
  if (typeface == nullptr || !SystemFonts::FontNamesMatch(family, typeface->fontFamily())) {
    return false;
  }
  return !requireStyle || style.empty() ||
         SystemFonts::FontNamesMatch(style, typeface->fontStyle());
}

static std::shared_ptr<tgfx::Typeface> LoadTypeface(const FontLocation& location) {
  if (location.path.empty()) {
    return nullptr;
  }
  return tgfx::Typeface::MakeFromPath(location.path, location.ttcIndex);
}

// Candidate weights for a missing face, in the order CSS Fonts 4 (5.2.2) picks one: the desired
// weight first, then the weights on the preferred side in order of increasing distance, then the
// opposite side. Targets in the 400..500 band check up to 500 before descending, and only reach
// above 500 once both sides are exhausted.
static std::vector<int> WeightSearchOrder(int target) {
  std::vector<int> order = {};
  order.reserve(9);
  if (target < 400) {
    for (int weight = target; weight >= 100; weight -= 100) {
      order.push_back(weight);
    }
    for (int weight = target + 100; weight <= 900; weight += 100) {
      order.push_back(weight);
    }
  } else if (target <= 500) {
    for (int weight = target; weight <= 500; weight += 100) {
      order.push_back(weight);
    }
    for (int weight = target - 100; weight >= 100; weight -= 100) {
      order.push_back(weight);
    }
    for (int weight = 600; weight <= 900; weight += 100) {
      order.push_back(weight);
    }
  } else {
    for (int weight = target; weight <= 900; weight += 100) {
      order.push_back(weight);
    }
    for (int weight = target - 100; weight >= 100; weight -= 100) {
      order.push_back(weight);
    }
  }
  return order;
}

std::shared_ptr<tgfx::Typeface> SystemFonts::ResolveTypeface(const std::string& family,
                                                             const std::string& style) {
  if (family.empty()) {
    return nullptr;
  }

  auto typeface = tgfx::Typeface::MakeFromName(family, style);
  if (TypefaceMatches(typeface, family, style, true)) {
    return typeface;
  }

  auto location = FindFont(family, style);
  typeface = LoadTypeface(location);
  if (TypefaceMatches(typeface, family, style, true)) {
    return typeface;
  }

  if (style.empty()) {
    return nullptr;
  }

  // CSS selects another available face from the requested family when that family does not contain
  // the requested style, so resolve the family's default face up front. This also rejects a family
  // the platform would only substitute before the per-weight search below pays for a probe per
  // candidate: that search can only reach faces of `family`, so a family that does not resolve at
  // all cannot be rescued by it.
  auto familyTypeface = tgfx::Typeface::MakeFromName(family, "");
  if (!TypefaceMatches(familyTypeface, family, "", false)) {
    familyTypeface = LoadTypeface(FindFont(family, ""));
  }
  if (!TypefaceMatches(familyTypeface, family, "", false)) {
    return nullptr;
  }

  // The requested face is not installed in this family, but the family itself is. CSS resolves the
  // nearest available weight, so a Bold request against a family that ships only
  // Regular/Medium/Semibold (PingFang SC) must resolve to Semibold rather than silently dropping
  // to Regular. Probe the candidate weights in CSS order.
  auto requested = ParseFontStyleName(style);
  auto candidates = WeightSearchOrder(requested.weight);
  for (int weight : candidates) {
    auto candidateStyle =
        ResolveFontStyleName(std::to_string(weight), requested.italic ? "italic" : "");
    if (FontNamesMatch(candidateStyle, style)) {
      continue;
    }
    typeface = tgfx::Typeface::MakeFromName(family, candidateStyle);
    if (TypefaceMatches(typeface, family, candidateStyle, true)) {
      return typeface;
    }
    location = FindFont(family, candidateStyle);
    typeface = LoadTypeface(location);
    if (TypefaceMatches(typeface, family, candidateStyle, true)) {
      return typeface;
    }
  }

  return familyTypeface;
}

}  // namespace pagx
