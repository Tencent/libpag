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

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include "zip.h"

namespace pagx {

// Growable byte buffer with a read/write cursor, driving minizip through a
// custom zlib_filefunc_def so an archive can be assembled entirely in RAM.
// minizip seeks backward to patch each local file header's CRC / sizes once the
// entry is closed, so this must support seek/tell/overwrite in addition to
// append. Shared by PPTExporter and HTMLExporter (in-memory ZIP backends).
class MemZipBuffer {
 public:
  MemZipBuffer() = default;
  ~MemZipBuffer() = default;

  MemZipBuffer(const MemZipBuffer&) = delete;
  MemZipBuffer& operator=(const MemZipBuffer&) = delete;

  size_t size() const {
    return _size;
  }

  // Transfers the allocation to the caller. The returned pointer must be
  // released with delete[]. The buffer is empty after this call.
  uint8_t* release();

  // Stream operations used by the minizip callback adapter.
  void clear();
  size_t read(void* bytes, size_t size);
  size_t write(const void* bytes, size_t size);
  size_t tell() const {
    return _position;
  }
  bool seek(size_t offset, int origin);

 private:
  bool reserve(size_t capacity);

  std::unique_ptr<uint8_t[]> _data = nullptr;
  size_t _size = 0;
  size_t _capacity = 0;
  size_t _position = 0;
};

zlib_filefunc_def MakeMemZipFileFunc(MemZipBuffer* buffer);

}  // namespace pagx
