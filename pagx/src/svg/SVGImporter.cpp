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

#include "pagx/svg/SVGImporter.h"
#include <fstream>
#include "SVGToPAGXConverter.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Stream.h"

namespace pagx {

std::string SVGImporter::ImportFromFile(const std::string& svgFilePath) {
  auto data = tgfx::Data::MakeFromFile(svgFilePath);
  if (!data) {
    return "";
  }
  auto stream = tgfx::Stream::MakeFromData(std::move(data));
  if (!stream) {
    return "";
  }
  return ImportFromStream(*stream);
}

std::string SVGImporter::ImportFromStream(tgfx::Stream& svgStream) {
  auto svgDOM = tgfx::SVGDOM::Make(svgStream);
  if (!svgDOM) {
    return "";
  }
  return ImportFromDOM(svgDOM);
}

std::string SVGImporter::ImportFromDOM(const std::shared_ptr<tgfx::SVGDOM>& svgDOM) {
  if (!svgDOM) {
    return "";
  }
  SVGToPAGXConverter converter(svgDOM);
  return converter.convert();
}

bool SVGImporter::SaveToFile(const std::string& pagxContent, const std::string& outputPath) {
  if (pagxContent.empty() || outputPath.empty()) {
    return false;
  }
  std::ofstream file(outputPath);
  if (!file.is_open()) {
    return false;
  }
  file << pagxContent;
  file.close();
  return true;
}

}  // namespace pagx
