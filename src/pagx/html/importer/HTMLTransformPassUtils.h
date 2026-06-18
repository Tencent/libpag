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

#include <memory>
#include <string>
#include "pagx/html/importer/HTMLTransformContext.h"

namespace pagx::html {

// Shared helpers used by the flex-related transformer passes (HTMLFlexInference,
// MarginToGapPromotion, SpaceJustifyOverflowCollapse). These all operate on the
// post-PropertyFilter resolved style map where every length has been normalised to plain px,
// so the parsers reject percent / calc / em / etc. by design.

// Float equality with explicit tolerance; used by passes that compare CSS lengths after
// arithmetic (rounding errors accumulate over `ResolveLength`).
bool ApproxEqual(float a, float b, float eps);

// Reads a property from a resolved style map, returning empty when missing. Thin wrapper
// over `LookupProperty` (in HTMLDetail) to keep the rich call sites in flex-related passes
// uniform with `LookupResolvedLower`.
const std::string& LookupResolved(const PropertyMap& props, const std::string& key);

// Lower-cased trimmed view of a resolved property.
std::string LookupResolvedLower(const PropertyMap& props, const std::string& key);

// Reads a property from a resolved style map, returning empty when missing. Thin wrapper
// over `LookupProperty` (in HTMLDetail) to keep the rich call sites in flex-related passes
// uniform with `LookupResolvedLower`.
const std::string& LookupResolved(const PropertyMap& props, const std::string& key);

// Lower-cased trimmed view of a resolved property.
std::string LookupResolvedLower(const PropertyMap& props, const std::string& key);

// Common walker entry-point guard. Returns true (caller must `return` immediately) when the
// node is null/non-element or the recursion depth limit has been reached. On the depth path a
// `subset:max-depth` warning is emitted whose body cites `phase` so authors can tell which
// pass aborted the descent.
bool ShouldSkipWalkerNode(const std::shared_ptr<DOMNode>& node, int depth,
                          HTMLTransformContext& ctx, const char* phase);

// Returns true for nodes whose subtree the cascade / property-filter / layout / emit passes
// treat as opaque. Currently only `<svg>` qualifies; its attributes are forwarded verbatim by
// the importer (the SVG resolver runs as a separate pipeline).
bool IsOpaqueSubtreeRoot(const std::shared_ptr<DOMNode>& node);

// Returns true for nodes whose subtree the cascade / property-filter / layout / emit passes
// treat as opaque. Currently only `<svg>` qualifies; its attributes are forwarded verbatim by
// the importer (the SVG resolver runs as a separate pipeline).
bool IsOpaqueSubtreeRoot(const std::shared_ptr<DOMNode>& node);

// Parses an already-normalised length value (`Npx`) into a finite float. Returns false when
// the value is empty, percent-based, or otherwise not a plain pixel length. PropertyFilter has
// already converted every supported unit to `px`, so anything else means "skip".
bool ParseNormalisedPx(const std::string& valueRaw, float& outPx);

// Expands a `padding`-style shorthand value (e.g. `4px 8px`) into top/right/bottom/left.
// Returns false when any token is not a plain px length. The output buffers are written only
// on success. Reused for `margin` shorthand as well — both share the same 1-4-token grammar.
bool ApplyPaddingShorthand(const std::string& value, float& top, float& right, float& bottom,
                           float& left);

// Erases the four longhand properties of a CSS edge shorthand (margin-* / padding-* / etc.)
// from `props`. Used by passes that fold per-side overrides back into a shorthand or otherwise
// drop the per-side declarations.
void ClearFourSideProperty(PropertyMap& props, const char* base);

// Returns true and writes the resolved px value into `out` when `props[key]` is present and
// parses as a finite px length. Returns false when the property is absent. When the property
// is present but malformed, behaviour depends on `requireParse`:
//   - true : the function returns false (caller should bail).
//   - false: the function returns true with `out` left at its incoming value (caller may
//            ignore the malformed token).
// Most call sites want `requireParse=true`; the legacy "tolerant" sites pass false.
bool TryParseResolvedPx(const PropertyMap& props, const std::string& key, float& out,
                        bool requireParse = true);

// Parses the resolved `padding` shorthand (1-4 px tokens) into top/right/bottom/left. Returns
// false if the value is missing or any token is not a plain px length. Per-side `padding-*`
// overrides (when present) win over the shorthand.
bool ParsePaddingFromResolved(const PropertyMap& props, float& top, float& right, float& bottom,
                              float& left);

// Resolves a child's main-axis edge margins (top + bottom for column flex, left + right for
// row flex). Reads the `margin` shorthand first, then per-side longhands which override the
// shorthand. Returns false if any encountered value is not a plain px length (e.g. `auto`,
// percentages) so callers can bail out conservatively. Missing values are treated as 0.
bool ResolveChildMainMargin(const PropertyMap& props, bool row, float& outLeading,
                            float& outTrailing);

// Removes the main-axis margin declarations (both shorthand and per-side longhands) that
// `ResolveChildMainMargin` would have read. Cross-axis margins on the same shorthand are
// preserved by re-emitting them as longhands before the shorthand is stripped.
void ClearChildMainMargin(PropertyMap& props, bool row);

// Returns true when the property `flex` shorthand resolves to a positive grow factor. The
// subset pipeline only emits `flex: <grow>` (see SubsetPropertyTable), so a single numeric
// token is the only shape we need to recognise here. Shared between MarginToGapPromotion and
// SpaceJustifyOverflowCollapse: both passes must skip containers whose children declare
// `flex-grow`, since the spec routes free space into the grown child rather than into the
// packing gaps these passes reason about.
bool ChildHasFlexGrow(const PropertyMap& props);

}  // namespace pagx::html
