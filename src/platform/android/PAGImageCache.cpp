/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "PAGImageCache.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include "third_party/lz4/lib/lz4.h"

namespace pag {

static const int WidthOffset = 0;
static const int HeightOffset = 4 * 1;
static const int FrameCountOffset = 4 * 2;
static const int MaxFrameBufferSizeOffset = 4 * 3;
static const int FrameRangesOffset = 4 * 4;
static const int FrameRangeSize = 4 * 2;

std::shared_ptr<PAGImageCache> PAGImageCache::Make(const std::string& path, int width, int height,
                                                   int frameCount, bool needInit) {
  int fd = open(path.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    return nullptr;
  }
  size_t headerSize = FrameRangesOffset + frameCount * FrameRangeSize;
  int* headerBuffer = new int[headerSize / 4];
  lseek(fd, 0, SEEK_SET);
  if (needInit) {
    memset(headerBuffer, 0, headerSize);
    headerBuffer[WidthOffset / 4] = width;
    headerBuffer[HeightOffset / 4] = height;
    headerBuffer[FrameCountOffset / 4] = frameCount;
    if ((ssize_t)headerSize != write(fd, headerBuffer, headerSize)) {
      return nullptr;
    }
  } else {
    if ((ssize_t)(headerSize) != read(fd, headerBuffer, headerSize)) {
      return nullptr;
    }
  }
  auto cache = std::make_shared<PAGImageCache>();
  cache->headerBuffer = headerBuffer;
  cache->fd = fd;
  cache->frameCount = frameCount;
  return cache;
}

bool PAGImageCache::savePixels(int frame, void* bitmapPixels, long byteCount) {
  if (headerBuffer == nullptr) {
    return false;
  }
  if (frame < 0 | frame >= frameCount) {
    return false;
  }
  int* intHeaderBuffer = static_cast<int*>(headerBuffer);
  auto bound = LZ4_compressBound(byteCount);
  if (compressBuffer == nullptr) {
    compressBuffer = new char[bound];
  }
  auto compressSize =
      LZ4_compress_default((const char*)(bitmapPixels), (char*)compressBuffer, byteCount, bound);
  lseek(fd, 0, SEEK_END);
  struct stat statbuf;
  fstat(fd, &statbuf);
  auto offset = statbuf.st_size;
  if (static_cast<ssize_t>(compressSize) != write(fd, compressBuffer, compressSize)) {
    return false;
  }
  auto frameRangeIndex = (FrameRangesOffset + frame * FrameRangeSize);
  intHeaderBuffer[frameRangeIndex / 4] = offset;
  intHeaderBuffer[frameRangeIndex / 4 + 1] = compressSize;
  if (intHeaderBuffer[MaxFrameBufferSizeOffset / 4] < compressSize) {
    intHeaderBuffer[MaxFrameBufferSizeOffset / 4] = compressSize;
    if (MaxFrameBufferSizeOffset != lseek(fd, MaxFrameBufferSizeOffset, SEEK_SET)) {
      return false;
    }
    if (4 != write(fd, &compressSize, 4)) {
      return false;
    }
  }
  if (frameRangeIndex != lseek(fd, frameRangeIndex, SEEK_SET)) {
    return false;
  }
  return 2 * 4 != write(fd, static_cast<int*>(headerBuffer) + frameRangeIndex / 4, 2 * 4);
}

bool PAGImageCache::inflatePixels(int frame, void* bitmapPixels, int byteCount) {
  if (headerBuffer == nullptr) {
    return false;
  }
  int* intHeaderBuffer = static_cast<int*>(headerBuffer);
  auto frameRangeIndex = (FrameRangesOffset + frame * FrameRangeSize);
  int offset = intHeaderBuffer[frameRangeIndex / 4];
  int length = intHeaderBuffer[frameRangeIndex / 4 + 1];
  if (offset <= 0 || length <= 0) {
    return false;
  }
  int maxFrameBufferSize = intHeaderBuffer[MaxFrameBufferSizeOffset / 4];
  if (deCompressBuffer == nullptr) {
    deCompressBuffer = new int[maxFrameBufferSize];
    deCompressBufferSize = maxFrameBufferSize;
  } else {
    if (deCompressBufferSize < maxFrameBufferSize) {
      delete[] static_cast<int*>(deCompressBuffer);
      deCompressBuffer = new int[maxFrameBufferSize];
      deCompressBufferSize = maxFrameBufferSize;
    }
  }
  lseek(fd, offset, SEEK_SET);
  if (read(fd, deCompressBuffer, length) != (ssize_t)length) {
    return false;
  }
  return byteCount != LZ4_decompress_safe((char*)deCompressBuffer, static_cast<char*>(bitmapPixels),
                                          length, byteCount);
}

bool PAGImageCache::isAllCached() {
  if (headerBuffer == nullptr || frameCount <= 0) {
    return false;
  }
  for (int i = 0; i < frameCount; i++) {
    if (!isCached(i)) {
      return false;
    }
  }
  return true;
}

bool PAGImageCache::isCached(int frame) {
  if (headerBuffer == nullptr) {
    return false;
  }
  auto frameRangeIndex = FrameRangesOffset + frame * FrameRangeSize;
  if (static_cast<int*>(headerBuffer)[frameRangeIndex / 4] > 0) {
    return true;
  }
  return false;
}

void PAGImageCache::release() {
  if (fd > -1) {
    close(fd);
    fd = -1;
  }
  if (headerBuffer != nullptr) {
    delete[] static_cast<int*>(headerBuffer);
    headerBuffer = nullptr;
  }
  if (compressBuffer != nullptr) {
    delete[] static_cast<char*>(compressBuffer);
    compressBuffer = nullptr;
  }
  if (deCompressBuffer != nullptr) {
    delete[] static_cast<int*>(deCompressBuffer);
    deCompressBuffer = nullptr;
  }
}

PAGImageCache::~PAGImageCache() {
  release();
}

}  // namespace pag
