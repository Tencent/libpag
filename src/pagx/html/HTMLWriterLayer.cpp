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
#include <string>
#include <vector>
#include "base/utils/MathUtil.h"
#include "pagx/html/FontHoist.h"
#include "pagx/html/HTMLWriter.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/InnerShadowStyle.h"
#include "pagx/nodes/MergePath.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/RoundCorner.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/nodes/TrimPath.h"
#include "pagx/svg/SVGPathParser.h"
#include "pagx/types/MergePathMode.h"
#include "pagx/utils/ExporterUtils.h"
#include "pagx/utils/StringParser.h"

namespace pagx {

using pag::FloatNearlyZero;

// Sentinel large value used to seed min/max accumulators when computing axis-aligned bounds.
// Any realistic layer coordinate is many orders of magnitude smaller, so the first observation
// always overwrites the seed. Mirrors the same sentinel used in HTMLWriter::ComputeGeosBoundingBox.
static constexpr float BBOX_SENTINEL_LARGE = 1e9f;

// Chromium's default `line-height: normal` resolves to ≈1.17 × font-size for typical Latin
// fonts. When a span's natural line-height would exceed the TextBox's declared lineHeight we

static void EmitLeftTopCss(std::string& style, bool& positionSet, float x, float y) {
  style += ";left:" + CssFloatToString(x) + "px;top:" + CssFloatToString(y) + "px";
  positionSet = true;
}

// CollectRichTextSpans pre-scans an element list to pull out every Group that should be hoisted
// into the enclosing TextBox as a single <span> child instead of being rendered as a stand-alone
// DOM wrapper. A Group qualifies when its children are exactly one Text plus at least one
// Fill/Stroke (and nothing else); see HTMLWriter.h for the RichTextSpan struct itself.

// Outputs of the first pre-scan pass: hasUpcomingRepeater, the first TextBox seen, the rich
// text spans pulled from sibling Groups, and the derived useRichText flag (true when there is
// a TextBox with no inline elements but at least two qualifying sibling Groups).
struct RichTextScanResult {
  bool hasUpcomingRepeater = false;
  const TextBox* preScannedTextBox = nullptr;
  std::vector<RichTextSpan> richTextSpans;
  bool useRichText = false;
};

// A single text span emitted inside a TextBox container. Collected from TextBox children at the
// top level (Text + Fill / Stroke painters that apply to the spans below them) and recursively
// from nested Group children. Used by renderTextBoxWithSpans.
struct TBSpan {
  const Text* text = nullptr;
  const Fill* fill = nullptr;
  const Stroke* stroke = nullptr;
};

// Recursive helper that flattens Group children into TBSpans. Each Group level may carry its
// own Fill/Stroke which applies to every Text at that same level regardless of their relative
// order (e.g. a Group with [Text, Fill] and a Group with [Fill, Text] both paint the Text with
// the Group's Fill). The painter is resolved with a first pass over direct children before
// emitting spans, otherwise a [Text, Fill] order would leak the caller's parentFill onto the
// Text. Defined as a struct with a static method rather than a free function so the recursive
// call can refer to its own type without an explicit forward declaration.
struct GroupSpanCollector {
  static void Collect(const Group* grp, const Fill* parentFill, const Stroke* parentStroke,
                      std::vector<TBSpan>& spans) {
    const Fill* grpFill = parentFill;
    const Stroke* grpStroke = parentStroke;
    for (auto* ge : grp->elements) {
      if (ge->nodeType() == NodeType::Fill) {
        grpFill = static_cast<const Fill*>(ge);
      } else if (ge->nodeType() == NodeType::Stroke) {
        grpStroke = static_cast<const Stroke*>(ge);
      }
    }
    for (auto* ge : grp->elements) {
      if (ge->nodeType() == NodeType::Text) {
        // Collect all Text nodes in the Group; the previous single-capture pattern dropped any
        // Text beyond the first (e.g. "\nAB" in Box I).
        spans.push_back({static_cast<const Text*>(ge), grpFill, grpStroke});
      } else if (ge->nodeType() == NodeType::Group) {
        Collect(static_cast<const Group*>(ge), grpFill, grpStroke, spans);
      }
    }
  }
};

static RichTextScanResult CollectRichTextSpans(const std::vector<Element*>& elements) {
  RichTextScanResult r;
  int richTextGroupCount = 0;
  for (auto* e : elements) {
    if (e->nodeType() == NodeType::Repeater) {
      r.hasUpcomingRepeater = true;
    } else if (e->nodeType() == NodeType::TextBox) {
      r.preScannedTextBox = static_cast<const TextBox*>(e);
    } else if (e->nodeType() == NodeType::Group && r.preScannedTextBox == nullptr) {
      auto grp = static_cast<const Group*>(e);
      const Text* spanText = nullptr;
      const Fill* spanFill = nullptr;
      const Stroke* spanStroke = nullptr;
      bool isTextSpan = true;
      for (auto* ge : grp->elements) {
        auto gt = ge->nodeType();
        if (gt == NodeType::Text) {
          spanText = static_cast<const Text*>(ge);
        } else if (gt == NodeType::Fill) {
          spanFill = static_cast<const Fill*>(ge);
        } else if (gt == NodeType::Stroke) {
          spanStroke = static_cast<const Stroke*>(ge);
        } else {
          isTextSpan = false;
          break;
        }
      }
      if (isTextSpan && spanText && (spanFill || spanStroke)) {
        richTextGroupCount++;
        r.richTextSpans.push_back({spanText, spanFill, spanStroke});
      }
    }
  }
  r.useRichText = r.preScannedTextBox != nullptr && richTextGroupCount >= 2;
  return r;
}

// Pre-scan Fill/Stroke pairing at targetPlacement so the Fill branch in the main dispatch can
// skip Text elements that a later Stroke will consume (otherwise two stacked <span>s with
// identical CSS end up sub-pixel offset from each other in Chromium, producing a doubled stroke
// edge around the glyphs - visible as e.g. TYPE's "E" sprouting an extra stroke stub to its
// right, or STROKE's "R" showing stroke bleed inside the letter body). Stroke then paints Text
// with (curFill, curStroke) as a single span.
//
// Matching rule: walk elements in order, pairing each Fill at targetPlacement with the next
// Stroke at targetPlacement before another Fill appears. Unpaired Fills still paint Text
// normally. TextModifier / TextPath paths have their own dispatch and are not covered here.
static std::vector<bool> ComputeFillStrokePairing(const std::vector<Element*>& elements,
                                                  LayerPlacement targetPlacement) {
  std::vector<bool> fillWillBePairedWithStroke(elements.size(), false);
  const Fill* lastUnpairedFill = nullptr;
  size_t lastUnpairedFillIndex = 0;
  for (size_t i = 0; i < elements.size(); i++) {
    auto* e = elements[i];
    auto et = e->nodeType();
    if (et == NodeType::Fill) {
      auto* f = static_cast<const Fill*>(e);
      if (f->placement == targetPlacement) {
        lastUnpairedFill = f;
        lastUnpairedFillIndex = i;
      }
    } else if (et == NodeType::Stroke) {
      auto* s = static_cast<const Stroke*>(e);
      if (s->placement == targetPlacement && lastUnpairedFill != nullptr) {
        fillWillBePairedWithStroke[lastUnpairedFillIndex] = true;
        lastUnpairedFill = nullptr;
      }
    }
  }
  return fillWillBePairedWithStroke;
}

//==============================================================================
// HTMLWriter – element processing
//==============================================================================

void HTMLWriter::paintGeos(HTMLBuilder& out, const std::vector<GeoInfo>& geos, const Fill* fill,
                           const Stroke* stroke, const TextBox* textBox, float alpha, bool hasTrim,
                           const TrimPath* curTrim, bool hasMerge, MergePathMode mergeMode) {
  if (geos.empty()) {
    return;
  }
  bool hasText = false;
  for (auto& g : geos) {
    if (g.type == NodeType::Text) {
      hasText = true;
      break;
    }
  }
  if (hasText) {
    for (auto& g : geos) {
      if (g.type == NodeType::Text) {
        writeText(out, static_cast<const Text*>(g.element), fill, stroke, textBox, alpha);
      } else {
        std::vector<GeoInfo> single = {g};
        renderGeo(out, single, fill, stroke, alpha, hasTrim, curTrim, hasMerge, mergeMode);
      }
    }
  } else {
    renderGeo(out, geos, fill, stroke, alpha, hasTrim, curTrim, hasMerge, mergeMode);
  }
}

void HTMLWriter::writeElements(HTMLBuilder& out, const std::vector<Element*>& elements, float alpha,
                               bool distribute, LayerPlacement targetPlacement,
                               const Padding* containerPadding) {
  RecursionGuard guard(_ctx);
  if (guard.overflowed()) {
    return;
  }
  std::vector<GeoInfo> geos = {};
  const Fill* curFill = nullptr;
  const Stroke* curStroke = nullptr;
  const TextBox* curTextBox = nullptr;
  bool hasTrim = false;
  const TrimPath* curTrim = nullptr;
  bool hasMerge = false;
  MergePathMode mergeMode = MergePathMode::Append;
  float roundCornerRadius = 0.0f;
  const TextModifier* curTextModifier = nullptr;
  const TextPath* curTextPath = nullptr;

  bool hasUpcomingRepeater = false;
  std::vector<RichTextSpan> richTextSpans = {};
  bool useRichText = false;
  {
    auto scan = CollectRichTextSpans(elements);
    hasUpcomingRepeater = scan.hasUpcomingRepeater;
    richTextSpans = std::move(scan.richTextSpans);
    useRichText = scan.useRichText;
  }

  auto fillWillBePairedWithStroke = ComputeFillStrokePairing(elements, targetPlacement);

  for (size_t elemIndex = 0; elemIndex < elements.size(); elemIndex++) {
    auto* element = elements[elemIndex];
    auto type = element->nodeType();
    switch (type) {
      case NodeType::Rectangle:
      case NodeType::Ellipse:
      case NodeType::Path:
      case NodeType::Polystar:
      case NodeType::Text:
        geos.push_back({type, element, {}, containerPadding});
        break;
      case NodeType::Fill: {
        auto fill = static_cast<const Fill*>(element);
        curFill = fill;
        if (fill->placement == targetPlacement && !hasUpcomingRepeater) {
          float a = distribute ? alpha : 1.0f;
          bool deferToStroke = fillWillBePairedWithStroke[elemIndex];
          if (curTextModifier && !geos.empty()) {
            if (!deferToStroke) {
              writeTextModifier(out, geos, curTextModifier, curFill, nullptr, curTextBox, a);
            }
          } else if (curTextPath && !geos.empty()) {
            if (!deferToStroke) {
              writeTextPath(out, geos, curTextPath, curFill, nullptr, curTextBox, a);
            }
          } else if (deferToStroke) {
            // A later Stroke at this placement will paint Text with (fill, stroke) as a
            // single span. Paint shapes here with fill-only, but skip Text geos so the
            // Stroke pass can merge them into one span instead of two stacked spans.
            std::vector<GeoInfo> shapeGeos;
            for (auto& g : geos) {
              if (g.type != NodeType::Text) {
                shapeGeos.push_back(g);
              }
            }
            if (!shapeGeos.empty()) {
              paintGeos(out, shapeGeos, curFill, nullptr, curTextBox, a, hasTrim, curTrim, hasMerge,
                        mergeMode);
            }
          } else {
            paintGeos(out, geos, curFill, nullptr, curTextBox, a, hasTrim, curTrim, hasMerge,
                      mergeMode);
          }
        }
        break;
      }
      case NodeType::Stroke: {
        auto stroke = static_cast<const Stroke*>(element);
        curStroke = stroke;
        if (stroke->placement == targetPlacement && !hasUpcomingRepeater) {
          float a = distribute ? alpha : 1.0f;
          if (curTextModifier && !geos.empty()) {
            // Merge with the deferred Fill so per-character spans carry both fill and
            // stroke in a single CSS declaration, avoiding Chromium's sub-pixel offset
            // between two stacked <div>s.
            writeTextModifier(out, geos, curTextModifier, curFill, curStroke, curTextBox, a);
          } else if (curTextPath && !geos.empty()) {
            writeTextPath(out, geos, curTextPath, curFill, curStroke, curTextBox, a);
          } else if (curFill != nullptr) {
            // Split geos into Text vs Shape: the Fill+Stroke span merge only matters for Text
            // (Chromium sub-pixel offset between two stacked spans produces doubled glyph
            // edges). For Shapes, the preceding Fill branch already painted every Shape geo
            // as a standalone div/SVG, so carrying curFill here would re-paint it a second
            // time on top — which silently overwrites an earlier Fill in a multi-Fill stack
            // (e.g. painter_multiple's Fill#1 blue + Fill#2 red-multiply: curFill becomes
            // Fill#2, and emitting it again inside Stroke#1's SVG buries Fill#1's blue).
            // Shape geos only need the stroke overlay; fill is intentionally nullptr so the
            // SVG emits fill="none".
            std::vector<GeoInfo> textGeos;
            std::vector<GeoInfo> shapeGeos;
            for (auto& g : geos) {
              if (g.type == NodeType::Text) {
                textGeos.push_back(g);
              } else {
                shapeGeos.push_back(g);
              }
            }
            if (!textGeos.empty()) {
              paintGeos(out, textGeos, curFill, curStroke, curTextBox, a, hasTrim, curTrim,
                        hasMerge, mergeMode);
            }
            if (!shapeGeos.empty()) {
              paintGeos(out, shapeGeos, nullptr, curStroke, curTextBox, a, hasTrim, curTrim,
                        hasMerge, mergeMode);
            }
          } else {
            paintGeos(out, geos, nullptr, curStroke, curTextBox, a, hasTrim, curTrim, hasMerge,
                      mergeMode);
          }
        }
        break;
      }
      case NodeType::TextBox: {
        curTextBox = static_cast<const TextBox*>(element);
        if (!curTextBox->elements.empty()) {
          renderTextBoxWithSpans(out, curTextBox);
        } else if (useRichText && !richTextSpans.empty()) {
          renderTextBoxAsRichText(out, curTextBox, richTextSpans);
        }
        break;
      }
      case NodeType::TrimPath:
        hasTrim = true;
        curTrim = static_cast<const TrimPath*>(element);
        break;
      case NodeType::RoundCorner: {
        auto rc = static_cast<const RoundCorner*>(element);
        roundCornerRadius = rc->radius;
        if (roundCornerRadius > 0) {
          for (auto& g : geos) {
            if (g.type == NodeType::Rectangle || g.type == NodeType::Ellipse ||
                g.type == NodeType::Path || g.type == NodeType::Polystar) {
              PathData pathData = PathDataFromSVGString("");
              GeoToPathData(g.element, g.type, pathData);
              PathData rounded = PathDataFromSVGString("");
              ApplyRoundCorner(pathData, roundCornerRadius, rounded);
              g.modifiedPathData = PathDataToSVGString(rounded);
            }
          }
        }
        break;
      }
      case NodeType::MergePath: {
        auto mp = static_cast<const MergePath*>(element);
        hasMerge = true;
        mergeMode = mp->mode;
        curFill = nullptr;
        curStroke = nullptr;
        break;
      }
      case NodeType::TextModifier:
        curTextModifier = static_cast<const TextModifier*>(element);
        break;
      case NodeType::TextPath:
        curTextPath = static_cast<const TextPath*>(element);
        break;
      case NodeType::Group: {
        auto group = static_cast<const Group*>(element);
        if (useRichText) {
          bool isRichSpan = false;
          for (auto& span : richTextSpans) {
            if (span.text != nullptr) {
              for (auto* ge : group->elements) {
                if (ge == span.text) {
                  isRichSpan = true;
                  break;
                }
              }
            }
            if (isRichSpan) {
              break;
            }
          }
          if (isRichSpan) {
            break;
          }
        }
        bool hasPainter = false;
        bool hasText = false;
        bool hasTextBox = false;
        for (auto* ge : group->elements) {
          auto gt = ge->nodeType();
          if (gt == NodeType::Fill || gt == NodeType::Stroke) {
            hasPainter = true;
          } else if (gt == NodeType::Text) {
            hasText = true;
          } else if (gt == NodeType::TextBox) {
            hasTextBox = true;
          }
        }
        // A Group with a non-identity transform (centerX/top/left constraints, scale, rotation…)
        // must keep its own DOM wrapper when it contains a Text child. The flatten path inlines
        // path/ellipse geos via TransformPathDataToSVG but Text is emitted by writeText, which
        // reads Text::renderPosition relative to the Group — not the enclosing Layer — so a
        // flattened Group would drop its constraint offset from the span's top/left and the
        // text would collapse onto the Layer's origin (app_icons Calendar "17" symptom).
        bool groupHasTransform = !BuildGroupMatrixForHTML(group).isIdentity();
        // Only use the DOM wrapper (writeGroup) when the Group has a transform AND contains
        // Text — the wrapper is needed so Text can resolve its renderPosition in Group space.
        // Groups with alpha but no transform/text must use the flatten path so their geometry
        // propagates upward to the enclosing scope (per PAGX spec §6.3 "Child Group geometry
        // propagates upward"). A DOM-isolated div would block that propagation, causing outer
        // Painters to miss the geometry (group_isolation: outer cyan Fill can't reach the
        // inner red Rectangle, making it appear unaffected by the group's own fill).
        // TextBox children always need the DOM wrapper: the flatten loop below has no case
        // for NodeType::TextBox (TextBox carries a TextLayout subtree, not bare geometry, and
        // is not something we can fold into the geos vector). Without writeGroup the TextBox
        // would be silently dropped — pagx_features Pipeline buttons (".pagx", ".pag",
        // "Production" labels) symptom: button background renders, label text disappears.
        if ((hasPainter && hasText && groupHasTransform) || hasTextBox) {
          writeGroup(out, group, alpha, distribute);
        } else {
          flattenGroup(out, group, alpha, distribute, targetPlacement, hasUpcomingRepeater,
                       curTextBox, geos, curFill, curStroke, hasTrim, curTrim, hasMerge, mergeMode,
                       curTextPath, curTextModifier);
        }
        break;
      }
      case NodeType::Repeater: {
        auto rep = static_cast<const Repeater*>(element);
        // tgfx's Repeater::apply multiplies `copyAlpha` onto the painters that already exist
        // when the repeater runs, i.e. only Fill/Stroke nodes emitted *before* the Repeater.
        // Painters added after the Repeater attach to every copy uniformly with alpha=1.0.
        // Mirror this ordering so the HTML decay (div opacity per copy) only kicks in when
        // the fill/stroke was declared before the Repeater (showcase_mandala Middle Ring has
        // the Stroke after, so every petal must render at full alpha).
        const Fill* repFillBefore = curFill;
        const Stroke* repStrokeBefore = curStroke;
        const Fill* repFillAfter = nullptr;
        const Stroke* repStrokeAfter = nullptr;
        if (!repFillBefore || !repStrokeBefore) {
          for (auto* e : elements) {
            if (e == element) {
              continue;
            }
            if (e->nodeType() == NodeType::Fill && !repFillBefore && !repFillAfter) {
              auto f = static_cast<const Fill*>(e);
              if (f->placement == targetPlacement) {
                repFillAfter = f;
              }
            } else if (e->nodeType() == NodeType::Stroke && !repStrokeBefore && !repStrokeAfter) {
              auto s = static_cast<const Stroke*>(e);
              if (s->placement == targetPlacement) {
                repStrokeAfter = s;
              }
            }
          }
        }
        const Fill* repFill = repFillBefore ? repFillBefore : repFillAfter;
        const Stroke* repStroke = repStrokeBefore ? repStrokeBefore : repStrokeAfter;
        bool applyDecay = (repFillBefore != nullptr) || (repStrokeBefore != nullptr);
        writeRepeater(out, rep, geos, repFill, repStroke, distribute ? alpha : 1.0f, curTrim,
                      applyDecay);
        geos.clear();
        curFill = nullptr;
        curStroke = nullptr;
        hasTrim = false;
        curTrim = nullptr;
        hasMerge = false;
        mergeMode = MergePathMode::Append;
        roundCornerRadius = 0.0f;
        curTextModifier = nullptr;
        curTextPath = nullptr;
        // Clear the flag so that geometry appearing after this Repeater (e.g. rotated Group lines
        // in game_hud Background) can be painted immediately by a subsequent Fill/Stroke rather
        // than being deferred indefinitely and silently discarded.
        hasUpcomingRepeater = false;
        break;
      }
      default:
        break;
    }
  }
}

void HTMLWriter::flattenGroup(HTMLBuilder& out, const Group* group, float alpha, bool distribute,
                              LayerPlacement targetPlacement, bool hasUpcomingRepeater,
                              const TextBox* curTextBox, std::vector<GeoInfo>& geos,
                              const Fill*& curFill, const Stroke*& curStroke, bool& hasTrim,
                              const TrimPath*& curTrim, bool& hasMerge, MergePathMode& mergeMode,
                              const TextPath*& curTextPath, const TextModifier*& curTextModifier) {
  auto savedFill = curFill;
  auto savedStroke = curStroke;
  auto savedHasTrim = hasTrim;
  auto savedTrim = curTrim;
  auto savedTextPath = curTextPath;
  auto savedTextModifier = curTextModifier;
  curFill = nullptr;
  curStroke = nullptr;
  hasTrim = false;
  curTrim = nullptr;
  // A Group defines its own element scope: TextPath/TextModifier declared outside the
  // Group must not bleed in and re-apply to Text children inside the Group. Clear them
  // so the flatten loop starts clean; they are restored after the Group exits.
  curTextPath = nullptr;
  curTextModifier = nullptr;
  auto savedGeos = std::move(geos);
  geos.clear();
  std::vector<GeoInfo> groupGeos;
  Matrix gm = BuildGroupMatrixForHTML(group);
  // When a Group has alpha < 1, its Painters render with that alpha applied. In the
  // flatten path, carry the group's alpha into every paintGeos/writeTextPath/
  // writeTextModifier call so the fill-opacity matches the tgfx compositing result.
  float groupAlpha = group->alpha;
  // The group's own padding insets its children's constraint frame, mirroring Layer's
  // behaviour; propagate it so stretch-rect children avoid the `inset:0` shortcut.
  const Padding* groupPadding = group->padding.isZero() ? nullptr : &group->padding;
  // Mirror the outer `hasUpcomingRepeater` scan: a Fill/Stroke that appears before a
  // Repeater inside the group must NOT paint immediately, otherwise the pre-repeater
  // single copy would render and the Repeater's path-level expansion would have no
  // painter to attach to (complete_example Modifiers cyan ellipse only shows 1 of 5).
  bool groupHasUpcomingRepeater = false;
  for (auto* ge : group->elements) {
    if (ge->nodeType() == NodeType::Repeater) {
      groupHasUpcomingRepeater = true;
      break;
    }
  }
  for (auto* ge : group->elements) {
    auto gt = ge->nodeType();
    if (gt == NodeType::Rectangle || gt == NodeType::Ellipse || gt == NodeType::Path ||
        gt == NodeType::Polystar) {
      PathData pathData = PathDataFromSVGString("");
      GeoToPathData(ge, gt, pathData);
      if (!pathData.isEmpty()) {
        std::string svgPath =
            gm.isIdentity() ? PathDataToSVGString(pathData) : TransformPathDataToSVG(pathData, gm);
        GeoInfo info = {gt, ge, svgPath, groupPadding};
        geos.push_back(info);
        groupGeos.push_back(info);
      }
    } else if (gt == NodeType::Text) {
      GeoInfo info = {gt, ge, {}, groupPadding};
      geos.push_back(info);
      groupGeos.push_back(info);
    } else if (gt == NodeType::TextPath) {
      // Flatten path mirrors the top-level TextPath handler: capture the element so
      // the subsequent Fill/Stroke triggers per-character path emission via
      // writeTextPath instead of a plain <span> (text_path Group TextPath symptom).
      curTextPath = static_cast<const TextPath*>(ge);
    } else if (gt == NodeType::TextModifier) {
      curTextModifier = static_cast<const TextModifier*>(ge);
    } else if (gt == NodeType::Fill) {
      auto fill = static_cast<const Fill*>(ge);
      curFill = fill;
      if (fill->placement == targetPlacement && !hasUpcomingRepeater && !groupHasUpcomingRepeater) {
        float a = groupAlpha * (distribute ? alpha : 1.0f);
        if (curTextPath && !geos.empty()) {
          writeTextPath(out, geos, curTextPath, curFill, nullptr, curTextBox, a);
          geos.clear();
          groupGeos.clear();
        } else if (curTextModifier && !geos.empty()) {
          writeTextModifier(out, geos, curTextModifier, curFill, nullptr, curTextBox, a);
          geos.clear();
          groupGeos.clear();
        } else {
          paintGeos(out, geos, curFill, nullptr, curTextBox, a, hasTrim, curTrim, hasMerge,
                    mergeMode);
        }
      }
    } else if (gt == NodeType::Stroke) {
      auto stroke = static_cast<const Stroke*>(ge);
      curStroke = stroke;
      if (stroke->placement == targetPlacement && !hasUpcomingRepeater &&
          !groupHasUpcomingRepeater) {
        float a = groupAlpha * (distribute ? alpha : 1.0f);
        if (curTextPath && !geos.empty()) {
          writeTextPath(out, geos, curTextPath, curFill, curStroke, curTextBox, a);
          geos.clear();
          groupGeos.clear();
        } else if (curTextModifier && !geos.empty()) {
          writeTextModifier(out, geos, curTextModifier, curFill, curStroke, curTextBox, a);
          geos.clear();
          groupGeos.clear();
        } else {
          paintGeos(out, geos, curFill, curStroke, curTextBox, a, hasTrim, curTrim, hasMerge,
                    mergeMode);
        }
      }
    } else if (gt == NodeType::TrimPath) {
      hasTrim = true;
      curTrim = static_cast<const TrimPath*>(ge);
    } else if (gt == NodeType::Repeater) {
      // Expand the Repeater at path level inside the Group: instead of emitting N copy
      // <div>s here (which would need Group-internal painters, undoing the "defer Fill
      // to outer Repeater" contract), we grow groupGeos from K to K*copies and stamp
      // each copy's transform into the geo's modifiedPathData. The outer Repeater then
      // sees the fully expanded path set and applies its own transforms on top, which
      // matches tgfx's Repeater-inside-Group semantics (inner multiplies geometry,
      // outer multiplies the result). Alpha decay ramps on the inner Repeater would
      // need a per-geo alpha to survive this collapse; carrying one through GeoInfo is
      // an open issue, so samples relying on inner startAlpha/endAlpha will lose the
      // decay — we keep the default alpha=1 case (verify_c6) correct and flag the
      // restriction for follow-up if it surfaces.
      auto innerRep = static_cast<const Repeater*>(ge);
      int copies = static_cast<int>(std::ceil(innerRep->copies));
      if (copies <= 0 || groupGeos.empty()) {
        break;
      }
      std::vector<GeoInfo> originalGeos = groupGeos;
      // Drop original-index entries we are about to re-emit in the expansion loop. We
      // also trim `geos` (which mirrors groupGeos at this point thanks to the dual
      // push_back pattern above) so we do not paint the pre-expansion copy twice.
      geos.resize(geos.size() - originalGeos.size());
      groupGeos.clear();
      for (int i = 0; i < copies; i++) {
        Matrix cm = BuildRepeaterCopyMatrix(innerRep, i);
        // Compose (gm ∘ cm): apply the repeater copy transform in Group-local space
        // first, then translate/rotate/scale the whole Group into the enclosing Layer's
        // space via gm. The previous code pulled the already-gm-baked path out of
        // `modifiedPathData` and rotated it around the Layer origin, which multiplied
        // any Group offset by sin/cos of the rotation angle and produced huge
        // off-viewBox coordinates (complete_example Modifiers cyan Repeater symptom:
        // ellipses scattered at ±500px instead of ±28px around the Group centre).
        Matrix combined = gm.isIdentity() ? cm : (gm * cm);
        for (const auto& orig : originalGeos) {
          GeoInfo copy = orig;
          PathData pathData = PathDataFromSVGString("");
          if (orig.type == NodeType::Rectangle || orig.type == NodeType::Ellipse ||
              orig.type == NodeType::Path || orig.type == NodeType::Polystar) {
            GeoToPathData(orig.element, orig.type, pathData);
          }
          if (!pathData.isEmpty()) {
            copy.modifiedPathData = combined.isIdentity()
                                        ? PathDataToSVGString(pathData)
                                        : TransformPathDataToSVG(pathData, combined);
          }
          groupGeos.push_back(copy);
          geos.push_back(copy);
        }
      }
      // If a Fill or Stroke was declared BEFORE this Repeater, its paint was deferred
      // (see `groupHasUpcomingRepeater` above) so the expansion now needs to render the
      // N copies explicitly. Without this a Group shaped like `<Ellipse/> <Fill/>
      // <Repeater copies=5/>` would fall off the end of the loop with `groupGeos`
      // holding all 5 copies but no paintGeos call to emit them (complete_example
      // Modifiers cyan 5-ellipse symptom: only 1 copy visible when the later Fill path
      // also forgot to paint).
      if (curFill || curStroke) {
        if (hasUpcomingRepeater) {
          // An outer Repeater will consume the expanded geos as its geometry source;
          // calling paintGeos here would render them once and then clear the vector,
          // leaving the outer Repeater with an empty geo list and producing only the
          // outer copy-0 div (verify_c6_nested_repeater: 1 row instead of 18).
          // Instead, keep geos in place so the outer Repeater sees all 30 copies.
        } else {
          float a = groupAlpha * (distribute ? alpha : 1.0f);
          paintGeos(out, geos, curFill, curStroke, curTextBox, a, hasTrim, curTrim, hasMerge,
                    mergeMode);
          geos.clear();
          groupGeos.clear();
          // `groupHasUpcomingRepeater` was only about deferring the pre-repeater paint;
          // now that we have painted, any later Fill/Stroke for the same group should
          // paint immediately again (not that PAGX samples normally need both).
          groupHasUpcomingRepeater = false;
        }
      }
    } else if (gt == NodeType::Group) {
      writeGroup(out, static_cast<const Group*>(ge), alpha, distribute, gm);
    } else if (gt == NodeType::RoundCorner) {
      // Mirror the top-level RoundCorner handler (case above): apply the radius to
      // every shape geo that came before it in the group's element list, replacing
      // each geo's modifiedPathData with the rounded path. Without this a Group that
      // wraps `<Rectangle/> <RoundCorner/> <Fill/>` would paint a sharp rectangle
      // (complete_example Modifiers emerald rect symptom).
      auto rc = static_cast<const RoundCorner*>(ge);
      float rcRadius = rc->radius;
      if (rcRadius > 0) {
        for (auto& g : groupGeos) {
          if (g.type == NodeType::Rectangle || g.type == NodeType::Ellipse ||
              g.type == NodeType::Path || g.type == NodeType::Polystar) {
            PathData pathData = PathDataFromSVGString("");
            GeoToPathData(g.element, g.type, pathData);
            PathData rounded = PathDataFromSVGString("");
            ApplyRoundCorner(pathData, rcRadius, rounded);
            std::string svgPath = gm.isIdentity() ? PathDataToSVGString(rounded)
                                                  : TransformPathDataToSVG(rounded, gm);
            g.modifiedPathData = svgPath;
          }
        }
        // `geos` mirrors groupGeos — keep them in sync so the later paintGeos sees
        // the rounded data as well.
        size_t headSize = geos.size() - groupGeos.size();
        for (size_t i = 0; i < groupGeos.size(); i++) {
          geos[headSize + i].modifiedPathData = groupGeos[i].modifiedPathData;
        }
      }
    } else if (gt == NodeType::MergePath) {
      // Propagate MergePath into the shared flag used by renderSVG / canCSS, mirroring
      // the top-level element handler. Without this, a Group containing
      // `<Rectangle/> <Ellipse/> <MergePath mode="xor"/> <Fill/>` would emit the two
      // shapes as independent SVG paths instead of combining them (complete_example
      // Modifiers purple rect+ellipse xor symptom).
      auto mp = static_cast<const MergePath*>(ge);
      hasMerge = true;
      mergeMode = mp->mode;
      curFill = nullptr;
      curStroke = nullptr;
    }
  }
  // Propagate the Group's Fill/Stroke out to the enclosing element stream. Without
  // this, a Group that declares a Fill (to be consumed by an outer Repeater that
  // copies the Group's geos 18× worth of times, say) would lose the Fill on Group exit
  // — the outer Repeater would then paint uncoloured paths. Only override the outer
  // painter when it was unset; an outer painter that existed before the Group enters
  // is preserved to keep the existing "Repeater consumes pre-existing painter" rule.
  if (!savedFill && curFill) {
    savedFill = curFill;
  }
  if (!savedStroke && curStroke) {
    savedStroke = curStroke;
  }
  curFill = savedFill;
  curStroke = savedStroke;
  hasTrim = savedHasTrim;
  curTrim = savedTrim;
  curTextPath = savedTextPath;
  curTextModifier = savedTextModifier;
  savedGeos.insert(savedGeos.end(), groupGeos.begin(), groupGeos.end());
  geos = std::move(savedGeos);
}

void HTMLWriter::renderTextBoxWithSpans(HTMLBuilder& out, const TextBox* tb) {
  // TextBox with child elements: render as a positioned container with text styling.
  auto tbBounds = tb->layoutBounds();
  auto tbPos = tb->renderPosition();
  // Determine whether the TextBox has an externally-determined width/height (as opposed to
  // auto-sizing from text content). The width can come from three sources:
  //   1. Explicit authored attribute: width="160"
  //   2. Opposite-edge constraints: left="0" right="0" (resolved by PerformConstraintLayout)
  //   3. Percent of parent: percentWidth="50"
  // When none of these apply, the box is auto-sized and we emit `width:max-content` so
  // Chromium sizes it to its own text measurement (avoiding unwanted line breaks from the
  // slight metric difference between tgfx and Chromium).
  bool layoutW = !std::isnan(tb->width) || (!std::isnan(tb->left) && !std::isnan(tb->right)) ||
                 !std::isnan(tb->percentWidth);
  bool layoutH = !std::isnan(tb->height) || (!std::isnan(tb->top) && !std::isnan(tb->bottom)) ||
                 !std::isnan(tb->percentHeight);
  float tbW = tbBounds.width;
  float tbH = tbBounds.height;
  float tbLeft = tbPos.x;
  float tbTop = tbPos.y;
  std::string style = "position:absolute;left:" + CssFloatToString(tbLeft) +
                      "px;top:" + CssFloatToString(tbTop) + "px";
  if (layoutW && tbW > 0) {
    style += ";width:" + CssFloatToString(tbW) + "px";
  } else if (!layoutW) {
    // For vertical writing-mode TextBoxes, tgfx has already measured the exact column
    // width needed; use that fixed value so Chromium produces the same column layout.
    // For horizontal auto-sized boxes, keep max-content so Chromium's slightly wider
    // font metrics don't trigger an unwanted line break at the tgfx boundary.
    if (tb->writingMode == WritingMode::Vertical && tbW > 0) {
      style += ";width:" + CssFloatToString(tbW) + "px";
    } else {
      style += ";width:max-content";
    }
  }
  if (layoutH && tbH > 0) {
    style += ";height:" + CssFloatToString(tbH) + "px";
  }
  // PAGX `<TextBox padding="...">` insets the text content from the box edges:
  // tgfx subtracts padding from layoutWidth/Height before line breaking, so the
  // wrapped lines obey the inner content rectangle. Emit the same inset in CSS
  // via `padding:...; box-sizing:border-box` so Chromium wraps at the same
  // inner width — without this, layoutWidth=200 leaks the full 200 to the line
  // breaker and we get one fewer line wrap than PAGX (visible in
  // layout/padding_unified where the HTML squeezes "The text" onto line 1).
  if (!tb->padding.isZero()) {
    style += ";padding:" + PaddingToCSS(tb->padding);
    style += ";box-sizing:border-box";
  }
  if (tb->textAlign == TextAlign::Center) {
    style += ";text-align:center";
  } else if (tb->textAlign == TextAlign::End) {
    style += ";text-align:end";
  } else if (tb->textAlign == TextAlign::Justify) {
    // text-align-last:left aligns the final paragraph line (after the last <br> or at end
    // of content) to the start, matching tgfx where the last line is not justified.
    style += ";text-align:justify;text-align-last:left";
  }
  // Anchor mode: TextBox authored with width="0" / height="0" (explicit zero, not NaN)
  // uses the TextBox's origin as a single anchor point and relies on textAlign /
  // paragraphAlign to place the text relative to that point. tgfx collapses the box to
  // zero size and lets each line flow past the anchor. CSS `text-align` alone can't
  // reproduce the horizontal case because our emitted container has `width:max-content`
  // (hugging the glyph run); `display:flex; justify-content:center/flex-end` alone can't
  // reproduce the vertical case because the box height is also `max-content`. Shift the
  // entire box with `transform:translate(-X%, -Y%)` so the anchor lands on the correct
  // glyph edge — -50% for Center/Middle, -100% for End/Far.
  bool widthAnchor =
      !std::isnan(tb->width) && tb->width == 0.0f && tb->textAlign != TextAlign::Start;
  bool heightAnchor =
      !std::isnan(tb->height) && tb->height == 0.0f && tb->paragraphAlign != ParagraphAlign::Near;
  if (widthAnchor || heightAnchor) {
    std::string tx = "0";
    std::string ty = "0";
    if (widthAnchor) {
      tx = (tb->textAlign == TextAlign::Center) ? "-50%" : "-100%";
    }
    if (heightAnchor) {
      ty = (tb->paragraphAlign == ParagraphAlign::Middle) ? "-50%" : "-100%";
    }
    style += ";transform:translate(" + tx + "," + ty + ")";
  }
  if (tb->paragraphAlign != ParagraphAlign::Near) {
    style += ";display:flex;flex-direction:column";
    if (tb->paragraphAlign == ParagraphAlign::Middle) {
      style += ";justify-content:center";
    } else if (tb->paragraphAlign == ParagraphAlign::Far) {
      style += ";justify-content:flex-end";
    }
  }
  if (tb->writingMode == WritingMode::Vertical) {
    style += ";writing-mode:vertical-rl";
  }
  // Only enable word-wrap when the box has an explicit width that was authored
  // to contain multi-line text. When the width comes from tgfx's own content
  // measurement (no explicit width), Chromium's slightly wider font metrics would
  // immediately overflow that measured width and trigger an unwanted line break.
  // Vertical writing-mode boxes rely on the box height to trigger column breaks;
  // white-space:nowrap would suppress column wrapping entirely, so skip it.
  //
  // Note: container-level `white-space:pre-wrap` is avoided because pretty-printed
  // HTML puts indentation and newlines between the container <div> and its inner
  // <span> tags; `pre-wrap` on the container would render those as leading blank
  // lines. TAB handling is applied per-span instead (see spanStyle below).
  if (tb->wordWrap && layoutW && tbW > 0) {
    style += ";word-wrap:break-word";
  } else if (tb->writingMode != WritingMode::Vertical) {
    style += ";white-space:nowrap";
  }
  // Defer the Overflow::Hidden emit until after tbSpans is collected: when the
  // TextBox has a known height and line-height we can translate tgfx's whole-line
  // drop semantics into CSS `-webkit-line-clamp`, which matches tgfx much more
  // closely than a raw `overflow:hidden` pixel clip (which otherwise renders a
  // partial line whose glyphs tgfx would have dropped entirely).
  std::vector<TBSpan> tbSpans;
  const Fill* topFill = nullptr;
  const Stroke* topStroke = nullptr;
  for (auto* childElem : tb->elements) {
    if (childElem->nodeType() == NodeType::Fill) {
      auto fill = static_cast<const Fill*>(childElem);
      topFill = fill;
      for (auto& s : tbSpans) {
        if (!s.fill) {
          s.fill = fill;
        }
      }
    } else if (childElem->nodeType() == NodeType::Stroke) {
      auto stroke = static_cast<const Stroke*>(childElem);
      topStroke = stroke;
      for (auto& s : tbSpans) {
        if (!s.stroke) {
          s.stroke = stroke;
        }
      }
    } else if (childElem->nodeType() == NodeType::Text) {
      tbSpans.push_back({static_cast<const Text*>(childElem), topFill, topStroke});
    } else if (childElem->nodeType() == NodeType::Group) {
      GroupSpanCollector::Collect(static_cast<const Group*>(childElem), topFill, topStroke,
                                  tbSpans);
    }
  }
  if (!tbSpans.empty()) {
    // Honour the authored TextBox.lineHeight only. The previous fallback derived a
    // container line-height from per-glyph fontLineHeight metrics; that path was removed
    // along with Text::fontLineHeight(), so when no lineHeight is authored Chromium falls
    // back to its own line-height:normal (read from the font's OS/2 metrics).
    float lineH = tb->lineHeight > 0 ? tb->lineHeight : 0.0f;
    if (lineH > 0) {
      style += ";line-height:" + CssFloatToString(lineH) + "px";
    }
    // Translate PAGX Overflow::Hidden with a simple pixel clip. tgfx's TextLayout drops
    // any line whose bottom extends past the box. A max-height based on tgfx's line count
    // would differ from CSS when Chromium wraps lines differently; using only overflow:hidden
    // clips at the declared box boundary and matches tgfx when line counts agree, and
    // gracefully clips at the box edge when they diverge.
    if (tb->overflow == Overflow::Hidden) {
      style += ";overflow:hidden";
      if (tb->writingMode == WritingMode::Vertical) {
        // CSS overflow:hidden pixel-clips; tgfx drops entire columns. The visual difference
        // (partial column visible in HTML vs fully hidden in PAGX) is accepted as a known
        // divergence until tgfx aligns with CSS pixel-clip semantics.
      } else {
        // Horizontal overflow:hidden — CSS pixel-clips naturally; no height manipulation.
      }
    }
    // Detect RTL paragraph direction from the first span's text so text aligns right
    // within the box, matching tgfx's bidi-resolved layout (paragraphRTL=true).
    if (!tbSpans.empty() && !tbSpans[0].text->text.empty()) {
      if (TextStartsWithRTL(tbSpans[0].text->text)) {
        style += ";direction:rtl";
      }
    }
    out.openTag("div");
    out.addAttr("style", style);
    out.closeTagStart();
    bool needsInnerWrap = tb->paragraphAlign != ParagraphAlign::Near;
    if (needsInnerWrap) {
      out.openTag("div");
      out.closeTagStart();
    }
    // Track the previous span's trailing newline count and font-size so we can wrap
    // empty-line `<br>`s in the previous span's font-size when the next span has its
    // own font-size. PAGX uses the fontLineHeight of the `\n` glyph that started the
    // empty line; CSS `<br>` inherits its parent's font-size, so a bare `<br>` between
    // two spans of different sizes would inherit the container's strut and produce a
    // line-box too short for what tgfx renders.
    size_t prevTrailingBreaks = 0;
    float prevFontSize = 0;
    bool isFirstSpan = true;
    for (auto& span : tbSpans) {
      std::string spanStyle;
      // When the span's text contains U+0009 (TAB) honour it by setting
      // `white-space:pre-wrap` and an explicit `tab-size`. Applied at the span
      // level (not the container) so pretty-printed indentation/newlines between
      // container div and span tags are still collapsed normally; only the
      // characters inside the span see pre-wrap. tgfx's tab stop is
      // `effectiveFontSize * 4` (see TextLayout.cpp CreateTabGlyph).
      if (span.text && span.text->text.find('\t') != std::string::npos) {
        float spanSize = span.text->renderFontSize();
        spanStyle += "white-space:pre-wrap";
        if (spanSize > 0) {
          spanStyle += ";tab-size:" + CssFloatToString(spanSize * 4) + "px";
        }
      }
      bool spanFontHoisted = !_ctx->fontHoistSignature.fontFamily.empty() ||
                             _ctx->fontHoistSignature.renderFontSize > 0;
      if (!spanFontHoisted) {
        if (!span.text->fontFamily.empty()) {
          if (!spanStyle.empty()) spanStyle += ';';
          spanStyle += "font-family:'" + EscapeCssFontFamily(span.text->fontFamily) + "'";
        }
        if (!spanStyle.empty()) spanStyle += ';';
        spanStyle += "font-size:" + CssFloatToString(span.text->renderFontSize()) + "px";
        if (!span.text->fontStyle.empty()) {
          if (span.text->fontStyle.find("Bold") != std::string::npos) {
            spanStyle += ";font-weight:bold";
          }
          if (span.text->fontStyle.find("Italic") != std::string::npos) {
            spanStyle += ";font-style:italic";
          }
        }
        if (span.text->letterSpacing != 0.0f) {
          spanStyle += ";letter-spacing:" + CssFloatToString(span.text->letterSpacing) + "px";
        }
      }
      if (span.fill && span.fill->color) {
        auto ct = span.fill->color->nodeType();
        if (ct == NodeType::SolidColor) {
          auto sc = static_cast<const SolidColor*>(span.fill->color);
          if (!spanStyle.empty()) spanStyle += ';';
          spanStyle += "color:" + ColorToRGBA(sc->color, span.fill->alpha);
        } else {
          float ca = 1.0f;
          std::string css = colorToCSS(span.fill->color, &ca);
          if (!css.empty()) {
            if (!spanStyle.empty()) spanStyle += ';';
            spanStyle += "background:" + css;
            spanStyle += ";-webkit-background-clip:text;background-clip:text";
            spanStyle += ";-webkit-text-fill-color:transparent";
          }
        }
      }
      if (span.text->fauxBold) {
        if (!spanStyle.empty()) spanStyle += ';';
        spanStyle += "font-weight:bold";
      }
      if (span.stroke && span.stroke->color &&
          span.stroke->color->nodeType() == NodeType::SolidColor) {
        auto sc = static_cast<const SolidColor*>(span.stroke->color);
        bool hasFill = span.fill && span.fill->color;
        auto strokeCss = ResolveTextStrokeCss(span.stroke->width, span.stroke->align, hasFill);
        if (strokeCss.width > 0.0f) {
          if (!spanStyle.empty()) spanStyle += ';';
          spanStyle += "-webkit-text-stroke:" + CssFloatToString(strokeCss.width) + "px " +
                       ColorToRGBA(sc->color, span.stroke->alpha);
          if (strokeCss.paintOrderStrokeFill) {
            spanStyle += ";paint-order:stroke fill";
          }
        }
      }
      if (span.text->fauxItalic) {
        if (!spanStyle.empty()) spanStyle += ';';
        spanStyle += "font-style:italic";
      }
      float spanFontSize = span.text->renderFontSize();
      // Emit between-span <br>s: prevTrailingBreaks (from prior span's trailing \n)
      // plus CountLeadingBreaks(current span's text). The first <br> ends the prior
      // span's content line and inherits its strut naturally; each subsequent <br>
      // is an empty line whose strut comes from the corresponding `\n` owner. PAGX
      // assigns ownership: \n_1..\n_{prevTrailingBreaks} → previous span; \n_{...} on
      // → current span. Wrap empty-line `<br>`s in the owner's font-size so the line
      // box matches what tgfx computed, instead of inheriting the container strut.
      //
      // Special case: the leading \n at the very start of the TextBox (first span's
      // first leading break) has no preceding glyph in tgfx, so its
      // pendingNewlineFontLineHeight is 0 and FinishLine assigns maxLineHeight=0 to
      // that empty line. Wrap it in a zero-height span so Chromium doesn't allocate a
      // full container line-height for what tgfx draws as a zero-height empty line
      // (e.g. text "\nLine2\n\nLine4" should hug the box top in HTML the way it does
      // in PAGX native).
      size_t leadingBreaks = HTMLBuilder::CountLeadingBreaks(span.text->text);
      size_t totalBreaks = prevTrailingBreaks + leadingBreaks;
      for (size_t bi = 0; bi < totalBreaks; ++bi) {
        if (bi == 0 && isFirstSpan && prevTrailingBreaks == 0) {
          // Leading \n at TextBox start: tgfx gives this empty line height 0 (its
          // pendingNewlineFontLineHeight is 0 because no glyph precedes it). Emit
          // nothing so Chromium starts the first content line at the box top, just
          // like PAGX native — wrapping the <br> in any element still leaves the
          // forced break consuming a full line-height in Chromium's inline formatting
          // context.
        } else if (bi == 0) {
          // First <br> closes the previous span's last line — its line box is already
          // sized by the prior span's content, so a bare <br> is sufficient.
          out.emitBreaks(1);
        } else {
          // Empty-line <br>. \n_bi (1-indexed) owner: bi <= prevTrailingBreaks → prev,
          // else current span.
          float ownerFontSize = (bi <= prevTrailingBreaks) ? prevFontSize : spanFontSize;
          if (ownerFontSize > 0) {
            out.emitRaw("<span style=\"font-size:" + CssFloatToString(ownerFontSize) +
                        "px\"><br></span>");
          } else {
            out.emitBreaks(1);
          }
        }
      }
      // Detect single-span horizontal justify-with-\n upfront — when true, we emit
      // a <div> per \n-segment instead of a single <span>...<br>... so each segment
      // becomes its own justify paragraph. CSS treats every <br> as a paragraph
      // terminator for `text-align:justify` purposes, so this rewrite is the only
      // way to get the line-before-<br> justified the way tgfx does.
      bool useJustifyParagraphSplit =
          tb->textAlign == TextAlign::Justify && tb->writingMode != WritingMode::Vertical &&
          tbSpans.size() == 1 && span.text && span.text->text.find('\n') != std::string::npos;
      if (useJustifyParagraphSplit) {
        const std::string& src = span.text->text;
        size_t pos = 0;
        std::vector<std::string> segments;
        while (pos <= src.size()) {
          size_t nl = src.find('\n', pos);
          if (nl == std::string::npos) {
            segments.push_back(src.substr(pos));
            break;
          }
          segments.push_back(src.substr(pos, nl - pos));
          pos = nl + 1;
        }
        for (size_t si = 0; si < segments.size(); ++si) {
          bool isLast = (si + 1 == segments.size());
          std::string segStyle = spanStyle;
          if (!isLast) {
            if (!segStyle.empty()) segStyle += ';';
            segStyle += "text-align-last:justify";
          }
          out.openTag("div");
          if (!segStyle.empty()) {
            out.addAttr("style", segStyle);
          }
          out.closeTagWithText(RewriteLineBreakHints(segments[si]));
        }
        prevTrailingBreaks = HTMLBuilder::CountTrailingBreaks(span.text->text);
        prevFontSize = spanFontSize;
        isFirstSpan = false;
        continue;
      }
      out.openTag("span");
      out.addAttr("style", spanStyle);
      // For vertical-writing-mode TextBoxes with textAlign=justify (or an explicit
      // lineHeight that exceeds the glyph's natural advance), emit per-CJK-character
      // wrappers so each CJK glyph's inline-axis advance is pinned to the value tgfx
      // computed (including justifyGap). CSS `text-align:justify` in vertical-rl does
      // not insert inter-CJK gaps (only inter-word), so we have to wrap the glyphs
      // ourselves to reproduce tgfx's justify distribution on mixed CJK/Latin runs.
      bool usedPerCharDom = false;
      if (tb->writingMode == WritingMode::Vertical && tb->textAlign == TextAlign::Justify) {
        std::string justifiedContent = BuildVerticalJustifyContent(span.text, tb->height);
        if (!justifiedContent.empty()) {
          out.closeTagWithRawContent(justifiedContent);
          usedPerCharDom = true;
        }
      }
      if (!usedPerCharDom) {
        // Use closeTagWithTextBreaks so U+000A (from &#10;) inside the inner content
        // renders as <br> rather than being folded into a space by the browser's
        // default white-space handling. Leading/trailing <br>s are hoisted outside
        // the span by HTMLWriterLayer (above) so they can be wrapped in the
        // appropriate empty-line owner font-size.
        //
        // For vertical TextBoxes (non-justify path) inject <br> at every column
        // break tgfx computed — Chromium otherwise uses its own line-breaker which
        // doesn't know about PAGX's UAX-14 punctuation-squash rules and would split
        // tokens like 「世界」 across columns where tgfx kept them together.
        std::string spanText = (tb->writingMode == WritingMode::Vertical)
                                   ? RewriteVerticalColumnBreaks(span.text)
                                   : span.text->text;
        out.closeTagWithTextBreaks(RewriteLineBreakHints(spanText));
      }
      prevTrailingBreaks = HTMLBuilder::CountTrailingBreaks(span.text->text);
      prevFontSize = spanFontSize;
      isFirstSpan = false;
    }
    // Flush any trailing breaks left over from the last span. These are dangling
    // empty lines after the final visible content; emit them so the box's reported
    // ink height matches PAGX (relevant for boxes that auto-measure cross-axis).
    for (size_t bi = 0; bi < prevTrailingBreaks; ++bi) {
      if (bi == 0) {
        out.emitBreaks(1);
      } else if (prevFontSize > 0) {
        out.emitRaw("<span style=\"font-size:" + CssFloatToString(prevFontSize) +
                    "px\"><br></span>");
      } else {
        out.emitBreaks(1);
      }
    }
    if (needsInnerWrap) {
      out.closeTag();
    }
    out.closeTag();
  }
}

void HTMLWriter::renderTextBoxAsRichText(HTMLBuilder& out, const TextBox* tb,
                                         const std::vector<RichTextSpan>& richTextSpans) {
  auto tbPos = tb->renderPosition();
  std::string style = "position:absolute;left:" + CssFloatToString(tbPos.x) +
                      "px;top:" + CssFloatToString(tbPos.y) + "px";
  if (!std::isnan(tb->width)) {
    style += ";width:" + CssFloatToString(tb->width) + "px";
  }
  if (!std::isnan(tb->height)) {
    style += ";height:" + CssFloatToString(tb->height) + "px";
  }
  // Match the tbSpans branch above: emit TextBox padding so Chromium wraps at
  // the same inner content rect tgfx used during layout.
  if (!tb->padding.isZero()) {
    style += ";padding:" + PaddingToCSS(tb->padding);
    style += ";box-sizing:border-box";
  }
  if (tb->paragraphAlign != ParagraphAlign::Near) {
    style += ";display:flex;flex-direction:column";
    if (tb->paragraphAlign == ParagraphAlign::Middle) {
      style += ";justify-content:center";
    } else if (tb->paragraphAlign == ParagraphAlign::Far) {
      style += ";justify-content:flex-end";
    }
  }
  if (tb->textAlign == TextAlign::Center) {
    style += ";text-align:center";
  } else if (tb->textAlign == TextAlign::End) {
    style += ";text-align:end";
  } else if (tb->textAlign == TextAlign::Justify) {
    style += ";text-align:justify;text-align-last:left";
  }
  if (tb->wordWrap) {
    style += ";word-wrap:break-word";
  }
  // Defer Overflow::Hidden until after line-height is known so we can apply
  // `-webkit-line-clamp` (matching tgfx's whole-line drop) whenever eligible.
  // Honour the authored TextBox.lineHeight only. The previous fallback derived a
  // container line-height from per-glyph fontLineHeight metrics; that path was removed
  // along with Text::fontLineHeight().
  float rtLineH = tb->lineHeight > 0 ? tb->lineHeight : 0.0f;
  if (rtLineH > 0) {
    style += ";line-height:" + CssFloatToString(rtLineH) + "px";
  }
  if (tb->overflow == Overflow::Hidden) {
    style += ";overflow:hidden";
  }
  out.openTag("div");
  out.addAttr("style", style);
  out.closeTagStart();
  bool needsInnerWrap = tb->paragraphAlign != ParagraphAlign::Near;
  if (needsInnerWrap) {
    out.openTag("div");
    out.closeTagStart();
  }
  // Same between-span <br> handling as the tbSpans branch above: empty-line <br>s
  // (those that aren't terminating the prior span's content line) need to inherit
  // the fontLineHeight of the `\n` that produced them, not the container strut.
  size_t rtPrevTrailingBreaks = 0;
  float rtPrevFontSize = 0;
  for (auto& span : richTextSpans) {
    std::string spanStyle = tb->wordWrap ? "" : "white-space:nowrap";
    // Honour U+0009 TAB when present (see tbSpans branch for rationale).
    if (span.text && span.text->text.find('\t') != std::string::npos) {
      float spanSize = span.text->renderFontSize();
      if (!spanStyle.empty()) spanStyle += ';';
      spanStyle += "white-space:pre-wrap";
      if (spanSize > 0) {
        spanStyle += ";tab-size:" + CssFloatToString(spanSize * 4) + "px";
      }
    }
    bool spanFontHoisted =
        !_ctx->fontHoistSignature.fontFamily.empty() || _ctx->fontHoistSignature.renderFontSize > 0;
    if (!spanFontHoisted) {
      if (!span.text->fontFamily.empty()) {
        spanStyle += ";font-family:'" + EscapeCssFontFamily(span.text->fontFamily) + "'";
      }
      spanStyle += ";font-size:" + CssFloatToString(span.text->renderFontSize()) + "px";
      if (!span.text->fontStyle.empty()) {
        if (span.text->fontStyle.find("Bold") != std::string::npos) {
          spanStyle += ";font-weight:bold";
        }
        if (span.text->fontStyle.find("Italic") != std::string::npos) {
          spanStyle += ";font-style:italic";
        }
      }
      if (span.text->letterSpacing != 0.0f) {
        spanStyle += ";letter-spacing:" + CssFloatToString(span.text->letterSpacing) + "px";
      }
    }
    if (span.fill && span.fill->color) {
      auto ct = span.fill->color->nodeType();
      if (ct == NodeType::SolidColor) {
        auto sc = static_cast<const SolidColor*>(span.fill->color);
        spanStyle += ";color:" + ColorToRGBA(sc->color, span.fill->alpha);
      } else {
        float ca = 1.0f;
        std::string css = colorToCSS(span.fill->color, &ca);
        if (!css.empty()) {
          spanStyle += ";background:" + css;
          spanStyle += ";-webkit-background-clip:text;background-clip:text";
          spanStyle += ";-webkit-text-fill-color:transparent";
        }
      }
    }
    if (span.text->fauxBold) {
      spanStyle += ";font-weight:bold";
    }
    if (span.stroke && span.stroke->color &&
        span.stroke->color->nodeType() == NodeType::SolidColor) {
      auto sc = static_cast<const SolidColor*>(span.stroke->color);
      bool hasFill = span.fill && span.fill->color;
      auto strokeCss = ResolveTextStrokeCss(span.stroke->width, span.stroke->align, hasFill);
      if (strokeCss.width > 0.0f) {
        spanStyle += ";-webkit-text-stroke:" + CssFloatToString(strokeCss.width) + "px " +
                     ColorToRGBA(sc->color, span.stroke->alpha);
        if (strokeCss.paintOrderStrokeFill) {
          spanStyle += ";paint-order:stroke fill";
        }
      }
    }
    if (span.text->fauxItalic) {
      spanStyle += ";font-style:italic";
    }
    float rtSpanFontSize = span.text->renderFontSize();
    // Emit between-span <br>s with empty-line owner wrapping (see tbSpans branch).
    size_t rtLeadingBreaks = HTMLBuilder::CountLeadingBreaks(span.text->text);
    size_t rtTotalBreaks = rtPrevTrailingBreaks + rtLeadingBreaks;
    for (size_t bi = 0; bi < rtTotalBreaks; ++bi) {
      if (bi == 0) {
        out.emitBreaks(1);
      } else {
        float ownerFontSize = (bi <= rtPrevTrailingBreaks) ? rtPrevFontSize : rtSpanFontSize;
        if (ownerFontSize > 0) {
          out.emitRaw("<span style=\"font-size:" + CssFloatToString(ownerFontSize) +
                      "px\"><br></span>");
        } else {
          out.emitBreaks(1);
        }
      }
    }
    out.openTag("span");
    out.addAttr("style", spanStyle);
    // Use closeTagWithTextBreaks so U+000A (from &#10;) inside the inner content
    // renders as <br>; trailing breaks are handled by the next iteration via the
    // between-span emission loop above.
    //
    // For vertical TextBoxes inject <br> at every column break tgfx computed (same
    // rationale as the tbSpans path above).
    std::string rtSpanText = (tb->writingMode == WritingMode::Vertical)
                                 ? RewriteVerticalColumnBreaks(span.text)
                                 : span.text->text;
    out.closeTagWithTextBreaks(RewriteLineBreakHints(rtSpanText));
    rtPrevTrailingBreaks = HTMLBuilder::CountTrailingBreaks(span.text->text);
    rtPrevFontSize = rtSpanFontSize;
  }
  // Flush remaining trailing breaks from the last rich-text span.
  for (size_t bi = 0; bi < rtPrevTrailingBreaks; ++bi) {
    if (bi == 0) {
      out.emitBreaks(1);
    } else if (rtPrevFontSize > 0) {
      out.emitRaw("<span style=\"font-size:" + CssFloatToString(rtPrevFontSize) +
                  "px\"><br></span>");
    } else {
      out.emitBreaks(1);
    }
  }
  if (needsInnerWrap) {
    out.closeTag();
  }
  out.closeTag();
}

static std::string ClipPathFromContents(const Layer* layer) {
  auto layerBounds = layer->layoutBounds();
  float containerW = layerBounds.width;
  float containerH = layerBounds.height;
  if (containerW <= 0 || containerH <= 0) {
    return {};
  }
  for (auto* e : layer->contents) {
    if (e->nodeType() == NodeType::Rectangle) {
      auto r = static_cast<const Rectangle*>(e);
      auto bounds = r->layoutBounds();
      if (bounds.isEmpty()) {
        continue;
      }
      float top = bounds.y;
      float left = bounds.x;
      float bottom = containerH - (bounds.y + bounds.height);
      float right = containerW - (bounds.x + bounds.width);
      if (top < 0) top = 0;
      if (left < 0) left = 0;
      if (bottom < 0) bottom = 0;
      if (right < 0) right = 0;
      std::string clip = ";clip-path:inset(" + CssFloatToString(top) + "px " +
                         CssFloatToString(right) + "px " + CssFloatToString(bottom) + "px " +
                         CssFloatToString(left) + "px";
      if (r->roundness > 0) {
        clip += " round " + CssFloatToString(r->roundness) + "px";
      }
      clip += ")";
      return clip;
    }
    if (e->nodeType() == NodeType::Ellipse) {
      auto el = static_cast<const Ellipse*>(e);
      auto bounds = el->layoutBounds();
      if (bounds.isEmpty()) {
        continue;
      }
      float cx = bounds.x + bounds.width / 2;
      float cy = bounds.y + bounds.height / 2;
      return ";clip-path:ellipse(" + CssFloatToString(bounds.width / 2) + "px " +
             CssFloatToString(bounds.height / 2) + "px at " + CssFloatToString(cx) + "px " +
             CssFloatToString(cy) + "px)";
    }
  }
  return {};
}

// Returns a CSS border-radius value string (without the property prefix, e.g. "20px" or "50%")
// when the layer's visual contour can be expressed by applying border-radius directly to the
// layer's own <div>. Only matches the narrow case where the first contents child is a Rectangle
// or Ellipse that fully covers the layer: this is the only shape whose box-shadow outline must
// trace a plain rounded rectangle. Returns empty string when no matching shape exists.
//
// Used exclusively by the DropShadowStyle + BackgroundBlurStyle coexistence workaround: CSS
// `filter: drop-shadow()` establishes an isolated stacking context that breaks the sibling
// `backdrop-filter` sampling path. `box-shadow` does not establish that context, but it paints
// along the element's own border-box — only correct when we can also put the matching
// border-radius on that border-box.
static std::string LayerBoxShadowBorderRadius(const Layer* layer) {
  auto layerBounds = layer->layoutBounds();
  float containerW = layerBounds.width;
  float containerH = layerBounds.height;
  if (containerW <= 0 || containerH <= 0) {
    return {};
  }
  for (auto* e : layer->contents) {
    if (e->nodeType() == NodeType::Rectangle) {
      auto r = static_cast<const Rectangle*>(e);
      auto bounds = r->layoutBounds();
      if (bounds.isEmpty()) {
        continue;
      }
      // Rectangle must fully cover the layer (same logic as ClipPathFromContents: top/left/
      // bottom/right insets are all zero). Otherwise box-shadow would trace the wrong outline.
      if (bounds.x > 0.5f || bounds.y > 0.5f || bounds.x + bounds.width < containerW - 0.5f ||
          bounds.y + bounds.height < containerH - 0.5f) {
        return {};
      }
      if (r->roundness > 0) {
        return CssFloatToString(r->roundness) + "px";
      }
      return "0";
    }
    if (e->nodeType() == NodeType::Ellipse) {
      auto el = static_cast<const Ellipse*>(e);
      auto bounds = el->layoutBounds();
      if (bounds.isEmpty()) {
        continue;
      }
      // Ellipse must exactly match the layer bounds (centered, full size).
      if (bounds.x > 0.5f || bounds.y > 0.5f || bounds.x + bounds.width < containerW - 0.5f ||
          bounds.y + bounds.height < containerH - 0.5f) {
        return {};
      }
      return "50%";
    }
    // Non-primitive first element (Group, Path, Text, etc.) cannot be represented as a simple
    // rounded box-shadow outline, so leave the caller to fall back to filter:drop-shadow.
    return {};
  }
  return {};
}

// Describes the geometry of a layer's primary fill shape (the first Rectangle or Ellipse in
// layer->contents) in layer-local coordinates, so a sibling <div> can reproduce just that outline
// without inheriting the alpha of the layer's other descendants or filter outputs.
//
// Used by DropShadowStyle emission: rendering the shadow as a blurred sibling div whose alpha
// comes from the layer's own geometry mirrors tgfx's DropShadowStyle (source = opaque content
// silhouette), whereas applying `filter:drop-shadow` on the layer div would include every child
// and every child's filter halo in the shadow source — producing a visibly stronger/larger
// shadow, especially in nested DropShadow scenarios like drop_shadow_show_behind.pagx Row 2.
struct ShadowShape {
  float left = 0;
  float top = 0;
  float width = 0;
  float height = 0;
  // CSS border-radius value without the property name (e.g. "12px" or "50%"). Empty when the
  // shape is a square-cornered Rectangle.
  std::string radius;
  bool valid = false;
};

static ShadowShape FindLayerShadowShape(const Layer* layer) {
  ShadowShape s = {};
  // The shadow source div is a solid-filled CSS box. This only makes sense when the layer has a
  // Fill — a stroke-only layer (e.g. game_hud circle outline) must not produce a filled shadow
  // div because that would paint a solid disc over the interior of the stroke shape.
  bool hasFill = false;
  for (auto* e : layer->contents) {
    if (e->nodeType() == NodeType::Fill) {
      hasFill = true;
      break;
    }
  }
  if (!hasFill) {
    return s;
  }
  for (auto* e : layer->contents) {
    if (e->nodeType() == NodeType::Rectangle) {
      auto r = static_cast<const Rectangle*>(e);
      auto bounds = r->layoutBounds();
      if (bounds.isEmpty()) {
        continue;
      }
      s.left = bounds.x;
      s.top = bounds.y;
      s.width = bounds.width;
      s.height = bounds.height;
      if (r->roundness > 0) {
        s.radius = CssFloatToString(r->roundness) + "px";
      }
      s.valid = true;
      return s;
    }
    if (e->nodeType() == NodeType::Ellipse) {
      auto el = static_cast<const Ellipse*>(e);
      auto bounds = el->layoutBounds();
      if (bounds.isEmpty()) {
        continue;
      }
      s.left = bounds.x;
      s.top = bounds.y;
      s.width = bounds.width;
      s.height = bounds.height;
      s.radius = "50%";
      s.valid = true;
      return s;
    }
    // Non-primitive geometry (Path, Polystar, Text fill, Group, etc.) cannot be rendered as a
    // simple CSS-styled div, so fall back to the SVG filter path.
    return s;
  }
  return s;
}

void HTMLWriter::writeLayerContents(HTMLBuilder& out, const Layer* layer, float alpha,
                                    bool distribute, LayerPlacement targetPlacement) {
  // Forward the layer's padding (when non-zero) so inner GeoInfo carries it. Skipping the
  // pointer when padding is zero keeps downstream fast paths unchanged for the common case.
  const Padding* layerPadding = layer->padding.isZero() ? nullptr : &layer->padding;
  writeElements(out, layer->contents, alpha, distribute, targetPlacement, layerPadding);
}

//==============================================================================
// HTMLWriter – layer
//==============================================================================

void HTMLWriter::writeLayer(HTMLBuilder& out, const Layer* layer, float parentAlpha,
                            bool distributeAlpha, bool isFlexItem, LayoutMode parentLayout) {
  RecursionGuard guard(_ctx);
  if (guard.overflowed()) {
    return;
  }
  // Save-and-restore the six Repeater-related offset fields across this entire writeLayer
  // scope: the setters below mutate them in place, and at least one early-return path
  // (hasContent == false, after the ctx writes) exits without clearing them, which would leak
  // a stale offset to the next sibling layer's writeLayer invocation. The guard restores the
  // caller's offsets at destruction regardless of how writeLayer returns.
  RepeaterOffsetGuard offsetGuard(_ctx);
  if (!layer->visible) {
    out.openTag("div");
    out.addAttr("style", "display:none");
    if (!layer->id.empty()) {
      out.addAttr("id", layer->id);
    }
    out.closeTagSelfClosing();
    return;
  }

  bool isFlexContainer = (layer->layout != LayoutMode::None);

  std::string style;
  style.reserve(300);

  if (isFlexItem) {
    // Flex item: positioned by parent's flexbox, no absolute positioning needed.
    //
    // PAGX flex semantics (src/pagx/nodes/Layer.cpp:370, HasExplicitMainSize): a child with
    // `flex > 0` only participates in flex distribution when the main-axis size is NOT declared.
    // When the child has an explicit main-axis width/height (or percent) the declared value is
    // used verbatim and the `flex` factor is ignored. CSS shorthand `flex: N` expands to
    // `flex: N 1 0%`, which resets flex-basis to 0 and would grow the child past its declared
    // width, so we must skip the `flex` declaration whenever the PAGX child already has an
    // explicit main-axis size. `parentLayout` tells us which axis is the main axis.
    bool mainAxisDeclared = false;
    if (parentLayout == LayoutMode::Horizontal) {
      mainAxisDeclared = !std::isnan(layer->width) || !std::isnan(layer->percentWidth);
    } else if (parentLayout == LayoutMode::Vertical) {
      mainAxisDeclared = !std::isnan(layer->height) || !std::isnan(layer->percentHeight);
    }
    if (layer->flex > 0 && !mainAxisDeclared) {
      style += "flex:" + CssFloatToString(layer->flex);
    }
    // When the layer has absolute-positioned contents (Text, shapes), it needs explicit size
    // so that contents' coordinates are correct. Use pagx explicit size first, then fall back
    // to layout-resolved size. Skip the main-axis dimension when flex > 0 to let flex work.
    bool needsSize = !layer->contents.empty() || !layer->styles.empty();
    auto bounds = needsSize ? layer->layoutBounds() : Rect{};
    float outputW = !std::isnan(layer->width) ? layer->width : (needsSize ? bounds.width : NAN);
    float outputH = !std::isnan(layer->height) ? layer->height : (needsSize ? bounds.height : NAN);
    if (!std::isnan(outputW) && outputW > 0) {
      if (!style.empty()) {
        style += ';';
      }
      style += "width:" + CssFloatToString(outputW) + "px";
    }
    if (!std::isnan(outputH) && outputH > 0) {
      if (!style.empty()) {
        style += ';';
      }
      style += "height:" + CssFloatToString(outputH) + "px";
    }
    // Flex item needs position:relative for absolute-positioned contents or child layers.
    if (isFlexContainer || needsSize || !layer->children.empty()) {
      if (!style.empty()) {
        style += ';';
      }
      style += "position:relative";
    }
  } else {
    style += "position:absolute";
    auto renderPos = layer->renderPosition();
    // If the parent layer's div was shifted to cover Repeater union bounds, child layers need
    // the inverse shift added to their own left/top so they still paint at the correct document
    // position. Consume and clear the offset so it is not applied to grandchild layers.
    if (!FloatNearlyZero(_ctx->childLayerOffsetX) || !FloatNearlyZero(_ctx->childLayerOffsetY)) {
      renderPos.x += _ctx->childLayerOffsetX;
      renderPos.y += _ctx->childLayerOffsetY;
      _ctx->childLayerOffsetX = 0;
      _ctx->childLayerOffsetY = 0;
    }
    std::string transform = LayerTransformCSS(layer);
    // `positionSet` becomes true after we emit `left/top`. The Repeater branch below may need
    // to shift `renderPos` by the union-bounds offset (uL, uT) so the layer div extends into
    // the negative quadrants of the layer origin (needed for stacking-context clipping like
    // mix-blend-mode, which otherwise drops Repeater copies that live at negative x/y).
    bool positionSet = false;
    // Absolute-positioned layers need explicit size when they have contents that use inset:0,
    // or when they are flex containers that need a reference frame for child layout.
    // When contents contain a Repeater, compute the union bounds of all repeated copies
    // instead of using layoutBounds(), which only covers the original (un-repeated) geometry.
    const Repeater* repeater = nullptr;
    for (auto* e : layer->contents) {
      if (e->nodeType() == NodeType::Repeater) {
        repeater = static_cast<const Repeater*>(e);
        break;
      }
    }
    // Repeater-copy origin offset: when union bounds extend into negative quadrants we shift
    // the layer div by (uL, uT) so the layer origin sits at (-uL, -uT) inside the div; each
    // Repeater copy then needs a compensating `translate(-uL, -uT)` prepended so its content
    // still paints at the layer origin in world space. See `_ctx->repeaterOriginOffset`.
    float repeaterOffsetX = 0;
    float repeaterOffsetY = 0;
    if (repeater && repeater->copies > 0) {
      // Collect bounds of geometry elements before the Repeater.
      float geoL = BBOX_SENTINEL_LARGE, geoT = BBOX_SENTINEL_LARGE, geoR = -BBOX_SENTINEL_LARGE,
            geoB = -BBOX_SENTINEL_LARGE;
      bool hasGeo = false;
      for (auto* e : layer->contents) {
        if (e == repeater) {
          break;
        }
        auto* node = LayoutNode::AsLayoutNode(e);
        if (!node) {
          continue;
        }
        auto nb = node->layoutBounds();
        if (nb.isEmpty()) {
          continue;
        }
        geoL = std::min(geoL, nb.x);
        geoT = std::min(geoT, nb.y);
        geoR = std::max(geoR, nb.x + nb.width);
        geoB = std::max(geoB, nb.y + nb.height);
        hasGeo = true;
      }
      if (hasGeo) {
        // Transform this rect through every Repeater copy matrix and compute union bounds.
        float uL = BBOX_SENTINEL_LARGE, uT = BBOX_SENTINEL_LARGE, uR = -BBOX_SENTINEL_LARGE,
              uB = -BBOX_SENTINEL_LARGE;
        int n = static_cast<int>(std::ceil(repeater->copies));
        for (int i = 0; i < n; i++) {
          float prog = static_cast<float>(i) + repeater->offset;
          Matrix m = {};
          if (repeater->anchor.x != 0 || repeater->anchor.y != 0) {
            m = Matrix::Translate(-repeater->anchor.x, -repeater->anchor.y);
          }
          float sx = std::pow(repeater->scale.x, prog);
          float sy = std::pow(repeater->scale.y, prog);
          if (sx != 1.0f || sy != 1.0f) {
            m = Matrix::Scale(sx, sy) * m;
          }
          float rot = repeater->rotation * prog;
          if (rot != 0) {
            m = Matrix::Rotate(rot) * m;
          }
          float px = repeater->position.x * prog;
          float py = repeater->position.y * prog;
          if (px != 0 || py != 0) {
            m = Matrix::Translate(px, py) * m;
          }
          if (repeater->anchor.x != 0 || repeater->anchor.y != 0) {
            m = Matrix::Translate(repeater->anchor.x, repeater->anchor.y) * m;
          }
          // Map the 4 corners of the geometry rect.
          Point corners[4] = {{geoL, geoT}, {geoR, geoT}, {geoR, geoB}, {geoL, geoB}};
          for (auto& c : corners) {
            auto p = m.mapPoint(c);
            uL = std::min(uL, p.x);
            uT = std::min(uT, p.y);
            uR = std::max(uR, p.x);
            uB = std::max(uB, p.y);
          }
        }
        float uw = uR - uL;
        float uh = uB - uT;
        // Shift the layer div so its border-box covers all Repeater copies, including those
        // that sit in negative quadrants relative to the layer origin. Without this, the
        // stacking context created by mix-blend-mode (seen in showcase_mandala's Middle/Outer
        // Ring) clips copies whose bounds fall outside the border-box, producing the visible
        // "half the petals disappear" artefact.
        if (uL < 0 || uT < 0) {
          repeaterOffsetX = uL;
          repeaterOffsetY = uT;
        }
        EmitLeftTopCss(style, positionSet, renderPos.x + repeaterOffsetX,
                       renderPos.y + repeaterOffsetY);
        if (uw > 0) {
          style += ";width:" + CssFloatToString(uw) + "px";
        }
        if (uh > 0) {
          style += ";height:" + CssFloatToString(uh) + "px";
        }
        style += ";overflow:visible";
      }
    }
    if (!positionSet) {
      if (!FloatNearlyZero(renderPos.x) || !FloatNearlyZero(renderPos.y)) {
        EmitLeftTopCss(style, positionSet, renderPos.x, renderPos.y);
      } else if (!transform.empty()) {
        EmitLeftTopCss(style, positionSet, 0, 0);
      }
    }
    if (!transform.empty()) {
      style += ";transform:" + transform;
      style += ";transform-origin:0 0";
    }
    // Stash the Repeater origin offset so writeRepeater can prepend a compensating translate
    // to each copy's transform; the matrix stays in layer-local coordinates as before.
    _ctx->repeaterOriginOffsetX = repeaterOffsetX;
    _ctx->repeaterOriginOffsetY = repeaterOffsetY;
    // Child layers (layer->children) are absolutely positioned inside this div and use their
    // own renderPos for CSS left/top. When this div was shifted up/left by the union-bounds
    // expansion, child layers need the inverse offset added to their renderPos so they remain
    // at the correct document position (e.g. game_hud ReticleComplex: div shifts up by 228px
    // but the circle/triangle children must still paint at the layer origin, not 228px higher).
    _ctx->childLayerOffsetX = -repeaterOffsetX;
    _ctx->childLayerOffsetY = -repeaterOffsetY;
    _ctx->savedChildLayerOffsetX = _ctx->childLayerOffsetX;
    _ctx->savedChildLayerOffsetY = _ctx->childLayerOffsetY;
    // The legacy branch below handled non-Repeater layers and still runs when `repeater` is
    // null. When a Repeater was present and sized the div above, skip this fallback.
    if (!repeater && (isFlexContainer || !layer->contents.empty())) {
      auto bounds = layer->layoutBounds();
      if (bounds.width > 0) {
        style += ";width:" + CssFloatToString(bounds.width) + "px";
      }
      if (bounds.height > 0) {
        style += ";height:" + CssFloatToString(bounds.height) + "px";
      }
    }
  }

  // Flex container properties
  if (isFlexContainer) {
    style += ";display:flex;box-sizing:border-box";
    if (layer->layout == LayoutMode::Horizontal) {
      style += ";flex-direction:row";
    } else {
      style += ";flex-direction:column";
    }

    // Space-* arrangement handling. PAGX's layout fully absorbs the declared gap into the
    // redistribution (extraGap = (availableMain - totalChildMain) / denom). CSS flex instead
    // treats `gap` as a minimum and adds justify-content's distribution on top. To make CSS
    // match PAGX, emit `justify-content:space-*` natively but *drop* the declared gap, then
    // make sure the container has a concrete main-axis size equal to PAGX's measured layout
    // size (totalChildMain + totalGap when shrink-to-fit; the stretched parent size when
    // stretched). Without the explicit size, a shrink-to-fit flex container would collapse to
    // the children's bare total width and space-* would have no room to distribute.
    bool isSpace = layer->arrangement == Arrangement::SpaceBetween ||
                   layer->arrangement == Arrangement::SpaceEvenly ||
                   layer->arrangement == Arrangement::SpaceAround;
    if (!isSpace && layer->gap > 0) {
      style += ";gap:" + CssFloatToString(layer->gap) + "px";
    }
    if (!layer->padding.isZero()) {
      style += ";padding:" + PaddingToCSS(layer->padding);
    }
    if (layer->alignment != Alignment::Stretch) {
      style += ";align-items:";
      style += AlignmentToCSS(layer->alignment);
    }
    if (layer->arrangement != Arrangement::Start) {
      style += ";justify-content:";
      style += ArrangementToCSS(layer->arrangement);
    }
    // If a space-* row doesn't already carry an explicit main-axis size, pin it to the pagx
    // measured size so CSS has the same distribution budget as the tgfx layout.
    if (isSpace && isFlexItem && layer->flex <= 0) {
      bool horizontal = layer->layout == LayoutMode::Horizontal;
      auto bounds = layer->layoutBounds();
      if (horizontal && std::isnan(layer->width) && bounds.width > 0) {
        if (style.find("width:") == std::string::npos) {
          style += ";width:" + CssFloatToString(bounds.width) + "px";
        }
      } else if (!horizontal && std::isnan(layer->height) && bounds.height > 0) {
        if (style.find("height:") == std::string::npos) {
          style += ";height:" + CssFloatToString(bounds.height) + "px";
        }
      }
    }
  }

  if (layer->preserve3D) {
    style += ";transform-style:preserve-3d";
  }

  bool groupOp = layer->groupOpacity;
  float layerAlpha = layer->alpha;
  if (distributeAlpha) {
    layerAlpha *= parentAlpha;
  }
  // Tentatively set up distribution; box-shadow fallback may override further below because
  // CSS `opacity < 1` creates a stacking context that breaks the sibling `backdrop-filter`
  // sampling path, which defeats the whole point of using box-shadow in the first place.
  bool childDistribute = !groupOp && layerAlpha < 1.0f;
  bool suppressGroupOpacity = false;  // set by the box-shadow fallback path below

  if (layer->blendMode != BlendMode::Normal) {
    // PlusDarker takes a dedicated filter-based path when the pre-pass has rendered a backdrop
    // for this layer; otherwise it falls through to the mix-blend-mode:darken approximation.
    bool emittedPlusDarkerFilter = false;
    if (layer->blendMode == BlendMode::PlusDarker) {
      auto it = _ctx->plusDarkerBackdrops.find(layer);
      if (it != _ctx->plusDarkerBackdrops.end()) {
        emitPlusDarkerFilterDef(it->second);
        style += ";filter:url(#" + it->second.filterId + ")";
        emittedPlusDarkerFilter = true;
      }
    }
    if (!emittedPlusDarkerFilter) {
      auto blendStr = BlendModeToMixBlendMode(layer->blendMode);
      if (blendStr) {
        style += ";mix-blend-mode:";
        style += blendStr;
      }
    }
  }

  // isolation: isolate is needed in two cases:
  // 1) passThroughBackground=false: the Layer itself is a rendering boundary in PAGX semantics.
  // 2) Any Fill/Stroke inside this Layer carries a non-default blendMode. In tgfx the painter
  //    blend is confined to the Layer's own canvas (Fill#2 multiply only mixes with Fill#1
  //    inside the same Layer, never with ancestor backgrounds). CSS mix-blend-mode defaults
  //    to blending against the cumulative backdrop of the enclosing stacking context, so
  //    without isolation here the multiply bleeds into whatever sits behind the Layer —
  //    e.g. painter_multiple's red multiply mixing with the white card bg above, flooding
  //    the blue Fill#1 with pink. Forcing isolation on the Layer box contains the blend to
  //    the siblings that actually need it, matching tgfx's per-layer canvas semantics.
  bool needsPainterBlendIsolation = false;
  for (auto* element : layer->contents) {
    if (element == nullptr) {
      continue;
    }
    auto et = element->nodeType();
    if (et == NodeType::Fill) {
      if (static_cast<const Fill*>(element)->blendMode != BlendMode::Normal) {
        needsPainterBlendIsolation = true;
        break;
      }
    } else if (et == NodeType::Stroke) {
      if (static_cast<const Stroke*>(element)->blendMode != BlendMode::Normal) {
        needsPainterBlendIsolation = true;
        break;
      }
    }
  }
  if (!layer->passThroughBackground || needsPainterBlendIsolation) {
    style += ";isolation:isolate";
  }

  if (!layer->antiAlias) {
    style += ";shape-rendering:crispEdges;image-rendering:pixelated";
  }

  std::string filterValues;

  // Detect BlurFilter.tileMode=Mirror: we switch to a 3x3 mirror-tile DOM emission path below
  // so the layer's own `filter:` property should NOT carry the blur (it's applied on the inner
  // grid wrapper instead), and the outer div gains `overflow:hidden` to clip the mirrored tiles
  // back to the source layer's size. The tile geometry uses the layer's own size, falling back
  // to its laid-out bounds when width/height are NaN.
  bool useMirrorTile = NeedsMirrorTiling(layer);
  float mirrorTileWidth = 0;
  float mirrorTileHeight = 0;
  if (useMirrorTile) {
    auto bounds = layer->layoutBounds();
    mirrorTileWidth = !std::isnan(layer->width) ? layer->width : bounds.width;
    mirrorTileHeight = !std::isnan(layer->height) ? layer->height : bounds.height;
    if (mirrorTileWidth <= 0 || mirrorTileHeight <= 0) {
      // Fall back to decal (current behaviour) when size is indeterminate: the grid wrapper
      // needs a concrete W/H to position the 9 tiles.
      useMirrorTile = false;
    }
  }

  if (!layer->filters.empty() && !useMirrorTile) {
    std::string filterCSS = writeFilterDefs(layer->filters);
    if (!filterCSS.empty()) {
      filterValues += filterCSS;
    }
  }

  std::vector<std::pair<NodeType, const LayerStyle*>> belowStyles = {};
  std::vector<std::pair<NodeType, const LayerStyle*>> aboveStyles = {};

  // Chromium evaluates `backdrop-filter` against the nearest enclosing stacking context that
  // does NOT itself have a `filter`. When a layer has BOTH BackgroundBlurStyle (which renders
  // via child `backdrop-filter` divs) and DropShadowStyle (which usually renders via the
  // parent's `filter: drop-shadow`), the drop-shadow creates an isolated stacking context and
  // the child backdrop-filter samples the empty group surface instead of the page below.
  // Visual symptom: glass cards fail to blur whatever is behind them.
  //
  // Swap drop-shadow for `box-shadow` in that specific combination. `box-shadow` paints into
  // the same stacking context as the element's siblings, so the child backdrop-filter still
  // sees the pre-layer backdrop. `box-shadow` only reproduces the right outline when the
  // layer's visible contour IS the element's rounded border-box, which we detect via
  // LayerBoxShadowBorderRadius.
  bool hasBackdropBlurFill = false;
  for (auto* ls : layer->styles) {
    if (ls->nodeType() == NodeType::BackgroundBlurStyle && ls->blendMode == BlendMode::Normal) {
      auto blur = static_cast<const BackgroundBlurStyle*>(ls);
      if ((blur->blurX + blur->blurY) * 0.5f > 0) {
        hasBackdropBlurFill = true;
        break;
      }
    }
  }
  std::string boxShadowValue;  // non-empty when the drop-shadow branch is redirected here
  std::string
      boxShadowBorderRadius;  // border-radius to apply on the layer div when using box-shadow
  // DropShadowStyle emissions that avoid the `filter:url(...)` path. Each entry is the full
  // `style=""` value for a sibling <div> placed below the layer's children in z-order. Using a
  // sibling div (instead of the layer's own `filter`) keeps the shadow source confined to the
  // layer's primary fill shape — critical for parity with tgfx when the layer has descendants
  // that paint their own filters (otherwise those descendants leak into the shadow's alpha
  // source and darken/widen the shadow beyond the layer silhouette).
  std::vector<std::string> pendingSiblingShadows;

  for (auto* ls : layer->styles) {
    bool hasBlendMode = ls->blendMode != BlendMode::Normal;

    if (ls->nodeType() == NodeType::DropShadowStyle) {
      auto ds = static_cast<const DropShadowStyle*>(ls);
      if (hasBlendMode) {
        belowStyles.push_back({NodeType::DropShadowStyle, ls});
      } else if (ds->blurX == ds->blurY && ds->showBehindLayer && hasBackdropBlurFill &&
                 boxShadowValue.empty()) {
        std::string radius = LayerBoxShadowBorderRadius(layer);
        if (!radius.empty()) {
          // box-shadow fallback: preserves the sibling backdrop-filter sampling path. Also
          // propagate group opacity down to children, because `opacity < 1` on the layer div
          // would re-introduce the stacking context we just eliminated.
          boxShadowValue = CssFloatToString(ds->offsetX) + "px " + CssFloatToString(ds->offsetY) +
                           "px " + CssFloatToString(ds->blurX) + "px " + ColorToRGBA(ds->color);
          boxShadowBorderRadius = radius;
          suppressGroupOpacity = true;
          continue;
        }
        // Fall through to the SVG filter path below; CSS `filter: drop-shadow()` is avoided
        // because it reads source alpha as-is, so semi-transparent fills produce proportionally
        // weaker shadows while PAGX's shadow shape comes from a saturated opaque silhouette.
      }
      // Sibling-div shadow path: emit one <div> that reproduces the layer's primary fill shape
      // (Rectangle/Ellipse), tinted with the shadow color and CSS-blurred. Avoids the
      // filter-cascade darkening that a `filter:url(...)` on the layer div suffers when the
      // layer has children painting their own filters. Only showBehindLayer=true is covered —
      // false requires an erase-mask that CSS has no direct equivalent for, so it continues
      // down the SVG filter path.
      if (!hasBlendMode && ds->showBehindLayer) {
        ShadowShape shape = FindLayerShadowShape(layer);
        if (shape.valid) {
          std::string style =
              "position:absolute;left:" + CssFloatToString(shape.left + ds->offsetX) +
              "px;top:" + CssFloatToString(shape.top + ds->offsetY) +
              "px;width:" + CssFloatToString(shape.width) +
              "px;height:" + CssFloatToString(shape.height) +
              "px;background-color:" + ColorToRGBA(ds->color);
          if (!shape.radius.empty()) {
            style += ";border-radius:" + shape.radius;
          }
          // CSS filter:blur is Gaussian with the given radius; matches feGaussianBlur
          // stdDeviation numerically when the values are equal.
          float blurAvg = (ds->blurX + ds->blurY) * 0.5f;
          if (blurAvg > 0) {
            style += ";filter:blur(" + CssFloatToString(blurAvg) + "px)";
          }
          pendingSiblingShadows.push_back(style);
          continue;
        }
      }
      {
        std::string signature = "dss:" + CssFloatToString(ds->offsetX) + "," +
                                CssFloatToString(ds->offsetY) + "," + CssFloatToString(ds->blurX) +
                                "," + CssFloatToString(ds->blurY) + "," + ColorToRGBA(ds->color) +
                                "," + (ds->showBehindLayer ? "1" : "0");
        std::string fid = lookupFilterId(signature);
        if (fid.empty()) {
          fid = _ctx->nextId("filter");
          _defs->openTag("filter");
          _defs->addAttr("id", fid);
          _defs->addAttr("x", "-50%");
          _defs->addAttr("y", "-50%");
          _defs->addAttr("width", "200%");
          _defs->addAttr("height", "200%");
          _defs->addAttr("color-interpolation-filters", "sRGB");
          _defs->closeTagStart();
          // Saturate SourceAlpha into a binary silhouette so both the shadow shape and the
          // erase mask below operate on the layer contour, as required by the spec
          // (§5.3.1: "Computes shadow shape based on opaque layer content"; showBehindLayer=false
          // "use layer contour as erase mask"). Without this, semi-transparent fills would
          // produce a weaker shadow and leave partially-visible shadow inside the layer.
          _defs->openTag("feColorMatrix");
          _defs->addAttr("in", "SourceAlpha");
          _defs->addAttr("type", "matrix");
          _defs->addAttr("values", "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 255 0");
          _defs->addAttr("result", "opaqueAlpha");
          _defs->closeTagSelfClosing();
          _defs->openTag("feGaussianBlur");
          _defs->addAttr("in", "opaqueAlpha");
          _defs->addAttr("stdDeviation",
                         CssFloatToString(ds->blurX) + " " + CssFloatToString(ds->blurY));
          _defs->addAttr("result", "blur");
          _defs->closeTagSelfClosing();
          _defs->openTag("feOffset");
          _defs->addAttr("in", "blur");
          if (!FloatNearlyZero(ds->offsetX)) {
            _defs->addAttr("dx", CssFloatToString(ds->offsetX));
          }
          if (!FloatNearlyZero(ds->offsetY)) {
            _defs->addAttr("dy", CssFloatToString(ds->offsetY));
          }
          _defs->addAttr("result", "off");
          _defs->closeTagSelfClosing();
          _defs->openTag("feColorMatrix");
          _defs->addAttr("in", "off");
          _defs->addAttr("type", "matrix");
          _defs->addAttr("values", "0 0 0 0 " + CssFloatToString(ds->color.red) + " 0 0 0 0 " +
                                       CssFloatToString(ds->color.green) + " 0 0 0 0 " +
                                       CssFloatToString(ds->color.blue) + " 0 0 0 " +
                                       CssFloatToString(ds->color.alpha) + " 0");
          _defs->addAttr("result", "shadow");
          _defs->closeTagSelfClosing();
          if (!ds->showBehindLayer) {
            _defs->openTag("feComposite");
            _defs->addAttr("in", "shadow");
            _defs->addAttr("in2", "opaqueAlpha");
            _defs->addAttr("operator", "out");
            _defs->addAttr("result", "shadowClipped");
            _defs->closeTagSelfClosing();
            _defs->openTag("feMerge");
            _defs->closeTagStart();
            _defs->openTag("feMergeNode");
            _defs->addAttr("in", "shadowClipped");
            _defs->closeTagSelfClosing();
            _defs->openTag("feMergeNode");
            _defs->addAttr("in", "SourceGraphic");
            _defs->closeTagSelfClosing();
            _defs->closeTag();
          } else {
            _defs->openTag("feMerge");
            _defs->closeTagStart();
            _defs->openTag("feMergeNode");
            _defs->addAttr("in", "shadow");
            _defs->closeTagSelfClosing();
            _defs->openTag("feMergeNode");
            _defs->addAttr("in", "SourceGraphic");
            _defs->closeTagSelfClosing();
            _defs->closeTag();
          }
          _defs->closeTag();
          registerFilterId(signature, fid);
        }
        if (!filterValues.empty()) {
          filterValues += ' ';
        }
        filterValues += "url(#" + fid + ")";
      }
    } else if (ls->nodeType() == NodeType::InnerShadowStyle) {
      auto is = static_cast<const InnerShadowStyle*>(ls);
      if (hasBlendMode) {
        aboveStyles.push_back({NodeType::InnerShadowStyle, ls});
      } else {
        std::string signature = "iss:" + CssFloatToString(is->offsetX) + "," +
                                CssFloatToString(is->offsetY) + "," + CssFloatToString(is->blurX) +
                                "," + CssFloatToString(is->blurY) + "," + ColorToRGBA(is->color);
        std::string fid = lookupFilterId(signature);
        if (fid.empty()) {
          fid = _ctx->nextId("filter");
          _defs->openTag("filter");
          _defs->addAttr("id", fid);
          _defs->addAttr("x", "-50%");
          _defs->addAttr("y", "-50%");
          _defs->addAttr("width", "200%");
          _defs->addAttr("height", "200%");
          _defs->addAttr("color-interpolation-filters", "sRGB");
          _defs->closeTagStart();
          // Invert source alpha -> blur -> offset -> flood -> clip twice. Matches tgfx
          // ImageFilter::InnerShadowOnly that InnerShadowStyle::onDraw uses; the old
          // arithmetic "SourceAlpha - offsetAlpha" only produced a one-sided band that
          // vanished for low-alpha shadow colours (e.g. showcase_infographic #00000010).
          _defs->openTag("feComponentTransfer");
          _defs->addAttr("in", "SourceAlpha");
          _defs->addAttr("result", "iInv");
          _defs->closeTagStart();
          _defs->openTag("feFuncA");
          _defs->addAttr("type", "table");
          _defs->addAttr("tableValues", "1 0");
          _defs->closeTagSelfClosing();
          _defs->closeTag();
          std::string sd = CssFloatToString(is->blurX);
          if (is->blurX != is->blurY) {
            sd += " " + CssFloatToString(is->blurY);
          }
          _defs->openTag("feGaussianBlur");
          _defs->addAttr("in", "iInv");
          _defs->addAttr("stdDeviation", sd);
          _defs->addAttr("result", "iBlur");
          _defs->closeTagSelfClosing();
          std::string blurredResult = "iBlur";
          if (!FloatNearlyZero(is->offsetX) || !FloatNearlyZero(is->offsetY)) {
            _defs->openTag("feOffset");
            _defs->addAttr("in", blurredResult);
            if (!FloatNearlyZero(is->offsetX)) {
              _defs->addAttr("dx", CssFloatToString(is->offsetX));
            }
            if (!FloatNearlyZero(is->offsetY)) {
              _defs->addAttr("dy", CssFloatToString(is->offsetY));
            }
            _defs->addAttr("result", "iOff");
            _defs->closeTagSelfClosing();
            blurredResult = "iOff";
          }
          _defs->openTag("feFlood");
          _defs->addAttr("flood-color", ColorToSVGHex(is->color));
          if (is->color.alpha < 1.0f) {
            _defs->addAttr("flood-opacity", CssFloatToString(is->color.alpha));
          }
          _defs->addAttr("result", "iFlood");
          _defs->closeTagSelfClosing();
          _defs->openTag("feComposite");
          _defs->addAttr("in", "iFlood");
          _defs->addAttr("in2", blurredResult);
          _defs->addAttr("operator", "in");
          _defs->addAttr("result", "iShadow");
          _defs->closeTagSelfClosing();
          _defs->openTag("feComposite");
          _defs->addAttr("in", "iShadow");
          _defs->addAttr("in2", "SourceAlpha");
          _defs->addAttr("operator", "in");
          _defs->addAttr("result", "iClip");
          _defs->closeTagSelfClosing();
          _defs->openTag("feMerge");
          _defs->closeTagStart();
          _defs->openTag("feMergeNode");
          _defs->addAttr("in", "SourceGraphic");
          _defs->closeTagSelfClosing();
          _defs->openTag("feMergeNode");
          _defs->addAttr("in", "iClip");
          _defs->closeTagSelfClosing();
          _defs->closeTag();
          _defs->closeTag();
          registerFilterId(signature, fid);
        }
        if (!filterValues.empty()) {
          filterValues += ' ';
        }
        filterValues += "url(#" + fid + ")";
      }
    } else if (ls->nodeType() == NodeType::BackgroundBlurStyle) {
      if (hasBlendMode) {
        belowStyles.push_back({NodeType::BackgroundBlurStyle, ls});
      }
    }
  }

  if (!filterValues.empty()) {
    style += ";filter:" + filterValues;
  }
  // Note: for useMirrorTile we intentionally do NOT add `overflow:hidden` on this outer layer
  // div. The clip is instead applied on an inner wrapper whose bounds are expanded outward by
  // `2 * blur-radius` on each side, so the mirror blur halo can extend beyond the layer's
  // original rect (matching tgfx's BlurFilter output bounds = layer rect outset by 2*sigma),
  // while the flex/layout-relevant outer div keeps its original W x H.
  if (!boxShadowValue.empty()) {
    style += ";border-radius:" + boxShadowBorderRadius;
    style += ";box-shadow:" + boxShadowValue;
  }
  // Now that we know whether the box-shadow fallback fired, apply group opacity. Under the
  // fallback path we switch to child alpha distribution so the layer div itself stays opaque,
  // preserving backdrop-filter semantics.
  if (groupOp && layerAlpha < 1.0f) {
    if (suppressGroupOpacity) {
      childDistribute = true;
    } else {
      style += ";opacity:" + CssFloatToString(layerAlpha);
    }
  }

  bool needScrollRectWrapper = layer->hasScrollRect;

  if (layer->mask != nullptr) {
    if (layer->maskType == MaskType::Contour) {
      auto clipId = writeClipDef(layer->mask);
      style += ";clip-path:url(#" + clipId + ")";
    } else {
      auto pos = layer->renderPosition();
      style += writeMaskCSS(layer->mask, layer->maskType, pos);
    }
  }

  // Font hoist: when all direct-child Text nodes share the same font signature, append the
  // shared font CSS to the Layer's style so child spans can inherit it. Skip when the Layer
  // style already contains font-* properties (defensive guard against future extensions).
  FontSignature fontSig = {};
  bool fontHoisted = false;
  if (style.find("font-") == std::string::npos) {
    fontSig = CollectUniformSignature(layer->contents);
    if (!fontSig.fontFamily.empty() || fontSig.renderFontSize > 0) {
      std::string fontCss = FontSignatureToCss(fontSig);
      if (!fontCss.empty()) {
        style += ';';
        style += fontCss;
        fontHoisted = true;
      }
    }
  }

  out.openTag("div");
  if (!layer->id.empty()) {
    out.addAttr("id", layer->id);
  }
  if (!layer->name.empty()) {
    out.addAttr("data-name", layer->name);
  }
  for (auto& [key, value] : layer->customData) {
    // IsValidCustomDataKey() is enforced at import time, but programmatic PAGXDocument
    // construction can bypass that check. Re-validate here so a crafted key cannot inject
    // attribute boundary characters (", >, space) into the emitted HTML tag.
    if (!IsValidCustomDataKey(key)) {
      continue;
    }
    out.addAttr(("data-" + key).c_str(), value);
  }
  out.addAttr("style", style);

  bool hasContent = !layer->contents.empty() || !layer->children.empty();
  if (layer->composition) {
    hasContent = true;
  }
  if (!hasContent) {
    out.closeTagSelfClosing();
    return;
  }

  out.closeTagStart();

  // Emit sibling-div DropShadow shadows first so they sit beneath both belowStyles-driven
  // elements and the layer's own contents/children in painter's order.
  for (const auto& shadowStyle : pendingSiblingShadows) {
    out.openTag("div");
    out.addAttr("style", shadowStyle);
    out.closeTagSelfClosing();
  }

  for (auto& [styleType, ls] : belowStyles) {
    auto blendStr = BlendModeToMixBlendMode(ls->blendMode);
    if (styleType == NodeType::DropShadowStyle) {
      auto ds = static_cast<const DropShadowStyle*>(ls);
      std::string shadowStyle = "position:absolute;inset:0";
      if (blendStr) {
        shadowStyle += ";mix-blend-mode:";
        shadowStyle += blendStr;
      }
      bool isUniformBlur = FloatNearlyZero(ds->blurX - ds->blurY);
      if (isUniformBlur && ds->showBehindLayer) {
        shadowStyle += ";filter:drop-shadow(" + CssFloatToString(ds->offsetX) + "px " +
                       CssFloatToString(ds->offsetY) + "px " + CssFloatToString(ds->blurX) + "px " +
                       ColorToRGBA(ds->color) + ")";
        out.openTag("div");
        out.addAttr("style", shadowStyle);
        out.closeTagSelfClosing();
      } else {
        std::string filterId = _ctx->nextId("dsblend");
        _defs->openTag("filter");
        _defs->addAttr("id", filterId);
        _defs->addAttr("x", "-50%");
        _defs->addAttr("y", "-50%");
        _defs->addAttr("width", "200%");
        _defs->addAttr("height", "200%");
        _defs->closeTagStart();
        _defs->openTag("feGaussianBlur");
        _defs->addAttr("in", "SourceGraphic");
        _defs->addAttr("stdDeviation",
                       CssFloatToString(ds->blurX / 2) + " " + CssFloatToString(ds->blurY / 2));
        _defs->addAttr("result", "blur");
        _defs->closeTagSelfClosing();
        _defs->openTag("feOffset");
        _defs->addAttr("in", "blur");
        _defs->addAttr("dx", CssFloatToString(ds->offsetX));
        _defs->addAttr("dy", CssFloatToString(ds->offsetY));
        _defs->addAttr("result", "offsetBlur");
        _defs->closeTagSelfClosing();
        _defs->openTag("feFlood");
        _defs->addAttr("flood-color", ColorToSVGHex(ds->color));
        if (ds->color.alpha < 1.0f) {
          _defs->addAttr("flood-opacity", CssFloatToString(ds->color.alpha));
        }
        _defs->addAttr("result", "color");
        _defs->closeTagSelfClosing();
        _defs->openTag("feComposite");
        _defs->addAttr("in", "color");
        _defs->addAttr("in2", "offsetBlur");
        _defs->addAttr("operator", "in");
        _defs->addAttr("result", "shadow");
        _defs->closeTagSelfClosing();
        if (!ds->showBehindLayer) {
          _defs->openTag("feComposite");
          _defs->addAttr("in", "shadow");
          _defs->addAttr("in2", "SourceGraphic");
          _defs->addAttr("operator", "out");
          _defs->addAttr("result", "shadow");
          _defs->closeTagSelfClosing();
        }
        _defs->closeTag();  // </filter>
        shadowStyle += ";filter:url(#" + filterId + ")";
        out.openTag("div");
        out.addAttr("style", shadowStyle);
        out.closeTagSelfClosing();
      }
    } else if (styleType == NodeType::BackgroundBlurStyle) {
      auto blur = static_cast<const BackgroundBlurStyle*>(ls);
      float avg = (blur->blurX + blur->blurY) / 2.0f;
      if (avg > 0) {
        std::string blurStyle = "position:absolute;inset:0;backdrop-filter:blur(" +
                                CssFloatToString(avg) + "px);-webkit-backdrop-filter:blur(" +
                                CssFloatToString(avg) + "px)";
        blurStyle += ClipPathFromContents(layer);
        if (blendStr) {
          blurStyle += ";mix-blend-mode:";
          blurStyle += blendStr;
        }
        out.openTag("div");
        out.addAttr("style", blurStyle);
        out.closeTagSelfClosing();
      }
    }
  }

  for (auto* ls : layer->styles) {
    if (ls->nodeType() == NodeType::BackgroundBlurStyle && ls->blendMode == BlendMode::Normal) {
      auto blur = static_cast<const BackgroundBlurStyle*>(ls);
      float avg = (blur->blurX + blur->blurY) / 2.0f;
      if (avg > 0) {
        std::string blurStyle = "position:absolute;inset:0;backdrop-filter:blur(" +
                                CssFloatToString(avg) + "px);-webkit-backdrop-filter:blur(" +
                                CssFloatToString(avg) + "px)" + ClipPathFromContents(layer);
        // The box-shadow fallback pushes group opacity down to siblings. The backdrop-filter
        // div is its own sibling, so it also needs the distributed alpha; otherwise the blurred
        // backdrop renders at full strength while the glass/inner content appear dimmed.
        if (suppressGroupOpacity && layerAlpha < 1.0f) {
          blurStyle += ";opacity:" + CssFloatToString(layerAlpha);
        }
        out.openTag("div");
        out.addAttr("style", blurStyle);
        out.closeTagSelfClosing();
      }
    }
  }

  if (needScrollRectWrapper) {
    auto& sr = layer->scrollRect;
    out.openTag("div");
    out.addAttr("style", "position:absolute;left:0px;top:0px;width:" + CssFloatToString(sr.width) +
                             "px;height:" + CssFloatToString(sr.height) + "px;overflow:hidden");
    out.closeTagStart();
    out.openTag("div");
    out.addAttr("style", "position:relative;left:" + CssFloatToString(-sr.x) +
                             "px;top:" + CssFloatToString(-sr.y) + "px");
    out.closeTagStart();
  }

  float contentAlpha = childDistribute ? layerAlpha : 1.0f;

  // Set font hoist context so writeText can skip inherited font properties.
  auto savedFontHoistSig = _ctx->fontHoistSignature;
  if (fontHoisted) {
    _ctx->fontHoistSignature = fontSig;
  } else {
    _ctx->fontHoistSignature = {};
  }

  if (useMirrorTile) {
    // Simulate BlurFilter.tileMode=Mirror with a 3-layer DOM:
    //   <clip>      position:absolute, left:-margin, top:-margin, size=(W+2m) x (H+2m),
    //               overflow:hidden — the visible blur-halo window (matches tgfx's
    //               filterBounds = rect.makeOutset(2*sigma, 2*sigma)).
    //   <grid>        position:absolute, size=(W+2m) x (H+2m), filter:blur(radius) —
    //                 receives the 9 mirrored tiles; the blur convolves across tile seams.
    //     <tile[0..8]> 9 copies of the layer's inner content, flipped via scale(±1,±1) and
    //                  translated into a 3x3 arrangement. Center tile sits at (margin, margin),
    //                  so after the overflow:hidden clip the visible rect is exactly the
    //                  layer's original W x H plus a `margin`-wide halo on every side.
    auto* bf = static_cast<const BlurFilter*>(layer->filters[0]);
    float blurRadius =
        bf->blurX;  // NeedsMirrorTiling only matches uniform blur (filters.size()==1)
    // Match tgfx's GaussianBlurImageFilter::onFilterBounds which outsets by 2*sigma. Using the
    // same margin keeps HTML's visible halo bbox consistent with PAGX native rendering.
    float margin = 2.0f * blurRadius;
    float clipW = mirrorTileWidth + 2.0f * margin;
    float clipH = mirrorTileHeight + 2.0f * margin;

    out.openTag("div");
    std::string clipStyle = "position:absolute;left:" + CssFloatToString(-margin) +
                            "px;top:" + CssFloatToString(-margin) +
                            "px;width:" + CssFloatToString(clipW) +
                            "px;height:" + CssFloatToString(clipH) + "px;overflow:hidden";
    out.addAttr("style", clipStyle);
    out.closeTagStart();

    out.openTag("div");
    std::string gridStyle = "position:absolute;left:0;top:0;width:" + CssFloatToString(clipW) +
                            "px;height:" + CssFloatToString(clipH) + "px;filter:blur(" +
                            CssFloatToString(blurRadius) + "px)";
    out.addAttr("style", gridStyle);
    out.closeTagStart();

    // Tile positions and mirror transforms. Tile wrappers are anchored at (0,0) with size
    // W x H; `transform: translate(tx, ty) scale(sx, sy)` places them in a 3x3 grid whose
    // center tile lands at (margin, margin) inside the clip+grid wrapper. Because CSS applies
    // scale with the default origin (box center), scale(-1,1) flips pixels around the tile's
    // vertical midline without changing its bounding box, and `translate(-W + margin, margin)
    // scale(-1, 1)` places the flipped tile so its right edge (now source x=0) is adjacent to
    // the center tile's left edge (source x=0) — the mirror-repeat seam.
    struct Tile {
      float tx, ty, sx, sy;
    };
    const float W = mirrorTileWidth;
    const float H = mirrorTileHeight;
    const Tile tiles[9] = {
        {-W + margin, -H + margin, -1.0f, -1.0f},  // TL
        {margin, -H + margin, 1.0f, -1.0f},        // T
        {W + margin, -H + margin, -1.0f, -1.0f},   // TR
        {-W + margin, margin, -1.0f, 1.0f},        // L
        {margin, margin, 1.0f, 1.0f},              // C
        {W + margin, margin, -1.0f, 1.0f},         // R
        {-W + margin, H + margin, -1.0f, -1.0f},   // BL
        {margin, H + margin, 1.0f, -1.0f},         // B
        {W + margin, H + margin, -1.0f, -1.0f},    // BR
    };
    for (const auto& t : tiles) {
      out.openTag("div");
      std::string ts = "position:absolute;left:0;top:0;width:" + CssFloatToString(W) +
                       "px;height:" + CssFloatToString(H) + "px";
      if (t.tx != 0 || t.ty != 0 || t.sx != 1.0f || t.sy != 1.0f) {
        ts += ";transform:translate(" + CssFloatToString(t.tx) + "px," + CssFloatToString(t.ty) +
              "px)";
        if (t.sx != 1.0f || t.sy != 1.0f) {
          ts += " scale(" + CssFloatToString(t.sx) + "," + CssFloatToString(t.sy) + ")";
        }
      }
      out.addAttr("style", ts);
      out.closeTagStart();
      writeLayerInner(out, layer, contentAlpha, childDistribute, isFlexContainer);
      out.closeTag();
    }
    out.closeTag();  // grid wrapper (blur)
    out.closeTag();  // clip wrapper
  } else {
    writeLayerInner(out, layer, contentAlpha, childDistribute, isFlexContainer);
  }

  for (auto& [styleType, ls] : aboveStyles) {
    auto blendStr = BlendModeToMixBlendMode(ls->blendMode);
    if (styleType == NodeType::InnerShadowStyle) {
      auto is = static_cast<const InnerShadowStyle*>(ls);
      std::string signature = "issb:" + CssFloatToString(is->offsetX) + "," +
                              CssFloatToString(is->offsetY) + "," + CssFloatToString(is->blurX) +
                              "," + CssFloatToString(is->blurY) + "," + ColorToRGBA(is->color);
      std::string fid = lookupFilterId(signature);
      if (fid.empty()) {
        fid = _ctx->nextId("isf");
        _defs->openTag("filter");
        _defs->addAttr("id", fid);
        _defs->addAttr("x", "-50%");
        _defs->addAttr("y", "-50%");
        _defs->addAttr("width", "200%");
        _defs->addAttr("height", "200%");
        _defs->addAttr("color-interpolation-filters", "sRGB");
        _defs->closeTagStart();
        _defs->openTag("feOffset");
        _defs->addAttr("in", "SourceAlpha");
        if (!FloatNearlyZero(is->offsetX)) {
          _defs->addAttr("dx", CssFloatToString(is->offsetX));
        }
        if (!FloatNearlyZero(is->offsetY)) {
          _defs->addAttr("dy", CssFloatToString(is->offsetY));
        }
        _defs->addAttr("result", "iOff");
        _defs->closeTagSelfClosing();
        _defs->openTag("feComposite");
        _defs->addAttr("in", "SourceAlpha");
        _defs->addAttr("in2", "iOff");
        _defs->addAttr("operator", "arithmetic");
        _defs->addAttr("k2", "-1");
        _defs->addAttr("k3", "1");
        _defs->addAttr("result", "iComp");
        _defs->closeTagSelfClosing();
        std::string sd = CssFloatToString(is->blurX);
        if (is->blurX != is->blurY) {
          sd += " " + CssFloatToString(is->blurY);
        }
        _defs->openTag("feGaussianBlur");
        _defs->addAttr("in", "iComp");
        _defs->addAttr("stdDeviation", sd);
        _defs->addAttr("result", "iBlur");
        _defs->closeTagSelfClosing();
        _defs->openTag("feColorMatrix");
        _defs->addAttr("in", "iBlur");
        _defs->addAttr("type", "matrix");
        _defs->addAttr("values", "0 0 0 0 " + CssFloatToString(is->color.red) + " 0 0 0 0 " +
                                     CssFloatToString(is->color.green) + " 0 0 0 0 " +
                                     CssFloatToString(is->color.blue) + " 0 0 0 " +
                                     CssFloatToString(is->color.alpha) + " 0");
        _defs->closeTagSelfClosing();
        _defs->closeTag();
        registerFilterId(signature, fid);
      }
      std::string shadowStyle = "position:absolute;inset:0;filter:url(#" + fid + ")";
      if (blendStr) {
        shadowStyle += ";mix-blend-mode:";
        shadowStyle += blendStr;
      }
      out.openTag("div");
      out.addAttr("style", shadowStyle);
      out.closeTagSelfClosing();
    }
  }

  if (needScrollRectWrapper) {
    out.closeTag();  // inner offset div
    out.closeTag();  // clip wrapper div
  }

  _ctx->fontHoistSignature = savedFontHoistSig;

  out.closeTag();
}

void HTMLWriter::writeLayerInner(HTMLBuilder& out, const Layer* layer, float contentAlpha,
                                 bool childDistribute, bool isFlexContainer) {
  // Snapshot the child-layer offset before any recursive processing; writeLayerContents and
  // writeComposition may invoke nested writeLayer calls that overwrite savedChildLayerOffset.
  float childOffX = _ctx->savedChildLayerOffsetX;
  float childOffY = _ctx->savedChildLayerOffsetY;

  bool hasForeground = false;
  for (auto* e : layer->contents) {
    if (e->nodeType() == NodeType::Fill) {
      if (static_cast<const Fill*>(e)->placement == LayerPlacement::Foreground) {
        hasForeground = true;
        break;
      }
    } else if (e->nodeType() == NodeType::Stroke) {
      if (static_cast<const Stroke*>(e)->placement == LayerPlacement::Foreground) {
        hasForeground = true;
        break;
      }
    }
  }

  writeLayerContents(out, layer, contentAlpha, childDistribute, LayerPlacement::Background);

  if (layer->composition) {
    writeComposition(out, layer->composition, contentAlpha, childDistribute);
  }

  // Restore childLayerOffset before each child writeLayer call: the offset is consumed (cleared)
  // by each child's own writeLayer entry, so it must be re-set for every sibling. We snapshot
  // the value into local variables here — after writeLayerContents, the ctx values may have been
  // overwritten by recursive writeLayer calls inside nested Group/Composition processing.
  // savedChildLayerOffset* was set by the parent writeLayer immediately after computing the
  // Repeater union-bounds shift; for layers without a Repeater it is (0,0).
  for (auto* child : layer->children) {
    bool childIsFlexItem = isFlexContainer && child->includeInLayout;
    _ctx->childLayerOffsetX = childOffX;
    _ctx->childLayerOffsetY = childOffY;
    writeLayer(out, child, contentAlpha, childDistribute, childIsFlexItem, layer->layout);
  }
  _ctx->savedChildLayerOffsetX = 0;
  _ctx->savedChildLayerOffsetY = 0;

  if (hasForeground) {
    writeLayerContents(out, layer, contentAlpha, childDistribute, LayerPlacement::Foreground);
  }
}

void HTMLWriter::emitPlusDarkerFilterDef(const PlusDarkerBackdrop& backdrop) {
  // filterUnits/primitiveUnits default to "objectBoundingBox" / "userSpaceOnUse" respectively in
  // SVG, which makes percentage widths refer to the bounding box while primitive widths refer to
  // user-space units (pixels) — a mix that is hard to reason about. We pin both to userSpaceOnUse
  // so all coordinates are plain CSS pixels; the filter region matches the layer's cropped bbox
  // exactly, and feImage's width/height match the backdrop PNG dimensions 1:1.
  _defs->openTag("filter");
  _defs->addAttr("id", backdrop.filterId);
  _defs->addAttr("x", "0");
  _defs->addAttr("y", "0");
  _defs->addAttr("width", CssFloatToString(backdrop.cropWidth));
  _defs->addAttr("height", CssFloatToString(backdrop.cropHeight));
  _defs->addAttr("filterUnits", "userSpaceOnUse");
  _defs->addAttr("primitiveUnits", "userSpaceOnUse");
  _defs->addAttr("color-interpolation-filters", "sRGB");
  _defs->closeTagStart();
  _defs->openTag("feImage");
  _defs->addAttr("href", backdrop.backdropDataURL);
  _defs->addAttr("x", "0");
  _defs->addAttr("y", "0");
  _defs->addAttr("width", CssFloatToString(backdrop.cropWidth));
  _defs->addAttr("height", CssFloatToString(backdrop.cropHeight));
  _defs->addAttr("preserveAspectRatio", "none");
  _defs->addAttr("result", "bg");
  _defs->closeTagSelfClosing();
  _defs->openTag("feComposite");
  _defs->addAttr("in", "SourceGraphic");
  _defs->addAttr("in2", "bg");
  _defs->addAttr("operator", "arithmetic");
  _defs->addAttr("k1", "0");
  _defs->addAttr("k2", "1");
  _defs->addAttr("k3", "1");
  _defs->addAttr("k4", "-1");
  _defs->closeTagSelfClosing();
  _defs->closeTag();
}

}  // namespace pagx
