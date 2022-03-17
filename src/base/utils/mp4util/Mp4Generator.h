//
// Created by katacai on 2022/3/10.
//

#pragma once
#include <chrono>
#include <map>
#include "codec/utils/ByteArray.h"

namespace pag {
class Mp4Track;

class Mp4Generator {
 public:
  static ByteData* ftyp();
  static ByteData* moov(std::vector<Mp4Track*> tracks, int32_t duration, int timescale);
  static ByteData* moof(int sequenceNumber, int32_t baseMediaDecodeTime, Mp4Track* track);
  static ByteData* mdat(ByteData* payload);
  static ByteData* mvhd(int timescale, int32_t duration);
  static ByteData* mvex(std::vector<Mp4Track*> tracks);
  static ByteData* mfhd(int sequenceNumber);
  static ByteData* traf(int32_t baseMediaDecodeTime, Mp4Track* track);
  static ByteData* mdhd(Mp4Track* track);
  static ByteData* mdia(Mp4Track* track);
  static ByteData* minf(Mp4Track* track);
  static ByteData* stbl(Mp4Track* track);
  static ByteData* trex(Mp4Track* track);
  static ByteData* sdtp(Mp4Track* track);
  static ByteData* avc1(Mp4Track* track);
  static ByteData* stsd(Mp4Track* track);
  static ByteData* tkhd(Mp4Track* track);
  static ByteData* trak(Mp4Track* track);
  static ByteData* edts(Mp4Track* track);
  static ByteData* elst(Mp4Track* track);
  static ByteData* trun(Mp4Track* track, int offset);
  static ByteData* stts(Mp4Track* track);
  static ByteData* ctts(Mp4Track* track);
  static ByteData* stss(Mp4Track* track);

  static const std::string HDLR_VIDEO;
  static const std::string HDLR_AUDIO;

 private:
  static ByteData* decimal2HexadecimalArray(int32_t payload);
  static void writeNumbersToByteArray(ByteArray* byteArray, std::vector<int32_t> numbers);
  static ByteData* getCharCode(std::string name);
  static ByteData* numberToByteData(std::vector<uint8_t> numbers);
  static ByteData* hdlr(std::string type);
  static ByteData* box(std::string type, std::vector<ByteData*> payloads,
                            bool release = true);
  static ByteData* dinf();
  static std::vector<ByteData*> toVector(ByteData* data);
  static int32_t getNowTime();

  static const int CORRECTION_UTC = 2082873600;

  static ByteData* hdlrVideo;
  static ByteData* hdlrAudio;
  static ByteData* fullbox;
  static ByteData* stscData;
  static ByteData* stcoData;
  static ByteData* stszData;
  static ByteData* vmhdData;
  static ByteData* smhdData;
  static ByteData* stsdData;
  static ByteData* drefData;
};
}  // namespace pag