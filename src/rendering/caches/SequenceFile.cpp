/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "SequenceFile.h"
#include <cstring>
#include "DiskCache.h"
#include "base/utils/Log.h"
#include "pag/file.h"
#include "rendering/utils/Directory.h"
#include "tgfx/core/Buffer.h"
#include "tgfx/core/DataView.h"

namespace pag {
static constexpr uint8_t FILE_VERSION = 1;
/**
 * [version: uint8_t]
 * [compression: uint8_t]
 * [ColorType: uint8_t]
 * [AlphaType: uint8_t]
 * [width: uint32_t]
 * [height: uint32_t]
 * [rowBytes: uint32_t]
 * [frameCount: uint32_t]
 * [frameRate: uint32_t]
 * [staticTimeRangeCount: uint32_t]
 */
static constexpr uint32_t FILE_HEAD_SIZE = 28;
/**
 * [start: uint32_t]
 * [end: uint32_t]
 */
static constexpr uint32_t TIME_RANGE_SIZE = 8;
/**
 * [frameIndex: uint32_t]
 * [frameSize: uint64_t]
 */
static constexpr uint32_t FRAME_HEAD_SIZE = 12;

std::shared_ptr<SequenceFile> SequenceFile::Open(const std::string& filePath,
                                                 const tgfx::ImageInfo& info, int frameCount,
                                                 float frameRate,
                                                 const std::vector<TimeRange>& staticTimeRanges) {
  if (filePath.empty() || info.isEmpty() || frameCount == 0 || frameRate <= 0) {
    return nullptr;
  }
  auto sequenceFile = std::shared_ptr<SequenceFile>(
      new SequenceFile(filePath, info, frameCount, frameRate, staticTimeRanges));
  return sequenceFile->file ? sequenceFile : nullptr;
}

SequenceFile::SequenceFile(const std::string& filePath, const tgfx::ImageInfo& info, int frameCount,
                           float frameRate, std::vector<TimeRange> staticTimeRanges)
    : _info(info), _numFrames(frameCount), _frameRate(frameRate),
      _staticTimeRanges(std::move(staticTimeRanges)) {
  decoder = LZ4Decoder::Make();
  Directory::CreateRecursively(Directory::GetParentDirectory(filePath));
#ifdef __APPLE__
  compressionType = CompressionType::LZ4_APPLE;
#endif
  frames.resize(frameCount, {0, 0});
  file = fopen(filePath.c_str(), "ab+");
  if (file == nullptr) {
    return;
  }
  fseek(file, 0, SEEK_END);
  _fileSize = static_cast<size_t>(ftell(file));
  if (_fileSize == 0) {
    return;
  }
  if (!readFramesFromFile()) {
    cachedFrames = 0;
    memset(frames.data(), 0, sizeof(FrameLocation) * frames.size());
    _fileSize = 0;
    fclose(file);
    file = fopen(filePath.c_str(), "wb+");
    LOGE("The existing sequence file has been reset, which may be corrupted!");
  }
}

SequenceFile::~SequenceFile() {
  if (file != nullptr) {
    fclose(file);
  }
  if (diskCache) {
    diskCache->notifyFileClosed(fileID);
  }
}

bool SequenceFile::readFramesFromFile() {
  fseek(file, 0, SEEK_SET);
  tgfx::Buffer buffer(FILE_HEAD_SIZE);
  auto data = tgfx::DataView(buffer.bytes(), buffer.size());
  auto readLength = fread(data.writableBytes(), 1, data.size(), file);
  if (readLength != data.size()) {
    return false;
  }
  auto version = data.getUint8(0);
  auto compression = data.getUint8(1);
  auto colorType = data.getUint8(2);
  auto alphaType = data.getUint8(3);
  auto fileWidth = data.getUint32(4);
  auto fileHeight = data.getUint32(8);
  auto rowBytes = data.getUint32(12);
  auto fileFrameCount = data.getUint32(16);
  auto fileFrameRate = data.getFloat(20);
  auto staticTimeRangeCount = data.getUint32(24);
  auto info = tgfx::ImageInfo::Make(static_cast<int>(fileWidth), static_cast<int>(fileHeight),
                                    static_cast<tgfx::ColorType>(colorType),
                                    static_cast<tgfx::AlphaType>(alphaType), rowBytes);
  if (version != FILE_VERSION || compression != static_cast<uint8_t>(compressionType) ||
      info != _info || fileFrameCount != static_cast<uint32_t>(_numFrames) ||
      fileFrameRate != _frameRate || staticTimeRangeCount != _staticTimeRanges.size()) {
    return false;
  }
  for (uint32_t i = 0; i < staticTimeRangeCount; i++) {
    readLength = fread(data.writableBytes(), 1, TIME_RANGE_SIZE, file);
    if (readLength != TIME_RANGE_SIZE) {
      return false;
    }
    const auto& timeRange = _staticTimeRanges[i];
    if (timeRange.start != data.getUint32(0) || timeRange.end != data.getUint32(4)) {
      return false;
    }
  }
  long position = 0;
  while (true) {
    readLength = fread(data.writableBytes(), 1, FRAME_HEAD_SIZE, file);
    if (readLength == 0) {
      break;
    }
    if (readLength != FRAME_HEAD_SIZE) {
      return false;
    }
    auto frameIndex = data.getUint32(0);
    auto frameSize = data.getUint64(4);
    if (frameIndex >= static_cast<uint32_t>(_numFrames)) {
      return false;
    }
    auto& frame = frames[frameIndex];
    frame.offset = static_cast<size_t>(ftell(file));
    frame.size = frameSize;
    cachedFrames++;
    if (fseek(file, static_cast<long>(frameSize), SEEK_CUR)) {
      return false;
    }
    position = ftell(file);
  }
  if (position != static_cast<long>(_fileSize)) {
    return false;
  }
  for (auto& timeRange : _staticTimeRanges) {
    auto& firstFrame = frames[timeRange.start];
    if (firstFrame.size > 0) {
      cachedFrames += static_cast<int>(timeRange.duration()) - 1;
      for (auto i = timeRange.start + 1; i <= timeRange.end; i++) {
        frames[i] = firstFrame;
      }
    }
  }
  return true;
}

bool SequenceFile::writeFileHead() {
  tgfx::Buffer buffer(FILE_HEAD_SIZE + _staticTimeRanges.size() * 8);
  auto data = tgfx::DataView(buffer.bytes(), buffer.size());
  data.setUint8(0, FILE_VERSION);
  data.setUint8(1, static_cast<uint8_t>(compressionType));
  data.setUint8(2, static_cast<uint8_t>(_info.colorType()));
  data.setUint8(3, static_cast<uint8_t>(_info.alphaType()));
  data.setUint32(4, static_cast<uint32_t>(_info.width()));
  data.setUint32(8, static_cast<uint32_t>(_info.height()));
  data.setUint32(12, static_cast<uint32_t>(_info.rowBytes()));
  data.setUint32(16, _numFrames);
  data.setFloat(20, _frameRate);
  data.setUint32(24, static_cast<uint32_t>(_staticTimeRanges.size()));
  for (size_t i = 0; i < _staticTimeRanges.size(); i++) {
    auto offset = 28 + i * 8;
    data.setUint32(offset, static_cast<uint32_t>(_staticTimeRanges[i].start));
    data.setUint32(offset + 4, static_cast<uint32_t>(_staticTimeRanges[i].end));
  }
  auto writeLength = fwrite(data.bytes(), 1, data.size(), file);
  _fileSize = writeLength;
  if (writeLength != data.size()) {
    LOGE("SequenceFile::writeFileHead() write file head failed!");
    return false;
  }
  return true;
}

size_t SequenceFile::fileSize() {
  std::lock_guard<std::mutex> autoLock(locker);
  return _fileSize;
}

bool SequenceFile::isComplete() {
  std::lock_guard<std::mutex> autoLock(locker);
  return cachedFrames == _numFrames;
}

bool SequenceFile::readFrame(int index, std::shared_ptr<BitmapBuffer> bitmap) {
  std::lock_guard<std::mutex> autoLock(locker);
  if (index < 0 || index >= _numFrames || bitmap == nullptr) {
    LOGE("SequenceFile::readFrame() invalid index or pixels!");
    return false;
  }
  if (bitmap->info() != _info) {
    LOGE("SequenceFile::readFrame() the info of the specified bitmap is different from ours!");
    return false;
  }
  const auto& frame = frames[index];
  if (frame.size == 0) {
    return false;
  }
  if (!checkScratchBuffer()) {
    return false;
  }
  if (fseek(file, static_cast<long>(frame.offset), SEEK_SET)) {
    LOGE("SequenceFile::readFrame() fseek failed! (offset: %zu)", frame.offset);
    return false;
  }
  auto encodedLength = fread(scratchBuffer.bytes(), 1, frame.size, file);
  if (encodedLength != frame.size) {
    LOGE("SequenceFile::readFrame() fread failed! (size: %zu)", frame.size);
    return false;
  }
  auto byteSize = _info.byteSize();
  auto pixels = bitmap->lockPixels();
  if (pixels == nullptr) {
    LOGE("SequenceFile::readFrame() failed to lock pixels from the specified bitmap!");
    return false;
  }
  auto decodedLength = decoder->decode(reinterpret_cast<uint8_t*>(pixels), byteSize,
                                       scratchBuffer.bytes(), encodedLength);
  bitmap->unlockPixels();
  if (decodedLength != byteSize) {
    LOGE("SequenceFile::readFrame() decode failed! (decoded: %zu, expected: %zu)", decodedLength,
         byteSize);
    return false;
  }
  return true;
}

bool SequenceFile::writeFrame(int index, std::shared_ptr<BitmapBuffer> bitmap) {
  std::lock_guard<std::mutex> autoLock(locker);
  if (index < 0 || index >= _numFrames || bitmap == nullptr) {
    LOGE("SequenceFile::writeFrame() invalid index or pixels!");
    return false;
  }
  if (bitmap->info() != _info) {
    LOGE("SequenceFile::writeFrame() the specified bitmap info is different from ours!");
    return false;
  }
  auto timeRange = GetTimeRangeContains(_staticTimeRanges, index);
  if (frames[timeRange.start].size != 0) {
    return false;
  }
  auto pixels = bitmap->lockPixels();
  if (pixels == nullptr) {
    LOGE("SequenceFile::writeFrame() failed to lock pixels from the specified bitmap!");
    return false;
  }
  auto compressedSize = compressFrame(static_cast<int>(timeRange.start), pixels, _info.byteSize());
  bitmap->unlockPixels();
  if (compressedSize == 0) {
    return false;
  }
  if (_fileSize == 0 && !writeFileHead()) {
    return false;
  }
  if (fseek(file, 0, SEEK_END)) {
    LOGE("SequenceFile::writeFrame() failed to seek to the end of the file");
    return false;
  }
  if (fwrite(scratchBuffer.bytes(), 1, compressedSize, file) != compressedSize) {
    LOGE("SequenceFile::writeFrame() failed to write the compressed frame to disk");
    return false;
  }
  for (auto i = timeRange.start; i <= timeRange.end; i++) {
    auto& frame = frames[i];
    frame.offset = _fileSize + FRAME_HEAD_SIZE;
    frame.size = compressedSize - FRAME_HEAD_SIZE;
    cachedFrames++;
  }
  _fileSize += compressedSize;
  if (cachedFrames == _numFrames) {
    scratchBuffer.reset();
    encoder = nullptr;
  }
  if (diskCache) {
    diskCache->notifyFileSizeChanged(fileID, _fileSize);
  }
  return true;
}

size_t SequenceFile::compressFrame(int index, const void* pixels, size_t byteSize) {
  if (!checkScratchBuffer()) {
    return 0;
  }
  if (encoder == nullptr) {
    encoder = LZ4Encoder::Make();
  }
  auto bytes = scratchBuffer.bytes() + FRAME_HEAD_SIZE;
  auto size = scratchBuffer.size() - FRAME_HEAD_SIZE;
  auto encodedLength =
      encoder->encode(bytes, size, reinterpret_cast<const uint8_t*>(pixels), byteSize);
  if (encodedLength == 0) {
    LOGE("SequenceFile::compressFrame() failed to encode frame %d!", index);
    return 0;
  }
  tgfx::DataView dataView(scratchBuffer.bytes(), scratchBuffer.size());
  dataView.setUint32(0, index);
  dataView.setUint64(4, encodedLength);
  return encodedLength + FRAME_HEAD_SIZE;
}

bool SequenceFile::checkScratchBuffer() {
  if (!scratchBuffer.isEmpty()) {
    return true;
  }
  size_t scratchBufferSize = 0;
  if (cachedFrames == _numFrames) {
    for (auto& frame : frames) {
      if (frame.size > scratchBufferSize) {
        scratchBufferSize = frame.size;
      }
    }
  } else {
    scratchBufferSize = LZ4Encoder::GetMaxOutputSize(_info.byteSize()) + FRAME_HEAD_SIZE;
  }
  scratchBuffer.alloc(scratchBufferSize);
  if (scratchBuffer.isEmpty()) {
    LOGE("SequenceFile::checkScratchBuffer() failed to alloc scratch buffer!");
    return false;
  }
  return true;
}

bool SequenceFile::compatible(const tgfx::ImageInfo& info, int frameCount, float frameRate,
                              const std::vector<TimeRange>& staticTimeRanges) {
  if (_info != info || _numFrames != frameCount || _frameRate != frameRate ||
      _staticTimeRanges.size() != staticTimeRanges.size()) {
    return false;
  }
  return memcmp(_staticTimeRanges.data(), staticTimeRanges.data(),
                sizeof(TimeRange) * staticTimeRanges.size()) == 0;
}
}  // namespace pag
