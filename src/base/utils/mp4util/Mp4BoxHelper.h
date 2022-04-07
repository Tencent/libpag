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

#include <memory>
#include "SimpleArray.h"
#include "pag/file.h"

namespace pag {
class Mp4BoxHelper {
 public:
  /**
   * Muxs h264 data in VideoSequence and return mp4 data
   */
  static std::unique_ptr<ByteData> CovertToMp4(const VideoSequence* videoSequence);

  /**
   * Creates mp4 header box data, and write into VideoSequence mp4Header member
   */
  static void WriteMp4Header(VideoSequence* videoSequence);

 private:
  static std::unique_ptr<ByteData> CreateMp4(const VideoSequence* videoSequence);
  static std::unique_ptr<ByteData> ConcatMp4(const VideoSequence* videoSequence);
  static void WriteMdatBox(const VideoSequence* videoSequence, SimpleArray* payload,
                           int32_t mdatSize);
};
}  // namespace pag
