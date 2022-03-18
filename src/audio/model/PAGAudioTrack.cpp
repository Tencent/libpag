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

#ifdef FILE_MOVIE

#include "AudioAsset.h"
#include "AudioCompositionTrack.h"
#include "pag/file.h"

namespace pag {
PAGAudioTrack::PAGAudioTrack() {
  track = std::make_shared<AudioCompositionTrack>(TRACK_ID_INVALID);
}

void PAGAudioTrack::insertTimeRange(const TimeRange& timeRange,
                                    const std::shared_ptr<AudioAsset>& ofAsset, int64_t atTime,
                                    float speed) {
  if (ofAsset == nullptr || ofAsset->tracks().empty()) {
    return;
  }
  track->insertTimeRange(timeRange, **ofAsset->tracks().begin(), atTime);
  if (speed != 1) {
    auto targetTimeRange = TimeRange{atTime, atTime + timeRange.duration()};
    track->scaleTimeRange(targetTimeRange,
                          static_cast<int64_t>(static_cast<float>(timeRange.duration()) / speed));
  }
}

void PAGAudioTrack::insertTimeRange(const TimeRange& timeRange, const std::string& ofFile,
                                    int64_t atTime, float speed) {
  insertTimeRange(timeRange, AudioAsset::Make(ofFile), atTime, speed);
}

void PAGAudioTrack::insertTimeRange(const TimeRange& timeRange,
                                    const std::shared_ptr<PAGPCMStream>& ofStream, int64_t atTime,
                                    float speed) {
  insertTimeRange(timeRange, AudioAsset::Make(ofStream), atTime, speed);
}

void PAGAudioTrack::setVolumeRamp(float fromStartVolume, float toEndVolume,
                                  const TimeRange& forTimeRange) {
  track->setVolumeRamp(fromStartVolume, toEndVolume, forTimeRange);
}
}  // namespace pag
#endif
