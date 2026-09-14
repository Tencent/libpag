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
#include <memory>
#include <string>
#include "pagx/types/Data.h"
#include "pagx/utils/MemZip.h"
#include "zip.h"

namespace pagx {

// ZIP output used exclusively by HTMLExporter::ToData. The writer owns the
// partial archive until finish() transfers its allocation into a Data object.
class HTMLZipWriter {
 public:
  HTMLZipWriter();
  ~HTMLZipWriter();

  HTMLZipWriter(const HTMLZipWriter&) = delete;
  HTMLZipWriter& operator=(const HTMLZipWriter&) = delete;

  // entryPath is the complete, '/'-separated path inside the archive, such as
  // "index.html" or "assets/img0.png".
  bool write(const std::string& entryPath, const void* bytes, size_t size, std::string* errorMsg);

  // Closes the archive and transfers its bytes into Data without copying them.
  // Returns nullptr on failure.
  std::shared_ptr<Data> finish(std::string* errorMsg);

 private:
  MemZipBuffer _buffer;
  zipFile _zip = nullptr;
};

}  // namespace pagx
