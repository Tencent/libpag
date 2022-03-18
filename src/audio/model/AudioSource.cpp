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

#include "AudioSource.h"

namespace pag {
AudioSource::AudioSource(std::string filePath) : filePath(std::move(filePath)) {
}

AudioSource::AudioSource(std::shared_ptr<ByteData> data) : data(std::move(data)) {
}

AudioSource::AudioSource(std::shared_ptr<PAGPCMStream> stream) : stream(std::move(stream)) {
}

bool AudioSource::empty() const {
  return filePath.empty() && data == nullptr && stream == nullptr;
}
}  // namespace pag
#endif
