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

#include "pag/audio.h"

namespace pag {
class AudioSource {
 public:
  AudioSource() = default;

  explicit AudioSource(std::string filePath);

  explicit AudioSource(std::shared_ptr<ByteData> data);

  explicit AudioSource(std::shared_ptr<PAGPCMStream> stream);

  bool empty() const;

  bool isPCMStream() const {
    return stream != nullptr;
  }

 private:
  std::shared_ptr<ByteData> data = nullptr;
  std::string filePath;
  std::shared_ptr<PAGPCMStream> stream = nullptr;

  friend class URLAudio;

  friend class PCMAudio;

  friend class AudioDemuxer;

  friend class AudioSourceReader;
};
}  // namespace pag

#endif
