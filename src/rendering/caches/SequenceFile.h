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

#pragma once

#include <mutex>
#include <string>
#include <vector>
#include "rendering/utils/LZ4Decoder.h"
#include "rendering/utils/LZ4Encoder.h"
#include "tgfx/utils/Buffer.h"

namespace pag {
class DiskCache;

struct FrameLocation {
  size_t offset = 0;
  size_t size = 0;
};

/**
 * SequenceFile provides a utility to read and write image frames in a disk file.
 */
class SequenceFile {
 public:
  ~SequenceFile();

  /**
   * Returns the width of the sequence.
   */
  uint32_t width() const {
    return _width;
  }

  /**
   * Returns the height of the sequence.
   */
  uint32_t height() const {
    return _height;
  }

  /**
   * Returns the number of frames in the sequence.
   */
  uint32_t frameCount() const {
    return _frameCount;
  }

  /**
   * Returns the frame rate of the sequence.
   */
  float frameRate() const {
    return _frameRate;
  }

  /**
   * Returns true if the sequence file already has all frames cached.
   */
  bool isComplete() const {
    return cachedFrames == _frameCount;
  }

  /**
   * Reads an image frame from the sequence. Returns false if the specified index is empty.
   */
  bool readFrame(uint32_t index, void* pixels, size_t byteSize);

  /**
   * Writes an image frame to the sequence. Returns false if the specified index is not empty and
   * leave the sequence unchanged.
   */
  bool writeFrame(uint32_t index, const void* pixels, size_t byteSize);

 private:
  std::mutex locker = {};
  DiskCache* diskCache = nullptr;
  uint32_t fileID = 0;
  FILE* file = nullptr;
  uint32_t _width = 0;
  uint32_t _height = 0;
  uint32_t _frameCount = 0;
  float _frameRate = 30.0f;
  uint32_t cachedFrames = 0;
  std::vector<FrameLocation> frames = {};
  tgfx::Buffer scratchBuffer = {};
  std::unique_ptr<LZ4Decoder> decoder = nullptr;
  std::unique_ptr<LZ4Encoder> encoder = nullptr;

  static std::shared_ptr<SequenceFile> Open(const std::string& filePath, uint32_t width,
                                            uint32_t height, uint32_t frameCount, float frameRate);

  SequenceFile(const std::string& filePath, uint32_t width, uint32_t height, uint32_t frameCount,
               float frameRate);

  bool readFramesFromFile();
  bool writeFileHead();
  size_t compressFrame(uint32_t index, const void* pixels, size_t byteSize);
  bool checkScratchBuffer(size_t inputSize);

  friend class DiskCache;
};
}  // namespace pag
