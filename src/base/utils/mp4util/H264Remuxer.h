//
// Created by katacai on 2022/3/8.
//

#pragma once
#include <math.h>
#include <algorithm>
#include "H264Nalu.h"
#include "H264Parser.h"
#include "Mp4Generator.h"
#include "codec/utils/ByteArray.h"

#define SEQUENCE_NUMBER 1
#define BASE_MEDIA_DECODE_TIME 0
#define BASE_MEDIA_TIME_SCALE 6000

namespace pag {
class Sample {
 public:
  std::vector<H264Nalu*> units;
  int size;
  bool keyFrame;
};

class Mp4Flags {
 public:
  int isLeading;
  int isDependedOn;
  int hasRedundancy;
  int degradPrio;
  int isNonSync;
  int dependsOn;
  bool isKeyFrame;
};

class Mp4Sample {
 public:
  int index;
  int size;
  int32_t duration;
  int32_t cts;
  Mp4Flags flags;
};

class Mp4Track {
 public:
  Mp4Track();
  ~Mp4Track();

  int id;
  std::string type;
  int len;
  bool fragmented;
  std::vector<ByteData*> sps;
  std::vector<ByteData*> pps;
  int width;
  int height;
  int timescale;
  int32_t duration;
  std::vector<Mp4Sample*> samples;
  std::vector<int32_t> pts;
  std::string codec;
  float fps;
  int32_t implicitOffset;
};

extern int trackId;

class H264Remuxer {
 public:
  H264Remuxer();
  ~H264Remuxer();
  static int getTrackID();
  static H264Remuxer* remux(std::vector<H264Frame*> frames, VideoSequence& videoSequence);
  std::unique_ptr<ByteData> convertMp4();
  void writeMp4BoxesInSequence(VideoSequence& sequence);
  ByteData* getPayload();

  Mp4Track mp4Track;
  std::vector<Sample*> samples;

 private:
  static int32_t getImplicitOffset(std::vector<int32_t> ptsList);
  static std::vector<H264Nalu*> filterBaseNalus(std::vector<H264Nalu*> units);
  static int getCurrentFrameSize(std::vector<H264Nalu*> nalus);
};
}  // namespace pag
