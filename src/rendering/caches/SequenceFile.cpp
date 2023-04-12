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
static constexpr uint32_t FILE_HEAD_SIZE = 17;   // [version, width, height, frameCount, frameRate]
static constexpr uint32_t FRAME_HEAD_SIZE = 12;  // [frameIndex, frameSize]

std::shared_ptr<SequenceFile> SequenceFile::Open(const std::string& filePath, uint32_t width,
                                                 uint32_t height, uint32_t frameCount,
                                                 float frameRate) {

  auto sequenceFile = std::shared_ptr<SequenceFile>(
      new SequenceFile(filePath, width, height, frameCount, frameRate));
  return sequenceFile->file ? sequenceFile : nullptr;
}

SequenceFile::SequenceFile(const std::string& filePath, uint32_t width, uint32_t height,
                           uint32_t frameCount, float frameRate)
    : _width(width), _height(height), _frameCount(frameCount), _frameRate(frameRate) {
  file = fopen(filePath.c_str(), "ab+");
  if (file == nullptr) {
    return;
  }
  decoder = LZ4Decoder::Make();
  fseek(file, 0, SEEK_END);
  _fileSize = static_cast<size_t>(ftell(file));
  if (readFramesFromFile()) {
    return;
  }
  cachedFrames = 0;
  frames.resize(frameCount, {0, 0});
  if (_fileSize > 0) {
    fclose(file);
    _fileSize = 0;
    file = fopen(filePath.c_str(), "wb+");
    if (file == nullptr) {
      return;
    }
  }
  if (!writeFileHead()) {
    fclose(file);
    file = nullptr;
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
  frames.resize(_frameCount, {0, 0});
  uint8_t buffer[FILE_HEAD_SIZE] = {};
  auto data = tgfx::DataView(buffer, FILE_HEAD_SIZE);
  auto readLength = fread(data.writableBytes(), 1, FILE_HEAD_SIZE, file);
  if (readLength != FILE_HEAD_SIZE) {
    return false;
  }
  auto version = data.getUint8(0);
  auto fileWidth = data.getUint32(1);
  auto fileHeight = data.getUint32(5);
  auto fileFrameCount = data.getUint32(9);
  auto fileFrameRate = data.getFloat(13);
  if (version != FILE_VERSION || fileWidth != _width || fileHeight != _height ||
      fileFrameCount != _frameCount || fileFrameRate != _frameRate) {
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
  data.setUint32(1, _width);
  data.setUint32(5, _height);
  data.setUint32(9, _frameCount);
  data.setFloat(13, _frameRate);
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
  if (index >= _frameCount || byteSize == 0) {
    return false;
  }
  const auto& frame = frames[index];
  if (frame.size == 0) {
    return false;
  }
  if (!checkScratchBuffer(byteSize)) {
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
  if (index >= _frameCount || byteSize == 0) {
    return false;
  }
  auto& frame = frames[index];
  if (frame.size != 0) {
    return false;
  }
  auto bufferSize = compressFrame(index, pixels, byteSize);
  if (bufferSize == 0) {
    return false;
  }
  if (fseek(file, 0, SEEK_END)) {
    return false;
  }
  auto fileEnd = static_cast<size_t>(ftell(file));
  if (fwrite(scratchBuffer.bytes(), 1, bufferSize, file) != bufferSize) {
    return false;
  }
  frame.offset = fileEnd + FRAME_HEAD_SIZE;
  frame.size = bufferSize - FRAME_HEAD_SIZE;
  cachedFrames++;
  if (cachedFrames == _frameCount) {
    scratchBuffer.reset();
    encoder = nullptr;
  }
  _fileSize = fileEnd + bufferSize;
  if (diskCache) {
    diskCache->notifyFileSizeChanged(fileID, _fileSize);
  }
  return true;
}

size_t SequenceFile::compressFrame(uint32_t index, const void* pixels, size_t byteSize) {
  if (!checkScratchBuffer(byteSize)) {
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

bool SequenceFile::checkScratchBuffer(size_t inputSize) {
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
    scratchBufferSize = LZ4Encoder::GetMaxOutputSize(inputSize) + FRAME_HEAD_SIZE;
  }
  scratchBuffer.alloc(scratchBufferSize);
  return !scratchBuffer.isEmpty();
}
}  // namespace pag
