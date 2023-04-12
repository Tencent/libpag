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

#include "ImageCache.h"
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

std::shared_ptr<ImageCache> ImageCache::Make(const std::string& path, int width, int height,
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
      delete[] static_cast<int*>(headerBuffer);
      return nullptr;
    }
  } else {
    if ((ssize_t)(headerSize) != read(fd, headerBuffer, headerSize)) {
      delete[] static_cast<int*>(headerBuffer);
      return nullptr;
    }
    utimensat(0, path.c_str(), 0, 0);
  }
  return std::shared_ptr<ImageCache>(new ImageCache(headerBuffer, fd, frameCount));
}

bool ImageCache::flushSave() {
  if (headerBuffer == nullptr || pendingSaveFrame < 0) {
    return false;
  }
  // From the memory saving point of view, locking is needed here.
  // But from a performance point of view, a copy is needed here.
  // Choose performance first for now
  if (pendingSaveBuffer == nullptr || pendingSaveBufferSize <= 0 || pendingSaveFrame < 0) {
    return true;
  }
  auto byteCount = pendingSaveBufferSize;
  auto frame = pendingSaveFrame;
  int* intHeaderBuffer = static_cast<int*>(headerBuffer);
  auto bound = LZ4_compressBound(byteCount);
  if (compressBuffer == nullptr) {
    compressBuffer = new char[bound];
  }
  auto compressSize = LZ4_compress_default((const char*)(pendingSaveBuffer), (char*)compressBuffer,
                                           byteCount, bound);
  pendingSaveFrame = -1;
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

bool ImageCache::putPixelsToSaveBuffer(int frame, void* bitmapPixels, int byteCount) {
  if (bitmapPixels == nullptr || byteCount <= 0 || frame < 0 || frame >= frameCount) {
    return false;
  }
  if (pendingSaveBufferSize != byteCount) {
    if (pendingSaveBuffer != nullptr) {
      delete[] static_cast<char*>(pendingSaveBuffer);
    }
    pendingSaveBuffer = new char[byteCount];
    pendingSaveBufferSize = byteCount;
  }
  memcpy(pendingSaveBuffer, bitmapPixels, pendingSaveBufferSize);
  pendingSaveFrame = frame;
  return true;
}

bool ImageCache::inflatePixels(int frame, void* bitmapPixels, int byteCount) {
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
  return byteCount == LZ4_decompress_safe((char*)deCompressBuffer, static_cast<char*>(bitmapPixels),
                                          length, byteCount);
}

bool ImageCache::isAllCached() {
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

bool ImageCache::isCached(int frame) {
  if (headerBuffer == nullptr) {
    return false;
  }
  auto frameRangeIndex = FrameRangesOffset + frame * FrameRangeSize;
  if (static_cast<int*>(headerBuffer)[frameRangeIndex / 4] > 0) {
    return true;
  }
  return false;
}

void ImageCache::release() {
  if (fd > -1) {
    close(fd);
    fd = -1;
  }
  if (headerBuffer != nullptr) {
    delete[] static_cast<int*>(headerBuffer);
    headerBuffer = nullptr;
  }
  if (deCompressBuffer != nullptr) {
    delete[] static_cast<int*>(deCompressBuffer);
    deCompressBuffer = nullptr;
  }
  releaseSaveBuffer();
}

ImageCache::~ImageCache() {
  release();
}

ImageCache::ImageCache(void* buffer, int fd, int frameCount)
    : headerBuffer(buffer), fd(fd), frameCount(frameCount) {
}

void ImageCache::releaseSaveBuffer() {
  if (compressBuffer != nullptr) {
    delete[] static_cast<char*>(compressBuffer);
    compressBuffer = nullptr;
  }
  if (pendingSaveBuffer != nullptr) {
    delete[] static_cast<char*>(pendingSaveBuffer);
    pendingSaveBuffer = nullptr;
    pendingSaveBufferSize = 0;
    pendingSaveFrame = -1;
  }
}

}  // namespace pag
