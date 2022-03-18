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

#include <mutex>
#include <unordered_map>
#include <vector>
#include "pag/types.h"

namespace pag {
/**
 * The data will be freed by internal.
 */
struct PAGAudioFrame {
  int64_t pts = 0;
  int64_t duration = 0;
  uint8_t* data = nullptr;
  int64_t length = 0;
};

class PAG_API PAGPCMStream {
 public:
  virtual ~PAGPCMStream() = default;

  virtual int64_t duration() = 0;

  virtual void seek(int64_t toTime) = 0;

  virtual std::shared_ptr<PAGAudioFrame> copyNextFrame() = 0;
};

class AudioCompositionTrack;

class AudioAsset;

class PAG_API PAGAudioTrack {
 public:
  PAGAudioTrack();

  /**
   * Inserts the first audio track within a given time range of a specified file
   * into the receiver.
   */
  void insertTimeRange(const TimeRange& timeRange, const std::string& ofFile, int64_t atTime,
                       float speed = 1.0f);

  void insertTimeRange(const TimeRange& timeRange, const std::shared_ptr<PAGPCMStream>& ofStream,
                       int64_t atTime, float speed = 1.0f);

  /**
   * Sets a volume ramp to apply during a specified time range.
   */
  void setVolumeRamp(float fromStartVolume, float toEndVolume, const TimeRange& forTimeRange);

 private:
  std::shared_ptr<AudioCompositionTrack> track = nullptr;

  void insertTimeRange(const TimeRange& timeRange, const std::shared_ptr<AudioAsset>& ofAsset,
                       int64_t atTime, float speed);

  friend class PAGAudio;
};

class PAGComposition;

class AudioComposition;

class PAGAudioReader;

class PAG_API PAGAudio {
 public:
  PAGAudio();

  /**
   * Adds tracks from PAGComposition and returns trackID.
   */
  int addTracks(PAGComposition* fromComposition);

  /**
   * Adds a specified track to the receiver and returns trackID.
   */
  int addTrack(const PAGAudioTrack& track);

  /**
   * Removes a specified track by trackID from the receiver.
   */
  void removeTrack(int byTrackID);

  /**
   * Removes all tracks from the receiver.
   */
  void removeAllTracks();

  /**
   * Indicates that the audio is or not empty.
   */
  bool isEmpty();

 private:
  std::shared_ptr<AudioComposition> audioComposition = nullptr;
  std::mutex locker{};
  std::vector<PAGAudioReader*> readers{};
  std::unordered_map<int, std::vector<int>> trackIDMap;

  void addReader(PAGAudioReader* reader);

  void removeReader(PAGAudioReader* reader);

  void setAudioChanged(bool changed);

  friend class FileMovie;

  friend class PAGAudioReader;
};

class AudioReader;

class PAG_API PAGAudioReader {
 public:
  static std::shared_ptr<PAGAudioReader> Make(std::shared_ptr<PAGAudio> audio,
                                              int sampleRate = 44100, int sampleCount = 1024,
                                              int channels = 2);

  ~PAGAudioReader();

  /**
   * Sets the current time to the specified time.
   */
  void seek(int64_t toTime);

  /**
   * output format: Signed 16
   * if channels == 1, channel layout is MONO
   * if channels == 2, channel layout is STEREO
   */
  std::shared_ptr<PAGAudioFrame> copyNextFrame();

 private:
  std::shared_ptr<PAGAudio> audio;
  std::shared_ptr<AudioReader> audioReader = nullptr;
  bool audioChanged = true;

  explicit PAGAudioReader(std::shared_ptr<PAGAudio> audio);

  void checkAudioChanged();

  friend class PAGAudio;
};

}  // namespace pag