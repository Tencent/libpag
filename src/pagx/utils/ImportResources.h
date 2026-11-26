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

class PAGXDocument;

// Returns the source directory including its separator, or empty when no directory is declared.
std::string GetImportSourceDirectory(const std::string& source);

// Anchors a relative resource path to its source directory, preserving absolute paths and URIs.
std::string ResolveImportResourcePath(const std::string& directory, const std::string& path);

// Anchors owned Image and Font resources to their source directory without parsing inline content.
void ResolveImportResources(PAGXDocument* document, const std::string& directory);

}  // namespace pagx
