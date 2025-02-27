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
#ifndef COMMONMETHOD_H
#define COMMONMETHOD_H
#include <fstream>

template <typename T>
class AssignRecover {
 public:
  // 构造函数暂存原值并赋新值，析构函数时恢复原值
  AssignRecover(T& storeValue) : value(storeValue), pAddr(&storeValue) {
  }

  AssignRecover(T& storeValue, T newValue) : value(storeValue), pAddr(&storeValue) {
    storeValue = newValue;
  }

  ~AssignRecover() {
    *pAddr = value;
  }

 private:
  T* pAddr = nullptr;
  T value;
};

class FileIO {
 public:
  static std::string ReadTextFile(const char* filename) {
    std::ifstream inputStream(filename);
    std::string text((std::istreambuf_iterator<char>(inputStream)),
                     (std::istreambuf_iterator<char>()));
    inputStream.close();
    return text;
  }

  static int WriteTextFile(const char* filename, const char* text) {
    int len = 0;
    FILE* fp = fopen(filename, "wt");
    if (fp != nullptr) {
      len = fprintf(fp, "%s", text);
      fclose(fp);
    }
    return len;
  }

  static int WriteTextFile(const char* filename, const std::string text) {
    return WriteTextFile(filename, text.c_str());
  }

  static int GetFileSize(const std::string fileName) {
    FILE* fp = fopen(fileName.c_str(), "rb");
    if (fp == nullptr) {
      return 0;
    }
    fseek(fp, 0, SEEK_END);
    auto size = ftell(fp);
    fclose(fp);
    return static_cast<int>(size);
  }
};
#endif  //COMMONMETHOD_H
