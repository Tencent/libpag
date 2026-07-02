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

#include "pagx/HTMLImporter.h"
#include "pagx/html/importer/HTMLParserContext.h"

namespace pagx {

//==================================================================================================
// HTMLImporter public entry points
//
// HTMLImporter is a thin facade. The DOM-walking, CSS resolution, and node emission all live in
// HTMLParserContext (see HTMLParserContext.cpp / HTMLLayerBuilder.cpp / HTMLStyleResolver.cpp /
// HTMLValueParser.cpp).
//==================================================================================================

std::shared_ptr<PAGXDocument> HTMLImporter::Parse(const std::string& filePath,
                                                  const Options& options) {
  HTMLParserContext parser(options);
  return parser.parseFile(filePath);
}

std::shared_ptr<PAGXDocument> HTMLImporter::Parse(const uint8_t* data, size_t length,
                                                  const Options& options) {
  HTMLParserContext parser(options);
  return parser.parse(data, length);
}

std::shared_ptr<PAGXDocument> HTMLImporter::ParseString(const std::string& htmlContent,
                                                        const Options& options) {
  return Parse(reinterpret_cast<const uint8_t*>(htmlContent.data()), htmlContent.size(), options);
}

}  // namespace pagx
