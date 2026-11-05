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

namespace pagx {

class Layer;
class PAGXDocument;

// Returns true when both explicit content sources are declared on one layer.
bool ImportSourcesConflict(bool hasComposition, bool hasImportSource);

// Returns true when the layer declares a composition rather than an inline-import wrapper.
bool LayerHasDeclaredComposition(const Layer* layer, const PAGXDocument* document);

// Returns true when a document contains a layer with both explicit content sources.
bool DocumentHasConflictingImportSources(const PAGXDocument* document);

// Checks active imports in the document and every loaded sub-document without parsing content.
bool DocumentHasUnresolvedImports(const PAGXDocument* document);

// Resolves an import format using the explicit override, the source extension, then the first
// content element. Known HTML aliases are normalized; unsupported formats remain unchanged so
// consumers can report them. Returns an empty string when no format can be determined.
std::string ResolveImportFormat(const std::string& format, const std::string& source,
                                const std::string& content);

// Returns true for an unresolved file import or active unmounted inline content. Inline content
// overridden by a declared composition is ignored. Mounted content is renderable but remains a
// separate read-only domain; resolution flattens it into the owner's node domain.
bool LayerHasUnresolvedImport(const Layer* layer);

}  // namespace pagx
