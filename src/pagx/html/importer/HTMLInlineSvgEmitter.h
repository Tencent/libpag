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
#include <unordered_map>

namespace pagx {

struct DOMNode;
struct Color;

/**
 * Captures inline `<svg>` subtrees as PAGX import-directive content.
 *
 * The downstream SVG importer in `pagx resolve` has no notion of CSS cascade — by the time it
 * parses the directive's content every `fill` / `stroke` must already carry a concrete colour
 * token. This emitter therefore performs two passes before serialising:
 *
 *   1. `resolveCurrentColor` rewrites every `currentColor` reference (attribute or inline-style
 *      form) using the colour cascade inside the SVG subtree, anchored at the caller-supplied
 *      root colour. Mirrors browser-snapshot's `freezeSvg`.
 *   2. `serialize` walks the subtree and produces XML preserving the original tag / attribute
 *      casing (SVG names are case-sensitive) and self-closing void elements.
 *
 * HTMLExporter output commonly hoists shared paint servers (gradients, patterns, clip paths) into
 * a single hidden `<svg><defs>` at the top of the document and references them by id from many
 * sibling `<svg>`s via `fill="url(#id)"`. A browser resolves those references through a global id
 * lookup, but each inline `<svg>` here becomes an independent import directive, so a reference to
 * a def living in another `<svg>` would fail and fall back to the SVG default fill (opaque black,
 * rendered red after error recovery). `collectSharedDefs` indexes every id-bearing `<defs>` child
 * up front; `injectReferencedDefs` then clones the transitive closure of defs a given `<svg>`
 * references but does not itself define into that `<svg>` before serialisation.
 */
class HTMLInlineSvgEmitter {
 public:
  /**
   * Walks `svgRoot` (the `<svg>` element) and substitutes every `currentColor` token in `fill`,
   * `stroke`, and inline `style="..."` declarations with the resolved concrete colour. The
   * root's own `color` / `style` is NOT re-evaluated — the caller pre-resolves it via the CSS
   * cascade and supplies it as `rootColor` (typically a hex string from
   * `FormatColorForSvgAttribute`).
   */
  void resolveCurrentColor(const std::shared_ptr<DOMNode>& svgRoot, const std::string& rootColor);

  /**
   * Indexes every id-bearing element nested inside a `<defs>` anywhere under `root` into the
   * shared-defs table, so a later inline `<svg>` that references one by id can have it injected.
   * Safe to call once per document before any `serialize` call.
   */
  void collectSharedDefs(const std::shared_ptr<DOMNode>& root);

  /**
   * Ensures `svgRoot` carries a local definition for every `url(#id)` / `href="#id"` reference it
   * makes but does not itself define, by cloning the referenced nodes (and their transitive id
   * references) from the shared-defs table into a synthesized `<defs>` prepended to `svgRoot`.
   * No-op when the SVG references nothing external or the references are already local.
   */
  void injectReferencedDefs(const std::shared_ptr<DOMNode>& svgRoot);

  /**
   * Serialises the subtree rooted at `svgNode` into XML suitable for embedding in
   * `Layer::importDirective::content`. Recursion is bounded by `MAX_HTML_RECURSION_DEPTH`.
   */
  std::string serialize(const std::shared_ptr<DOMNode>& svgNode);

  /**
   * Returns the shared `<defs>` child previously indexed by `collectSharedDefs` under `id`, or
   * nullptr when no such id was seen. Used to resolve a `clip-path: url(#id)` reference back to
   * its `<clipPath>` definition so the importer can rebuild a contour mask layer from it.
   */
  std::shared_ptr<DOMNode> lookupSharedDef(const std::string& id) const;

  /**
   * Formats a `Color` as the colour token spelling accepted by PAGX's SVG importer for
   * `fill` / `stroke` attributes (`#RRGGBB`, `#RRGGBBAA` when alpha < 1). Exposed so the
   * caller can pre-resolve the SVG root's colour before invoking `resolveCurrentColor`.
   */
  static std::string formatColorForAttribute(const Color& color);

 private:
  // id -> the shared definition node (a `<defs>` child) bearing that id. Populated by
  // `collectSharedDefs`; read by `injectReferencedDefs`.
  std::unordered_map<std::string, std::shared_ptr<DOMNode>> _sharedDefs = {};
};

}  // namespace pagx
