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

#include "tgfx/core/Stream.h"

namespace tgfx {

class FileStream : public Stream {
 public:
  FileStream(FILE* file, size_t length) : file(file), length(length) {
  }

  ~FileStream() override {
    fclose(file);
  }

  size_t size() const override {
    return length;
  }

  bool seek(size_t position) override {
    return fseek(file, position, SEEK_SET) == 0;
  }

  bool move(int offset) override {
    return fseek(file, offset, SEEK_CUR) == 0;
  }

  size_t read(void* buffer, size_t size) override {
    return fread(buffer, 1, size, file);
  }

  bool rewind() override {
    return fseek(file, 0, SEEK_SET) == 0;
  }

 private:
  FILE* file = nullptr;
  size_t length = 0;
};

std::unique_ptr<Stream> Stream::MakeFromFile(const std::string& filePath) {
  auto file = fopen(filePath.c_str(), "rb");
  if (file == nullptr) {
    return nullptr;
  }
  fseek(file, 0, SEEK_END);
  auto length = ftell(file);
  if (length <= 0) {
    fclose(file);
    return nullptr;
  }
  fseek(file, 0, SEEK_SET);
  return std::make_unique<FileStream>(file, length);
}

}  // namespace tgfx
