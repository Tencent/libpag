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

#include "pagx/utils/MemZip.h"
#include <algorithm>
#include <cstring>

namespace pagx {

namespace {

voidpf ZCALLBACK MemZipOpen(voidpf opaque, const char*, int) {
  // A single buffer is shared for the whole archive; reset the cursor so the
  // stream starts writing at the beginning.
  auto* buffer = static_cast<MemZipBuffer*>(opaque);
  buffer->data.clear();
  buffer->position = 0;
  return opaque;
}

uLong ZCALLBACK MemZipRead(voidpf, voidpf stream, void* buf, uLong size) {
  auto* buffer = static_cast<MemZipBuffer*>(stream);
  if (buffer->position >= buffer->data.size()) {
    return 0;
  }
  uLong available = static_cast<uLong>(buffer->data.size() - buffer->position);
  uLong toRead = std::min(size, available);
  std::memcpy(buf, buffer->data.data() + buffer->position, toRead);
  buffer->position += toRead;
  return toRead;
}

uLong ZCALLBACK MemZipWrite(voidpf, voidpf stream, const void* buf, uLong size) {
  auto* buffer = static_cast<MemZipBuffer*>(stream);
  size_t end = buffer->position + size;
  if (end > buffer->data.size()) {
    buffer->data.resize(end);
  }
  std::memcpy(&buffer->data[buffer->position], buf, size);
  buffer->position += size;
  return size;
}

long ZCALLBACK MemZipTell(voidpf, voidpf stream) {
  auto* buffer = static_cast<MemZipBuffer*>(stream);
  return static_cast<long>(buffer->position);
}

long ZCALLBACK MemZipSeek(voidpf, voidpf stream, uLong offset, int origin) {
  auto* buffer = static_cast<MemZipBuffer*>(stream);
  size_t base = 0;
  switch (origin) {
    case ZLIB_FILEFUNC_SEEK_SET:
      base = 0;
      break;
    case ZLIB_FILEFUNC_SEEK_CUR:
      base = buffer->position;
      break;
    case ZLIB_FILEFUNC_SEEK_END:
      base = buffer->data.size();
      break;
    default:
      return -1;
  }
  // minizip only ever seeks within the bytes it has already written (back to a
  // local header to patch its CRC / sizes, then forward to the end again).
  // Rejecting anything beyond that turns an unexpected seek into a reported
  // failure instead of a silently zero-filled, corrupt archive.
  if (offset > buffer->data.size() - base) {
    return -1;
  }
  buffer->position = base + offset;
  return 0;
}

int ZCALLBACK MemZipClose(voidpf, voidpf) {
  return 0;
}

int ZCALLBACK MemZipError(voidpf, voidpf) {
  return 0;
}

}  // namespace

zlib_filefunc_def MakeMemZipFileFunc(MemZipBuffer* buffer) {
  zlib_filefunc_def def = {};
  def.zopen_file = MemZipOpen;
  def.zread_file = MemZipRead;
  def.zwrite_file = MemZipWrite;
  def.ztell_file = MemZipTell;
  def.zseek_file = MemZipSeek;
  def.zclose_file = MemZipClose;
  def.zerror_file = MemZipError;
  def.opaque = buffer;
  return def;
}

}  // namespace pagx
