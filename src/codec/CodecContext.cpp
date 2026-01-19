/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "CodecContext.h"

namespace pag {
CodecContext::~CodecContext() {
  for (auto& font : fontIDMap) {
    delete font.second;
  }
  fontIDMap.clear();
  for (auto& font : fontNameMap) {
    delete font.second;
  }
  fontNameMap.clear();
  for (auto composition : compositions) {
    delete composition;
  }
  compositions.clear();
  for (auto image : images) {
    delete image;
  }
  images.clear();
  errorMessages.clear();
  delete scaledTimeRange;
}

FontData CodecContext::getFontData(int id) {
  auto result = fontIDMap.find(id);
  if (result != fontIDMap.end()) {
    auto font = result->second;
    return {font->fontFamily, font->fontStyle};
  }
  return FontData("", "");
}

ImageBytes* CodecContext::getImageBytes(pag::ID imageID) {
  for (auto image : images) {
    if (image->id == imageID) {
      return image;
    }
  }
  for (auto image : images) {
    if (image->fileBytes == nullptr) {
      return image;
    }
  }
  auto image = new ImageBytes();
  images.push_back(image);
  return image;
}

std::vector<Composition*> CodecContext::releaseCompositions() {
  auto compositions = this->compositions;
  this->compositions.clear();
  return compositions;
}

std::vector<pag::ImageBytes*> CodecContext::releaseImages() {
  auto images = this->images;
  this->images.clear();
  return images;
}

uint32_t CodecContext::getFontID(const std::string& fontFamily, const std::string& fontStyle) {
  auto result = fontNameMap.find(fontFamily + " - " + fontStyle);
  if (result != fontNameMap.end()) {
    return result->second->id;
  }
  return 0;
}

}  // namespace pag
