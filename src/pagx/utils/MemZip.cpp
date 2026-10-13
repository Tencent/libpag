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
#include <limits>
#include <new>

namespace pagx {

namespace {

voidpf ZCALLBACK MemZipOpen(voidpf opaque, const char*, int) {
  // A single buffer is shared for the whole archive; reset the cursor so the
  // stream starts writing at the beginning.
  auto* buffer = static_cast<MemZipBuffer*>(opaque);
  buffer->clear();
  return opaque;
}

uLong ZCALLBACK MemZipRead(voidpf, voidpf stream, void* buf, uLong size) {
  auto* buffer = static_cast<MemZipBuffer*>(stream);
  return static_cast<uLong>(buffer->read(buf, size));
}

uLong ZCALLBACK MemZipWrite(voidpf, voidpf stream, const void* buf, uLong size) {
  auto* buffer = static_cast<MemZipBuffer*>(stream);
  return static_cast<uLong>(buffer->write(buf, size));
}

long ZCALLBACK MemZipTell(voidpf, voidpf stream) {
  auto* buffer = static_cast<MemZipBuffer*>(stream);
  if (buffer->tell() > static_cast<size_t>(std::numeric_limits<long>::max())) {
    return -1;
  }
  return static_cast<long>(buffer->tell());
}

long ZCALLBACK MemZipSeek(voidpf, voidpf stream, uLong offset, int origin) {
  auto* buffer = static_cast<MemZipBuffer*>(stream);
  return buffer->seek(offset, origin) ? 0 : -1;
}

int ZCALLBACK MemZipClose(voidpf, voidpf) {
  return 0;
}

int ZCALLBACK MemZipError(voidpf, voidpf) {
  return 0;
}

}  // namespace

uint8_t* MemZipBuffer::release() {
  _size = 0;
  _capacity = 0;
  _position = 0;
  return _data.release();
}

void MemZipBuffer::clear() {
  _size = 0;
  _position = 0;
}

bool MemZipBuffer::reserve(size_t capacity) {
  if (capacity <= _capacity) {
    return true;
  }
  size_t newCapacity = std::max<size_t>(_capacity, 4096);
  while (newCapacity < capacity) {
    if (newCapacity > std::numeric_limits<size_t>::max() / 2) {
      newCapacity = capacity;
      break;
    }
    newCapacity *= 2;
  }
  auto newData = std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[newCapacity]);
  if (!newData) {
    return false;
  }
  if (_size > 0) {
    std::memcpy(newData.get(), _data.get(), _size);
  }
  _data = std::move(newData);
  _capacity = newCapacity;
  return true;
}

size_t MemZipBuffer::read(void* bytes, size_t size) {
  if (_position >= _size) {
    return 0;
  }
  size_t readSize = std::min(size, _size - _position);
  std::memcpy(bytes, _data.get() + _position, readSize);
  _position += readSize;
  return readSize;
}

size_t MemZipBuffer::write(const void* bytes, size_t size) {
  if (size == 0) {
    return 0;
  }
  if (size > std::numeric_limits<size_t>::max() - _position) {
    return 0;
  }
  size_t end = _position + size;
  if (!reserve(end)) {
    return 0;
  }
  std::memcpy(_data.get() + _position, bytes, size);
  _position = end;
  _size = std::max(_size, end);
  return size;
}

bool MemZipBuffer::seek(size_t offset, int origin) {
  size_t base = 0;
  switch (origin) {
    case ZLIB_FILEFUNC_SEEK_SET:
      break;
    case ZLIB_FILEFUNC_SEEK_CUR:
      base = _position;
      break;
    case ZLIB_FILEFUNC_SEEK_END:
      base = _size;
      break;
    default:
      return false;
  }
  // minizip only seeks within bytes it has already written. Rejecting anything
  // beyond that reports an error instead of silently creating a corrupt archive.
  if (base > _size || offset > _size - base) {
    return false;
  }
  _position = base + offset;
  return true;
}

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
