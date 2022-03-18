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
#ifdef FILE_MOVIE

#include <list>
#include "AudioTrackSegment.h"

namespace pag {
class AudioTrack {
 public:
  virtual std::string type() const {
    return "AudioTrack";
  }

  virtual ~AudioTrack() = default;

  int64_t duration() const;

  int trackID() const {
    return _trackID;
  }

 protected:
  int _trackID = TRACK_ID_INVALID;

  explicit AudioTrack(int trackID) : _trackID(trackID) {
  }

 private:
  std::list<AudioTrackSegment> _segments{};

  std::list<AudioTrackSegment> segmentsForTimeRange(TimeRange timeRange) const;

  friend class URLAudio;

  friend class PCMAudio;

  friend class AudioTrackReader;

  friend class AudioCompositionTrack;

  friend class PAGAudio;
};
}  // namespace pag

#endif
