/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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
#ifndef PSDTEXTPARSE_H
#define PSDTEXTPARSE_H
#include <vector>
#include <string>

namespace aep {

class PsdValueType {
 public:
  static const int None = 0;
  static const int Dictionary = 1; // Dictionary/Hash(Vector)
  static const int Array = 2;      // Array(single or multi-line)
  static const int Float = 3;      // Float
  static const int Integer = 4;    // Integer 2=bool 3=other
  static const int String = 5;     // String
  static const int Boolean = 6;    // Boolean
};

class PsdTextAttribute {
 public:
  static int StringFormatTransform(char* dst, const uint8_t* src, int len);

  PsdTextAttribute(int size, const uint8_t* data);
  ~PsdTextAttribute();

  bool getIntegerByKeys(int& result, int keys[], int keyCount);
  void printToJson();
  void skipSpaces();
  bool getKey();
  bool getBoolean(bool& value, char* str, int len);
  bool getInteger(int& num, char* str, int len);
  bool getNumber(double& num, char* str, int len);
  bool getValue();
  int getAttibute();
  void buildAttributeList(int startPos, int endPos);
  void buildAttributeArray(int startPos, int endPos);

 private:
  PsdTextAttribute(char* src, int len) : src(src), len(len) {}

  char* mem = nullptr;
  char* src = nullptr;
  int len = 0;
  int pos = 0;
  int key = -1;
  int valueType = PsdValueType::None;
  std::vector<PsdTextAttribute*> attributeList;
  int intValue = 0;
  double doubleValue = 0.0;
  bool boolValue = false;
  std::string stringValue = "";
  void* otherValue = nullptr;
};

}
#endif //PSDTEXTPARSE_H
