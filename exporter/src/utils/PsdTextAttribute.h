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
#include <memory>
#include <string>
#include <vector>

namespace exporter {

enum class PsdValueType {
  None = 0,
  Dictionary = 1,  // Dictionary/Hash(Vector)
  Array = 2,       // Array(single or multi-line)
  Float = 3,       // Float
  Integer = 4,     // Integer 2=bool 3=other
  String = 5,      // String
  Boolean = 6,     // Boolean
};

class PsdTextAttribute {
 public:
  PsdTextAttribute(int size, const uint8_t* data);
  PsdTextAttribute(char* src, int len) : src(src), len(len) {
  }
  ~PsdTextAttribute() = default;

  bool getIntegerByKeys(int& result, std::vector<int> keys, int keyCount);
  void skipSpaces();
  bool getKey();
  bool isBoolean(std::string_view& srcStr);
  bool isInteger(int& num, std::string_view& srcStr);
  bool isNumber(std::string_view& srcStr);
  bool getValue();
  int getAttribute();
  void buildAttributeList(int startPos, int endPos);
  void buildAttributeArray(int startPos, int endPos);

 private:
  size_t stringFormatTransform(std::vector<char>& dst, const uint8_t* src, int len);
  std::vector<char> mem = {};
  char* src = nullptr;
  int len = 0;
  int pos = 0;
  int key = -1;
  PsdValueType valueType = PsdValueType::None;
  std::vector<std::shared_ptr<PsdTextAttribute>> attributeList = {};
  int intValue = 0;
  std::string stringValue = "";
};

}  // namespace exporter
