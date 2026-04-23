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

#include <string>
#include <vector>
#include "base/utils/MathUtil.h"
#include "pagx/html/HTMLWriter.h"
#include "pagx/nodes/BlendFilter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorMatrixFilter.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/InnerShadowFilter.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/utils/StringParser.h"

namespace pagx {

using pag::FloatNearlyZero;

//==============================================================================
// HTMLWriter – mirror-tile eligibility
//==============================================================================

bool HTMLWriter::needsMirrorTiling(const Layer* layer) {
  // Guard: need exactly one filter that is a BlurFilter with tileMode=Mirror, and no layer
  // styles (which would need to be applied outside the tile container and require separate
  // stacking-context handling). Mixed filter chains also fall through because the tile wrapper
  // would need to participate in those filter stages.
  if (!layer->styles.empty()) {
    return false;
  }
  if (layer->filters.size() != 1) {
    return false;
  }
  auto* f = layer->filters[0];
  if (f->nodeType() != NodeType::BlurFilter) {
    return false;
  }
  auto* bf = static_cast<const BlurFilter*>(f);
  return bf->tileMode == TileMode::Mirror;
}

//==============================================================================
// HTMLWriter – filter defs
//==============================================================================

std::string HTMLWriter::writeFilterDefs(const std::vector<LayerFilter*>& filters) {
  if (filters.empty()) {
    return {};
  }
  bool needSVG = false;
  for (auto* f : filters) {
    auto t = f->nodeType();
    if (t == NodeType::BlurFilter) {
      auto b = static_cast<const BlurFilter*>(f);
      if (b->blurX != b->blurY) {
        needSVG = true;
      }
    } else if (t == NodeType::DropShadowFilter) {
      auto s = static_cast<const DropShadowFilter*>(f);
      if (s->blurX != s->blurY || s->shadowOnly) {
        needSVG = true;
      }
    } else if (t == NodeType::InnerShadowFilter || t == NodeType::ColorMatrixFilter ||
               t == NodeType::BlendFilter) {
      needSVG = true;
    }
  }
  if (!needSVG) {
    std::string css;
    for (auto* f : filters) {
      if (!css.empty()) {
        css += ' ';
      }
      if (f->nodeType() == NodeType::BlurFilter) {
        auto b = static_cast<const BlurFilter*>(f);
        css += "blur(" + FloatToString(b->blurX) + "px)";
      } else if (f->nodeType() == NodeType::DropShadowFilter) {
        auto s = static_cast<const DropShadowFilter*>(f);
        css += "drop-shadow(" + FloatToString(s->offsetX) + "px " + FloatToString(s->offsetY) +
               "px " + FloatToString(s->blurX) + "px " + ColorToRGBA(s->color) + ")";
      }
    }
    return css;
  }
  std::string fid = _ctx->nextId("filter");
  _defs->openTag("filter");
  _defs->addAttr("id", fid);
  _defs->addAttr("x", "-50%");
  _defs->addAttr("y", "-50%");
  _defs->addAttr("width", "200%");
  _defs->addAttr("height", "200%");
  _defs->addAttr("color-interpolation-filters", "sRGB");
  _defs->closeTagStart();

  // Pipeline state: each filter consumes currentInput (the RGBA result of the preceding filter)
  // and advances it to its own RGBA output. This mirrors tgfx's ImageFilter::MakeCompose, where a
  // chain of filters is function composition: filter[n](filter[n-1](...filter[0](source)...)).
  // alphaResult caches the name of a result whose alpha channel equals the current pipeline alpha;
  // alphaDirty=true means it is stale and must be recomputed before the next filter that needs
  // alpha (Drop/Inner), via feColorMatrix extracting the alpha of currentInput.
  int si = 0;
  std::string currentInput = "SourceGraphic";
  std::string alphaResult = "SourceAlpha";
  bool alphaDirty = false;

  auto ensureAlpha = [&]() -> std::string {
    if (!alphaDirty) {
      return alphaResult;
    }
    std::string aId = "a" + std::to_string(si++);
    _defs->openTag("feColorMatrix");
    _defs->addAttr("in", currentInput);
    _defs->addAttr("type", "matrix");
    _defs->addAttr("values", "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0");
    _defs->addAttr("result", aId);
    _defs->closeTagSelfClosing();
    alphaResult = aId;
    alphaDirty = false;
    return alphaResult;
  };

  for (auto* f : filters) {
    switch (f->nodeType()) {
      case NodeType::BlurFilter: {
        auto b = static_cast<const BlurFilter*>(f);
        // TODO: BlurFilter.tileMode is ignored. SVG feGaussianBlur has no equivalent of
        // TileMode (Clamp/Repeat/Mirror/Decal); it always uses Decal-like behavior.
        std::string blurId = "blur" + std::to_string(si++);
        _defs->openTag("feGaussianBlur");
        _defs->addAttr("in", currentInput);
        std::string sd = FloatToString(b->blurX);
        if (b->blurX != b->blurY) {
          sd += " " + FloatToString(b->blurY);
        }
        _defs->addAttr("stdDeviation", sd);
        _defs->addAttr("result", blurId);
        _defs->closeTagSelfClosing();
        currentInput = blurId;
        alphaDirty = true;
        break;
      }
      case NodeType::DropShadowFilter: {
        auto s = static_cast<const DropShadowFilter*>(f);
        std::string i = std::to_string(si++);
        std::string alphaIn = ensureAlpha();
        _defs->openTag("feGaussianBlur");
        _defs->addAttr("in", alphaIn);
        std::string sd = FloatToString(s->blurX);
        if (s->blurX != s->blurY) {
          sd += " " + FloatToString(s->blurY);
        }
        _defs->addAttr("stdDeviation", sd);
        _defs->addAttr("result", "sBlur" + i);
        _defs->closeTagSelfClosing();
        std::string offsetRes = "sBlur" + i;
        if (!FloatNearlyZero(s->offsetX) || !FloatNearlyZero(s->offsetY)) {
          _defs->openTag("feOffset");
          _defs->addAttr("in", offsetRes);
          if (!FloatNearlyZero(s->offsetX)) {
            _defs->addAttr("dx", FloatToString(s->offsetX));
          }
          if (!FloatNearlyZero(s->offsetY)) {
            _defs->addAttr("dy", FloatToString(s->offsetY));
          }
          _defs->addAttr("result", "sOff" + i);
          _defs->closeTagSelfClosing();
          offsetRes = "sOff" + i;
        }
        _defs->openTag("feColorMatrix");
        _defs->addAttr("in", offsetRes);
        _defs->addAttr("type", "matrix");
        _defs->addAttr("values", "0 0 0 0 " + FloatToString(s->color.red) + " 0 0 0 0 " +
                                     FloatToString(s->color.green) + " 0 0 0 0 " +
                                     FloatToString(s->color.blue) + " 0 0 0 " +
                                     FloatToString(s->color.alpha) + " 0");
        _defs->addAttr("result", "sDrop" + i);
        _defs->closeTagSelfClosing();
        if (s->shadowOnly) {
          // Pipeline becomes the shadow-colored offset alpha only.
          currentInput = "sDrop" + i;
        } else {
          // Compose shadow underneath the source: feMerge stacks bottom-first.
          _defs->openTag("feMerge");
          _defs->addAttr("result", "sMerged" + i);
          _defs->closeTagStart();
          _defs->openTag("feMergeNode");
          _defs->addAttr("in", "sDrop" + i);
          _defs->closeTagSelfClosing();
          _defs->openTag("feMergeNode");
          _defs->addAttr("in", currentInput);
          _defs->closeTagSelfClosing();
          _defs->closeTag();
          currentInput = "sMerged" + i;
        }
        alphaDirty = true;
        break;
      }
      case NodeType::InnerShadowFilter: {
        auto s = static_cast<const InnerShadowFilter*>(f);
        std::string i = std::to_string(si++);
        std::string alphaIn = ensureAlpha();
        // Invert current alpha so that exterior becomes opaque and interior becomes transparent.
        // Blurring the inverted alpha produces a falloff that bleeds into the shape from every
        // edge, then offsetting and clipping back to the source mask yields the inner shadow.
        // The arithmetic "SourceAlpha - offsetAlpha" variant only keeps a one-sided edge band
        // whose blurred energy is too faint to match tgfx's InnerShadowImageFilter.
        _defs->openTag("feComponentTransfer");
        _defs->addAttr("in", alphaIn);
        _defs->addAttr("result", "iInv" + i);
        _defs->closeTagStart();
        _defs->openTag("feFuncA");
        _defs->addAttr("type", "table");
        _defs->addAttr("tableValues", "1 0");
        _defs->closeTagSelfClosing();
        _defs->closeTag();
        std::string sd = FloatToString(s->blurX);
        if (s->blurX != s->blurY) {
          sd += " " + FloatToString(s->blurY);
        }
        _defs->openTag("feGaussianBlur");
        _defs->addAttr("in", "iInv" + i);
        _defs->addAttr("stdDeviation", sd);
        _defs->addAttr("result", "iBlur" + i);
        _defs->closeTagSelfClosing();
        std::string blurredResult = "iBlur" + i;
        if (!FloatNearlyZero(s->offsetX) || !FloatNearlyZero(s->offsetY)) {
          _defs->openTag("feOffset");
          _defs->addAttr("in", blurredResult);
          if (!FloatNearlyZero(s->offsetX)) {
            _defs->addAttr("dx", FloatToString(s->offsetX));
          }
          if (!FloatNearlyZero(s->offsetY)) {
            _defs->addAttr("dy", FloatToString(s->offsetY));
          }
          _defs->addAttr("result", "iOff" + i);
          _defs->closeTagSelfClosing();
          blurredResult = "iOff" + i;
        }
        _defs->openTag("feFlood");
        _defs->addAttr("flood-color", ColorToSVGHex(s->color));
        if (s->color.alpha < 1.0f) {
          _defs->addAttr("flood-opacity", FloatToString(s->color.alpha));
        }
        _defs->addAttr("result", "iFlood" + i);
        _defs->closeTagSelfClosing();
        _defs->openTag("feComposite");
        _defs->addAttr("in", "iFlood" + i);
        _defs->addAttr("in2", blurredResult);
        _defs->addAttr("operator", "in");
        _defs->addAttr("result", "iShadow" + i);
        _defs->closeTagSelfClosing();
        _defs->openTag("feComposite");
        _defs->addAttr("in", "iShadow" + i);
        _defs->addAttr("in2", alphaIn);
        _defs->addAttr("operator", "in");
        _defs->addAttr("result", "iClip" + i);
        _defs->closeTagSelfClosing();
        if (s->shadowOnly) {
          currentInput = "iClip" + i;
          alphaDirty = true;
        } else {
          // Compose inner shadow on top of source: feMerge stacks bottom-first, so
          // currentInput first then iClip over it.
          _defs->openTag("feMerge");
          _defs->addAttr("result", "iMerged" + i);
          _defs->closeTagStart();
          _defs->openTag("feMergeNode");
          _defs->addAttr("in", currentInput);
          _defs->closeTagSelfClosing();
          _defs->openTag("feMergeNode");
          _defs->addAttr("in", "iClip" + i);
          _defs->closeTagSelfClosing();
          _defs->closeTag();
          currentInput = "iMerged" + i;
          // Alpha shape is unchanged by inner shadow (clipped to source), but
          // the cached alphaResult may point to an upstream result that feMerge
          // has now superseded; keep alphaIn valid by reusing it.
          alphaResult = alphaIn;
          alphaDirty = false;
        }
        break;
      }
      case NodeType::ColorMatrixFilter: {
        auto cm = static_cast<const ColorMatrixFilter*>(f);
        std::string cmId = "cm" + std::to_string(si++);
        _defs->openTag("feColorMatrix");
        _defs->addAttr("in", currentInput);
        _defs->addAttr("type", "matrix");
        std::string v;
        for (size_t j = 0; j < cm->matrix.size(); j++) {
          if (j > 0) {
            v += ' ';
          }
          v += FloatToString(cm->matrix[j]);
        }
        _defs->addAttr("values", v);
        _defs->addAttr("result", cmId);
        _defs->closeTagSelfClosing();
        currentInput = cmId;
        alphaDirty = true;
        break;
      }
      case NodeType::BlendFilter: {
        auto bf = static_cast<const BlendFilter*>(f);
        std::string i = std::to_string(si++);
        std::string alphaIn = ensureAlpha();
        _defs->openTag("feFlood");
        _defs->addAttr("flood-color", ColorToSVGHex(bf->color));
        if (bf->color.alpha < 1.0f) {
          _defs->addAttr("flood-opacity", FloatToString(bf->color.alpha));
        }
        _defs->addAttr("result", "bFlood" + i);
        _defs->closeTagSelfClosing();
        _defs->openTag("feBlend");
        _defs->addAttr("in", "bFlood" + i);
        _defs->addAttr("in2", currentInput);
        auto ms = BlendModeToMixBlendMode(bf->blendMode);
        if (ms) {
          _defs->addAttr("mode", ms);
        }
        _defs->addAttr("result", "bBlend" + i);
        _defs->closeTagSelfClosing();
        _defs->openTag("feComposite");
        _defs->addAttr("in", "bBlend" + i);
        _defs->addAttr("in2", alphaIn);
        _defs->addAttr("operator", "in");
        _defs->addAttr("result", "bClip" + i);
        _defs->closeTagSelfClosing();
        currentInput = "bClip" + i;
        // Blend preserves the alpha shape it was clipped against.
        alphaResult = alphaIn;
        alphaDirty = false;
        break;
      }
      default:
        break;
    }
  }
  _defs->closeTag();
  return "url(#" + fid + ")";
}

//==============================================================================
// HTMLWriter – mask / clip defs
//==============================================================================

std::string HTMLWriter::writeMaskCSS(const Layer* mask, MaskType type) {
  const Fill* maskFill = nullptr;
  for (auto* e : mask->contents) {
    if (e->nodeType() == NodeType::Fill) {
      maskFill = static_cast<const Fill*>(e);
      break;
    }
  }

  bool useFillColor = (type == MaskType::Luminance && maskFill != nullptr && maskFill->color);
  std::string fillAttr = "white";
  float fillOpacity = 1.0f;

  // Compute bounding box of mask geometry for SVG viewBox.
  float maxX = 0;
  float maxY = 0;
  for (auto* e : mask->contents) {
    if (e->nodeType() == NodeType::Rectangle) {
      auto rect = static_cast<const Rectangle*>(e);
      float r = rect->position.x + rect->size.width / 2;
      float b = rect->position.y + rect->size.height / 2;
      maxX = std::max(maxX, r);
      maxY = std::max(maxY, b);
    } else if (e->nodeType() == NodeType::Ellipse) {
      auto el = static_cast<const Ellipse*>(e);
      maxX = std::max(maxX, el->position.x + el->size.width / 2);
      maxY = std::max(maxY, el->position.y + el->size.height / 2);
    } else if (e->nodeType() == NodeType::Polystar) {
      auto ps = static_cast<const Polystar*>(e);
      float r = std::max(ps->outerRadius, ps->innerRadius);
      maxX = std::max(maxX, ps->position.x + r);
      maxY = std::max(maxY, ps->position.y + r);
    }
  }
  if (FloatNearlyZero(maxX)) {
    maxX = _ctx->docWidth;
  }
  if (FloatNearlyZero(maxY)) {
    maxY = _ctx->docHeight;
  }

  HTMLBuilder svg(0);
  svg.openTag("svg");
  svg.addAttr("width", FloatToString(maxX));
  svg.addAttr("height", FloatToString(maxY));
  svg.addAttr("viewBox", "0 0 " + FloatToString(maxX) + " " + FloatToString(maxY));
  svg.closeTagStart();

  if (useFillColor) {
    auto* src = maskFill->color;
    if (src->nodeType() == NodeType::LinearGradient ||
        src->nodeType() == NodeType::RadialGradient) {
      std::string gradId = "g";
      svg.openTag("defs");
      svg.closeTagStart();
      if (src->nodeType() == NodeType::LinearGradient) {
        auto g = static_cast<const LinearGradient*>(src);
        svg.openTag("linearGradient");
        svg.addAttr("id", gradId);
        svg.addAttr("x1", FloatToString(g->startPoint.x));
        svg.addAttr("y1", FloatToString(g->startPoint.y));
        svg.addAttr("x2", FloatToString(g->endPoint.x));
        svg.addAttr("y2", FloatToString(g->endPoint.y));
        svg.addAttr("gradientUnits", "userSpaceOnUse");
        if (!g->matrix.isIdentity()) {
          svg.addAttr("gradientTransform", MatrixToCSS(g->matrix));
        }
        svg.closeTagStart();
        for (auto* stop : g->colorStops) {
          svg.openTag("stop");
          svg.addAttr("offset", FloatToString(stop->offset));
          svg.addAttr("stop-color", ColorToSVGHex(stop->color));
          if (stop->color.alpha < 1.0f) {
            svg.addAttr("stop-opacity", FloatToString(stop->color.alpha));
          }
          svg.closeTagSelfClosing();
        }
        svg.closeTag();
      } else {
        auto g = static_cast<const RadialGradient*>(src);
        svg.openTag("radialGradient");
        svg.addAttr("id", gradId);
        svg.addAttr("cx", FloatToString(g->center.x));
        svg.addAttr("cy", FloatToString(g->center.y));
        svg.addAttr("r", FloatToString(g->radius));
        svg.addAttr("gradientUnits", "userSpaceOnUse");
        if (!g->matrix.isIdentity()) {
          svg.addAttr("gradientTransform", MatrixToCSS(g->matrix));
        }
        svg.closeTagStart();
        for (auto* stop : g->colorStops) {
          svg.openTag("stop");
          svg.addAttr("offset", FloatToString(stop->offset));
          svg.addAttr("stop-color", ColorToSVGHex(stop->color));
          if (stop->color.alpha < 1.0f) {
            svg.addAttr("stop-opacity", FloatToString(stop->color.alpha));
          }
          svg.closeTagSelfClosing();
        }
        svg.closeTag();
      }
      svg.closeTag();  // </defs>
      fillAttr = "url(#g)";
      fillOpacity = maskFill->alpha;
    } else if (src->nodeType() == NodeType::SolidColor) {
      auto sc = static_cast<const SolidColor*>(src);
      fillAttr = ColorToSVGHex(sc->color);
      fillOpacity = sc->color.alpha * maskFill->alpha;
    }
  }

  for (auto* e : mask->contents) {
    if (e->nodeType() == NodeType::Rectangle) {
      auto rect = static_cast<const Rectangle*>(e);
      svg.openTag("rect");
      float x = rect->position.x - rect->size.width / 2;
      float y = rect->position.y - rect->size.height / 2;
      if (!FloatNearlyZero(x)) {
        svg.addAttr("x", FloatToString(x));
      }
      if (!FloatNearlyZero(y)) {
        svg.addAttr("y", FloatToString(y));
      }
      svg.addAttr("width", FloatToString(rect->size.width));
      svg.addAttr("height", FloatToString(rect->size.height));
      if (rect->roundness > 0) {
        svg.addAttr("rx", FloatToString(rect->roundness));
      }
      svg.addAttr("fill", fillAttr);
      if (fillOpacity < 1.0f) {
        svg.addAttr("fill-opacity", FloatToString(fillOpacity));
      }
      svg.closeTagSelfClosing();
    } else if (e->nodeType() == NodeType::Ellipse) {
      auto el = static_cast<const Ellipse*>(e);
      svg.openTag("ellipse");
      svg.addAttr("cx", FloatToString(el->position.x));
      svg.addAttr("cy", FloatToString(el->position.y));
      svg.addAttr("rx", FloatToString(el->size.width / 2));
      svg.addAttr("ry", FloatToString(el->size.height / 2));
      svg.addAttr("fill", fillAttr);
      if (fillOpacity < 1.0f) {
        svg.addAttr("fill-opacity", FloatToString(fillOpacity));
      }
      svg.closeTagSelfClosing();
    } else if (e->nodeType() == NodeType::Path) {
      auto p = static_cast<const Path*>(e);
      std::string d = GetPathSVGString(p);
      if (!d.empty()) {
        svg.openTag("path");
        svg.addAttr("d", d);
        svg.addAttr("fill", fillAttr);
        if (fillOpacity < 1.0f) {
          svg.addAttr("fill-opacity", FloatToString(fillOpacity));
        }
        svg.closeTagSelfClosing();
      }
    } else if (e->nodeType() == NodeType::Polystar) {
      auto ps = static_cast<const Polystar*>(e);
      std::string d = BuildPolystarPath(ps);
      if (!d.empty()) {
        svg.openTag("path");
        svg.addAttr("d", d);
        svg.addAttr("fill", fillAttr);
        if (fillOpacity < 1.0f) {
          svg.addAttr("fill-opacity", FloatToString(fillOpacity));
        }
        svg.closeTagSelfClosing();
      }
    }
  }
  svg.closeTag();  // </svg>

  std::string svgContent = svg.release();
  std::string compactSVG;
  compactSVG.reserve(svgContent.size());
  for (char c : svgContent) {
    if (c != '\n') {
      compactSVG += c;
    }
  }
  std::string encoded;
  encoded.reserve(compactSVG.size() * 2);
  for (char c : compactSVG) {
    switch (c) {
      case '<':
        encoded += "%3C";
        break;
      case '>':
        encoded += "%3E";
        break;
      case '#':
        encoded += "%23";
        break;
      case '"':
        encoded += "%22";
        break;
      default:
        encoded += c;
        break;
    }
  }
  std::string dataURI = "url('data:image/svg+xml," + encoded + "')";
  std::string css = ";-webkit-mask-image:" + dataURI;
  css += ";mask-image:" + dataURI;
  if (type == MaskType::Luminance) {
    css += ";-webkit-mask-mode:luminance;mask-mode:luminance";
  } else {
    css += ";-webkit-mask-mode:alpha;mask-mode:alpha";
  }
  return css;
}

std::string HTMLWriter::writeClipDef(const Layer* mask) {
  std::string id = _ctx->nextId("clip");
  _defs->openTag("clipPath");
  _defs->addAttr("id", id);
  _defs->closeTagStart();
  writeClipContent(*_defs, mask, {});
  _defs->closeTag();
  return id;
}

void HTMLWriter::writeClipContent(HTMLBuilder& out, const Layer* layer, const Matrix& parent) {
  Matrix lm = layer->matrix;
  if (layer->x != 0 || layer->y != 0) {
    lm = Matrix::Translate(layer->x, layer->y) * lm;
  }
  Matrix combined = parent * lm;
  std::string tr = combined.isIdentity() ? "" : MatrixToCSS(combined);
  for (auto* e : layer->contents) {
    if (e->nodeType() == NodeType::Path) {
      auto p = static_cast<const Path*>(e);
      std::string d = GetPathSVGString(p);
      if (!d.empty()) {
        out.openTag("path");
        if (!tr.empty()) {
          out.addAttr("transform", tr);
        }
        out.addAttr("d", d);
        out.closeTagSelfClosing();
      }
    } else if (e->nodeType() == NodeType::Rectangle) {
      auto r = static_cast<const Rectangle*>(e);
      out.openTag("rect");
      if (!tr.empty()) {
        out.addAttr("transform", tr);
      }
      float x = r->position.x - r->size.width / 2;
      float y = r->position.y - r->size.height / 2;
      if (!FloatNearlyZero(x)) {
        out.addAttr("x", FloatToString(x));
      }
      if (!FloatNearlyZero(y)) {
        out.addAttr("y", FloatToString(y));
      }
      out.addAttr("width", FloatToString(r->size.width));
      out.addAttr("height", FloatToString(r->size.height));
      if (r->roundness > 0) {
        out.addAttr("rx", FloatToString(r->roundness));
      }
      out.closeTagSelfClosing();
    } else if (e->nodeType() == NodeType::Ellipse) {
      auto el = static_cast<const Ellipse*>(e);
      out.openTag("ellipse");
      if (!tr.empty()) {
        out.addAttr("transform", tr);
      }
      out.addAttr("cx", FloatToString(el->position.x));
      out.addAttr("cy", FloatToString(el->position.y));
      out.addAttr("rx", FloatToString(el->size.width / 2));
      out.addAttr("ry", FloatToString(el->size.height / 2));
      out.closeTagSelfClosing();
    } else if (e->nodeType() == NodeType::Polystar) {
      auto ps = static_cast<const Polystar*>(e);
      std::string d = BuildPolystarPath(ps);
      if (!d.empty()) {
        out.openTag("path");
        if (!tr.empty()) {
          out.addAttr("transform", tr);
        }
        out.addAttr("d", d);
        out.closeTagSelfClosing();
      }
    }
  }
  for (auto* child : layer->children) {
    writeClipContent(out, child, combined);
  }
}

}  // namespace pagx
