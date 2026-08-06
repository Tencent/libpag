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

#include <memory>
#include <string>
#include "pagx/types/Data.h"
#include "pagx/utils/MemZip.h"
#include "zip.h"

namespace pagx {

// Resource output abstraction used by HTMLExporter. ToHTML leaves the writer
// unset on HTMLWriterContext so resources are written to resourceDir exactly
// as before; ToData sets it to an HTMLZipResourceWriter so every resource lands
// inside the in-memory archive instead.
class HTMLResourceWriter {
 public:
  virtual ~HTMLResourceWriter() = default;

  // relativePath is the complete archive entry path (e.g. "index.html" or
  // "assets/img0.png"), always '/'-separated. HTMLWriterContext prepends
  // staticImgUrlPrefix ("assets/" in ToData mode) before calling this.
  // Returns false on failure with errorMsg populated if non-null.
  virtual bool write(const std::string& relativePath, const void* bytes, size_t size,
                     std::string* errorMsg) = 0;
};

// ZIP-backed resource writer holding the whole archive in RAM. finish() returns
// the complete archive as one contiguous Data buffer; the writer owns all bytes
// until then. Destroying the writer without calling finish() releases the
// partial archive.
class HTMLZipResourceWriter final : public HTMLResourceWriter {
 public:
  HTMLZipResourceWriter();
  ~HTMLZipResourceWriter() override;

  HTMLZipResourceWriter(const HTMLZipResourceWriter&) = delete;
  HTMLZipResourceWriter& operator=(const HTMLZipResourceWriter&) = delete;

  bool write(const std::string& relativePath, const void* bytes, size_t size,
             std::string* errorMsg) override;

  // Closes the archive and returns its bytes. Returns nullptr on failure.
  std::shared_ptr<Data> finish(std::string* errorMsg);

 private:
  MemZipBuffer _buffer;
  zipFile _zip = nullptr;
};

}  // namespace pagx
