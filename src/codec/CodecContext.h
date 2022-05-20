/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <unordered_map>
#include "codec/utils/StreamContext.h"
#include "pag/file.h"

namespace pag {
struct FontDescriptor {
  uint32_t id;
  std::string fontFamily;
  std::string fontStyle;
};

class CodecContext : public StreamContext {
 public:
  ~CodecContext() override;
  uint32_t getFontID(const std::string& fontFamily, const std::string& fontStyle);
  FontData getFontData(int id);
  ImageBytes* getImageBytes(ID imageID);
  std::vector<Composition*> releaseCompositions();
  std::vector<ImageBytes*> releaseImages();

  std::vector<std::string> errorMessages;
  std::unordered_map<std::string, FontDescriptor*> fontNameMap;
  std::unordered_map<int, FontDescriptor*> fontIDMap;
  std::vector<Composition*> compositions;
  std::vector<ImageBytes*> images;
  int timeStretchMode = PAGTimeStretchMode::Repeat;
  TimeRange* scaledTimeRange = nullptr;
  FileAttributes fileAttributes = {};

  std::vector<int>* editableImages = nullptr;
  std::vector<int>* editableTexts = nullptr;
  uint16_t tagLevel = 0;
};
}  // namespace pag
