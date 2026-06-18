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

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/html/importer/HTMLTransformPassUtils.h"
#include "pagx/html/importer/HTMLTransformPasses.h"

namespace pagx::html {

namespace {

struct ChildBox {
  std::shared_ptr<DOMNode> node;
  float left = 0.0f;
  float top = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
};

// Returns true when `props` describes an `position: absolute` element with all four of
// `left`/`top`/`width`/`height` resolved into plain px lengths and no `right`/`bottom` /
// `flex` overrides that would conflict with a flex rewrite.
bool ExtractAbsoluteBox(const PropertyMap& props, ChildBox& out) {
  if (LookupLowerTrimmed(props, "position") != "absolute") return false;
  // Conflict guards: we don't try to rewrite children that pin against the parent's right /
  // bottom edges, or that already declare a flex grow factor.
  auto rightVal = LookupProperty(props, "right");
  auto bottomVal = LookupProperty(props, "bottom");
  if (!rightVal.empty()) return false;
  if (!bottomVal.empty()) return false;
  if (!LookupProperty(props, "flex").empty()) return false;
  float left = 0.0f;
  float top = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
  if (!ParseNormalisedPx(LookupProperty(props, "left"), left)) return false;
  if (!ParseNormalisedPx(LookupProperty(props, "top"), top)) return false;
  if (!ParseNormalisedPx(LookupProperty(props, "width"), width)) return false;
  if (!ParseNormalisedPx(LookupProperty(props, "height"), height)) return false;
  if (!std::isfinite(left) || !std::isfinite(top)) return false;
  if (!(width > 0.0f) || !(height > 0.0f)) return false;
  out.left = left;
  out.top = top;
  out.width = width;
  out.height = height;
  return true;
}

enum class FlexAxis { Row, Column };

// Comparators for sorting `ChildBox` along the row / column main axes. Hoisted out of
// `InferAxis` to honour the project's "no lambdas" rule.
bool ChildBoxLeftLess(const ChildBox& a, const ChildBox& b) {
  return a.left < b.left;
}
bool ChildBoxTopLess(const ChildBox& a, const ChildBox& b) {
  return a.top < b.top;
}

// Sorts `children` along `row` (true) or column (false) into `outSorted` and returns true
// when sorting yields a sequence with no main-axis overlap (within tolerance `tol`).
bool TryAxisSortedNoOverlap(const std::vector<ChildBox>& children, bool row, float tol,
                            std::vector<ChildBox>& outSorted) {
  outSorted = children;
  std::sort(outSorted.begin(), outSorted.end(), row ? ChildBoxLeftLess : ChildBoxTopLess);
  for (size_t i = 0; i + 1 < outSorted.size(); i++) {
    float curEnd =
        row ? outSorted[i].left + outSorted[i].width : outSorted[i].top + outSorted[i].height;
    float nextStart = row ? outSorted[i + 1].left : outSorted[i + 1].top;
    if (nextStart < curEnd - tol) return false;
  }
  return true;
}

// Returns the spread (max - min) of the main-axis starting offsets of `children`.
float AxisStartSpread(const std::vector<ChildBox>& children, bool row) {
  float lo = std::numeric_limits<float>::infinity();
  float hi = -std::numeric_limits<float>::infinity();
  for (const auto& c : children) {
    float v = row ? c.left : c.top;
    lo = std::min(lo, v);
    hi = std::max(hi, v);
  }
  return hi - lo;
}

// Returns the inferred main axis when every child's bounding box is ordered along it without
// overlap. When both axes are valid (single child / collinear), prefers the axis with the
// larger spread. On success populates `outSorted` with the children sorted along the chosen
// main axis so the caller does not have to sort again.
bool InferAxis(const std::vector<ChildBox>& children, float tol, FlexAxis& out,
               std::vector<ChildBox>& outSorted) {
  if (children.size() < 2) return false;
  std::vector<ChildBox> sortedRow;
  std::vector<ChildBox> sortedCol;
  bool rowOk = TryAxisSortedNoOverlap(children, /*row=*/true, tol, sortedRow);
  bool colOk = TryAxisSortedNoOverlap(children, /*row=*/false, tol, sortedCol);
  if (!rowOk && !colOk) return false;
  if (rowOk && colOk) {
    out = AxisStartSpread(children, /*row=*/true) >= AxisStartSpread(children, /*row=*/false)
              ? FlexAxis::Row
              : FlexAxis::Column;
  } else {
    out = rowOk ? FlexAxis::Row : FlexAxis::Column;
  }
  outSorted = (out == FlexAxis::Row) ? std::move(sortedRow) : std::move(sortedCol);
  return true;
}

enum class CrossAlign { Start, Center, End, Stretch, Mixed };

// Determines a single `align-items` value compatible with all children's cross-axis position
// inside the parent's content box. Returns CrossAlign::Mixed when no single keyword fits.
CrossAlign InferCrossAlign(const std::vector<ChildBox>& children, FlexAxis axis, float contentLow,
                           float contentHigh, float tol) {
  if (children.empty()) return CrossAlign::Mixed;
  bool stretchOk = true;
  bool startOk = true;
  bool endOk = true;
  bool centerOk = true;
  float contentMid = 0.5f * (contentLow + contentHigh);
  float contentSize = contentHigh - contentLow;
  for (const auto& c : children) {
    float lo = axis == FlexAxis::Row ? c.top : c.left;
    float size = axis == FlexAxis::Row ? c.height : c.width;
    float hi = lo + size;
    float mid = lo + size * 0.5f;
    if (std::fabs(lo - contentLow) > tol) startOk = false;
    if (std::fabs(hi - contentHigh) > tol) endOk = false;
    if (std::fabs(mid - contentMid) > tol) centerOk = false;
    if (!(std::fabs(lo - contentLow) <= tol && std::fabs(size - contentSize) <= tol)) {
      stretchOk = false;
    }
  }
  if (stretchOk) return CrossAlign::Stretch;
  if (startOk) return CrossAlign::Start;
  if (centerOk) return CrossAlign::Center;
  if (endOk) return CrossAlign::End;
  return CrossAlign::Mixed;
}

struct MainAxisFit {
  bool ok = false;
  float paddingLeading = 0.0f;
  float paddingTrailing = 0.0f;
  float gap = 0.0f;
};

// Returns the main-axis low (start) or high (end) coordinate of a child box on the given axis.
// Hoisted out of `InferMainAxisSpacing` to honour the project's "no lambdas" rule.
float GetMainAxisCoord(const ChildBox& c, FlexAxis axis, bool low) {
  if (axis == FlexAxis::Row) {
    return low ? c.left : c.left + c.width;
  }
  return low ? c.top : c.top + c.height;
}

MainAxisFit InferMainAxisSpacing(const std::vector<ChildBox>& sorted, FlexAxis axis,
                                 float contentLow, float contentHigh, float tol) {
  MainAxisFit fit = {};
  fit.paddingLeading = GetMainAxisCoord(sorted.front(), axis, /*low=*/true) - contentLow;
  fit.paddingTrailing = contentHigh - GetMainAxisCoord(sorted.back(), axis, /*low=*/false);
  if (fit.paddingLeading < -tol) return fit;
  if (fit.paddingTrailing < -tol) return fit;
  if (fit.paddingLeading < 0) fit.paddingLeading = 0;
  if (fit.paddingTrailing < 0) fit.paddingTrailing = 0;
  if (sorted.size() == 1) {
    fit.ok = true;
    return fit;
  }
  float first = GetMainAxisCoord(sorted[1], axis, /*low=*/true) -
                GetMainAxisCoord(sorted[0], axis, /*low=*/false);
  if (first < -tol) return fit;
  float gap = std::max(0.0f, first);
  for (size_t i = 1; i + 1 < sorted.size(); i++) {
    float g = GetMainAxisCoord(sorted[i + 1], axis, /*low=*/true) -
              GetMainAxisCoord(sorted[i], axis, /*low=*/false);
    if (std::fabs(g - first) > tol) return fit;
    gap = std::max(gap, std::max(0.0f, g));
  }
  fit.gap = gap;
  fit.ok = true;
  return fit;
}

// Looks up the parent's content box (inside-padding rectangle) along a single axis. When the
// parent has no explicit dimension, falls back to the children's bounding extents so that the
// inference still works on layout-only wrapper containers. `outFromExplicit` reports whether
// the returned range came from the parent's declared dimension (true) or from the children
// bbox fallback (false); callers use this to decide whether shared child insets should be
// folded into padding (only safe in the fallback path — an explicit dimension already accounts
// for them via the child's offset, so re-attributing them to padding would double-count).
bool ResolveContentRange(const PropertyMap* parentProps, const std::vector<ChildBox>& children,
                         float padLow, float padHigh, FlexAxis axis, float& contentLow,
                         float& contentHigh, bool& outFromExplicit) {
  contentLow = padLow;
  contentHigh = std::numeric_limits<float>::quiet_NaN();
  outFromExplicit = false;
  float dim = std::numeric_limits<float>::quiet_NaN();
  if (parentProps) {
    const std::string& key = axis == FlexAxis::Row ? "width" : "height";
    auto val = LookupProperty(*parentProps, key);
    float px = 0.0f;
    if (!val.empty() && ParseNormalisedPx(val, px) && px > 0.0f) {
      dim = px;
    }
  }
  if (std::isfinite(dim)) {
    contentHigh = dim - padHigh;
    outFromExplicit = true;
    return contentHigh > contentLow;
  }
  float lo = std::numeric_limits<float>::infinity();
  float hi = -std::numeric_limits<float>::infinity();
  for (const auto& c : children) {
    float a = axis == FlexAxis::Row ? c.left : c.top;
    float b = axis == FlexAxis::Row ? c.left + c.width : c.top + c.height;
    lo = std::min(lo, a);
    hi = std::max(hi, b);
  }
  if (!std::isfinite(lo) || !std::isfinite(hi)) return false;
  contentLow = lo;
  contentHigh = hi;
  return true;
}

const char* AxisName(FlexAxis a) {
  return a == FlexAxis::Row ? "row" : "column";
}

// Mutates the parent's resolved style to declare the inferred flex layout. Strips any
// per-side padding overrides we just folded into the shorthand.
void ApplyFlexToParent(PropertyMap& parentProps, FlexAxis axis, float padTop, float padRight,
                       float padBottom, float padLeft, float gap, CrossAlign align) {
  parentProps["display"] = "flex";
  parentProps["flex-direction"] = AxisName(axis);
  if (gap > 0.0f) {
    parentProps["gap"] = EmitPx(gap);
  } else {
    parentProps.erase("gap");
  }
  if (padTop > 0.0f || padRight > 0.0f || padBottom > 0.0f || padLeft > 0.0f) {
    parentProps["padding"] = EmitPaddingShorthand(padTop, padRight, padBottom, padLeft);
  } else {
    parentProps.erase("padding");
  }
  ClearFourSideProperty(parentProps, "padding");
  switch (align) {
    case CrossAlign::Stretch:
      parentProps["align-items"] = "stretch";
      break;
    case CrossAlign::Center:
      parentProps["align-items"] = "center";
      break;
    case CrossAlign::Start:
      parentProps["align-items"] = "flex-start";
      break;
    case CrossAlign::End:
      parentProps["align-items"] = "flex-end";
      break;
    case CrossAlign::Mixed:
      break;
  }
}

// Strips position-related declarations from a child that the parent's flex layout now
// places. When the chosen alignment is Stretch the cross-axis size is also dropped so that
// the flex engine fills the cross axis (matching CSS `align-items: stretch`).
void StripChildAbsolute(PropertyMap& childProps, FlexAxis axis, CrossAlign align) {
  childProps.erase("position");
  childProps.erase("left");
  childProps.erase("right");
  childProps.erase("top");
  childProps.erase("bottom");
  if (align == CrossAlign::Stretch) {
    childProps.erase(axis == FlexAxis::Row ? "height" : "width");
  }
}

// Reorders `parent`'s element children to match `sortedElements`, while keeping any
// non-element (text) children in their original ordinal slots. Required because flex
// honours document order but the absolute children we just folded did not.
// Returns true when the order changed.
bool ReorderElementChildrenToMainAxis(const std::shared_ptr<DOMNode>& parent,
                                      const std::vector<ChildBox>& sortedElements) {
  if (!parent) return false;
  std::vector<std::shared_ptr<DOMNode>> all;
  size_t elementCount = 0;
  bool needsReorder = false;
  for (auto c = parent->firstChild; c; c = c->nextSibling) {
    all.push_back(c);
    if (c->type == DOMNodeType::Element) {
      if (elementCount >= sortedElements.size()) return false;
      if (c != sortedElements[elementCount].node) needsReorder = true;
      elementCount++;
    }
  }
  if (elementCount != sortedElements.size()) return false;
  if (!needsReorder) return false;

  size_t idx = 0;
  std::shared_ptr<DOMNode> prev;
  parent->firstChild = nullptr;
  for (auto& c : all) {
    if (c->type == DOMNodeType::Element) c = sortedElements[idx++].node;
    c->nextSibling = nullptr;
    if (!prev) {
      parent->firstChild = c;
    } else {
      prev->nextSibling = c;
    }
    prev = c;
  }
  return true;
}

void TryInferFlexOnContainer(const std::shared_ptr<DOMNode>& parent, HTMLTransformContext& ctx) {
  if (!parent || parent->type != DOMNodeType::Element) return;
  const std::string& tag = parent->name;
  if (!IsContainerTag(tag)) return;

  auto* parentResolved = ctx.findResolved(parent.get());
  if (parentResolved) {
    if (LookupLowerTrimmed(*parentResolved, "display") == "flex") return;
  }

  std::vector<ChildBox> boxes;
  bool sawNonAbsoluteChild = false;
  size_t totalElementChildren = 0;
  for (auto c = parent->firstChild; c; c = c->nextSibling) {
    if (c->type != DOMNodeType::Element) continue;
    totalElementChildren++;
    auto* cp = ctx.findResolved(c.get());
    if (!cp) {
      sawNonAbsoluteChild = true;
      continue;
    }
    ChildBox box = {};
    box.node = c;
    if (!ExtractAbsoluteBox(*cp, box)) {
      sawNonAbsoluteChild = true;
      continue;
    }
    boxes.push_back(box);
  }
  if (sawNonAbsoluteChild) return;
  if (boxes.size() < 2) return;
  if (totalElementChildren != boxes.size()) return;

  float tol = std::max(0.0f, ctx.options().flexInferenceTolerancePx);

  FlexAxis axis = FlexAxis::Row;
  std::vector<ChildBox> sorted;
  if (!InferAxis(boxes, tol, axis, sorted)) {
    ctx.warn("subset:flex-inference-skipped",
             "html: <" + tag +
                 "> children overlap on both axes; cannot recover flex layout, kept absolute",
             parent);
    return;
  }

  float padTopExisting = 0.0f;
  float padRightExisting = 0.0f;
  float padBottomExisting = 0.0f;
  float padLeftExisting = 0.0f;
  if (parentResolved) {
    if (!ParsePaddingFromResolved(*parentResolved, padTopExisting, padRightExisting,
                                  padBottomExisting, padLeftExisting)) {
      return;
    }
  }
  float mainContentLow = 0.0f;
  float mainContentHigh = 0.0f;
  float crossContentLow = 0.0f;
  float crossContentHigh = 0.0f;
  float mainPadLow = axis == FlexAxis::Row ? padLeftExisting : padTopExisting;
  float mainPadHigh = axis == FlexAxis::Row ? padRightExisting : padBottomExisting;
  float crossPadLow = axis == FlexAxis::Row ? padTopExisting : padLeftExisting;
  float crossPadHigh = axis == FlexAxis::Row ? padBottomExisting : padRightExisting;
  bool mainFromExplicit = false;
  bool crossFromExplicit = false;
  if (!ResolveContentRange(parentResolved, boxes, mainPadLow, mainPadHigh, axis, mainContentLow,
                           mainContentHigh, mainFromExplicit)) {
    return;
  }
  // Mirror the cross-axis fallback handling below: when the parent has no explicit main-axis
  // dimension, `ResolveContentRange` anchors `mainContentLow` at the first child's offset,
  // which silently absorbs any shared leading inset. Pull the low edge back to the padding
  // edge so `InferMainAxisSpacing` surfaces that inset as `paddingLeading`.
  if (!mainFromExplicit && mainContentLow > mainPadLow + tol) {
    mainContentLow = mainPadLow;
  }
  FlexAxis crossAxis = axis == FlexAxis::Row ? FlexAxis::Column : FlexAxis::Row;
  if (!ResolveContentRange(parentResolved, boxes, crossPadLow, crossPadHigh, crossAxis,
                           crossContentLow, crossContentHigh, crossFromExplicit)) {
    return;
  }
  // Lift any inset that every child shares on the cross axis into extra padding. Only safe
  // when the cross dimension came from the children-bbox fallback: an explicit parent
  // dimension already reserves the inset.
  float extraCrossLeading = 0.0f;
  float extraCrossTrailing = 0.0f;
  if (!crossFromExplicit) {
    float childCrossLoMin = std::numeric_limits<float>::infinity();
    float childCrossLoMax = -std::numeric_limits<float>::infinity();
    float childCrossHiMin = std::numeric_limits<float>::infinity();
    float childCrossHiMax = -std::numeric_limits<float>::infinity();
    for (const auto& c : boxes) {
      float lo = (crossAxis == FlexAxis::Row) ? c.left : c.top;
      float size = (crossAxis == FlexAxis::Row) ? c.width : c.height;
      childCrossLoMin = std::min(childCrossLoMin, lo);
      childCrossLoMax = std::max(childCrossLoMax, lo);
      childCrossHiMin = std::min(childCrossHiMin, lo + size);
      childCrossHiMax = std::max(childCrossHiMax, lo + size);
    }
    if (crossContentLow > crossPadLow + tol) {
      crossContentLow = crossPadLow;
    }
    if (childCrossLoMax - childCrossLoMin <= tol && childCrossLoMin - crossContentLow > tol) {
      extraCrossLeading = childCrossLoMin - crossContentLow;
      crossContentLow = childCrossLoMin;
    }
    if (childCrossHiMax - childCrossHiMin <= tol && crossContentHigh - childCrossHiMax > tol) {
      extraCrossTrailing = crossContentHigh - childCrossHiMax;
      crossContentHigh = childCrossHiMax;
    }
  }
  if (crossContentHigh <= crossContentLow) return;

  CrossAlign align = InferCrossAlign(boxes, axis, crossContentLow, crossContentHigh, tol);
  if (align == CrossAlign::Mixed) {
    ctx.warn("subset:flex-inference-skipped",
             "html: <" + tag + "> children have mixed cross-axis alignment; kept absolute", parent);
    return;
  }

  MainAxisFit fit = InferMainAxisSpacing(sorted, axis, mainContentLow, mainContentHigh, tol);
  if (!fit.ok) {
    ctx.warn("subset:flex-inference-skipped",
             "html: <" + tag + "> children have inconsistent main-axis spacing; kept absolute",
             parent);
    return;
  }

  float padTop = axis == FlexAxis::Row ? padTopExisting + extraCrossLeading : fit.paddingLeading;
  float padBottom =
      axis == FlexAxis::Row ? padBottomExisting + extraCrossTrailing : fit.paddingTrailing;
  float padLeft = axis == FlexAxis::Row ? fit.paddingLeading : padLeftExisting + extraCrossLeading;
  float padRight =
      axis == FlexAxis::Row ? fit.paddingTrailing : padRightExisting + extraCrossTrailing;

  if (!parentResolved) {
    PropertyMap fresh;
    ctx.setResolved(parent.get(), std::move(fresh));
    parentResolved = ctx.findResolved(parent.get());
  }
  ApplyFlexToParent(*parentResolved, axis, padTop, padRight, padBottom, padLeft, fit.gap, align);

  for (auto& b : boxes) {
    auto* cp = ctx.findResolved(b.node.get());
    if (!cp) continue;
    StripChildAbsolute(*cp, axis, align);
  }

  // Flex follows DOM order; the absolute children we just folded did not. Re-link the child
  // list so the inferred axis order matches what the user saw.
  bool reordered = ReorderElementChildrenToMainAxis(parent, sorted);

  ctx.warn("subset:flex-inferred",
           "html: <" + tag + "> rewritten as display:flex (" + AxisName(axis) + ", " +
               std::to_string(boxes.size()) + " children" +
               (reordered ? ", reordered to match position" : "") + ")",
           parent);
}

void WalkInferFlex(const std::shared_ptr<DOMNode>& node, HTMLTransformContext& ctx, int depth) {
  if (ShouldSkipWalkerNode(node, depth, ctx, "flex inference")) return;
  if (IsOpaqueSubtreeRoot(node)) return;
  // Recurse children first: a parent that folds into `align-items: stretch` erases its
  // children's cross-axis size, so each child must finish its own inference while its
  // explicit width/height is still intact.
  auto child = node->firstChild;
  while (child) {
    WalkInferFlex(child, ctx, depth + 1);
    child = child->nextSibling;
  }
  TryInferFlexOnContainer(node, ctx);
}

}  // namespace

void HTMLFlexInferencePass::apply(const std::shared_ptr<DOMNode>& root, HTMLTransformContext& ctx) {
  if (!root || ctx.hasFatal()) return;
  if (!ctx.options().inferFlexFromAbsolute) return;
  auto body = root->getFirstChild("body");
  if (!body) return;
  WalkInferFlex(body, ctx, 0);
}

}  // namespace pagx::html
