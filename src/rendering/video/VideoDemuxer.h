/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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
#include "VideoSample.h"
#include "rendering/video/VideoFormat.h"

namespace pag {
/**
 * VideoDemuxer extracts encoded video sample data from a data source.
 */
class VideoDemuxer {
 public:
  virtual ~VideoDemuxer() = default;

  /**
   * Returns true if all of the frames in the video are the same .
   */
  virtual bool staticContent() const {
    return false;
  }

  /**
   * Returns the descriptions of the video format.
   */
  virtual VideoFormat getFormat() = 0;

  /**
   * Returns the next sample data in the demuxer. Returns an empty VideoSample if no more sample
   * data is available. Note: The returned VideoSample is valid until the next call to the
   * nextSample() method.
   */
  virtual VideoSample nextSample() = 0;

  /**
   * Returns the sample time closest to the specified target time.
   */
  virtual int64_t getSampleTimeAt(int64_t targetTime) = 0;

  /**
   * Returns true if calling seekTo() is required from currentSampleTime to targetSampleTime.
   */
  virtual bool needSeeking(int64_t currentTime, int64_t targetTime) = 0;

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
