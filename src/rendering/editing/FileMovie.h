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
#pragma once

#include "audio/utils/AudioUtils.h"
#include "base/utils/UniqueID.h"
#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/graphics/Graphic.h"

namespace pag {
class MovieInfo {
 public:
  static std::shared_ptr<MovieInfo> Load(const std::string& filePath);

  MovieInfo() : _uniqueID(UniqueID::Next()) {
  }

  ID uniqueID() const {
    return _uniqueID;
  }

  std::string filePath() const {
    return _filePath;
  }

  int videoWidth() const {
    return _videoWidth;
  }

  int videoHeight() const {
    return _videoHeight;
  }

  float frameRate() const {
    return _frameRate;
  }

  int64_t duration() const {
    return _duration;
  }

  int rotation() const {
    return _rotation;
  }

  int trackID() const {
    return _trackID;
  }

 private:
  ID _uniqueID = 0;
  std::string _filePath;
  int _videoWidth = 0;
  int _videoHeight = 0;
  float _frameRate = 0;
  int64_t _duration = 0;
  int _rotation = 0;
  int _trackID = 0;
  bool shareDecoder = false;

  friend class MovieReader;
};

class FileMovie : public PAGMovie {
 public:
  int64_t duration() override;

  void setVolumeRamp(float fromStartVolume, float toEndVolume,
                     const TimeRange& forTimeRange) override;

  // internal methods start here:

  void draw(Recorder* recorder) override;

  void measureBounds(tgfx::Rect* bounds) override;

  bool setCurrentTime(int64_t time) override;

  int64_t durationInternal() override;

  bool isFile() const override {
    return true;
  }

  std::vector<AudioClip> generateAudioClips() override;

  MovieInfo* getInfo() const {
    return info.get();
  }

 protected:
  tgfx::Rect getContentSize() const override;

 private:
  std::shared_ptr<MovieInfo> info;
  int64_t startTime = 0;
  int _width = 0;
  int _height = 0;
  int64_t _duration = 0;
  float speed = 1.0f;
  int64_t contentTime = 0;
  int64_t movieTime = 0;
  Frame lastMovieFrame = 0;
  std::vector<VolumeRange> volumeRanges = {};
  std::shared_ptr<Graphic> graphic = nullptr;
  tgfx::Matrix matrix = tgfx::Matrix::I();

  explicit FileMovie(std::shared_ptr<MovieInfo> movieInfo);

  std::shared_ptr<Graphic> getGraphic();

  friend class PAGMovie;

  friend class RenderCache;

  friend class PAGAudio;
};
}  // namespace pag
#endif
