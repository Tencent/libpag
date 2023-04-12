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

#include "SequenceFile.h"
#include "DiskCache.h"
#include "tgfx/utils/DataView.h"

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
 */
static constexpr uint32_t FILE_HEAD_SIZE = 24;
/**
 * [frameIndex: uint32_t]
 * [frameSize: uint64_t]
 */
static constexpr uint32_t FRAME_HEAD_SIZE = 12;

std::shared_ptr<SequenceFile> SequenceFile::Open(const std::string& filePath,
                                                 const tgfx::ImageInfo& info, uint32_t frameCount,
                                                 float frameRate) {
  if (filePath.empty() || info.isEmpty() || frameCount == 0 || frameRate <= 0) {
    return nullptr;
  }
  auto sequenceFile =
      std::shared_ptr<SequenceFile>(new SequenceFile(filePath, info, frameCount, frameRate));
  return sequenceFile->file ? sequenceFile : nullptr;
}

SequenceFile::SequenceFile(const std::string& filePath, const tgfx::ImageInfo& info,
                           uint32_t frameCount, float frameRate)
    : _info(info), _frameCount(frameCount), _frameRate(frameRate) {
  decoder = LZ4Decoder::Make();
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
  uint8_t buffer[FILE_HEAD_SIZE] = {};
  auto data = tgfx::DataView(buffer, FILE_HEAD_SIZE);
  auto readLength = fread(data.writableBytes(), 1, FILE_HEAD_SIZE, file);
  if (readLength != FILE_HEAD_SIZE) {
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
  auto info = tgfx::ImageInfo::Make(static_cast<int>(fileWidth), static_cast<int>(fileHeight),
                                    static_cast<tgfx::ColorType>(colorType),
                                    static_cast<tgfx::AlphaType>(alphaType), rowBytes);
  if (version != FILE_VERSION || compression != static_cast<uint8_t>(compressionType) ||
      info != _info || fileFrameCount != _frameCount || fileFrameRate != _frameRate) {
    return false;
  }
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
    if (frameIndex >= _frameCount) {
      return false;
    }
    auto& frame = frames[frameIndex];
    frame.offset = static_cast<size_t>(ftell(file));
    frame.size = frameSize;
    cachedFrames++;
    if (fseek(file, static_cast<long>(frameSize), SEEK_CUR)) {
      return false;
    }
  }
  return true;
}

bool SequenceFile::writeFileHead() {
  uint8_t buffer[FILE_HEAD_SIZE] = {};
  auto data = tgfx::DataView(buffer, FILE_HEAD_SIZE);
  data.setUint8(0, FILE_VERSION);
  data.setUint8(1, static_cast<uint8_t>(compressionType));
  data.setUint8(2, static_cast<uint8_t>(_info.colorType()));
  data.setUint8(3, static_cast<uint8_t>(_info.alphaType()));
  data.setUint32(4, static_cast<uint32_t>(_info.width()));
  data.setUint32(8, static_cast<uint32_t>(_info.height()));
  data.setUint32(12, static_cast<uint32_t>(_info.rowBytes()));
  data.setUint32(16, _frameCount);
  data.setFloat(20, _frameRate);
  auto writeLength = fwrite(data.bytes(), 1, FILE_HEAD_SIZE, file);
  _fileSize = writeLength;
  return writeLength == data.size();
}

size_t SequenceFile::fileSize() {
  std::lock_guard<std::mutex> autoLock(locker);
  return _fileSize;
}

bool SequenceFile::isComplete() {
  std::lock_guard<std::mutex> autoLock(locker);
  return cachedFrames == _frameCount;
}

bool SequenceFile::readFrame(uint32_t index, void* pixels, size_t byteSize) {
  std::lock_guard<std::mutex> autoLock(locker);
  if (index >= _frameCount || pixels == nullptr || byteSize != _info.byteSize()) {
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
    return false;
  }
  auto encodedLength = fread(scratchBuffer.bytes(), 1, frame.size, file);
  if (encodedLength != frame.size) {
    return false;
  }
  auto decodedLength = decoder->decode(reinterpret_cast<uint8_t*>(pixels), byteSize,
                                       scratchBuffer.bytes(), encodedLength);
  return (decodedLength == byteSize);
}

bool SequenceFile::writeFrame(uint32_t index, const void* pixels, size_t byteSize) {
  std::lock_guard<std::mutex> autoLock(locker);
  if (index >= _frameCount || pixels == nullptr || byteSize != _info.byteSize()) {
    return false;
  }
  auto& frame = frames[index];
  if (frame.size != 0) {
    return false;
  }
  auto compressedSize = compressFrame(index, pixels, byteSize);
  if (compressedSize == 0) {
    return false;
  }
  if (_fileSize == 0 && !writeFileHead()) {
    return false;
  }
  if (fseek(file, 0, SEEK_END)) {
    return false;
  }
  if (fwrite(scratchBuffer.bytes(), 1, compressedSize, file) != compressedSize) {
    return false;
  }
  frame.offset = _fileSize + FRAME_HEAD_SIZE;
  frame.size = compressedSize - FRAME_HEAD_SIZE;
  _fileSize += compressedSize;
  cachedFrames++;
  if (cachedFrames == _frameCount) {
    scratchBuffer.reset();
    encoder = nullptr;
  }
  if (diskCache) {
    diskCache->notifyFileSizeChanged(fileID, _fileSize);
  }
  return true;
}

size_t SequenceFile::compressFrame(uint32_t index, const void* pixels, size_t byteSize) {
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
  if (cachedFrames == _frameCount) {
    for (auto& frame : frames) {
      if (frame.size > scratchBufferSize) {
        scratchBufferSize = frame.size;
      }
    }
  } else {
    scratchBufferSize = LZ4Encoder::GetMaxOutputSize(_info.byteSize()) + FRAME_HEAD_SIZE;
  }
  scratchBuffer.alloc(scratchBufferSize);
  return !scratchBuffer.isEmpty();
}
}  // namespace pag
