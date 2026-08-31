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

#include "pagx/html/HTMLZipWriter.h"
#include <limits>

namespace pagx {

namespace {

bool AddZipEntry(zipFile zf, const char* name, const void* data, unsigned size) {
  zip_fileinfo zi = {};
  if (zipOpenNewFileInZip(zf, name, &zi, nullptr, 0, nullptr, 0, nullptr, Z_DEFLATED,
                          Z_DEFAULT_COMPRESSION) != ZIP_OK) {
    return false;
  }
  if (zipWriteInFileInZip(zf, data, size) != ZIP_OK) {
    zipCloseFileInZip(zf);
    return false;
  }
  return zipCloseFileInZip(zf) == ZIP_OK;
}

}  // namespace

HTMLZipWriter::HTMLZipWriter() {
  zlib_filefunc_def fileFunc = MakeMemZipFileFunc(&_buffer);
  _zip = zipOpen2("in-memory.html", APPEND_STATUS_CREATE, nullptr, &fileFunc);
}

HTMLZipWriter::~HTMLZipWriter() {
  if (_zip != nullptr) {
    zipClose(_zip, nullptr);
  }
}

bool HTMLZipWriter::write(const std::string& entryPath, const void* bytes, size_t size,
                          std::string* errorMsg) {
  if (_zip == nullptr) {
    if (errorMsg) {
      *errorMsg = "HTMLZipWriter: archive not open.";
    }
    return false;
  }
  if (size > std::numeric_limits<unsigned>::max()) {
    if (errorMsg) {
      *errorMsg = "HTMLZipWriter: entry too large: " + entryPath;
    }
    return false;
  }
  if (!AddZipEntry(_zip, entryPath.c_str(), bytes, static_cast<unsigned>(size))) {
    if (errorMsg) {
      *errorMsg = "HTMLZipWriter: failed to add entry: " + entryPath;
    }
    return false;
  }
  return true;
}

std::shared_ptr<Data> HTMLZipWriter::finish(std::string* errorMsg) {
  if (_zip == nullptr) {
    if (errorMsg) {
      *errorMsg = "HTMLZipWriter: archive not open.";
    }
    return nullptr;
  }
  if (_buffer.size() == 0) {
    if (errorMsg) {
      *errorMsg = "HTMLZipWriter: archive is empty.";
    }
    zipClose(_zip, nullptr);
    _zip = nullptr;
    return nullptr;
  }
  if (zipClose(_zip, nullptr) != ZIP_OK) {
    _zip = nullptr;
    if (errorMsg) {
      *errorMsg = "HTMLZipWriter: failed to close archive.";
    }
    return nullptr;
  }
  _zip = nullptr;
  size_t size = _buffer.size();
  return Data::MakeAdopt(_buffer.release(), size);
}

}  // namespace pagx
