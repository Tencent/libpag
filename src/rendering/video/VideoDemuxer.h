/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <memory>
#include <vector>
#include "VideoFormat.h"

namespace pag {
/**
 * This structure stores encoded sample data.
 */
struct SampleData {
  /**
   * The byte data of the sample.
   */
  void* data = nullptr;
  /**
   * The size in bytes of the data.
   */
  size_t length = 0;

  /**
   * Sample's presentation time in microseconds.
   */
  int64_t time = 0;
};

/**
 * VideoDemuxer extracts encoded video sample data from a data source.
 */
class VideoDemuxer {
 public:
  virtual ~VideoDemuxer() = default;

  /**
   * Returns the descriptions of the video format.
   */
  virtual VideoFormat getFormat() const = 0;

  /**
   * Returns the next sample data in the demuxer. Returns an empty SampleData if no more sample data
   * is available. Note: The returned SampleData is valid until the next call to the nextSample().
   */
  virtual SampleData nextSample() = 0;

  /**
   * Returns the sample time closest to the specified target time.
   */
  virtual int64_t getSampleTimeAt(int64_t targetTime) const = 0;

  /**
   * Returns true if calling seekTo() is required from currentSampleTime to targetSampleTime.
   */
  virtual bool needSeeking(int64_t currentSampleTime, int64_t targetSampleTime) const = 0;

  /**
   * Seek to a sync sample at or before the specified target time.
   */
  virtual void seekTo(int64_t targetTime) = 0;

  /**
   * Resets the demuxer to its initial state.
   */
  virtual void reset() = 0;
};
}  // namespace pag
