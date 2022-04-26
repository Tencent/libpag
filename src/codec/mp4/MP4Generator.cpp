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

#include "MP4Generator.h"
#include "tgfx/core/Clock.h"

#define PushInWriteFun(funName)                                                         \
  writeFun.emplace_back([this](auto&& PH1, auto&& PH2) -> int {                         \
    return funName(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2)); \
  })

namespace pag {
static const char* VIDEO = "video";
static const char* AUDIO = "audio";

MP4Generator::MP4Generator(BoxParam param) : param(std::move(param)) {
}

static int WriteCharCode(EncodeStream* stream, std::string stringData, bool write) {
  int size = static_cast<int>(stringData.length());
  if (!write) {
    return size;
  }
  for (int i = 0; i < size; i++) {
    stream->writeUint8(stringData[i]);
  }
  return size;
}

int MP4Generator::writeH264Nalus(EncodeStream* stream, bool write) const {
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

int MP4Generator::ftyp(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [](EncodeStream* stream, bool write) -> int {
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
  return box(stream, "ftyp", writeFun, write);
}

int MP4Generator::moov(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1 + param.tracks.size());
  PushInWriteFun(mvhd);
  for (auto& mp4Track : param.tracks) {
    param.track = mp4Track;
    PushInWriteFun(trak);
  }
  PushInWriteFun(mvex);
  return box(stream, "moov", writeFun, write);
}

int MP4Generator::moof(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(2);
  PushInWriteFun(mfhd);
  PushInWriteFun(traf);
  return box(stream, "moof", writeFun, write);
}

int MP4Generator::mdat(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  PushInWriteFun(writeH264Nalus);
  return box(stream, "mdat", writeFun, write);
}

int MP4Generator::hdlr(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  if (param.track->type == VIDEO) {
    auto innerWriteFun = [](EncodeStream* stream, bool write) -> int {
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
  return box(stream, "hdlr", writeFun, write);
}

int MP4Generator::mdhd(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [&](EncodeStream* stream, bool write) -> int {
    int len = 24;
    if (!write) {
      return len;
    }
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(param.track->timescale);
    stream->writeInt32(0);
    stream->writeInt32(0x55C40000);
    return len;
  };
  writeFun.emplace_back(innerWriteFun);
  return box(stream, "mdhd", writeFun, write);
}

int MP4Generator::mdia(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(3);
  PushInWriteFun(mdhd);
  PushInWriteFun(hdlr);
  PushInWriteFun(minf);
  return box(stream, "mdia", writeFun, write);
}

int MP4Generator::mfhd(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [&](EncodeStream* stream, bool write) -> int {
    int len = 8;
    if (!write) {
      return len;
    }
    stream->writeInt32(0);
    stream->writeInt32(param.sequenceNumber);
    return len;
  };
  writeFun.emplace_back(innerWriteFun);
  return box(stream, "mfhd", writeFun, write);
}

int MP4Generator::minf(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(3);
  if (param.track->type == AUDIO) {
    PushInWriteFun(smhd);
  } else {
    PushInWriteFun(vmhd);
  }
  PushInWriteFun(dinf);
  PushInWriteFun(stbl);
  return box(stream, "minf", writeFun, write);
}

int MP4Generator::smhd(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [](EncodeStream* stream, bool write) -> int {
    int len = 8;
    if (!write) {
      return len;
    }
    stream->writeInt32(0);
    stream->writeInt32(0);
    return len;
  };
  writeFun.emplace_back(innerWriteFun);
  return box(stream, "smhd", writeFun, write);
}

int MP4Generator::vmhd(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [](EncodeStream* stream, bool write) -> int {
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
  return box(stream, "vmhd", writeFun, write);
}

int MP4Generator::mvex(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(param.tracks.size());
  for (auto& mp4Track : param.tracks) {
    param.track = mp4Track;
    PushInWriteFun(trex);
  }
  return box(stream, "mvex", writeFun, write);
}

int MP4Generator::mvhd(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [&](EncodeStream* stream, bool write) -> int {
    int len = 100;
    if (!write) {
      return len;
    }
    stream->writeInt32(0);
    stream->writeInt32(0);
    stream->writeInt32(0);
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
  return box(stream, "mvhd", writeFun, write);
}

int MP4Generator::sdtp(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [&](EncodeStream* stream, bool write) -> int {
    auto samples = param.track->samples;
    int dataLen = 4 + static_cast<int>(samples.size());
    if (!write) {
      return dataLen;
    }
    stream->writeInt32(0);
    MP4Flags flags;
    for (size_t i = 0; i < samples.size(); i++) {
      flags = samples[i]->flags;
      stream->writeUint8((flags.dependsOn << 4) | (flags.isDependedOn << 2) | flags.hasRedundancy);
    }
    return dataLen;
  };
  writeFun.emplace_back(innerWriteFun);
  return box(stream, "sdtp", writeFun, write);
}

int MP4Generator::stbl(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(7);
  PushInWriteFun(stsd);
  PushInWriteFun(stts);
  PushInWriteFun(ctts);
  PushInWriteFun(stss);
  PushInWriteFun(stsc);
  PushInWriteFun(stsz);
  PushInWriteFun(stco);
  return box(stream, "stbl", writeFun, write);
}

int MP4Generator::stsc(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  auto innerWrite = [](EncodeStream* stream, bool write) -> int {
    int len = 8;
    if (!write) {
      return len;
    }
    stream->writeInt32(0);
    stream->writeInt32(0);
    return len;
  };
  writeFun.emplace_back(innerWrite);
  return box(stream, "stsc", writeFun, write);
}

int MP4Generator::stsz(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  auto innerWrite = [](EncodeStream* stream, bool write) -> int {
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
  return box(stream, "stsz", writeFun, write);
}

int MP4Generator::stco(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  auto innerWrite = [](EncodeStream* stream, bool write) -> int {
    int len = 8;
    if (!write) {
      return len;
    }
    stream->writeInt32(0);
    stream->writeInt32(0);
    return len;
  };
  writeFun.emplace_back(innerWrite);

  return box(stream, "stco", writeFun, write);
}

int MP4Generator::avc1(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(2);
  auto innerWrite = [&](EncodeStream* stream, bool write) -> int {
    int len = 78;
    if (!write) {
      return len;
    }

    int width = param.track->width;
    int height = param.track->height;
    stream->writeInt32(0);
    stream->writeInt32(1);
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
  PushInWriteFun(avcc);
  // generate avc1 data
  return box(stream, "avc1", writeFun, write);
}

int MP4Generator::avcc(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  auto innerWrite = [&](EncodeStream* stream, bool write) -> int {
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
  return box(stream, "avcC", writeFun, write);
}

int MP4Generator::stsd(EncodeStream* stream, bool write) {
  if (param.track->type == VIDEO) {
    std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
    writeFun.reserve(1);
    auto innerWriteFun = [](EncodeStream* stream, bool write) -> int {
      int len = 8;
      if (!write) {
        return len;
      }
      stream->writeInt32(0);
      stream->writeInt32(1);
      return len;
    };
    writeFun.emplace_back(innerWriteFun);
    PushInWriteFun(avc1);
    return box(stream, "stsd", writeFun, write);
  }
  return 0;
}

int MP4Generator::tkhd(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  auto innerWrite = [&](EncodeStream* stream, bool write) -> int {
    int len = 84;
    if (!write) {
      return len;
    }

    int volumn = 1;
    stream->writeInt32(1);
    stream->writeInt32(0);
    stream->writeInt32(0);
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
  return box(stream, "tkhd", writeFun, write);
}

int MP4Generator::traf(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(4);
  int sdtpLen = sdtp(stream, false);
  param.offset = sdtpLen + 72;
  PushInWriteFun(tfhd);
  PushInWriteFun(tfdt);
  PushInWriteFun(trun);
  PushInWriteFun(sdtp);
  return box(stream, "traf", writeFun, write);
}

int MP4Generator::tfhd(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [&](EncodeStream* stream, bool write) -> int {
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
  return box(stream, "tfhd", writeFun, write);
}

int MP4Generator::tfdt(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [&](EncodeStream* stream, bool write) -> int {
    int len = 8;
    if (!write) {
      return len;
    }

    stream->writeInt32(0);
    stream->writeInt32(param.baseMediaDecodeTime);
    return len;
  };
  writeFun.emplace_back(innerWriteFun);
  return box(stream, "tfdt", writeFun, write);
}

int MP4Generator::trak(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(3);
  PushInWriteFun(tkhd);
  PushInWriteFun(edts);
  PushInWriteFun(mdia);
  return box(stream, "trak", writeFun, write);
}

int MP4Generator::edts(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  PushInWriteFun(elst);
  return box(stream, "edts", writeFun, write);
}

int MP4Generator::elst(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [&](EncodeStream* stream, bool write) -> int {
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
  return box(stream, "elst", writeFun, write);
}

int MP4Generator::trex(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [&](EncodeStream* stream, bool write) -> int {
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
  return box(stream, "trex", writeFun, write);
}

int MP4Generator::trun(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [&](EncodeStream* stream, bool write) -> int {
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
      MP4Flags flags = sample->flags;
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
  return box(stream, "trun", writeFun, write);
}

int MP4Generator::stts(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [&](EncodeStream* stream, bool write) -> int {
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
  return box(stream, "stts", writeFun, write);
}

int MP4Generator::ctts(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [&](EncodeStream* stream, bool write) -> int {
    auto sampleCount = static_cast<int>(param.track->samples.size());
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
  return box(stream, "ctts", writeFun, write);
}

int MP4Generator::stss(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [&](EncodeStream* stream, bool write) -> int {
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
  return box(stream, "stss", writeFun, write);
}

int MP4Generator::box(EncodeStream* stream, const std::string& type,
                      const std::vector<std::function<int(EncodeStream*, bool)>>& boxFunctions,
                      bool write) {
  int size = 8;
  auto iter = boxSizeMap.find(type);
  if (iter != boxSizeMap.end()) {
    size = iter->second;
  } else {
    for (const auto& writeStreamFun : boxFunctions) {
      size += writeStreamFun(stream, false);
    }
    boxSizeMap[type] = size;
  }
  if (!write) {
    return size;
  }
  stream->writeInt32(size);
  WriteCharCode(stream, type, true);

  for (const auto& writeStreamFun : boxFunctions) {
    writeStreamFun(stream, true);
  }
  return size;
}

int MP4Generator::dinf(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  PushInWriteFun(dref);
  return box(stream, "dinf", writeFun, write);
}

int MP4Generator::dref(EncodeStream* stream, bool write) {
  std::vector<std::function<int(EncodeStream*, bool)>> writeFun;
  writeFun.reserve(1);
  auto innerWriteFun = [](EncodeStream* stream, bool write) -> int {
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
  return box(stream, "dref", writeFun, write);
}
}  // namespace pag
