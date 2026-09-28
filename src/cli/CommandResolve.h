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

#include <string>
#include "cli/CommandImport.h"

namespace pagx {
class PAGXDocument;
}

namespace pagx::cli {

struct ResolveStats {
  int resolvedCount = 0;
  int errorCount = 0;
};

/**
 * Resolves all import directives (inline <svg> and `import` attribute) in `doc` in place,
 * expanding them into native PAGX nodes. External `import` sources are resolved relative to
 * `baseDir` (pass an empty string when the directive sources are already absolute/resolved,
 * e.g. straight out of the HTML importer). After merging, colliding auto-generated ids from
 * multiple inline SVGs are renamed to be unique. Does not run the optimizer.
 */
ResolveStats ResolveDocument(PAGXDocument* doc, const std::string& baseDir,
                             const ImportFormatOptions& formatOptions);

/**
 * Clears any import directive still set on a Layer (its `source`/`content`/`format`) after a
 * resolve pass could not expand it — e.g. an external SVG `<img>` whose source was unreachable.
 * The Layer is kept (its box/id/position survive) but becomes an empty, directive-free node so the
 * document stays fully flattened and renderable (`pagx render` rejects any leftover directive).
 * Returns the number of directives dropped.
 */
int DropUnresolvedDirectives(PAGXDocument* doc);

/**
 * Resolves all import directives (inline <svg> and `import` attribute) in a PAGX file,
 * expanding them into native PAGX nodes.
 */
int RunResolve(int argc, char* argv[]);

}  // namespace pagx::cli
