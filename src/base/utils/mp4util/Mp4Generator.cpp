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

#include "H264Remuxer.h"
#include "base/utils/Log.h"
#include "core/Clock.h"

namespace pag {
static const char* VIDEO = "video";
static const char* AUDIO = "audio";

std::unordered_map<std::string, int> Mp4Generator::BoxSizeMap =
    std::unordered_map<std::string, int>();
BoxParam Mp4Generator::param = BoxParam();

int Mp4Generator::WriteCharCode(SimpleArray* stream, std::string stringData, bool write) {
  int size = static_cast<int>(stringData.length());
  if (!write) {
    return size;
  }
  for (int i = 0; i < size; i++) {
    stream->writeUint8(stringData[i]);
  }
  return size;
}

int Mp4Generator::WriteH264Nalus(SimpleArray* stream, bool write) {
  if (!write) {
    return param.nalusBytesLen;
  }
  for (const auto* header : param.videoSequence->headers) {
    int32_t payloadSize = static_cast<int32_t>(header->length()) - 4;
    stream->writeInt32(payloadSize);
    stream->writeBytes(header->data(), payloadSize, 4);
  }
  for (const auto* frame : param.videoSequence->frames) {
    int32_t payloadSize = static_cast<int32_t>(frame->fileBytes->length()) - 4;
    stream->writeInt32(payloadSize);
    stream->writeBytes(frame->fileBytes->data(), payloadSize, 4);
  }
  return param.nalusBytesLen;
}

int Mp4Generator::FTYP(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [](SimpleArray* stream, bool write) -> int {
    LOGI("param->duration: %d", param.duration);
    int len = 24;
    if (!write) {
      return len;
    }
    WriteCharCode(stream, "isom", true);
    stream->writeInt32(1);
    WriteCharCode(stream, "isom", true);
    WriteCharCode(stream, "iso2", true);
    WriteCharCode(stream, "avc1", true);
    WriteCharCode(stream, "mp41", true);
    return len;
  };
  writeFun.emplace_back(innerWriteFun);

  return Box(stream, "ftyp", &writeFun, write);
}

int Mp4Generator::MOOV(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1 + param.tracks.size());
  writeFun.emplace_back(MVHD);
  for (auto& mp4Track : param.tracks) {
    param.track = mp4Track;
    writeFun.emplace_back(TRAK);
  }
  writeFun.emplace_back(MVEX);

  return Box(stream, "moov", &writeFun, write);
}

int Mp4Generator::MOOF(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(2);
  writeFun.emplace_back(MFHD);
  writeFun.emplace_back(TRAF);

  return Box(stream, "moof", &writeFun, write);
}

int Mp4Generator::MDAT(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  writeFun.emplace_back(WriteH264Nalus);

  return Box(stream, "mdat", &writeFun, write);
}

int Mp4Generator::HDLR(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  if (param.track->type == VIDEO) {
    auto innerWriteFun = [](SimpleArray* stream, bool write) -> int {
      int len = 37;
      if (!write) {
        return len;
      }
      stream->writeInt32(0);
      stream->writeInt32(0);
      stream->writeUint8(0x76);
      stream->writeUint8(0x69);
      stream->writeUint8(0x64);
      stream->writeUint8(0x65);
      stream->writeInt32(0);
      stream->writeInt32(0);
      stream->writeInt32(0);
      stream->writeUint8(0x56);
      stream->writeUint8(0x69);
      stream->writeUint8(0x64);
      stream->writeUint8(0x65);
      stream->writeUint8(0x6f);
      stream->writeUint8(0x48);
      stream->writeUint8(0x61);
      stream->writeUint8(0x6e);
      stream->writeUint8(0x64);
      stream->writeUint8(0x6c);
      stream->writeUint8(0x65);
      stream->writeUint8(0x72);
      stream->writeUint8(0x00);
      return len;
    };
    writeFun.emplace_back(innerWriteFun);
  }
  return Box(stream, "hdlr", &writeFun, write);
}

int Mp4Generator::MDHD(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [](SimpleArray* stream, bool write) -> int {
    int len = 24;
    if (!write) {
      return len;
    }
    int32_t createTime = std::floor(GetNowTime());
    stream->writeInt32(0);
    stream->writeInt32(createTime);
    stream->writeInt32(createTime);  // modify time
    stream->writeInt32(param.track->timescale);
    stream->writeInt32(0);
    stream->writeInt32(0x55C40000);
    return len;
  };
  writeFun.emplace_back(innerWriteFun);

  return Box(stream, "mdhd", &writeFun, write);
}

int Mp4Generator::MDIA(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(3);
  writeFun.emplace_back(MDHD);
  writeFun.emplace_back(HDLR);
  writeFun.emplace_back(MINF);

  return Box(stream, "mdia", &writeFun, write);
}

int Mp4Generator::MFHD(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [](SimpleArray* stream, bool write) -> int {
    int len = 8;
    if (!write) {
      return len;
    }
    stream->writeInt32(0);
    stream->writeInt32(param.sequenceNumber);
    return len;
  };
  writeFun.emplace_back(innerWriteFun);

  return Box(stream, "mfhd", &writeFun, write);
}

int Mp4Generator::MINF(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(3);
  if (param.track->type == AUDIO) {
    writeFun.emplace_back(SMHD);
  } else {
    writeFun.emplace_back(VMHD);
  }
  writeFun.emplace_back(DINF);
  writeFun.emplace_back(STBL);

  return Box(stream, "minf", &writeFun, write);
}

int Mp4Generator::SMHD(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [](SimpleArray* stream, bool write) -> int {
    int len = 8;
    if (!write) {
      return len;
    }
    stream->writeInt32(0);
    stream->writeInt32(0);
    return len;
  };
  writeFun.emplace_back(innerWriteFun);

  return Box(stream, "smhd", &writeFun, write);
}

int Mp4Generator::VMHD(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [](SimpleArray* stream, bool write) -> int {
    int len = 12;
    if (!write) {
      return len;
    }
    stream->writeInt32(1);
    stream->writeInt32(0);
    stream->writeInt32(0);
    return len;
  };
  writeFun.emplace_back(innerWriteFun);

  return Box(stream, "vmhd", &writeFun, write);
}

int Mp4Generator::MVEX(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(param.tracks.size());
  for (auto& mp4Track : param.tracks) {
    param.track = mp4Track;
    writeFun.emplace_back(TREX);
  }
  return Box(stream, "mvex", &writeFun, write);
}

int Mp4Generator::MVHD(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [](SimpleArray* stream, bool write) -> int {
    int len = 100;
    if (!write) {
      return len;
    }
    int32_t createTime = std::floor(GetNowTime());
    int32_t modifitime = createTime;
    stream->writeInt32(0);
    stream->writeInt32(createTime);
    stream->writeInt32(modifitime);
    stream->writeInt32(param.timescale);
    stream->writeInt32(param.duration);
    stream->writeInt32(0x00010000);
    stream->writeInt32(0x01000000);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0x00010000);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0x00010000);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0x40000000);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0x00000002);

    return len;
  };
  writeFun.emplace_back(innerWriteFun);

  return Box(stream, "mvhd", &writeFun, write);
}

int Mp4Generator::SDTP(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [](SimpleArray* stream, bool write) -> int {
    auto samples = param.track->samples;
    int dataLen = 4 + static_cast<int>(samples.size());
    if (!write) {
      return dataLen;
    }

    stream->writeInt32(0);

    Mp4Flags flags;
    // leave the full box header (4 byteData) all zero
    // write the sample table
    for (size_t i = 0; i < samples.size(); i++) {
      flags = samples[i]->flags;
      stream->writeUint8((flags.dependsOn << 4) | (flags.isDependedOn << 2) | flags.hasRedundancy);
    }
    return dataLen;
  };
  writeFun.emplace_back(innerWriteFun);

  return Box(stream, "sdtp", &writeFun, write);
}

int Mp4Generator::STBL(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(7);
  writeFun.emplace_back(STSD);
  writeFun.emplace_back(STTS);
  writeFun.emplace_back(CTTS);
  writeFun.emplace_back(STSS);
  writeFun.emplace_back(STSC);
  writeFun.emplace_back(STSZ);
  writeFun.emplace_back(STCO);

  return Box(stream, "stbl", &writeFun, write);
}

int Mp4Generator::STSC(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  auto innerWrite = [](SimpleArray* stream, bool write) -> int {
    int len = 8;
    if (!write) {
      return len;
    }
    stream->writeInt32(0);
    stream->writeInt32(0);
    return len;
  };
  writeFun.emplace_back(innerWrite);

  return Box(stream, "stsc", &writeFun, write);
}

int Mp4Generator::STSZ(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  auto innerWrite = [](SimpleArray* stream, bool write) -> int {
    int len = 12;
    if (!write) {
      return len;
    }
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0);
    return len;
  };
  writeFun.emplace_back(innerWrite);

  return Box(stream, "stsz", &writeFun, write);
}

int Mp4Generator::STCO(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  auto innerWrite = [](SimpleArray* stream, bool write) -> int {
    int len = 8;
    if (!write) {
      return len;
    }
    stream->writeInt32(0);
    stream->writeInt32(0);
    return len;
  };
  writeFun.emplace_back(innerWrite);

  return Box(stream, "stco", &writeFun, write);
}

int Mp4Generator::AVC1(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(2);
  auto innerWrite = [](SimpleArray* stream, bool write) -> int {
    int len = 78;
    if (!write) {
      return len;
    }

    int width = param.track->width;
    int height = param.track->height;
    stream->writeInt32(0);
    stream->writeInt32(0x00000001);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeUint8((width >> 8) & 0xff);
    stream->writeUint8(width & 0xff);
    stream->writeUint8((height >> 8) & 0xff);
    stream->writeUint8(height & 0xff);
    stream->writeInt32(0x00480000);
    stream->writeInt32(0x00480000);
    stream->writeInt32(0);
    stream->writeInt32(0x00011262);
    stream->writeInt32(0x696e6500);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0x00000018);
    stream->writeUint8(0xff);
    stream->writeUint8(0xff);
    return len;
  };
  writeFun.emplace_back(innerWrite);
  writeFun.emplace_back(AVCC);

  // generate avc1 data
  return Box(stream, "avc1", &writeFun, write);
}

int Mp4Generator::AVCC(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  auto innerWrite = [](SimpleArray* stream, bool write) -> int {
    int spsDataLen = 0;
    for (auto& spsData : param.track->sps) {
      spsDataLen += 2 + static_cast<int>(spsData->length()) - 4;
    }
    int ppsDataLen = 0;
    for (auto& ppsData : param.track->pps) {
      ppsDataLen += 2 + static_cast<int>(ppsData->length()) - 4;
    }
    int avccDataLen = 7 + spsDataLen + ppsDataLen;
    if (!write) {
      return avccDataLen;
    }

    // write sps data
    stream->writeUint8(0x01);
    if (param.track && !param.track->sps.empty()) {
      auto spsData = param.track->sps.at(0);
      stream->writeUint8(spsData->data()[5]);
      stream->writeUint8(spsData->data()[6]);
      stream->writeUint8(spsData->data()[7]);
    }
    stream->writeUint8(0xfc | 3);
    stream->writeUint8(0xe0 | param.track->sps.size());
    for (auto& spsData : param.track->sps) {
      int len = static_cast<int>(spsData->length()) - 4;
      stream->writeUint8((len >> 8) & 0xff);
      stream->writeUint8(len & 0xff);
      stream->writeBytes(spsData->data(), len, 4);
    }

    // write pps data
    stream->writeUint8(param.track->pps.size());
    for (auto& ppsData : param.track->pps) {
      int len = static_cast<int>(ppsData->length()) - 4;
      stream->writeUint8((len >> 8) & 0xff);
      stream->writeUint8(len & 0xff);
      stream->writeBytes(ppsData->data(), len, 4);
    }
    return avccDataLen;
  };
  writeFun.emplace_back(innerWrite);

  return Box(stream, "avcC", &writeFun, write);
}

int Mp4Generator::STSD(SimpleArray* stream, bool write) {
  if (param.track->type == VIDEO) {
    std::vector<WriteStreamFun> writeFun;
    writeFun.reserve(1);
    auto innerWriteFun = [](SimpleArray* stream, bool write) -> int {
      int len = 8;
      if (!write) {
        return len;
      }
      stream->writeInt32(0);
      stream->writeInt32(1);
      return len;
    };
    writeFun.emplace_back(innerWriteFun);
    writeFun.emplace_back(AVC1);
    return Box(stream, "stsd", &writeFun, write);
  }
  return 0;
}

int Mp4Generator::TKHD(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  auto innerWrite = [](SimpleArray* stream, bool write) -> int {
    int len = 84;
    if (!write) {
      return len;
    }

    int32_t createTime = std::floor(GetNowTime());
    int32_t modifitime = createTime;
    int volumn = 1;

    stream->writeInt32(1);
    stream->writeInt32(createTime);
    stream->writeInt32(modifitime);
    stream->writeInt32(param.track->id);
    stream->writeInt32(0);
    stream->writeInt32(param.track->duration);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeUint8((volumn >> 0) & 0xff);
    stream->writeUint8((((volumn % 1) * 10) >> 0) & 0xff);
    stream->writeInt32(1);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(1);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0x00004000);
    stream->writeUint8(0);
    stream->writeUint8(0);
    stream->writeUint8((param.track->width >> 8) & 0xff);
    stream->writeUint8(param.track->width & 0xff);
    stream->writeUint8(0);
    stream->writeUint8(0);
    stream->writeUint8((param.track->height >> 8) & 0xff);
    stream->writeUint8(param.track->height & 0xff);
    stream->writeUint8(0);
    stream->writeUint8(0);

    return len;
  };
  writeFun.emplace_back(innerWrite);

  return Box(stream, "tkhd", &writeFun, write);
}

int Mp4Generator::TRAF(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(4);
  int sdtpLen = SDTP(stream, false);
  param.offset = sdtpLen + 72;
  writeFun.emplace_back(TFHD);
  writeFun.emplace_back(TFDT);
  writeFun.emplace_back(TRUN);
  writeFun.emplace_back(SDTP);

  return Box(stream, "traf", &writeFun, write);
}

int Mp4Generator::TFHD(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [](SimpleArray* stream, bool write) -> int {
    int len = 8;
    if (!write) {
      return len;
    }

    int id = param.track->id;
    stream->writeInt32(0);
    stream->writeInt32(id);
    return len;
  };
  writeFun.emplace_back(innerWriteFun);

  return Box(stream, "tfhd", &writeFun, write);
}

int Mp4Generator::TFDT(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [](SimpleArray* stream, bool write) -> int {
    int len = 8;
    if (!write) {
      return len;
    }

    stream->writeInt32(0);
    stream->writeInt32(param.baseMediaDecodeTime);
    return len;
  };
  writeFun.emplace_back(innerWriteFun);

  return Box(stream, "tfdt", &writeFun, write);
}

int Mp4Generator::TRAK(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(3);
  writeFun.emplace_back(TKHD);
  writeFun.emplace_back(EDTS);
  writeFun.emplace_back(MDIA);

  return Box(stream, "trak", &writeFun, write);
}

int Mp4Generator::EDTS(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  writeFun.emplace_back(ELST);

  return Box(stream, "edts", &writeFun, write);
}

int Mp4Generator::ELST(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [](SimpleArray* stream, bool write) -> int {
    int len = 20;
    if (!write) {
      return len;
    }
    int sampleSize = static_cast<int>(param.track->samples.size());
    int sampleDelta = std::floor(param.track->duration / sampleSize);
    stream->writeInt32(0);
    stream->writeInt32(1);
    stream->writeInt32(param.track->duration);
    stream->writeInt32(param.track->implicitOffset * sampleDelta);
    stream->writeInt32(0x00010000);
    return len;
  };
  writeFun.emplace_back(innerWriteFun);

  return Box(stream, "elst", &writeFun, write);
}

int Mp4Generator::TREX(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [](SimpleArray* stream, bool write) -> int {
    int len = 24;
    if (!write) {
      return len;
    }

    int id = param.track->id;
    stream->writeInt32(0);
    stream->writeInt32(id);
    stream->writeInt32(1);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0x00010001);
    return len;
  };
  writeFun.emplace_back(innerWriteFun);

  return Box(stream, "trex", &writeFun, write);
}

int Mp4Generator::TRUN(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [](SimpleArray* stream, bool write) -> int {
    auto& samples = param.track->samples;
    int len = static_cast<int>(samples.size());
    int arraylen = 12 + 16 * len;
    param.offset += 8 + arraylen;

    if (!write) {
      return arraylen;
    }
    stream->writeInt32(0x00000f01);
    stream->writeInt32(len);
    stream->writeInt32(param.offset);

    for (auto& sample : samples) {
      Mp4Flags flags = sample->flags;
      uint8_t paddingValue = 0;
      stream->writeInt32(sample->duration);
      stream->writeInt32(sample->size);
      stream->writeUint8((flags.isLeading << 2) | flags.dependsOn);
      stream->writeUint8((flags.isDependedOn << 6) | (flags.hasRedundancy << 4) |
                         (paddingValue << 1) | flags.isNonSync);
      stream->writeUint8(flags.degradPrio & (0xf0 << 8));
      stream->writeUint8(flags.degradPrio & 0x0f);
      stream->writeInt32(sample->cts);
    }

    return arraylen;
  };
  writeFun.emplace_back(innerWriteFun);

  return Box(stream, "trun", &writeFun, write);
}

int Mp4Generator::STTS(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [](SimpleArray* stream, bool write) -> int {
    int len = 16;
    if (!write) {
      return len;
    }

    int sampleCount = static_cast<int>(param.track->samples.size());
    int32_t sampleDelta = std::floor(param.track->duration / sampleCount);
    stream->writeInt32(0);
    stream->writeInt32(1);
    stream->writeInt32(sampleCount);
    stream->writeInt32(sampleDelta);
    return len;
  };
  writeFun.emplace_back(innerWriteFun);

  return Box(stream, "stts", &writeFun, write);
}

int Mp4Generator::CTTS(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [](SimpleArray* stream, bool write) -> int {
    int sampleCount = param.track->samples.size();
    int32_t sampleDelta = std::floor(param.track->duration / sampleCount);
    int len = (2 + sampleCount * 2) * 4;
    if (!write) {
      return len;
    }

    stream->writeInt32(0);
    stream->writeInt32(sampleCount);
    for (int i = 0; i < sampleCount; i++) {
      stream->writeInt32(1);
      int32_t dts = i * sampleDelta;
      int32_t pts = (param.track->pts[i] + param.track->implicitOffset) * sampleDelta;
      stream->writeInt32((pts - dts));
    }
    return len;
  };
  writeFun.emplace_back(innerWriteFun);

  return Box(stream, "ctts", &writeFun, write);
}

int Mp4Generator::STSS(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [](SimpleArray* stream, bool write) -> int {
    std::vector<int> iFrames;
    iFrames.reserve(param.track->samples.size());
    for (auto& sample : param.track->samples) {
      if (sample->flags.isKeyFrame) {
        iFrames.emplace_back(sample->index + 1);
      }
    }
    int len = (2 + static_cast<int>(iFrames.size())) * 4;
    if (!write) {
      return len;
    }
    stream->writeInt32(0);
    stream->writeInt32(static_cast<int>(iFrames.size()));
    for (int frame : iFrames) {
      stream->writeInt32(frame);
    }
    return len;
  };
  writeFun.emplace_back(innerWriteFun);

  return Box(stream, "stss", &writeFun, write);
}

int Mp4Generator::Box(SimpleArray* stream, const std::string& type,
                      std::vector<WriteStreamFun>* boxFunctions, bool write) {
  int size = 8;
  auto iter = BoxSizeMap.find(type);
  if (iter != BoxSizeMap.end()) {
    size = iter->second;
  } else {
    for (WriteStreamFun& writeStreamFun : *boxFunctions) {
      size += writeStreamFun(stream, false);
    }
    BoxSizeMap[type] = size;
  }
  if (!write) {
    return size;
  }
  stream->writeInt32(size);
  WriteCharCode(stream, type, true);

  for (WriteStreamFun& writeStreamFun : *boxFunctions) {
    writeStreamFun(stream, true);
  }
  return size;
}

int Mp4Generator::DINF(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  writeFun.emplace_back(DREF);
  return Box(stream, "dinf", &writeFun, write);
}

int Mp4Generator::DREF(SimpleArray* stream, bool write) {
  std::vector<WriteStreamFun> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [](SimpleArray* stream, bool write) -> int {
    int len = 20;
    if (!write) {
      return len;
    }
    stream->writeInt32(0);
    stream->writeInt32(1);
    stream->writeInt32(0x0000000C);
    stream->writeInt32(0x75726C20);
    stream->writeInt32(1);
    return len;
  };
  writeFun.emplace_back(innerWriteFun);

  return Box(stream, "dref", &writeFun, write);
}

int32_t Mp4Generator::GetNowTime() {
  return static_cast<int32_t>(static_cast<double>(tgfx::Clock::Now()) * 1e-6);
}

void Mp4Generator::Clear() {
  BoxSizeMap.clear();
}

void Mp4Generator::InitParam(const BoxParam& boxParam) {
  param = boxParam;
}
}  // namespace pag
