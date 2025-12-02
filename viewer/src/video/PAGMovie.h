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

#include "MovieInfo.h"
#include "audio/model/AudioClip.h"
#include "pag/pag.h"

namespace pag {

class PAGMovie : public PAGImage {
 public:
  static std::shared_ptr<PAGMovie> MakeFromFile(const std::string& filePath);
  static std::shared_ptr<PAGMovie> MakeFromFile(const std::string& filePath, int64_t startTime,
                                                int64_t duration, float speed = 1.0f);

  int64_t duration();
  std::vector<AudioClip> generateAudioClips();

 protected:
  std::shared_ptr<Graphic> getGraphic(Frame contentFrame) const override;
  bool isStill() const override;
  Frame getContentFrame(int64_t time) const override;

 private:
  PAGMovie(std::shared_ptr<MovieInfo> movieInfo);

  std::shared_ptr<MovieInfo> movieInfo = nullptr;
};

}  // namespace pag