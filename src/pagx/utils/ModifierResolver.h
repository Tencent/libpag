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

#include <unordered_map>
#include <vector>
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Element.h"

namespace pagx {

class PathData;
class RoundCorner;
class Text;
class TextModifier;
class TrimPath;

/**
 * ModifierResolver bakes path-modifier elements into editable geometry so downstream exporters
 * (PPT, SVG, ...) can emit each shape as native geometry without reimplementing the modifier
 * semantics. The resolver:
 *
 * - Converts every `Polystar` into a `Path` whose `PathData` contains the analytical star/polygon
 *   geometry (mirroring the math used by `tgfx::Polystar`).
 * - Expands every `Repeater` into duplicate copies of all preceding shape siblings, applying the
 *   progressive transform per the PAGX spec (offset, anchor, position, rotation, scale, alpha
 *   ramp, RepeaterOrder). This is a strict improvement over V1, which only repeated the
 *   immediately-preceding shape.
 * - Bakes `TrimPath`, `RoundCorner`, and `MergePath` into the preceding geometry by routing each
 *   sibling through `tgfx::Path` operations (`PathEffect::MakeTrim`, `PathEffect::MakeCorner`,
 *   `Path::addPath` with `PathOp`). The original modifier nodes are dropped from the output.
 *
 * The resolver allocates new `Path`/`PathData` nodes through `PAGXDocument::makeNode` so they
 * remain owned by the document for the lifetime of the export. It never mutates the original
 * elements.
 */
class ModifierResolver {
 public:
  explicit ModifierResolver(PAGXDocument* doc, size_t maxGeneratedElements = 200000)
      : _doc(doc), _nodeCountBefore(doc->nodes.size()),
        _maxGeneratedElements(maxGeneratedElements) {
  }

  ~ModifierResolver();

  /**
   * Returns a normalized copy of `elements` with all path modifiers baked. Fill, Stroke, Group,
   * TextBox, and Text elements are passed through verbatim. Sub-element lists inside Group are
   * not recursed (the caller invokes the resolver again when descending).
   */
  std::vector<Element*> resolve(const std::vector<Element*>& elements);

  /**
   * Returns true when every TextModifier inside `elements` can be baked into its target Texts
   * through GlyphRun channels alone, i.e. resolve() drops them instead of leaving them for a
   * rasterized fallback. Identity modifiers and modifiers with no target Text count as bakeable.
   * The walk mirrors resolve(), including nested Group / TextBox element lists.
   */
  bool CanBakeTextModifiers(const std::vector<Element*>& elements) const;

  /**
   * Returns the authored element a baked Path came from when the Path is the direct conversion of
   * that element (currently Polystar). Returns null when the Path was produced by a path modifier
   * or has no recorded source. Writers use this to re-emit an animated `d` track for the source.
   */
  const Element* sourceOf(const Element* baked) const;

 private:
  PAGXDocument* _doc;
  size_t _nodeCountBefore = 0;
  size_t _generatedElements = 0;
  size_t _maxGeneratedElements = 200000;

  Element* makePathFromData(PathData* data) const;

  Element* applyTrimToElement(Element* shape, const TrimPath* trim) const;

  /**
   * Bakes a Continuous-mode TrimPath into the shapes referenced by `shapeSlots`. Each shape keeps
   * its own identity in `output` (instead of being fused into one combined path) so downstream
   * gradient fitting matches the renderer's per-shape semantics. Shapes that fall entirely outside
   * the trim range are erased from both `output` and `shapeSlots`; the caller's surrounding
   * indices stay consistent because erasures happen in descending order.
   */
  void applyContinuousTrim(std::vector<Element*>& output, std::vector<size_t>& shapeSlots,
                           const TrimPath* trim) const;

  Element* applyRoundCornerToElement(Element* shape, const RoundCorner* corner) const;

  /**
   * Returns a new Text owned by the document with `modifier`'s geometry channels baked into its
   * GlyphRun channels, or nullptr when the modifier cannot be expressed through those channels.
   * `baseIndex`/`totalCount` place this Text in the scope's selector index domain.
   */
  Text* bakeTextModifier(const Text* text, const TextModifier* modifier, size_t baseIndex,
                         size_t totalCount);

  /**
   * Returns a document-owned copy of `modifier` with its geometry channels reset to identity and
   * its paint channels/selectors preserved. The exporter uses it to apply paint overrides after
   * the geometry has already been baked into the Text clone.
   */
  TextModifier* makePaintOnlyModifier(const TextModifier* modifier);

  // Direct conversion results: baked Path -> authored source element. Entries are only consulted
  // for Paths that survive in the resolved output, so a modifier replacing the bake naturally
  // drops the mapping.
  std::unordered_map<const Element*, const Element*> bakedSources = {};
};

}  // namespace pagx
