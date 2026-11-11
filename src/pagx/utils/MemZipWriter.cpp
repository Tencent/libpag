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
//  license is distributed on an "AS IS" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/utils/MemZipWriter.h"
#include <limits>

namespace pagx {

namespace {

bool AddZipEntry(zipFile zf, const char* name, const void* data, unsigned size,
                 int compressionLevel) {
  zip_fileinfo zi = {};
  if (zipOpenNewFileInZip(zf, name, &zi, nullptr, 0, nullptr, 0, nullptr, Z_DEFLATED,
                          compressionLevel) != ZIP_OK) {
    return false;
  }
  if (zipWriteInFileInZip(zf, data, size) != ZIP_OK) {
    zipCloseFileInZip(zf);
    return false;
  }
  return zipCloseFileInZip(zf) == ZIP_OK;
}

}  // namespace

MemZipWriter::MemZipWriter() {
  zlib_filefunc_def fileFunc = MakeMemZipFileFunc(&_buffer);
  _zip = zipOpen2("in-memory.zip", APPEND_STATUS_CREATE, nullptr, &fileFunc);
}

MemZipWriter::~MemZipWriter() {
  if (_zip != nullptr) {
    zipClose(_zip, nullptr);
  }
}

bool MemZipWriter::write(const std::string& entryPath, const void* bytes, size_t size,
                         std::string* errorMsg, int compressionLevel) {
  if (_zip == nullptr) {
    if (errorMsg) {
      *errorMsg = "MemZipWriter: archive not open.";
    }
    return false;
  }
  if (size > std::numeric_limits<unsigned>::max()) {
    if (errorMsg) {
      *errorMsg = "MemZipWriter: entry too large: " + entryPath;
    }
    return false;
  }
  if (!AddZipEntry(_zip, entryPath.c_str(), bytes, static_cast<unsigned>(size), compressionLevel)) {
    if (errorMsg) {
      *errorMsg = "MemZipWriter: failed to add entry: " + entryPath;
    }
    return false;
  }
  return true;
}

std::shared_ptr<Data> MemZipWriter::finish(std::string* errorMsg) {
  if (_zip == nullptr) {
    if (errorMsg) {
      *errorMsg = "MemZipWriter: archive not open.";
    }
    return nullptr;
  }
  if (_buffer.size() == 0) {
    if (errorMsg) {
      *errorMsg = "MemZipWriter: archive is empty.";
    }
    zipClose(_zip, nullptr);
    _zip = nullptr;
    return nullptr;
  }
  if (zipClose(_zip, nullptr) != ZIP_OK) {
    _zip = nullptr;
    if (errorMsg) {
      *errorMsg = "MemZipWriter: failed to close archive.";
    }
    return nullptr;
  }
  _zip = nullptr;
  size_t size = _buffer.size();
  return Data::MakeAdopt(_buffer.release(), size);
}

}  // namespace pagx
