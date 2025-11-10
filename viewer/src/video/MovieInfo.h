/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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

#include "MovieDemuxer.h"
#include "pag/pag.h"
#include "rendering/sequences/SequenceInfo.h"

namespace pag {

class MovieInfo : public SequenceInfo {
 public:
  static std::shared_ptr<MovieInfo> Load(const std::string& filePath, int64_t startTime,
                                         int64_t duration, float speed);

  std::shared_ptr<SequenceReader> makeReader(std::shared_ptr<File> file, PAGFile* pagFile = nullptr,
                                             bool useDiskCache = false) override;
  std::shared_ptr<tgfx::Image> makeStaticImage(std::shared_ptr<File> file,
                                               bool useDiskCache) override;
  std::shared_ptr<tgfx::Image> makeFrameImage(std::shared_ptr<SequenceReader> reader,
                                              Frame targetFrame, bool useDiskCache) override;

  ID uniqueID() const override;
  int width() const override;
  int height() const override;
  Frame duration() const override;
  bool isVideo() const override;
  bool staticContent() const override;
  Frame firstVisibleFrame(const Layer* layer) const override;

 private:
  explicit MovieInfo(const std::string& filePath);
  void setMovieID(ID movieID);

  ID movieID = 0;
  float speed = 0;
  float frameRate = 0;
  int trackID = 0;
  int rotation = 0;
  int videoWidth = 0;
  int videoHeight = 0;
  int64_t startTime = 0;
  int64_t durationMs = 0;
  std::string filePath = "";
  std::unique_ptr<MovieDemuxer> demuxer = nullptr;

  friend class PAGMovie;
};

}  // namespace pag