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

#include <vector>
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Element.h"

namespace pagx {

/**
 * PPTModifierResolver bakes path-modifier elements into editable geometry so the PPT exporter can
 * emit each shape as `a:custGeom` (or `a:prstGeom` for trivial shapes). The resolver:
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
class PPTModifierResolver {
 public:
  explicit PPTModifierResolver(PAGXDocument* doc) : _doc(doc) {
  }

  /**
   * Returns a normalized copy of `elements` with all path modifiers baked. Fill, Stroke, Group,
   * TextBox, and Text elements are passed through verbatim. Sub-element lists inside Group are
   * not recursed (the caller invokes the resolver again when descending).
   */
  std::vector<Element*> resolve(const std::vector<Element*>& elements);

 private:
  PAGXDocument* _doc;

  // Allocates a new pagx::Path whose data is a fresh PathData owned by _doc.
  Element* makePathFromData(class PathData* data) const;
};

}  // namespace pagx
