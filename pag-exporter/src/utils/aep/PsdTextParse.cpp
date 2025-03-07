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
#include "PsdTextParse.h"
#include <stdio.h>
#include <stdlib.h>

namespace aep {

static inline bool IsDigit(char c) {
  return c >= '0' && c <= '9';
}

static inline int CharToInt(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  } else if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  } else {
    printf("error!\n");
    return -1;
  }
}

int PsdTextAttribute::StringFormatTransform(char* dst, const uint8_t* src, int len) {
  int k = 0;
  dst[k++] = '/';
  dst[k++] = '0';
  dst[k++] = ' ';
  dst[k++] = '<';
  dst[k++] = ' ';
  for (int i = 0; i < len; i++) {
    char c = src[i];

    if (c & 0x80) { // 宽字符
      dst[k++] = 'W';
      i++;
    } else if (c == '\\') { // 转义字符
      dst[k++] = 'Z';
      i++;
    } else if (c == '<') {
      if (src[i + 1] == '<') {
        dst[k++] = '<'; // 将<<转化为<，方便后续处理

        dst[k++] = ' ';
        i++;
      } else {
        dst[k++] = 'J';
      }
    } else if (c == '>') {
      if (src[i + 1] == '>') {
        dst[k++] = '>'; // 将>>转化为>，方便后续处理
        dst[k++] = ' ';
        i++;
      } else {
        dst[k++] = 'J';
      }
    } else {
      dst[k++] = c;
      if (c == '(') {  // (TM...  0x28 0xFE 0xFF 开头的认为是UTF16的字符串
        if (i + 2 < len && ((src[i+1] == 0xFE && src[i+2] == 0xFF) || (src[i+1] == 0xFF && src[i+2] == 0xFE))) {
          i++;
          bool isBE = true;  //  0xFEFF ->  UTF-16BE
          if (src[i] == 0xFF) {  // 0xFFFE -> UTF-16LE
            isBE = false;
          }
          dst[k++] = 'T';
          dst[k++] = 'M';
          i += 2;
          while (i < len && src[i] != ')') {
            uint16_t w1 = isBE ? ((src[i] << 8) | src[i+1]) : ((src[i+1] << 8) | src[i]);
            if ((w1 & 0xFF) == 0x5C && i + 3 < len) {  // '\'
              i += 3;  // 转义符

              dst[k++] = '.';
              dst[k++] = '.';
              dst[k++] = '.';
            } else {
              if (w1 >= 0xD800 && w1 <= 0xDFFF && i + 4 < len) {
                i += 4;  // 四字节
                dst[k++] = '.';
                dst[k++] = '.';
                dst[k++] = '.';
                dst[k++] = '.';
              } else {
                i += 2;  // 两字节
                dst[k++] = '.';
                dst[k++] = '.';
              }
            }
          }
          if (i < len) {
            dst[k++] = src[i];  // ")"
          }
        }
      }
    }
  }
  dst[k++] = ' ';
  dst[k++] = '>';
  dst[k++] = '\0';
  return k;
}

PsdTextAttribute::PsdTextAttribute(int size, const uint8_t* data) {
  mem = new char[size + 1024];
  len = StringFormatTransform(mem, data, size);
  src = mem;
}

PsdTextAttribute::~PsdTextAttribute() {
  if (mem != nullptr) {
    delete[] mem;
  }
}

bool PsdTextAttribute::getIntegerByKeys(int& result, int keys[], int keyCount) {
  if (key != keys[0]) {
    return false;
  }
  if (keyCount == 1) {
    if (valueType == PsdValueType::Integer) {
      result = intValue;
      return true;
    } else {
      return false;
    }
  }
  if (valueType == PsdValueType::Dictionary || valueType == PsdValueType::Array) {
    for (auto attribute : attributeList) {
      auto ret = attribute->getIntegerByKeys(result, keys + 1, keyCount - 1);
      if (ret) {
        return true;
      }
    }
  }
  return false;
}

void PsdTextAttribute::printToJson() {
  if (key != -1) {
    printf(" \"K%d\": ", key);
  }
  if (valueType == PsdValueType::Integer) {
    printf(" %d ", intValue);
  } else if (valueType == PsdValueType::Boolean) {
    printf(" %s ", boolValue ? "true" : "false");
  } else if (valueType == PsdValueType::Float) {
    printf(" %f ", doubleValue);
  } else if (valueType == PsdValueType::String) {
    printf(" \"%s\" ", stringValue.c_str());
  } else if (valueType == PsdValueType::Array) {
    printf(" [ ");
    for (int index = 0; index < attributeList.size(); index++) {
      attributeList[index]->printToJson();
      if (index < attributeList.size() - 1) {
        printf(" , ");
      }
    }
    printf(" ] ");
  } else if (valueType == PsdValueType::Dictionary) {
    printf(" { ");
    for (int index = 0; index < attributeList.size(); index++) {
      attributeList[index]->printToJson();
      if (index < attributeList.size() - 1) {
        printf(" , ");
      }
    }
    printf(" } ");
  } else {
    printf(" error! ");
  }
}

void PsdTextAttribute::skipSpaces() {
  while (pos < len && src[pos] == ' ') {
    pos++;
  }
}

bool PsdTextAttribute::getKey() {
  skipSpaces();
  if (pos >= len) {
    return false;
  }
  if (src[pos] != '/') {
    printf("error!\n");
    return false;
  }

  auto hasDigit = false;
  key = 0;
  while (++pos < len) {
    if (IsDigit(src[pos])) {
      key = key * 10 + CharToInt(src[pos]);
      hasDigit = true;
    } else if (src[pos] == ' ') {
      if (!hasDigit) {
        printf("error!\n");
      }
      return hasDigit;
    } else {
      printf("error!\n");
      return false;
    }
  }
  printf("error!\n");
  return false;
}

bool PsdTextAttribute::getBoolean(bool& value, char* str, int len) {
  const char* (fmt[]) = {
      "false",
      "FALSE",
      "true",
      "TRUE"
  };

  for (int j = 0; j < sizeof(fmt) / sizeof(fmt[0]); j++) {
    if (len == strlen(fmt[j])) {
      int i;
      for (i = 0; i < len; i++) {
        if (str[i] != fmt[j][i]) {
          break;
        }
      }
      if (i == len) {
        value = (j >= 2);
        return true;
      }
    }
  }

  return false;
}

bool PsdTextAttribute::getInteger(int& num, char* str, int len) {
  num = 0;
  for (int i = 0; i < len; i++) {
    if (!IsDigit(str[i])) {
      return false;
    }
    num = num * 10 + CharToInt(str[i]);
  }
  return true;
}

bool PsdTextAttribute::getNumber(double& num, char* str, int len) {
  num = 0.0;
  int dotCount = 0;
  double den = 0.0;
  for (int i = 0; i < len; i++) {
    if (str[i] == '.') {
      dotCount++;
      den = 0.1;
    } else if (!IsDigit(str[i])) {
      return false;
    } else {
      if (dotCount == 0) {
        num = num * 10 + CharToInt(str[i]);
      } else {
        num = num + CharToInt(str[i]) * den;
        den /= 10;
      }
    }
  }
  return dotCount <= 1;
}

bool PsdTextAttribute::getValue() {
  skipSpaces();
  if (pos >= len) {
    return false;
  }

  if (src[pos] == '<') {
    valueType = PsdValueType::Dictionary;
    auto startFlag = '<';
    auto endFlag = '>';

    pos++;
    skipSpaces();

    auto startPos = pos;
    int num = 1;
    for (NULL; pos < len && num > 0; pos++) {
      if (src[pos] == startFlag) {
        num++;
      } else if (src[pos] == endFlag) {
        num--;
      }
    }
    if (num != 0) {
      printf("error!\n");
      return false;
    }

    buildAttributeList(startPos, pos - 1);
  } else if (src[pos] == '[') {
    valueType = PsdValueType::Array;
    auto startFlag = '[';
    auto endFlag = ']';

    pos++;
    skipSpaces();

    auto startPos = pos;
    int num = 1;
    for (NULL; pos < len && num > 0; pos++) {
      if (src[pos] == startFlag) {
        num++;
      } else if (src[pos] == endFlag) {
        num--;
      }
    }
    if (num != 0) {
      printf("error!\n");
      return false;
    }

    if (src[startPos] == '<') {
      buildAttributeArray(startPos, pos - 1);
    } else {
      if (src[startPos] == '[') {
        // to do
        printf("to do\n");
      }
      valueType = PsdValueType::String;
      stringValue = "Array";
    }
  } else {
    char endFlag = ' ';
    if (src[pos] == '(') {
      endFlag = ')';
      valueType = PsdValueType::String;
      stringValue = "WString";
    }

    int startPos = pos;
    while (pos < len) {
      if (src[pos++] == endFlag) {
        break;
      }
    }

    if (valueType != PsdValueType::String) {
      if (getInteger(intValue, src + startPos, pos - startPos - 1)) {
        valueType = PsdValueType::Integer;
      } else if (getNumber(doubleValue, src + startPos, pos - startPos - 1)) {
        valueType = PsdValueType::Float;
      } else if (getBoolean(boolValue, src + startPos, pos - startPos - 1)) {
        valueType = PsdValueType::Boolean;
      } else {
        valueType = PsdValueType::String;
        stringValue = "CString";
      }
    }
  }

  return true;
}

int PsdTextAttribute::getAttibute() {
  if (getKey()) {
    if (!getValue()) {
      printf("error!\n");
    }
  }
  return pos;
}

void PsdTextAttribute::buildAttributeList(int startPos, int endPos) {
  while (startPos < endPos) {
    auto attribute = new PsdTextAttribute(src + startPos, endPos - startPos);
    startPos += attribute->getAttibute();

    if (attribute->key >= 0 && attribute->valueType != PsdValueType::None) {
      attributeList.push_back(attribute);
    }
  }
}

void PsdTextAttribute::buildAttributeArray(int startPos, int endPos) {
  int index = 0;
  while (startPos < endPos) {
    auto attribute = new PsdTextAttribute(src + startPos, endPos - startPos);
    auto hasValue = attribute->getValue();
    startPos += attribute->pos;
    if (!hasValue) {
      delete attribute;
      break;
    }
    attribute->key = index++;
    attributeList.push_back(attribute);
  }
}

}
