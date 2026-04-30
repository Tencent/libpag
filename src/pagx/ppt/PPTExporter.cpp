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

#include "pagx/PPTExporter.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#include "pagx/LayoutContext.h"
#include "pagx/PAGXDocument.h"
#include "pagx/TextLayout.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/ppt/PPTBoilerplate.h"
#include "pagx/ppt/PPTContourUtils.h"
#include "pagx/ppt/PPTFeatureProbe.h"
#include "pagx/ppt/PPTGeomEmitter.h"
#include "pagx/ppt/PPTWriter.h"
#include "pagx/ppt/PPTWriterContext.h"
#include "pagx/utils/ExporterUtils.h"
#include "pagx/utils/StringParser.h"
#include "pagx/xml/XMLBuilder.h"
#include "renderer/LayerBuilder.h"
#include "tgfx/layers/DisplayList.h"
#include "zip.h"

namespace pagx {

using pag::FloatNearlyZero;
using pag::RadiansToDegrees;

// ── Transform decomposition ────────────────────────────────────────────────

PPTWriter::Xform PPTWriter::decomposeXform(float localX, float localY, float localW, float localH,
                                           const Matrix& m) {
  float cx = localX + localW / 2.0f;
  float cy = localY + localH / 2.0f;
  float tcx = m.a * cx + m.c * cy + m.tx;
  float tcy = m.b * cx + m.d * cy + m.ty;

  float sx = 0;
  float sy = 0;
  DecomposeScale(m, &sx, &sy);

  float tw = localW * sx;
  float th = localH * sy;

  float theta = RadiansToDegrees(std::atan2(m.b, m.a));

  Xform xf;
  xf.offX = PxToEMU(tcx - tw / 2.0f);
  xf.offY = PxToEMU(tcy - th / 2.0f);
  xf.extCX = std::max(int64_t(1), PxToEMU(tw));
  xf.extCY = std::max(int64_t(1), PxToEMU(th));
  xf.rotation = FloatNearlyZero(theta) ? 0 : AngleToPPT(theta);
  return xf;
}

void PPTWriter::WriteXfrm(XMLBuilder& out, const Xform& xf) {
  if (xf.rotation != 0) {
    out.openElement("a:xfrm").addRequiredAttribute("rot", xf.rotation).closeElementStart();
  } else {
    out.openElement("a:xfrm").closeElementStart();
  }
  out.openElement("a:off")
      .addRequiredAttribute("x", xf.offX)
      .addRequiredAttribute("y", xf.offY)
      .closeElementSelfClosing();
  out.openElement("a:ext")
      .addRequiredAttribute("cx", xf.extCX)
      .addRequiredAttribute("cy", xf.extCY)
      .closeElementSelfClosing();
  out.closeElement();  // a:xfrm
}

// ── Shape envelope ─────────────────────────────────────────────────────────

void PPTWriter::beginShape(XMLBuilder& out, const char* name, int64_t offX, int64_t offY,
                           int64_t extCX, int64_t extCY, int rot) {
  int id = _ctx->nextShapeId();
  out.openElement("p:sp").closeElementStart();

  out.openElement("p:nvSpPr").closeElementStart();
  out.openElement("p:cNvPr")
      .addRequiredAttribute("id", id)
      .addRequiredAttribute("name", name)
      .closeElementSelfClosing();
  out.openElement("p:cNvSpPr").closeElementSelfClosing();
  out.openElement("p:nvPr").closeElementSelfClosing();
  out.closeElement();  // p:nvSpPr

  out.openElement("p:spPr").closeElementStart();
  WriteXfrm(out, {offX, offY, extCX, extCY, rot});
}

void PPTWriter::endShape(XMLBuilder& out) {
  out.closeElement();  // p:spPr
  out.openElement("p:txBody").closeElementStart();
  out.openElement("a:bodyPr").closeElementSelfClosing();
  out.openElement("a:lstStyle").closeElementSelfClosing();
  out.openElement("a:p").closeElementStart();
  // Default to en-US as a language-neutral fallback.
  out.openElement("a:endParaRPr").addRequiredAttribute("lang", "en-US").closeElementSelfClosing();
  out.closeElement();  // a:p
  out.closeElement();  // p:txBody
  out.closeElement();  // p:sp
}

void PPTWriter::writeGlyphShape(XMLBuilder& out, const Fill* fill, float alpha) {
  writeFill(out, fill, alpha);
  out.openElement("a:ln").closeElementStart();
  out.openElement("a:noFill").closeElementSelfClosing();
  out.closeElement();
  endShape(out);
}

void PPTWriter::writeShapeTail(XMLBuilder& out, const FillStrokeInfo& fs, float alpha,
                               const Rect& shapeBounds, bool imageWritten,
                               const std::vector<LayerFilter*>& filters,
                               const std::vector<LayerStyle*>& styles) {
  if (imageWritten) {
    out.openElement("a:noFill").closeElementSelfClosing();
  } else {
    writeFill(out, fs.fill, alpha, shapeBounds);
  }
  writeStroke(out, fs.stroke, alpha);
  writeEffects(out, filters, styles);
  endShape(out);
}

// ── Lazy build result ───────────────────────────────────────────────────────

const LayerBuildResult& PPTWriter::ensureBuildResult() {
  if (!_buildResultReady) {
    _buildResult = LayerBuilder::BuildWithMap(_doc);
    _buildResultReady = true;
  }
  return _buildResult;
}

// ── Shared XML helpers ──────────────────────────────────────────────────────

// ── Custom geometry ────────────────────────────────────────────────────────

// Bridging is only meaningful with multiple contours — with a single contour
// there's nothing to stitch together, so emit it flat. When bridging is
// disabled globally, always emit flat regardless of contour count.
void PPTWriter::WriteContourGeom(XMLBuilder& out, std::vector<PathContour>& contours,
                                 int64_t pathWidth, int64_t pathHeight, float scaleX, float scaleY,
                                 float scaledOfsX, float scaledOfsY, FillRule fillRule) {
  if (!_bridgeContours || contours.size() <= 1) {
    EmitFlatContourGeom(out, contours, pathWidth, pathHeight, scaleX, scaleY, scaledOfsX,
                        scaledOfsY);
    return;
  }
  auto groups = PrepareContourGroups(contours, fillRule);
  EmitContourGeomFromGroups(out, contours, groups, pathWidth, pathHeight, scaleX, scaleY,
                            scaledOfsX, scaledOfsY);
}

// ── Shape writers ──────────────────────────────────────────────────────────

void PPTWriter::writeRectangle(XMLBuilder& out, const Rectangle* rect, const FillStrokeInfo& fs,
                               const Matrix& m, float alpha,
                               const std::vector<LayerFilter*>& filters,
                               const std::vector<LayerStyle*>& styles) {
  auto renderPos = rect->renderPosition();
  auto renderSize = rect->renderSize();
  float x = renderPos.x - renderSize.width / 2.0f;
  float y = renderPos.y - renderSize.height / 2.0f;
  float w = renderSize.width;
  float h = renderSize.height;
  float roundness = rect->roundness;

  // OOXML strokes are always centre-aligned on the geometry, so to mimic
  // StrokeAlign::Inside / Outside we shift this stroke painter's geometry in /
  // out by half the stroke width.  Fill-only painters skip this branch (they
  // see fs.stroke == nullptr from processVectorScope), so the fill geometry
  // remains untouched.
  ApplyStrokeBoxInset(fs.stroke, x, y, w, h, &roundness);

  Rect shapeBounds = Rect::MakeXYWH(x, y, w, h);

  bool imageWritten = writeImagePatternAsPicture(out, fs.fill, shapeBounds, m, alpha);
  if (imageWritten && !fs.stroke && filters.empty() && styles.empty()) {
    return;
  }

  auto xf = decomposeXform(x, y, w, h, m);
  beginShape(out, "Rectangle", xf.offX, xf.offY, xf.extCX, xf.extCY, xf.rotation);

  if (roundness > 0) {
    float minSide = std::min(w, h);
    int adj =
        (minSide > 0) ? std::clamp(static_cast<int>(roundness * 100000.0f / minSide), 0, 50000) : 0;
    out.openElement("a:prstGeom").addRequiredAttribute("prst", "roundRect").closeElementStart();
    out.openElement("a:avLst").closeElementStart();
    out.openElement("a:gd")
        .addRequiredAttribute("name", "adj")
        .addRequiredAttribute("fmla", "val " + std::to_string(adj))
        .closeElementSelfClosing();
    out.closeElement();  // a:avLst
    out.closeElement();  // a:prstGeom
  } else {
    out.openElement("a:prstGeom").addRequiredAttribute("prst", "rect").closeElementStart();
    out.openElement("a:avLst").closeElementSelfClosing();
    out.closeElement();
  }

  writeShapeTail(out, fs, alpha, shapeBounds, imageWritten, filters, styles);
}

void PPTWriter::writeEllipse(XMLBuilder& out, const Ellipse* ellipse, const FillStrokeInfo& fs,
                             const Matrix& m, float alpha, const std::vector<LayerFilter*>& filters,
                             const std::vector<LayerStyle*>& styles) {
  auto renderSize = ellipse->renderSize();
  float rx = renderSize.width / 2.0f;
  float ry = renderSize.height / 2.0f;
  auto renderPos = ellipse->renderPosition();
  float x = renderPos.x - rx;
  float y = renderPos.y - ry;
  float w = renderSize.width;
  float h = renderSize.height;

  // See writeRectangle: emulate StrokeAlign::Inside / Outside by inset/outset
  // because OOXML can only draw centre-aligned strokes on the geometry.
  ApplyStrokeBoxInset(fs.stroke, x, y, w, h);

  Rect shapeBounds = Rect::MakeXYWH(x, y, w, h);

  bool imageWritten = writeImagePatternAsPicture(out, fs.fill, shapeBounds, m, alpha);
  if (imageWritten && !fs.stroke && filters.empty() && styles.empty()) {
    return;
  }

  auto xf = decomposeXform(x, y, w, h, m);
  beginShape(out, "Ellipse", xf.offX, xf.offY, xf.extCX, xf.extCY, xf.rotation);

  out.openElement("a:prstGeom").addRequiredAttribute("prst", "ellipse").closeElementStart();
  out.openElement("a:avLst").closeElementSelfClosing();
  out.closeElement();

  writeShapeTail(out, fs, alpha, shapeBounds, imageWritten, filters, styles);
}

void PPTWriter::writePath(XMLBuilder& out, const Path* path, const FillStrokeInfo& fs,
                          const Matrix& m, float alpha, const std::vector<LayerFilter*>& filters,
                          const std::vector<LayerStyle*>& styles) {
  if (!path->data || path->data->isEmpty()) {
    return;
  }
  Rect bounds = path->data->getBounds();
  if (bounds.width <= 0 && bounds.height <= 0) {
    return;
  }

  float strokePad = (fs.stroke && fs.stroke->width > 0) ? fs.stroke->width : 0;
  float minDim = std::max(1.0f, strokePad);
  float adjustedW = std::max(bounds.width, minDim);
  float adjustedH = std::max(bounds.height, minDim);
  float adjustedX = bounds.x - (adjustedW - bounds.width) / 2.0f;
  float adjustedY = bounds.y - (adjustedH - bounds.height) / 2.0f;
  Rect shapeBounds = Rect::MakeXYWH(adjustedX, adjustedY, adjustedW, adjustedH);

  bool imageWritten = writeImagePatternAsPicture(out, fs.fill, shapeBounds, m, alpha);
  if (imageWritten && !fs.stroke && filters.empty() && styles.empty()) {
    return;
  }

  auto xf = decomposeXform(adjustedX, adjustedY, adjustedW, adjustedH, m);
  FillRule fillRule = (fs.fill) ? fs.fill->fillRule : FillRule::Winding;

  auto contours = ParsePathContours(path->data);
  int64_t pw = std::max(int64_t(1), PxToEMU(adjustedW));
  int64_t ph = std::max(int64_t(1), PxToEMU(adjustedH));

  // Fast path: bridging off or nothing to bridge → emit one flat custGeom.
  if (!_bridgeContours || contours.size() <= 1) {
    beginShape(out, "Path", xf.offX, xf.offY, xf.extCX, xf.extCY, xf.rotation);
    WriteContourGeom(out, contours, pw, ph, 1.0f, 1.0f, adjustedX, adjustedY, fillRule);
    writeShapeTail(out, fs, alpha, shapeBounds, imageWritten, filters, styles);
    return;
  }

  auto groups = PrepareContourGroups(contours, fillRule);

  // Multiple disjoint groups: emit one <p:sp> per group so each keeps its own
  // bounds marker. StandaloneStrokelessPath keeps the corner markers from
  // showing as dots under a round-capped sp-level stroke.
  if (groups.size() > 1) {
    for (const auto& group : groups) {
      beginShape(out, "Path", xf.offX, xf.offY, xf.extCX, xf.extCY, xf.rotation);
      EmitGroupCustGeom(out, contours, group, pw, ph, 1.0f, 1.0f, adjustedX, adjustedY,
                        BoundsMarkerStyle::StandaloneStrokelessPath);
      writeShapeTail(out, fs, alpha, shapeBounds, imageWritten, filters, styles);
    }
    return;
  }

  // Single bridged group: emit directly (skip the redundant grouping pass
  // that WriteContourGeom would otherwise repeat).
  beginShape(out, "Path", xf.offX, xf.offY, xf.extCX, xf.extCY, xf.rotation);
  EmitContourGeomFromGroups(out, contours, groups, pw, ph, 1.0f, 1.0f, adjustedX, adjustedY);
  writeShapeTail(out, fs, alpha, shapeBounds, imageWritten, filters, styles);
}

namespace {

// Look for the first non-container <TextBox> in `elements` so it can be
// associated with sibling Text geometry (matches the legacy
// CollectFillStroke().textBox behaviour). Container TextBoxes (those with
// children) are handled separately as scopes by processVectorScope.
const TextBox* FindModifierTextBox(const std::vector<Element*>& elements) {
  for (auto* e : elements) {
    if (e->nodeType() == NodeType::TextBox) {
      auto* tb = static_cast<const TextBox*>(e);
      if (tb->elements.empty()) {
        return tb;
      }
    }
  }
  return nullptr;
}

}  // namespace

// Dispatch a single accumulated geometry through the appropriate per-shape
// writer with the given Painter applied. Each painter that renders a geometry
// goes through this function so that multi-fill / multi-stroke produces one
// PowerPoint shape per painter (overlapping in document order).
void PPTWriter::emitGeometryWithFs(XMLBuilder& out, const AccumulatedGeometry& entry,
                                   const FillStrokeInfo& fs,
                                   const std::vector<LayerFilter*>& filters,
                                   const std::vector<LayerStyle*>& styles) {
  FillStrokeInfo localFs = fs;
  if (localFs.textBox == nullptr) {
    localFs.textBox = entry.textBox;
  }
  switch (entry.element->nodeType()) {
    case NodeType::Rectangle:
      writeRectangle(out, static_cast<const Rectangle*>(entry.element), localFs, entry.transform,
                     entry.alpha, filters, styles);
      break;
    case NodeType::Ellipse:
      writeEllipse(out, static_cast<const Ellipse*>(entry.element), localFs, entry.transform,
                   entry.alpha, filters, styles);
      break;
    case NodeType::Path:
      writePath(out, static_cast<const Path*>(entry.element), localFs, entry.transform, entry.alpha,
                filters, styles);
      break;
    case NodeType::Text: {
      auto* text = static_cast<const Text*>(entry.element);
      // GlyphRun-only Text (no readable text content) carries pre-shaped glyph
      // outlines from a custom font; PowerPoint's native a:r runs can't express
      // arbitrary glyph IDs + per-glyph transforms, so the only way to render
      // these is via path geometry — regardless of the convertTextToPath flag.
      bool glyphRunOnly = text->text.empty() && !text->glyphRuns.empty();
      if ((_convertTextToPath || glyphRunOnly) && !text->glyphRuns.empty()) {
        writeTextAsPath(out, text, localFs, entry.transform, entry.alpha, filters, styles);
      } else {
        writeNativeText(out, text, localFs, entry.transform, entry.alpha, filters, styles);
      }
      break;
    }
    default:
      break;
  }
}

// Walk a single scope's element list, growing `accumulator` with new geometry
// and emitting a copy of every visible geometry whenever a Painter is hit.
// `scopeStart` is the index where the current Group's scope begins — Painters
// inside this scope can only render entries from [scopeStart, end), enforcing
// the "Painters within the group only render geometry accumulated within the
// group" rule from the spec while allowing geometry to propagate upward when
// the scope unwinds.
void PPTWriter::processVectorScope(XMLBuilder& out, const std::vector<Element*>& elements,
                                   const Matrix& transform, float alpha,
                                   const std::vector<LayerFilter*>& filters,
                                   const std::vector<LayerStyle*>& styles,
                                   const TextBox* parentTextBox,
                                   std::vector<AccumulatedGeometry>& accumulator,
                                   size_t scopeStart) {
  // The TextBox modifier-association rule is "first <TextBox> in this element
  // list applies to all sibling Text geometry"; we pre-scan once to mirror the
  // legacy CollectFillStroke().textBox behaviour, then fall back to the
  // parent's TextBox so Text inside a Group still inherits an outer one.
  const TextBox* localTextBox = FindModifierTextBox(elements);
  if (localTextBox == nullptr) {
    localTextBox = parentTextBox;
  }

  for (auto* element : elements) {
    auto type = element->nodeType();
    switch (type) {
      case NodeType::Fill:
      case NodeType::Stroke: {
        FillStrokeInfo painterFs = {};
        if (type == NodeType::Fill) {
          painterFs.fill = static_cast<const Fill*>(element);
        } else {
          painterFs.stroke = static_cast<const Stroke*>(element);
        }
        for (size_t i = scopeStart; i < accumulator.size(); ++i) {
          emitGeometryWithFs(out, accumulator[i], painterFs, filters, styles);
        }
        break;
      }
      case NodeType::Rectangle:
      case NodeType::Ellipse:
      case NodeType::Path:
      case NodeType::Text: {
        AccumulatedGeometry entry;
        entry.element = element;
        entry.transform = transform;
        entry.alpha = alpha;
        entry.textBox = localTextBox;
        accumulator.push_back(entry);
        break;
      }
      case NodeType::TextBox: {
        auto* tb = static_cast<const TextBox*>(element);
        if (tb->elements.empty()) {
          // Modifier-only TextBox — already captured by FindModifierTextBox above.
          break;
        }
        Matrix tbMatrix = transform * BuildGroupMatrix(tb);
        float tbAlpha = alpha * tb->alpha;
        if (_convertTextToPath) {
          // Container TextBox under glyph-path mode: process as its own scope so
          // child Text is added to the accumulator with the box's transform/alpha
          // and box-level params surface as their textBox. Geometry still
          // propagates upward like Group, and an outer Painter can render it.
          // Path-modifier resolution is scoped per Group/TextBox so that a
          // TrimPath / RoundCorner / MergePath / Repeater nested inside the
          // box only sees its sibling shapes (matches the renderer behaviour).
          const std::vector<Element*>& innerWalked =
              _resolveModifiers ? _resolver.resolve(tb->elements) : tb->elements;
          processVectorScope(out, innerWalked, tbMatrix, tbAlpha, filters, styles, tb, accumulator,
                             accumulator.size());
        } else {
          // Native PowerPoint text rendering still goes through the dedicated
          // multi-run text-box writer: PPTX represents multi-style text with
          // its own a:p/a:r runs and we don't accumulate Text geometry into
          // the surrounding scope in that mode.
          writeTextBoxGroup(out, tb, tb->elements, tbMatrix, tbAlpha, filters, styles);
        }
        break;
      }
      case NodeType::Group: {
        auto* group = static_cast<const Group*>(element);
        Matrix groupMatrix = transform * BuildGroupMatrix(group);
        float groupAlpha = alpha * group->alpha;
        // New scope start: Painters inside the Group can only see geometry
        // added from this point forward. After the recursive call returns the
        // accumulator still contains the Group's geometry, so outer Painters
        // can render it (geometry propagates upward across Group boundaries).
        // Re-run the path-modifier resolver on the inner element list so that
        // TrimPath / RoundCorner / MergePath / Repeater nested inside this
        // Group are baked into editable geometry. Group is a scope boundary —
        // these modifiers only operate on sibling shapes inside the same
        // Group, matching the tgfx renderer's per-Group VectorContext.
        const std::vector<Element*>& innerWalked =
            _resolveModifiers ? _resolver.resolve(group->elements) : group->elements;
        processVectorScope(out, innerWalked, groupMatrix, groupAlpha, filters, styles, localTextBox,
                           accumulator, accumulator.size());
        break;
      }
      case NodeType::Repeater:
        // Reaching this case means the resolver was disabled or the input
        // contained a Repeater inside an empty scope (output cleared by a
        // copies==0 case, etc.); silently skip so the remaining content still
        // renders (matches the behaviour for other unresolved modifiers like
        // TrimPath / RoundCorner / MergePath).
        break;
      default:
        // TextPath, TextModifier, RangeSelector, Polystar (when the resolver
        // is disabled), and any unrecognized element type fall through
        // silently. The layer-level rasterization probe runs in writeLayer to
        // escalate cases where dropping these elements would change the
        // visual result.
        break;
    }
  }
}

void PPTWriter::writeElements(XMLBuilder& out, const std::vector<Element*>& elements,
                              const Matrix& transform, float alpha,
                              const std::vector<LayerFilter*>& filters,
                              const std::vector<LayerStyle*>& styles,
                              const TextBox* parentTextBox) {
  // Bake every path-modifier (Polystar -> Path, Repeater -> grouped copies,
  // TrimPath / RoundCorner / MergePath -> editable Path via tgfx). Painters
  // (Fill / Stroke), Group, and text-related elements pass through unchanged
  // so the accumulator walk below behaves exactly as in the unresolved case.
  const std::vector<Element*>& walked = _resolveModifiers ? _resolver.resolve(elements) : elements;

  std::vector<AccumulatedGeometry> accumulator;
  accumulator.reserve(walked.size());
  processVectorScope(out, walked, transform, alpha, filters, styles, parentTextBox, accumulator,
                     /*scopeStart=*/0);
}

// Rasterize the entire layer (including its sub-tree) to a single embedded PNG
// and emit it as a positioned p:pic. Returns true if a picture was emitted.
bool PPTWriter::rasterizeLayerAsPicture(XMLBuilder& out, const Layer* layer, bool withBackdrop) {
  auto& buildResult = ensureBuildResult();
  auto it = buildResult.layerMap.find(layer);
  if (it == buildResult.layerMap.end() || !buildResult.root) {
    return false;
  }
  auto tgfxLayer = it->second;
  if (!tgfxLayer) {
    return false;
  }
  auto pixelScale = static_cast<float>(_rasterDPI) / 96.0f;
  auto pngData = withBackdrop ? RenderLayerCompositeWithBackdrop(&_gpu, buildResult.root, tgfxLayer,
                                                                 pixelScale)
                              : RenderMaskedLayer(&_gpu, buildResult.root, tgfxLayer, pixelScale);
  if (!pngData) {
    return false;
  }
  auto bounds = tgfxLayer->getBounds(buildResult.root.get(), true);
  auto offX = PxToEMU(bounds.left);
  auto offY = PxToEMU(bounds.top);
  auto extCX = std::max(int64_t(1), PxToEMU(bounds.width()));
  auto extCY = std::max(int64_t(1), PxToEMU(bounds.height()));
  auto relId = _ctx->addRawImage(std::move(pngData));
  writePicture(out, relId, offX, offY, extCX, extCY);
  return true;
}

// Layer filters and styles follow identical merge semantics: own entries come
// first (so CollectEffectSources picks the layer's own effect when both the
// layer and a parent carry the same effect type), then inherited entries.
// Shortcuts handle the common empty-on-either-side cases without allocating a
// new vector.
template <typename T>
static std::vector<T*> MergeLayerLists(const std::vector<T*>& own,
                                       const std::vector<T*>& inherited) {
  if (inherited.empty()) {
    return own;
  }
  if (own.empty()) {
    return inherited;
  }
  std::vector<T*> merged;
  merged.reserve(own.size() + inherited.size());
  merged.insert(merged.end(), own.begin(), own.end());
  merged.insert(merged.end(), inherited.begin(), inherited.end());
  return merged;
}

void PPTWriter::writeLayer(XMLBuilder& out, const Layer* layer, const Matrix& parentMatrix,
                           float parentAlpha, const std::vector<LayerFilter*>& inheritedFilters,
                           const std::vector<LayerStyle*>& inheritedStyles) {
  if (!layer->visible && layer->mask == nullptr) {
    return;
  }

  Matrix layerMatrix = parentMatrix * BuildLayerMatrix(layer);
  float layerAlpha = parentAlpha * layer->alpha;

  if (layer->mask != nullptr && _rasterizeUnsupported) {
    if (rasterizeLayerAsPicture(out, layer)) {
      return;
    }
    // Bake fell through (zero bounds, no GPU, etc.) - fall through to writing
    // the layer as a regular layer so its content is at least visible without
    // the mask effect.
  }

  // OOXML has no native clipping primitive for arbitrary shape children, so a
  // layer that carries a scrollRect (set explicitly or generated by clipToBounds
  // during layout) can only be honoured by rasterizing the whole layer subtree
  // through the renderer, which already applies setScrollRect via tgfx. When
  // rasterizeUnsupported is off the clip is silently dropped and child shapes
  // are emitted unclipped (matches the behaviour of unsupported features
  // elsewhere).
  if (layer->hasScrollRect && _rasterizeUnsupported) {
    if (rasterizeLayerAsPicture(out, layer)) {
      return;
    }
    // Bake fell through (zero bounds, no GPU, etc.) - fall through and emit the
    // layer's content unclipped so it remains at least partially visible.
  }

  // Probe the layer for features that OOXML cannot represent natively. Features
  // with no meaningful vector fallback (TextPath, TextModifier, ColorMatrix,
  // conic/diamond gradients, shear transforms) always bake to a PNG; features
  // with a degraded-but-valid fallback (unsupported blend modes, wide-gamut
  // color) only bake when the caller opts in via `rasterizeUnsupported`. The
  // alternative (silently dropping unsupported elements) was the V1 behaviour
  // and produced obviously wrong slides for documents with TextPath, complex
  // blends, ColorMatrix filters, or wide-gamut colors.
  auto features = ProbeLayerFeatures(layer);
  if (features.needsRasterization(_rasterizeUnsupported)) {
    // For a non-Normal blend mode, compositing against the real backdrop
    // requires rendering the whole scene clipped to the layer's bounds —
    // this turns any editable native content beneath the patch into baked
    // pixels, which is why `rasterizeUnsupported` defaults to false.
    // Every other unsupported feature (TextPath, ColorMatrix, wide-gamut
    // color, diamond/conic gradient, shear transform) is self-contained and
    // renders fine against an empty canvas.
    bool withBackdrop = features.hasUnsupportedBlend && _rasterizeUnsupported;
    if (rasterizeLayerAsPicture(out, layer, withBackdrop)) {
      return;
    }
  }

  // Merge the layer's own filters/styles with any inherited from a parent layer.
  // Own entries come first so CollectEffectSources picks the layer's own effects
  // when both layers carry the same effect type (e.g. both have a BlurFilter).
  auto effectiveFilters = MergeLayerLists(layer->filters, inheritedFilters);
  auto effectiveStyles = MergeLayerLists(layer->styles, inheritedStyles);

  writeElements(out, layer->contents, layerMatrix, layerAlpha, effectiveFilters, effectiveStyles);

  if (layer->composition != nullptr) {
    for (const auto* compLayer : layer->composition->layers) {
      writeLayer(out, compLayer, layerMatrix, layerAlpha, effectiveFilters, effectiveStyles);
    }
  }

  for (const auto* child : layer->children) {
    writeLayer(out, child, layerMatrix, layerAlpha, effectiveFilters, effectiveStyles);
  }
}

//==============================================================================
// ZIP helpers
//==============================================================================

static bool AddZipEntry(zipFile zf, const char* name, const void* data, unsigned size) {
  zip_fileinfo zi = {};
  if (zipOpenNewFileInZip(zf, name, &zi, nullptr, 0, nullptr, 0, nullptr, Z_DEFLATED,
                          Z_DEFAULT_COMPRESSION) != ZIP_OK) {
    return false;
  }
  if (zipWriteInFileInZip(zf, data, size) != ZIP_OK) {
    zipCloseFileInZip(zf);
    return false;
  }
  zipCloseFileInZip(zf);
  return true;
}

static bool AddZipString(zipFile zf, const char* name, const std::string& content) {
  return AddZipEntry(zf, name, content.data(), static_cast<unsigned>(content.size()));
}

//==============================================================================
// PPTExporter::ToFile
//==============================================================================

bool PPTExporter::ToFile(PAGXDocument& doc, const std::string& filePath, const Options& options) {
  if (!doc.isLayoutApplied()) {
    doc.applyLayout();
  }

  auto layoutContext = std::make_unique<LayoutContext>(options.fontConfig);

  PPTWriterContext context;
  PPTWriter writer(&context, &doc, options, layoutContext.get());

  // Build slide body content
  XMLBuilder body(false, 2, 0, 16384);
  for (const auto* layer : doc.layers) {
    writer.writeLayer(body, layer);
  }
  std::string bodyContent = body.release();

  // Assemble slide XML
  std::string slide;
  slide.reserve(2048 + bodyContent.size());
  slide += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>";
  slide +=
      "<p:sld xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
      "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
      "xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\">";
  slide += "<p:cSld><p:spTree>";
  slide += "<p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>";
  slide +=
      "<p:grpSpPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"0\" cy=\"0\"/>"
      "<a:chOff x=\"0\" y=\"0\"/><a:chExt cx=\"0\" cy=\"0\"/></a:xfrm></p:grpSpPr>";
  slide += bodyContent;
  slide += "</p:spTree></p:cSld>";
  slide += "<p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr>";
  slide += "</p:sld>";

  // Write ZIP via minizip
  zipFile zf = zipOpen(filePath.c_str(), APPEND_STATUS_CREATE);
  if (!zf) {
    return false;
  }

  bool ok = true;
  ok = ok && AddZipString(zf, "[Content_Types].xml", GenerateContentTypes(context));
  ok = ok && AddZipString(zf, "_rels/.rels", GenerateRootRels());
  ok = ok && AddZipString(zf, "ppt/presentation.xml", GeneratePresentation(doc.width, doc.height));
  ok = ok && AddZipString(zf, "ppt/_rels/presentation.xml.rels", GeneratePresentationRels());
  ok = ok && AddZipString(zf, "ppt/slides/slide1.xml", slide);
  ok = ok && AddZipString(zf, "ppt/slides/_rels/slide1.xml.rels", GenerateSlideRels(context));
  ok = ok && AddZipString(zf, "ppt/slideMasters/slideMaster1.xml", GenerateSlideMaster());
  ok = ok &&
       AddZipString(zf, "ppt/slideMasters/_rels/slideMaster1.xml.rels", GenerateSlideMasterRels());
  ok = ok && AddZipString(zf, "ppt/slideLayouts/slideLayout1.xml", GenerateSlideLayout());
  ok = ok &&
       AddZipString(zf, "ppt/slideLayouts/_rels/slideLayout1.xml.rels", GenerateSlideLayoutRels());
  ok = ok && AddZipString(zf, "ppt/theme/theme1.xml", GenerateTheme());
  ok = ok && AddZipString(zf, "ppt/presProps.xml", GeneratePresProps());
  ok = ok && AddZipString(zf, "ppt/viewProps.xml", GenerateViewProps());
  ok = ok && AddZipString(zf, "ppt/tableStyles.xml", GenerateTableStyles());
  ok = ok && AddZipString(zf, "docProps/core.xml", GenerateCoreProps());
  ok = ok && AddZipString(zf, "docProps/app.xml", GenerateAppProps());

  for (const auto& img : context.images()) {
    if (!ok) {
      break;
    }
    if (img.cachedData && img.cachedData->size() > 0) {
      // minizip's zipWriteInFileInZip takes a 32-bit length. Reject entries
      // that would be silently truncated rather than writing a corrupt PPTX.
      if (img.cachedData->size() > std::numeric_limits<unsigned>::max()) {
        ok = false;
        break;
      }
      ok = AddZipEntry(zf, img.mediaPath.c_str(), img.cachedData->bytes(),
                       static_cast<unsigned>(img.cachedData->size()));
    }
  }

  if (!ok) {
    zipClose(zf, nullptr);
    return false;
  }
  return zipClose(zf, nullptr) == ZIP_OK;
}

}  // namespace pagx
