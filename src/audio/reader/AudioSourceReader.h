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

#include "audio/model/AudioSource.h"
#include "audio/utils/AudioUtils.h"

namespace pag {
class SampleData;

class AudioSourceReader {
 public:
  static std::unique_ptr<AudioSourceReader> Make(const AudioSource& source, int trackID,
                                                 std::shared_ptr<PCMOutputConfig> outputConfig);

  virtual ~AudioSourceReader() = default;

  virtual void seek(int64_t toTime) = 0;

  virtual SampleData copyNextSample() = 0;

 protected:
  virtual bool valid() = 0;
};
}  // namespace pag
#endif
