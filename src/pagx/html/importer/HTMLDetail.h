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

#include <cctype>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "pagx/types/Color.h"
#include "pagx/types/Padding.h"
#include "pagx/xml/XMLDOM.h"

namespace pagx::html {

// ----- String helpers -----------------------------------------------------------------------

// Lower-case ASCII in place.
inline std::string ToLower(std::string s) {
  for (auto& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

// Trim ASCII whitespace from both ends.
inline std::string Trim(const std::string& s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return {};
  }
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

// Returns true when `text` is empty or consists entirely of ASCII whitespace.
inline bool IsBlankText(const std::string& text) {
  for (char c : text) {
    if (!std::isspace(static_cast<unsigned char>(c))) return false;
  }
  return true;
}

// Split a CSS value list on top-level commas (commas inside parentheses are not split).
std::vector<std::string> SplitTopLevelCommas(const std::string& s);

// Split a CSS value on whitespace runs while respecting parenthesised groups (so
// "0 2px 6px rgba(0,0,0,0.2)" yields four tokens).
std::vector<std::string> SplitTopLevelWhitespace(const std::string& s);

// Walks an inline `style="..."` declaration list. Returns (propertyName, propertyValue) in
// source order. Property names are preserved as-is (caller may lower-case if needed); values
// are trimmed but otherwise verbatim. Respects /* ... */ comments and parenthesised values
// (so a `;` inside `linear-gradient(...)` doesn't terminate the declaration). Empty
// names / values are skipped.
std::vector<std::pair<std::string, std::string>> ParseStyleDeclarations(const std::string& style);

// Parse a CSS style string into a property map. Later properties override earlier ones.
// Property names are lower-cased before insertion so callers can use canonical CSS names.
// Thin wrapper over `ParseStyleDeclarations` — see it for syntactic guarantees.
void ParseStyleString(const std::string& styleStr,
                      std::unordered_map<std::string, std::string>& out);

// Splits a CSS font-family value into individual family tokens. Top-level commas split,
// surrounding "..." or '...' quotes are stripped, blank tokens are dropped. Generic
// keywords are NOT mapped here; pass each token through ResolveGenericFontFamily.
std::vector<std::string> ParseFontFamilyTokens(const std::string& raw);

// Maps a CSS generic family keyword (sans-serif, monospace, ...) to a concrete platform
// font name. Returns the input unchanged when `name` is not a recognised generic.
// Returns an empty string when the keyword is a recognised generic with no sensible
// mapping (cursive, fantasy, ui-rounded, emoji, math, fangsong). Recognises -apple-system
// and BlinkMacSystemFont as system-ui aliases. Matching is ASCII case-insensitive.
std::string ResolveGenericFontFamily(const std::string& name);

// ----- Tag-set predicates -------------------------------------------------------------------

bool IsContainerTag(const std::string& tag);

bool IsTextLeafTag(const std::string& tag);

bool IsInlineRunTag(const std::string& tag);

// True for elements that may legally appear inside a text leaf (`<span>`, `<a>`, `<br>`).
// Hoisted out of HTMLImporter to keep the project's "no lambda" rule.
bool IsInlineLeafChildName(const std::string& name);

// ----- CSS tables ---------------------------------------------------------------------------

// CSS named color table (CSS Color 3 + rebeccapurple).
const std::unordered_map<std::string, uint32_t>& NamedColors();

// Default style sheet by element tag.
const std::unordered_map<std::string, std::string>& ElementDefaults();

// Same as `ElementDefaults` but with each style string pre-parsed into a property map. Built
// once on first call from `ElementDefaults`. Hot paths that resolve element defaults during
// a DOM walk should consult this overload instead of re-parsing the same CSS string per node.
const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>&
ParsedElementDefaults();

// ----- DOM helpers --------------------------------------------------------------------------

// Lower-case the tag name of every element node in place. `depth` is internal recursion
// bookkeeping; subtrees deeper than `MAX_HTML_RECURSION_DEPTH` are silently left alone to
// preserve stack safety on pathological inputs.
void LowercaseTagsInPlace(const std::shared_ptr<DOMNode>& node, int depth = 0);

// Builds a synthetic `<span>` DOMNode wrapping a single text child. Used by the importer to
// promote stray text content of a container into a text leaf without changing the surrounding
// shape. The returned node owns its child text node.
std::shared_ptr<DOMNode> MakeStrayTextSpan(const std::string& text);

// Escape XML text/attribute values for round-tripping inline SVG content.
std::string EscapeXml(const std::string& text, bool isAttribute);

// Linked-list child operations. Several importer passes need to splice / remove children
// of a DOM element while iterating; these helpers keep the prev/next bookkeeping in one
// place so a fix to one path applies everywhere. They do not own the unlinked nodes —
// callers' shared_ptrs continue to keep the subtree alive.

// Unlinks `child` from `parent`'s linked list. `prev` is the predecessor of `child` (may
// be null when `child` is the first sibling). After the call `child->nextSibling` is
// cleared but `child->firstChild` is left intact, so the caller can re-attach the
// detached node elsewhere.
void UnlinkChild(const std::shared_ptr<DOMNode>& parent, const std::shared_ptr<DOMNode>& prev,
                 const std::shared_ptr<DOMNode>& child);

// Replace `child` (currently a sibling under `parent`, with predecessor `prev`) with
// `replacement`, preserving the iteration cursor. Returns the next sibling (the value
// the caller should advance their cursor to).
std::shared_ptr<DOMNode> ReplaceChild(const std::shared_ptr<DOMNode>& parent,
                                      const std::shared_ptr<DOMNode>& prev,
                                      const std::shared_ptr<DOMNode>& child,
                                      const std::shared_ptr<DOMNode>& replacement);

// ----- Numeric / unit helpers ---------------------------------------------------------------

// Try to parse a CSS angle in `deg`, `rad`, `turn`, or unitless (deg).
float ParseAngle(const std::string& raw);

// Returns the converted PAGX gradient angle (degrees, 0deg = +X axis) for a CSS angle
// (degrees, 0deg = top, clockwise).
inline float CssToPagxAngle(float cssDeg) {
  return cssDeg - 90.0f;
}

// Convert CSS keyword direction ("to bottom right", etc.) to a CSS angle in degrees.
float CssDirectionToAngle(const std::string& kw);

// Pull the bracketed args of a function-like CSS value:
// "linear-gradient(a, b, c)" -> "a, b, c".
std::string ExtractParenArgs(const std::string& value);

// Strip trailing slashes so file path resolution doesn't pick up wrong directories.
std::string DirectoryOf(const std::string& filePath);

bool LooksAbsolutePath(const std::string& src);

// Convert a CSS hex value (already parsed into a uint32_t) into a Color. When `hasAlpha`
// is true the alpha channel is the most significant byte, otherwise alpha defaults to 1.
Color HexToColor(uint32_t hex, bool hasAlpha);

// Builds a Padding shorthand from 1-4 numbers (CSS top/right/bottom/left expansion).
Padding BuildPaddingShorthand(const std::vector<float>& nums);

// Parses a CSS dimension string into either an explicit pixel value or a percent value.
// Returns true on success and writes one of `outPx` / `outPct` (the other stays NaN).
bool ParseSizingDimension(const std::string& raw, float& outPx, float& outPct);

// Single source of truth for CSS length unit conversion.
// `num` is the numeric component, `unit` is the unit suffix already lower-cased (or empty
// for unitless). `fontSizePx`/`canvasW`/`canvasH` provide context for em / rem / vw / vh; pass
// NaN or 0 when not available. On success returns the px equivalent and sets `*recognized`
// to true. When the unit is unknown or context-dependent context is missing (e.g. vw without
// a canvas), returns NaN and leaves `*recognized` false.
//   - "" / "px"  -> num
//   - "em"       -> num * (fontSizePx finite ? fontSizePx : 16)
//   - "rem"      -> num * 16
//   - "%"        -> NaN, recognized=false (caller must opt into percent semantics)
//   - "pt"       -> num * 4 / 3
//   - "vw"/"vh"  -> num * canvas / 100 when canvas is finite & > 0; otherwise unrecognized
float ConvertCssLengthToPx(float num, const std::string& unit, float fontSizePx, float canvasW,
                           float canvasH, bool& recognized);

// Collapse HTML whitespace inside a fragment: convert tabs/CR to spaces and collapse
// adjacent ASCII whitespace into a single space. `trimLeading` strips remaining ASCII
// spaces from the front (leading `\n` is preserved so a leading `<br>` keeps its hard
// newline), and `trimTrailing` strips trailing spaces and `\n`. When the fragment is
// part of a multi-fragment inline run, the caller should disable the trim that falls in
// the middle of the run so that whitespace at the boundary between two differently
// styled fragments is preserved with its own font size — see `convertTextLeaf`.
std::string CollapseHTMLWhitespace(const std::string& raw, bool trimLeading = true,
                                   bool trimTrailing = true);

// ----- Property-map helpers -----------------------------------------------------------------

// Returns the value of property `key` from `props`, or an empty string when absent.
inline const std::string& LookupProperty(const std::unordered_map<std::string, std::string>& props,
                                         const std::string& key) {
  static const std::string emptyValue;
  auto it = props.find(key);
  return it == props.end() ? emptyValue : it->second;
}

// Convenience: lower-cased and trimmed lookup, used a lot when interpreting CSS keyword
// values such as `display`, `position`, `align-items`.
inline std::string LookupLowerTrimmed(const std::unordered_map<std::string, std::string>& props,
                                      const std::string& key) {
  return ToLower(Trim(LookupProperty(props, key)));
}

// Copies a property value from `props[key]` into `slot` when present. Hoisted out of
// HTMLStyleResolver so the project rule banning lambdas is honoured uniformly.
inline void CopyProperty(const std::unordered_map<std::string, std::string>& props,
                         const std::string& key, std::string& slot) {
  auto it = props.find(key);
  if (it != props.end()) slot = it->second;
}

// ----- Numeric formatting (single source of truth for two pipelines) ------------------------

// Rounds `v` to 0.001 and emits the shortest exact text. Integers are emitted without a
// decimal point; finite non-integers use "%g"-style precision-6 output. Used by both the
// subset transformer and pass code that writes `style="…"`.
std::string FormatRoundedNumber(float v);

// `<n>px` and `<n>%` shorthand emitters.
std::string EmitPx(float px);
std::string EmitPercent(float pct);

// Builds a CSS px shorthand string from top/right/bottom/left, collapsing to the shortest
// equivalent form ("N", "T R", "T R B" vs "T R B L"). Each component is formatted by
// `EmitPx` so the output is deterministic across the two pipelines.
std::string EmitPaddingShorthand(float top, float right, float bottom, float left);

}  // namespace pagx::html
