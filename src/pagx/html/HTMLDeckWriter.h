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
#include <utility>
#include <vector>

namespace pagx {

/**
 * Builds a self-contained presentation HTML document that hosts one page fragment per entry. The
 * document shows a single page at a time and scales each page to fit the window while keeping it
 * centered, so pages of different sizes are all presented at their largest legible scale.
 *
 * Built-in interaction: keyboard navigation (arrow keys, PageUp/PageDown, Space, Home/End), click
 * and touch-swipe paging, URL hash deep links (#pN), a page indicator, fade transitions, and
 * manual zoom (Ctrl+wheel, double-click, pinch, +/-/0) with drag-to-pan while zoomed. Paging
 * resets the zoom so every page starts fit to the window.
 *
 * All page fragments must already be namespaced by the caller (see HTMLWriterContext::idPrefix)
 * so that element ids, class names, and resource filenames stay unique across pages.
 *
 * @param pageFragments the page bodies in display order, each produced by BuildHTML in Fragment
 *        mode for one document.
 * @param pageSizes the logical width/height of each page, written to the section's data-w/data-h
 *        attributes that drive the per-page fit-to-window scale.
 * @return the complete HTML document as a string.
 */
std::string BuildDeckHTML(const std::vector<std::string>& pageFragments,
                          const std::vector<std::pair<float, float>>& pageSizes);

}  // namespace pagx
