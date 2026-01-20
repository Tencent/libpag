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

#include "pagx/layers/LayerBuilder.h"
#include "PAGXParser.h"
#include "tgfx/core/Data.h"
#include "tgfx/svg/xml/XMLDOM.h"

namespace pagx {

PAGXContent LayerBuilder::FromFile(const std::string& filePath) {
  auto data = tgfx::Data::MakeFromFile(filePath);
  if (!data) {
    return {};
  }
  auto stream = tgfx::Stream::MakeFromData(data);
  if (!stream) {
    return {};
  }

  std::string basePath;
  auto lastSlash = filePath.find_last_of("/\\");
  if (lastSlash != std::string::npos) {
    basePath = filePath.substr(0, lastSlash);
  }

  return FromStream(*stream, basePath);
}

PAGXContent LayerBuilder::FromStream(tgfx::Stream& stream, const std::string& basePath) {
  auto dom = tgfx::DOM::Make(stream);
  if (!dom) {
    return {};
  }
  auto rootNode = dom->getRootNode();
  if (!rootNode || rootNode->name != "pagx") {
    return {};
  }

  PAGXParser parser(rootNode, basePath);
  if (!parser.parse()) {
    return {};
  }
  return {parser.getRoot(), parser.width(), parser.height()};
}

}  // namespace pagx
