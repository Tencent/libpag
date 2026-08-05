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

#include "pagx/html/HTMLResourceWriter.h"
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

HTMLZipResourceWriter::HTMLZipResourceWriter() {
  zlib_filefunc_def fileFunc = MakeMemZipFileFunc(&_buffer);
  _zip = zipOpen2("in-memory.html", APPEND_STATUS_CREATE, nullptr, &fileFunc);
}

HTMLZipResourceWriter::~HTMLZipResourceWriter() {
  if (_zip != nullptr) {
    zipClose(_zip, nullptr);
    _zip = nullptr;
  }
}

bool HTMLZipResourceWriter::write(const std::string& relativePath, const void* bytes, size_t size,
                                  std::string* errorMsg) {
  if (_zip == nullptr) {
    if (errorMsg) {
      *errorMsg = "HTMLZipResourceWriter: archive not open.";
    }
    return false;
  }
  if (size > std::numeric_limits<unsigned>::max()) {
    if (errorMsg) {
      *errorMsg = "HTMLZipResourceWriter: entry too large: " + relativePath;
    }
    return false;
  }
  if (!AddZipEntry(_zip, relativePath.c_str(), bytes, static_cast<unsigned>(size))) {
    if (errorMsg) {
      *errorMsg = "HTMLZipResourceWriter: failed to add entry: " + relativePath;
    }
    return false;
  }
  return true;
}

std::shared_ptr<Data> HTMLZipResourceWriter::finish(std::string* errorMsg) {
  if (_zip == nullptr) {
    if (errorMsg) {
      *errorMsg = "HTMLZipResourceWriter: archive not open.";
    }
    return nullptr;
  }
  if (zipClose(_zip, nullptr) != ZIP_OK) {
    _zip = nullptr;
    if (errorMsg) {
      *errorMsg = "HTMLZipResourceWriter: failed to close archive.";
    }
    return nullptr;
  }
  _zip = nullptr;
  if (_buffer.data.empty()) {
    if (errorMsg) {
      *errorMsg = "HTMLZipResourceWriter: archive is empty.";
    }
    return nullptr;
  }
  return Data::MakeWithCopy(_buffer.data.data(), _buffer.data.size());
}

}  // namespace pagx
