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
#include "pagx/html/HTMLWriter.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
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
#include "pagx/utils/StringParser.h"

namespace pagx {

using pag::FloatNearlyZero;

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
                               bool distribute, LayerPlacement targetPlacement) {
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
  const TextBox* preScannedTextBox = nullptr;
  struct RichTextSpan {
    const Text* text = nullptr;
    const Fill* fill = nullptr;
    const Stroke* stroke = nullptr;
  };
  std::vector<RichTextSpan> richTextSpans = {};
  int richTextGroupCount = 0;
  for (auto* e : elements) {
    if (e->nodeType() == NodeType::Repeater) {
      hasUpcomingRepeater = true;
    } else if (e->nodeType() == NodeType::TextBox) {
      preScannedTextBox = static_cast<const TextBox*>(e);
    } else if (e->nodeType() == NodeType::Group && preScannedTextBox == nullptr) {
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
        richTextSpans.push_back({spanText, spanFill, spanStroke});
      }
    }
  }
  bool useRichText = preScannedTextBox != nullptr && richTextGroupCount >= 2;

  for (auto* element : elements) {
    auto type = element->nodeType();
    switch (type) {
      case NodeType::Rectangle:
      case NodeType::Ellipse:
      case NodeType::Path:
      case NodeType::Polystar:
      case NodeType::Text:
        geos.push_back({type, element, {}});
        break;
      case NodeType::Fill: {
        auto fill = static_cast<const Fill*>(element);
        curFill = fill;
        if (fill->placement == targetPlacement && !hasUpcomingRepeater) {
          float a = distribute ? alpha : 1.0f;
          if (curTextModifier && !geos.empty()) {
            writeTextModifier(out, geos, curTextModifier, curFill, nullptr, curTextBox, a);
          } else if (curTextPath && !geos.empty()) {
            writeTextPath(out, geos, curTextPath, curFill, nullptr, curTextBox, a);
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
            writeTextModifier(out, geos, curTextModifier, nullptr, curStroke, curTextBox, a);
          } else if (curTextPath && !geos.empty()) {
            writeTextPath(out, geos, curTextPath, nullptr, curStroke, curTextBox, a);
          } else {
            paintGeos(out, geos, nullptr, curStroke, curTextBox, a, hasTrim, curTrim, hasMerge,
                      mergeMode);
          }
        }
        break;
      }
      case NodeType::TextBox: {
        curTextBox = static_cast<const TextBox*>(element);
        if (useRichText && !richTextSpans.empty()) {
          auto tb = curTextBox;
          std::string style = "position:absolute;left:" + FloatToString(tb->position.x) +
                              "px;top:" + FloatToString(tb->position.y) + "px";
          if (!std::isnan(tb->width)) {
            style += ";width:" + FloatToString(tb->width) + "px";
          }
          if (!std::isnan(tb->height)) {
            style += ";height:" + FloatToString(tb->height) + "px";
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
            style += ";text-align:justify";
          }
          if (tb->wordWrap) {
            style += ";word-wrap:break-word";
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
          for (auto& span : richTextSpans) {
            std::string spanStyle = tb->wordWrap ? "white-space:pre-wrap" : "white-space:pre";
            if (!span.text->fontFamily.empty()) {
              spanStyle += ";font-family:'" + span.text->fontFamily + "'";
            }
            spanStyle += ";font-size:" + FloatToString(span.text->fontSize) + "px";
            if (!span.text->fontStyle.empty()) {
              if (span.text->fontStyle.find("Bold") != std::string::npos) {
                spanStyle += ";font-weight:bold";
              }
              if (span.text->fontStyle.find("Italic") != std::string::npos) {
                spanStyle += ";font-style:italic";
              }
            }
            if (span.text->letterSpacing != 0.0f) {
              spanStyle += ";letter-spacing:" + FloatToString(span.text->letterSpacing) + "px";
            }
            if (span.fill && span.fill->color &&
                span.fill->color->nodeType() == NodeType::SolidColor) {
              auto sc = static_cast<const SolidColor*>(span.fill->color);
              spanStyle += ";color:" + ColorToRGBA(sc->color, span.fill->alpha);
            }
            if (span.stroke && span.stroke->color &&
                span.stroke->color->nodeType() == NodeType::SolidColor) {
              auto sc = static_cast<const SolidColor*>(span.stroke->color);
              spanStyle += ";-webkit-text-stroke:" + FloatToString(span.stroke->width) + "px " +
                           ColorToRGBA(sc->color, span.stroke->alpha);
              spanStyle += ";paint-order:stroke fill";
            }
            out.openTag("span");
            out.addAttr("style", spanStyle);
            out.closeTagWithText(span.text->text);
          }
          if (needsInnerWrap) {
            out.closeTag();
          }
          out.closeTag();
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
        for (auto* ge : group->elements) {
          auto gt = ge->nodeType();
          if (gt == NodeType::Fill || gt == NodeType::Stroke) {
            hasPainter = true;
            break;
          }
        }
        if (hasPainter) {
          writeGroup(out, group, alpha, distribute);
        } else {
          Matrix gm = BuildGroupMatrix(group);
          for (auto* ge : group->elements) {
            auto gt = ge->nodeType();
            if (gt == NodeType::Rectangle || gt == NodeType::Ellipse || gt == NodeType::Path ||
                gt == NodeType::Polystar) {
              PathData pathData = PathDataFromSVGString("");
              GeoToPathData(ge, gt, pathData);
              if (!pathData.isEmpty()) {
                std::string svgPath = gm.isIdentity() ? PathDataToSVGString(pathData)
                                                      : TransformPathDataToSVG(pathData, gm);
                geos.push_back({gt, ge, svgPath});
              }
            } else if (gt == NodeType::Text) {
              geos.push_back({gt, ge, {}});
            } else if (gt == NodeType::Group) {
              writeGroup(out, static_cast<const Group*>(ge), alpha, distribute);
            }
          }
        }
        break;
      }
      case NodeType::Repeater: {
        auto rep = static_cast<const Repeater*>(element);
        const Fill* repFill = curFill;
        const Stroke* repStroke = curStroke;
        if (!repFill || !repStroke) {
          for (auto* e : elements) {
            if (e == element) {
              continue;
            }
            if (e->nodeType() == NodeType::Fill && !repFill) {
              auto f = static_cast<const Fill*>(e);
              if (f->placement == targetPlacement) {
                repFill = f;
              }
            } else if (e->nodeType() == NodeType::Stroke && !repStroke) {
              auto s = static_cast<const Stroke*>(e);
              if (s->placement == targetPlacement) {
                repStroke = s;
              }
            }
          }
        }
        writeRepeater(out, rep, geos, repFill, repStroke, distribute ? alpha : 1.0f, curTrim);
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
        break;
      }
      default:
        break;
    }
  }
}

void HTMLWriter::writeLayerContents(HTMLBuilder& out, const Layer* layer, float alpha,
                                    bool distribute, LayerPlacement targetPlacement) {
  writeElements(out, layer->contents, alpha, distribute, targetPlacement);
}

//==============================================================================
// HTMLWriter – layer
//==============================================================================

void HTMLWriter::writeLayer(HTMLBuilder& out, const Layer* layer, float parentAlpha,
                            bool distributeAlpha, bool isFlexItem) {
  if (!layer->visible) {
    out.openTag("div");
    out.addAttr("class", "pagx-layer");
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
    if (layer->flex > 0) {
      style += "flex:" + FloatToString(layer->flex);
    }
    if (!std::isnan(layer->width)) {
      if (!style.empty()) {
        style += ';';
      }
      style += "width:" + FloatToString(layer->width) + "px";
    }
    if (!std::isnan(layer->height)) {
      if (!style.empty()) {
        style += ';';
      }
      style += "height:" + FloatToString(layer->height) + "px";
    }
    // Flex container also needs position:relative for absolute-positioned contents.
    if (isFlexContainer || !layer->contents.empty() || !layer->styles.empty()) {
      if (!style.empty()) {
        style += ';';
      }
      style += "position:relative";
    }
  } else {
    style += "position:absolute";
    std::string transform = LayerTransformCSS(layer);
    if (!transform.empty()) {
      style += ";transform:" + transform;
      style += ";transform-origin:0 0";
    }
  }

  // Flex container properties
  if (isFlexContainer) {
    style += ";display:flex";
    if (layer->layout == LayoutMode::Horizontal) {
      style += ";flex-direction:row";
    } else {
      style += ";flex-direction:column";
    }
    if (layer->gap > 0) {
      style += ";gap:" + FloatToString(layer->gap) + "px";
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
  }

  if (layer->preserve3D) {
    style += ";transform-style:preserve-3d";
  }

  bool groupOp = layer->groupOpacity;
  float layerAlpha = layer->alpha;
  if (distributeAlpha) {
    layerAlpha *= parentAlpha;
  }
  bool childDistribute = !groupOp && layerAlpha < 1.0f;

  if (groupOp && layerAlpha < 1.0f) {
    style += ";opacity:" + FloatToString(layerAlpha);
  }

  if (layer->blendMode != BlendMode::Normal) {
    auto blendStr = BlendModeToMixBlendMode(layer->blendMode);
    if (blendStr) {
      style += ";mix-blend-mode:";
      style += blendStr;
    }
  }

  if (!layer->passThroughBackground) {
    style += ";isolation:isolate";
  }

  if (!layer->antiAlias) {
    style += ";shape-rendering:crispEdges;image-rendering:pixelated";
  }

  std::string filterValues;

  if (!layer->filters.empty()) {
    std::string filterCSS = writeFilterDefs(layer->filters);
    if (!filterCSS.empty()) {
      filterValues += filterCSS;
    }
  }

  std::vector<std::pair<NodeType, const LayerStyle*>> belowStyles = {};
  std::vector<std::pair<NodeType, const LayerStyle*>> aboveStyles = {};

  for (auto* ls : layer->styles) {
    bool hasBlendMode = ls->blendMode != BlendMode::Normal;

    if (ls->nodeType() == NodeType::DropShadowStyle) {
      auto ds = static_cast<const DropShadowStyle*>(ls);
      if (hasBlendMode) {
        belowStyles.push_back({NodeType::DropShadowStyle, ls});
      } else if (ds->blurX == ds->blurY && ds->showBehindLayer) {
        if (!filterValues.empty()) {
          filterValues += ' ';
        }
        filterValues += "drop-shadow(" + FloatToString(ds->offsetX) + "px " +
                        FloatToString(ds->offsetY) + "px " + FloatToString(ds->blurX) + "px " +
                        ColorToRGBA(ds->color) + ")";
      } else {
        std::string fid = _ctx->nextId("filter");
        _defs->openTag("filter");
        _defs->addAttr("id", fid);
        _defs->addAttr("x", "-50%");
        _defs->addAttr("y", "-50%");
        _defs->addAttr("width", "200%");
        _defs->addAttr("height", "200%");
        _defs->addAttr("color-interpolation-filters", "sRGB");
        _defs->closeTagStart();
        _defs->openTag("feGaussianBlur");
        _defs->addAttr("in", "SourceAlpha");
        _defs->addAttr("stdDeviation", FloatToString(ds->blurX) + " " + FloatToString(ds->blurY));
        _defs->addAttr("result", "blur");
        _defs->closeTagSelfClosing();
        _defs->openTag("feOffset");
        _defs->addAttr("in", "blur");
        if (!FloatNearlyZero(ds->offsetX)) {
          _defs->addAttr("dx", FloatToString(ds->offsetX));
        }
        if (!FloatNearlyZero(ds->offsetY)) {
          _defs->addAttr("dy", FloatToString(ds->offsetY));
        }
        _defs->addAttr("result", "off");
        _defs->closeTagSelfClosing();
        _defs->openTag("feColorMatrix");
        _defs->addAttr("in", "off");
        _defs->addAttr("type", "matrix");
        _defs->addAttr("values", "0 0 0 0 " + FloatToString(ds->color.red) + " 0 0 0 0 " +
                                     FloatToString(ds->color.green) + " 0 0 0 0 " +
                                     FloatToString(ds->color.blue) + " 0 0 0 " +
                                     FloatToString(ds->color.alpha) + " 0");
        _defs->addAttr("result", "shadow");
        _defs->closeTagSelfClosing();
        if (!ds->showBehindLayer) {
          _defs->openTag("feComposite");
          _defs->addAttr("in", "shadow");
          _defs->addAttr("in2", "SourceAlpha");
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
        std::string fid = _ctx->nextId("filter");
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
          _defs->addAttr("dx", FloatToString(is->offsetX));
        }
        if (!FloatNearlyZero(is->offsetY)) {
          _defs->addAttr("dy", FloatToString(is->offsetY));
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
        std::string sd = FloatToString(is->blurX);
        if (is->blurX != is->blurY) {
          sd += " " + FloatToString(is->blurY);
        }
        _defs->openTag("feGaussianBlur");
        _defs->addAttr("in", "iComp");
        _defs->addAttr("stdDeviation", sd);
        _defs->addAttr("result", "iBlur");
        _defs->closeTagSelfClosing();
        _defs->openTag("feColorMatrix");
        _defs->addAttr("in", "iBlur");
        _defs->addAttr("type", "matrix");
        _defs->addAttr("values", "0 0 0 0 " + FloatToString(is->color.red) + " 0 0 0 0 " +
                                     FloatToString(is->color.green) + " 0 0 0 0 " +
                                     FloatToString(is->color.blue) + " 0 0 0 " +
                                     FloatToString(is->color.alpha) + " 0");
        _defs->addAttr("result", "iShadow");
        _defs->closeTagSelfClosing();
        _defs->openTag("feMerge");
        _defs->closeTagStart();
        _defs->openTag("feMergeNode");
        _defs->addAttr("in", "SourceGraphic");
        _defs->closeTagSelfClosing();
        _defs->openTag("feMergeNode");
        _defs->addAttr("in", "iShadow");
        _defs->closeTagSelfClosing();
        _defs->closeTag();
        _defs->closeTag();
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

  bool needScrollRectWrapper = layer->hasScrollRect;

  if (layer->mask != nullptr) {
    if (layer->maskType == MaskType::Contour) {
      auto clipId = writeClipDef(layer->mask);
      style += ";clip-path:url(#" + clipId + ")";
    } else {
      style += writeMaskCSS(layer->mask, layer->maskType);
    }
  }

  out.openTag("div");
  out.addAttr("class", "pagx-layer");
  if (!layer->id.empty()) {
    out.addAttr("id", layer->id);
  }
  if (!layer->name.empty()) {
    out.addAttr("data-name", layer->name);
  }
  for (auto& [key, value] : layer->customData) {
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
        shadowStyle += ";filter:drop-shadow(" + FloatToString(ds->offsetX) + "px " +
                       FloatToString(ds->offsetY) + "px " + FloatToString(ds->blurX) + "px " +
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
                       FloatToString(ds->blurX / 2) + " " + FloatToString(ds->blurY / 2));
        _defs->addAttr("result", "blur");
        _defs->closeTagSelfClosing();
        _defs->openTag("feOffset");
        _defs->addAttr("in", "blur");
        _defs->addAttr("dx", FloatToString(ds->offsetX));
        _defs->addAttr("dy", FloatToString(ds->offsetY));
        _defs->addAttr("result", "offsetBlur");
        _defs->closeTagSelfClosing();
        _defs->openTag("feFlood");
        _defs->addAttr("flood-color", ColorToSVGHex(ds->color));
        if (ds->color.alpha < 1.0f) {
          _defs->addAttr("flood-opacity", FloatToString(ds->color.alpha));
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
                                FloatToString(avg) + "px);-webkit-backdrop-filter:blur(" +
                                FloatToString(avg) + "px)";
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
        out.openTag("div");
        out.addAttr("style", "position:absolute;inset:0;backdrop-filter:blur(" +
                                 FloatToString(avg) + "px);-webkit-backdrop-filter:blur(" +
                                 FloatToString(avg) + "px)");
        out.closeTagSelfClosing();
      }
    }
  }

  if (needScrollRectWrapper) {
    auto& sr = layer->scrollRect;
    out.openTag("div");
    out.addAttr("style", "position:absolute;left:0px;top:0px;width:" + FloatToString(sr.width) +
                             "px;height:" + FloatToString(sr.height) + "px;overflow:hidden");
    out.closeTagStart();
    out.openTag("div");
    out.addAttr("style", "position:relative;left:" + FloatToString(-sr.x) +
                             "px;top:" + FloatToString(-sr.y) + "px");
    out.closeTagStart();
  }

  float contentAlpha = childDistribute ? layerAlpha : 1.0f;

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

  for (auto* child : layer->children) {
    bool childIsFlexItem = isFlexContainer && child->includeInLayout;
    writeLayer(out, child, contentAlpha, childDistribute, childIsFlexItem);
  }

  if (hasForeground) {
    writeLayerContents(out, layer, contentAlpha, childDistribute, LayerPlacement::Foreground);
  }

  for (auto& [styleType, ls] : aboveStyles) {
    auto blendStr = BlendModeToMixBlendMode(ls->blendMode);
    if (styleType == NodeType::InnerShadowStyle) {
      auto is = static_cast<const InnerShadowStyle*>(ls);
      std::string fid = _ctx->nextId("isf");
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
        _defs->addAttr("dx", FloatToString(is->offsetX));
      }
      if (!FloatNearlyZero(is->offsetY)) {
        _defs->addAttr("dy", FloatToString(is->offsetY));
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
      std::string sd = FloatToString(is->blurX);
      if (is->blurX != is->blurY) {
        sd += " " + FloatToString(is->blurY);
      }
      _defs->openTag("feGaussianBlur");
      _defs->addAttr("in", "iComp");
      _defs->addAttr("stdDeviation", sd);
      _defs->addAttr("result", "iBlur");
      _defs->closeTagSelfClosing();
      _defs->openTag("feColorMatrix");
      _defs->addAttr("in", "iBlur");
      _defs->addAttr("type", "matrix");
      _defs->addAttr("values", "0 0 0 0 " + FloatToString(is->color.red) + " 0 0 0 0 " +
                                   FloatToString(is->color.green) + " 0 0 0 0 " +
                                   FloatToString(is->color.blue) + " 0 0 0 " +
                                   FloatToString(is->color.alpha) + " 0");
      _defs->closeTagSelfClosing();
      _defs->closeTag();
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

  out.closeTag();
}

}  // namespace pagx
