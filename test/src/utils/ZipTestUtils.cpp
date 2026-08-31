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

#include "utils/ZipTestUtils.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>
#include "pagx/types/Data.h"
#include "unzip.h"

namespace pag {

namespace {

struct MemoryZipView {
  const uint8_t* bytes = nullptr;
  size_t size = 0;
  size_t position = 0;
};

void SetError(std::string* errorMsg, const std::string& message) {
  if (errorMsg) {
    *errorMsg = message;
  }
}

voidpf ZCALLBACK OpenMemoryZip(voidpf opaque, const char*, int) {
  auto* view = static_cast<MemoryZipView*>(opaque);
  view->position = 0;
  return view;
}

uLong ZCALLBACK ReadMemoryZip(voidpf, voidpf stream, void* output, uLong size) {
  auto* view = static_cast<MemoryZipView*>(stream);
  if (view->position >= view->size) {
    return 0;
  }
  size_t readSize = std::min<size_t>(size, view->size - view->position);
  std::memcpy(output, view->bytes + view->position, readSize);
  view->position += readSize;
  return static_cast<uLong>(readSize);
}

uLong ZCALLBACK WriteMemoryZip(voidpf, voidpf, const void*, uLong) {
  return 0;
}

long ZCALLBACK TellMemoryZip(voidpf, voidpf stream) {
  auto* view = static_cast<MemoryZipView*>(stream);
  if (view->position > static_cast<size_t>(std::numeric_limits<long>::max())) {
    return -1;
  }
  return static_cast<long>(view->position);
}

long ZCALLBACK SeekMemoryZip(voidpf, voidpf stream, uLong offset, int origin) {
  auto* view = static_cast<MemoryZipView*>(stream);
  size_t base = 0;
  switch (origin) {
    case ZLIB_FILEFUNC_SEEK_SET:
      break;
    case ZLIB_FILEFUNC_SEEK_CUR:
      base = view->position;
      break;
    case ZLIB_FILEFUNC_SEEK_END:
      base = view->size;
      break;
    default:
      return -1;
  }
  if (base > view->size || offset > view->size - base) {
    return -1;
  }
  view->position = base + offset;
  return 0;
}

int ZCALLBACK CloseMemoryZip(voidpf, voidpf) {
  return 0;
}

int ZCALLBACK ErrorMemoryZip(voidpf, voidpf) {
  return 0;
}

zlib_filefunc_def MakeMemoryZipFileFunc(MemoryZipView* view) {
  zlib_filefunc_def fileFunc = {};
  fileFunc.zopen_file = OpenMemoryZip;
  fileFunc.zread_file = ReadMemoryZip;
  fileFunc.zwrite_file = WriteMemoryZip;
  fileFunc.ztell_file = TellMemoryZip;
  fileFunc.zseek_file = SeekMemoryZip;
  fileFunc.zclose_file = CloseMemoryZip;
  fileFunc.zerror_file = ErrorMemoryZip;
  fileFunc.opaque = view;
  return fileFunc;
}

}  // namespace

bool ExtractZipEntries(const pagx::Data* data,
                       std::unordered_map<std::string, std::string>* entries,
                       std::string* errorMsg) {
  if (data == nullptr || data->empty() || entries == nullptr) {
    SetError(errorMsg, "invalid ZIP input");
    return false;
  }
  entries->clear();
  MemoryZipView view = {data->bytes(), data->size(), 0};
  auto fileFunc = MakeMemoryZipFileFunc(&view);
  unzFile archive = unzOpen2("in-memory.zip", &fileFunc);
  if (archive == nullptr) {
    SetError(errorMsg, "failed to open ZIP archive");
    return false;
  }

  int status = unzGoToFirstFile(archive);
  while (status == UNZ_OK) {
    unz_file_info info = {};
    if (unzGetCurrentFileInfo(archive, &info, nullptr, 0, nullptr, 0, nullptr, 0) != UNZ_OK) {
      SetError(errorMsg, "failed to read ZIP entry metadata");
      unzClose(archive);
      return false;
    }
    std::vector<char> name(info.size_filename + 1, '\0');
    if (unzGetCurrentFileInfo(archive, &info, name.data(), static_cast<uLong>(name.size()), nullptr,
                              0, nullptr, 0) != UNZ_OK ||
        unzOpenCurrentFile(archive) != UNZ_OK) {
      SetError(errorMsg, "failed to open ZIP entry");
      unzClose(archive);
      return false;
    }

    std::string contents;
    contents.reserve(info.uncompressed_size);
    char buffer[4096] = {};
    int readSize = 0;
    while ((readSize = unzReadCurrentFile(archive, buffer, sizeof(buffer))) > 0) {
      contents.append(buffer, static_cast<size_t>(readSize));
    }
    if (readSize < 0 || unzCloseCurrentFile(archive) != UNZ_OK) {
      SetError(errorMsg, "failed to extract ZIP entry or validate its CRC");
      unzClose(archive);
      return false;
    }
    if (!entries->emplace(name.data(), std::move(contents)).second) {
      SetError(errorMsg, "duplicate ZIP entry: " + std::string(name.data()));
      unzClose(archive);
      return false;
    }
    status = unzGoToNextFile(archive);
  }

  bool ok = status == UNZ_END_OF_LIST_OF_FILE && unzClose(archive) == UNZ_OK;
  if (!ok) {
    SetError(errorMsg, "failed to traverse ZIP central directory");
  }
  return ok;
}

}  // namespace pag
