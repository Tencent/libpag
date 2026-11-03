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

#include "pagx/utils/ImportResources.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/Image.h"

namespace pagx {

std::string GetImportSourceDirectory(const std::string& source) {
  if (source.empty()) {
    return {};
  }
  auto slash = source.find_last_of("/\\");
  if (slash != std::string::npos) {
    return source.substr(0, slash + 1);
  }
  return {};
}

std::string ResolveImportResourcePath(const std::string& directory, const std::string& path) {
  if (directory.empty() || path.empty() || path[0] == '/' || path[0] == '\\' ||
      path.find(':') != std::string::npos) {
    return path;
  }
  auto last = directory.back();
  return directory + (last == '/' || last == '\\' ? "" : "/") + path;
}

void ResolveImportResources(PAGXDocument* document, const std::string& directory) {
  if (document == nullptr || directory.empty()) {
    return;
  }
  for (const auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Image) {
      auto* image = static_cast<Image*>(node.get());
      image->filePath = ResolveImportResourcePath(directory, image->filePath);
    } else if (node->nodeType() == NodeType::Font) {
      auto* font = static_cast<Font*>(node.get());
      font->file = ResolveImportResourcePath(directory, font->file);
    }
  }
}

}  // namespace pagx
