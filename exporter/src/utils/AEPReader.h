/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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
#include <vector>
#include "ByteArray.h"
#include "FileHelper.h"
using namespace exporter;

namespace exporter {

struct Tag {
  std::string name;
  ByteArray bytes;
};

struct Composition {
  std::string name;
  int32_t id;
  ByteArray bytes;
};

struct Layer {
  int32_t id;
  uint16_t flags;
  std::string name;
  int32_t type;
  ByteArray bytes;
};

std::string ReadKeyName(ByteArray* bytes);

Tag ReadTag(ByteArray* bytes);

ByteArray ReadBody(ByteArray* bytes);

Tag ReadFirstTagByName(ByteArray* bytes, const std::string& tagName);

Tag ReadFirstTagByNames(ByteArray* bytes, const std::vector<std::string>& tagNames);

Tag ReadFirstGroupByMatchName(ByteArray* bytes, const std::string& matchName);

Tag ReadFirstGroupByMatchNames(ByteArray* bytes, const std::vector<std::string>& matchNames);

std::vector<Composition> ReadCompositions(ByteArray* bytes);

std::vector<Layer> ReadLayers(ByteArray* bytes);

}  // namespace exporter
