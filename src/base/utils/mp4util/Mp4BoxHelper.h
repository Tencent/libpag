/*
 * Copyright 2022 Tencent Inc. All Rights Reserved.
 *
 * Mp4BoxHelper.h
 *
 *  Created on: 2022/2/22
 *      Author: katacai
 */
#pragma once
#include "SimpleArray.h"
#include "pag/file.h"

namespace pag {
class Mp4BoxHelper {
 public:
  /**
   * mux h264 data in VideoSequence and return mp4 data
   */
  static std::unique_ptr<ByteData> CovertToMp4(VideoSequence* videoSequence);

  /**
   * create mp4 header box data, and write into VideoSequence mp4Header member
   */
  static void WriteMp4Header(VideoSequence* videoSequence);

 private:
  static std::unique_ptr<ByteData> CreateMp4(VideoSequence& videoSequence);
  static std::unique_ptr<ByteData> ConcatMp4(VideoSequence& videoSequence);
  static void WriteMdatBox(VideoSequence& sequence, SimpleArray& payload, int mdatSize);
};
}  // namespace pag
