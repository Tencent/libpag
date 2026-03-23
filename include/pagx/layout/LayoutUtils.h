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

#include "pagx/types/Rect.h"

namespace pagx {

class Element;
class Text;
class FontConfig;

/**
 * Shared utilities for layout computation, including text bounds calculation and content
 * bounds measurement with TextBox write-back support.
 */
namespace LayoutUtils {

/**
 * Computes the tight bounding box of a Text element, accounting for text anchor offset.
 * Falls back to font size estimation when font information is unavailable.
 *
 * @param text The Text element to measure
 * @param fontProvider Font provider for typeface lookup (can be nullptr)
 * @return Bounding rectangle in absolute coordinates
 */
Rect ComputeTextBounds(const Text* text, FontConfig* fontProvider);

/**
 * Wraps ElementMeasure::GetContentBounds with TextBox measurement write-back.
 * For TextBox elements without explicit dimensions (NaN), automatically measures and
 * writes back the measured dimensions. This ensures TextBox content sizing is computed
 * before typesetting operations that depend on explicit dimensions.
 *
 * @param element The element to measure
 * @param fontProvider Font provider for text measurement
 * @return Content bounds rectangle
 */
Rect GetContentBounds(const Element* element, FontConfig* fontProvider);

}  // namespace LayoutUtils

}  // namespace pagx
