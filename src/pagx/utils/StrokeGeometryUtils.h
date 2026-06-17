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

namespace pagx {

class Stroke;

/**
 * Returns the per-side geometry inset that emulates StrokeAlign::Inside / StrokeAlign::Outside on
 * exporters whose stroke primitive always centres the stroke on the geometry (OOXML, SVG without
 * adding a separate clip). Positive values shrink the geometry (Inside), negative grow it
 * (Outside), zero leaves it unchanged.
 */
float StrokeAlignInset(const Stroke* stroke);

/**
 * Applies the stroke-alignment inset to an axis-aligned shape rect. Clamps the inset so the
 * geometry never collapses past the centre; `roundness` is reduced by the same amount when
 * provided so rounded-rectangle corners stay visually consistent after the inset.
 */
void ApplyStrokeBoxInset(const Stroke* stroke, float& x, float& y, float& w, float& h,
                         float* roundness = nullptr);

}  // namespace pagx
