/*
 * Copyright 2022 Tencent Inc. All Rights Reserved.
 *
 * Mp4BoxHelper.h
 *
 *  Created on: 2022/2/22
 *      Author: katacai
 */
#pragma once

#include "codec/utils/ByteArray.h"

namespace pag {
class Mp4BoxHelper {
 public:
  /**
   * mux h264 data in VideoSequence and return mp4 data
   */
  static std::unique_ptr<ByteData> covertToMp4(VideoSequence* videoSequence);

  /**
   * create mp4 header box data, and write into VideoSequence mp4Header member
   */
  static void writeMp4Header(VideoSequence* videoSequence);

 private:
  static std::unique_ptr<ByteData> createMp4(VideoSequence& videoSequence);
  static std::unique_ptr<ByteData> concatMp4(VideoSequence& videoSequence);
  static void writeMdatBox(VideoSequence& sequence, ByteArray& payload, int mdatSize);
  static int64_t getCurrentTimeMillis();
  static int64_t getCurrentTimeMicrosecond();
};
}  // namespace pag
