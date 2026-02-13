/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Image.h"

namespace pagx {

std::shared_ptr<PAGXDocument> PAGXDocument::Make(float docWidth, float docHeight) {
  auto doc = std::shared_ptr<PAGXDocument>(new PAGXDocument());
  doc->width = docWidth;
  doc->height = docHeight;
  return doc;
}

Node* PAGXDocument::findNode(const std::string& id) const {
  auto it = nodeMap.find(id);
  return it != nodeMap.end() ? it->second : nullptr;
}

std::vector<std::string> PAGXDocument::getExternalFilePaths() const {
  std::vector<std::string> paths = {};
  for (auto& node : nodes) {
    if (node->nodeType() != NodeType::Image) {
      continue;
    }
    auto* image = static_cast<Image*>(node.get());
    if (image->data != nullptr || image->filePath.empty()) {
      continue;
    }
    if (image->filePath.find("data:") == 0) {
      continue;
    }
    paths.push_back(image->filePath);
  }
  return paths;
}

bool PAGXDocument::loadFileData(const std::string& filePath, std::shared_ptr<Data> data) {
  if (filePath.empty() || data == nullptr) {
    return false;
  }
  for (auto& node : nodes) {
    if (node->nodeType() != NodeType::Image) {
      continue;
    }
    auto* image = static_cast<Image*>(node.get());
    if (image->filePath != filePath) {
      continue;
    }
    image->data = std::move(data);
    image->filePath = {};
    return true;
  }
  return false;
}

}  // namespace pagx
