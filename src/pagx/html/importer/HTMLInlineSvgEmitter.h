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
   * Serialises the subtree rooted at `svgNode` into XML suitable for embedding in
   * `Layer::importDirective::content`. Recursion is bounded by `MAX_HTML_RECURSION_DEPTH`.
   */
  std::string serialize(const std::shared_ptr<DOMNode>& svgNode);

  /**
   * Formats a `Color` as the colour token spelling accepted by PAGX's SVG importer for
   * `fill` / `stroke` attributes (`#RRGGBB`, `#RRGGBBAA` when alpha < 1). Exposed so the
   * caller can pre-resolve the SVG root's colour before invoking `resolveCurrentColor`.
   */
  static std::string formatColorForAttribute(const Color& color);
};

}  // namespace pagx
