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

#include "pagx/html/importer/HTMLImageResources.h"
#include <utility>
#include "pagx/PAGXDocument.h"
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/html/importer/HTMLIdAllocator.h"
#include "pagx/nodes/Image.h"

namespace pagx {

HTMLImageResources::HTMLImageResources(HTMLIdAllocator& idAllocator) : _idAllocator(idAllocator) {
}

void HTMLImageResources::bindDocument(PAGXDocument* document) {
  _document = document;
}

void HTMLImageResources::setBasePath(std::string basePath) {
  _basePath = std::move(basePath);
}

std::string HTMLImageResources::resolveSource(const std::string& src) const {
  return pagx::html::LooksAbsolutePath(src) ? src : (_basePath + src);
}

Image* HTMLImageResources::registerResource(const std::string& imageSource) {
  if (imageSource.empty()) return nullptr;
  auto it = _sourceToImage.find(imageSource);
  if (it != _sourceToImage.end()) return it->second;
  auto imageNode = _document->makeNode<Image>();
  imageNode->id = _idAllocator.generateUnique("image");
  imageNode->filePath = imageSource;
  _sourceToImage[imageSource] = imageNode;
  return imageNode;
}

}  // namespace pagx
