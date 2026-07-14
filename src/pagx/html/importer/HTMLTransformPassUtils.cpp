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

#include "pagx/html/importer/HTMLTransformPassUtils.h"
#include <cmath>
#include <cstdlib>
#include <vector>
#include "pagx/html/importer/HTMLDetail.h"

namespace pagx::html {

bool ApproxEqual(float a, float b, float eps) {
  return std::fabs(a - b) <= eps;
}

const std::string& LookupResolved(const PropertyMap& props, const std::string& key) {
  return LookupProperty(props, key);
}

std::string LookupResolvedLower(const PropertyMap& props, const std::string& key) {
  return LookupLowerTrimmed(props, key);
}

bool ShouldSkipWalkerNode(const std::shared_ptr<DOMNode>& node, int depth,
                          HTMLTransformContext& ctx, const char* phase) {
  if (!node || node->type != DOMNodeType::Element) return true;
  if (depth >= MAX_HTML_RECURSION_DEPTH) {
    std::string msg = "html: maximum recursion depth reached during ";
    msg += phase;
    msg += "; subtree skipped";
    ctx.warn("subset:max-depth", msg, node);
    return true;
  }
  return false;
}

bool IsOpaqueSubtreeRoot(const std::shared_ptr<DOMNode>& node) {
  return node && node->type == DOMNodeType::Element && node->name == "svg";
}

bool ParseNormalisedPx(const std::string& valueRaw, float& outPx) {
  std::string value = Trim(valueRaw);
  if (value.empty()) return false;
  const char* c = value.c_str();
  char* end = nullptr;
  float v = std::strtof(c, &end);
  if (end == c) return false;
  if (!std::isfinite(v)) return false;
  std::string suffix = Trim(end);
  if (suffix.empty() || suffix == "px") {
    outPx = v;
    return true;
  }
  return false;
}

bool ApplyPaddingShorthand(const std::string& value, float& top, float& right, float& bottom,
                           float& left) {
  auto tokens = SplitTopLevelWhitespace(value);
  std::vector<float> nums;
  nums.reserve(tokens.size());
  for (auto& t : tokens) {
    float px = 0.0f;
    if (!ParseNormalisedPx(t, px)) return false;
    nums.push_back(px);
  }
  if (nums.empty()) return false;
  auto p = BuildPaddingShorthand(nums);
  top = p.top;
  right = p.right;
  bottom = p.bottom;
  left = p.left;
  return true;
}

void ClearFourSideProperty(PropertyMap& props, const char* base) {
  std::string prefix(base);
  prefix.push_back('-');
  props.erase(prefix + "top");
  props.erase(prefix + "right");
  props.erase(prefix + "bottom");
  props.erase(prefix + "left");
}

bool TryParseResolvedPx(const PropertyMap& props, const std::string& key, float& out,
                        bool requireParse) {
  const std::string& v = LookupProperty(props, key);
  if (v.empty()) return false;
  float px = 0.0f;
  if (!ParseNormalisedPx(v, px)) {
    return !requireParse;
  }
  out = px;
  return true;
}

bool ParsePaddingFromResolved(const PropertyMap& props, float& top, float& right, float& bottom,
                              float& left) {
  top = right = bottom = left = 0.0f;
  bool any = false;
  auto sh = LookupProperty(props, "padding");
  if (!sh.empty()) {
    if (!ApplyPaddingShorthand(sh, top, right, bottom, left)) return false;
    any = true;
  }
  if (TryParseResolvedPx(props, "padding-top", top)) any = true;
  if (TryParseResolvedPx(props, "padding-right", right)) any = true;
  if (TryParseResolvedPx(props, "padding-bottom", bottom)) any = true;
  if (TryParseResolvedPx(props, "padding-left", left)) any = true;
  return any || sh.empty();
}

bool ResolveChildMainMargin(const PropertyMap& props, bool row, float& outLeading,
                            float& outTrailing) {
  outLeading = 0.0f;
  outTrailing = 0.0f;
  auto sh = LookupProperty(props, "margin");
  if (!sh.empty()) {
    float t = 0.0f;
    float r = 0.0f;
    float b = 0.0f;
    float l = 0.0f;
    if (!ApplyPaddingShorthand(sh, t, r, b, l)) return false;
    outLeading = row ? l : t;
    outTrailing = row ? r : b;
  }
  const std::string leadKey = row ? "margin-left" : "margin-top";
  const std::string trailKey = row ? "margin-right" : "margin-bottom";
  if (auto v = LookupProperty(props, leadKey); !v.empty()) {
    float px = 0.0f;
    if (!ParseNormalisedPx(v, px)) return false;
    outLeading = px;
  }
  if (auto v = LookupProperty(props, trailKey); !v.empty()) {
    float px = 0.0f;
    if (!ParseNormalisedPx(v, px)) return false;
    outTrailing = px;
  }
  return true;
}

void ClearChildMainMargin(PropertyMap& props, bool row) {
  auto sh = LookupProperty(props, "margin");
  if (!sh.empty()) {
    float t = 0.0f;
    float r = 0.0f;
    float b = 0.0f;
    float l = 0.0f;
    if (ApplyPaddingShorthand(sh, t, r, b, l)) {
      if (row) {
        if (t != 0.0f) props["margin-top"] = EmitPx(t);
        if (b != 0.0f) props["margin-bottom"] = EmitPx(b);
      } else {
        if (l != 0.0f) props["margin-left"] = EmitPx(l);
        if (r != 0.0f) props["margin-right"] = EmitPx(r);
      }
    }
    props.erase("margin");
  }
  props.erase(row ? "margin-left" : "margin-top");
  props.erase(row ? "margin-right" : "margin-bottom");
}

bool ChildHasFlexGrow(const PropertyMap& props) {
  const std::string& flex = LookupProperty(props, "flex");
  if (flex.empty()) return false;
  std::string trimmed = Trim(flex);
  if (trimmed.empty()) return false;
  char* end = nullptr;
  float v = std::strtof(trimmed.c_str(), &end);
  if (end == trimmed.c_str()) return false;
  if (!std::isfinite(v)) return false;
  return v > 0.0f;
}

}  // namespace pagx::html
