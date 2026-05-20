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
#include <unordered_map>
#include "pagx/html/importer/HTMLTransformContext.h"

namespace pagx::html {

enum class PropAction {
  Keep,       // Pass the value through untouched.
  Drop,       // Strip the property; record a diagnostic.
  Transform,  // Run the entry's `transform` callback.
};

/**
 * Context handed to per-property transform callbacks. Mirrors the inherited cascade so that
 * `em` and similar relative units can resolve against the element's current font-size.
 */
struct PropertyContext {
  std::string propertyName = {};   // lower-case CSS property name
  std::string ownerTag = {};       // tag of the element being transformed
  float canvasWidth = 0.0f;        // NaN-safe; 0 when unknown
  float canvasHeight = 0.0f;       // NaN-safe; 0 when unknown
  float currentFontSizePx = 0.0f;  // computed font-size for `em` resolution; 0 when unknown
};

using PropertyTransformFn = std::string (*)(const std::string& value, const PropertyContext& ctx,
                                            HTMLTransformContext& diagnostics);

struct PropertyHandler {
  PropAction action = PropAction::Drop;
  // Callback invoked when action == Transform. Must return the new value, or an empty string to
  // drop the property. nullptr is allowed only when action != Transform.
  PropertyTransformFn transform = nullptr;
  // Static diagnostic message to attach when action == Drop. Empty means use a generic message.
  const char* dropMessage = nullptr;
};

/**
 * Returns the static allow-list of CSS properties the subset accepts. Properties not in the
 * table default to "drop with a generic warning". To support a new property, add one entry.
 */
const std::unordered_map<std::string, PropertyHandler>& SubsetPropertyTable();

/**
 * Resolves a single CSS length (`px`, `%`, `em`, `rem`, `vw`, `vh`, or unitless treated as px)
 * into a value the subset accepts. The output preserves the original kind:
 *
 *   - `Wpx` / unitless           → `Wpx`
 *   - `W%`                       → `W%`
 *   - `Wem`  (with parent size F) → `(W*F)px`
 *   - `Wrem`                     → `(W*16)px`
 *   - `Wvw` / `Wvh`              → percent of canvas dimension when known, else dropped
 *   - `calc(...)` and friends    → empty string + diagnostic
 *
 * Returns the empty string when the value cannot be converted; in that case a diagnostic has
 * been appended to `diagnostics` describing why.
 */
std::string ResolveLength(const std::string& value, const PropertyContext& ctx,
                          HTMLTransformContext& diagnostics);

/**
 * Resolves a CSS shorthand made of 1–4 lengths (e.g. `padding: 12px 16px`). Each component is
 * passed through `ResolveLength`. Components that fail to resolve are dropped from the output
 * (a single diagnostic is recorded per failed component).
 */
std::string ResolveLengthShorthand(const std::string& value, const PropertyContext& ctx,
                                   HTMLTransformContext& diagnostics);

/**
 * Returns true when `name` follows the `data-*` HTML pattern. Such properties are passed
 * through untouched.
 */
bool IsDataAttribute(const std::string& name);

}  // namespace pagx::html
