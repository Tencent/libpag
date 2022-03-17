//
// Created by katacai on 2022/3/8.
//

#include "H264Remuxer.h"
#include "CommonConfig.h"

namespace pag {

int trackId = 0;

Mp4Track::Mp4Track() {
  id = H264Remuxer::getTrackID();
  type = "video";
  len = 0;
  fragmented = true;
  width = 0;
  height = 0;
  timescale = 0;
  duration = 0;
  codec = "";
  fps = 0;
  implicitOffset = 0;
}

Mp4Track::~Mp4Track() {}

H264Remuxer::H264Remuxer() {}

H264Remuxer::~H264Remuxer() {}

H264Remuxer* H264Remuxer::remux(std::vector<H264Frame*> frames, VideoSequence& videoSequence) {
  if (frames.empty()) {
    LOGE("视频序列帧上Frame不存在！");
    return nullptr;
  }

  H264Remuxer* remuxer = new H264Remuxer();
  remuxer->mp4Track.timescale = BASE_MEDIA_TIME_SCALE;
  remuxer->mp4Track.duration =
      std::floor((frames.size() / videoSequence.frameRate) * BASE_MEDIA_TIME_SCALE);
  remuxer->mp4Track.fps = videoSequence.frameRate;
  for (auto frame : videoSequence.frames) {
    remuxer->mp4Track.pts.emplace_back(frame->frame);
  }
  remuxer->mp4Track.implicitOffset = getImplicitOffset(remuxer->mp4Track.pts);  // 抵消 PTS 与 DTS
                                                                                // 差值为负的情况
  bool readyToDecode = false;

  for (H264Frame* frame : frames) {
    std::vector<H264Nalu*> units = filterBaseNalus(frame->units);
    if (units.empty()) {
      continue;
    }
    auto size = getCurrentFrameSize(units);
    for (H264Nalu* unit : units) {
      if (unit->type == NaluType::NAL_SPS) {
        SpsData spsData = H264Parser::parseSPS(unit);
        remuxer->mp4Track.width = spsData.width;
        remuxer->mp4Track.height = spsData.height;
        remuxer->mp4Track.sps = {spsData.sps};
        remuxer->mp4Track.codec = spsData.codec;
      }
    }
    if (!remuxer->mp4Track.sps.empty() && !remuxer->mp4Track.pps.empty()) {
      readyToDecode = true;
    }

    for (H264Nalu* unit : units) {
      if (unit->type == NaluType::NAL_PPS) {
        ByteData* pps =
            ByteData::MakeAdopted(unit->payload, unit->getPayloadSize()).release();
        remuxer->mp4Track.pps = {pps};
      }
    }

    if (!remuxer->mp4Track.sps.empty() && !remuxer->mp4Track.pps.empty()) {
      readyToDecode = true;
    }

    if (readyToDecode) {
      remuxer->mp4Track.len += size;
      Sample* sample = new Sample();
      sample->units = units;
      sample->size = size;
      sample->keyFrame = frame->isKeyFrame;
      remuxer->samples.emplace_back(sample);
    }
  }

  return remuxer;
}

std::unique_ptr<ByteData> H264Remuxer::convertMp4() {
  auto payload = getPayload();
  if (!payload) {
    return nullptr;
  }

  Mp4Generator mp4Generator;
  ByteData* ftyp = mp4Generator.ftyp();
  ByteData* moov = mp4Generator.moov({&mp4Track}, mp4Track.duration, mp4Track.timescale);
  ByteData* moof = mp4Generator.moof(SEQUENCE_NUMBER, BASE_MEDIA_DECODE_TIME, &mp4Track);
  ByteData* mdat = mp4Generator.mdat(payload);

  int dataSize = ftyp->length() + moov->length() + moof->length() + mdat->length();
  ByteArray mp4Data(nullptr, dataSize);
  mp4Data.setOrder(byteOrder);
  mp4Data.writeBytes(ftyp->data(), ftyp->length());
  mp4Data.writeBytes(moov->data(), moov->length());
  mp4Data.writeBytes(moof->data(), moof->length());
  mp4Data.writeBytes(mdat->data(), mdat->length());

  delete ftyp;
  delete moov;
  delete moof;
  delete mdat;

  return mp4Data.release();
}

void H264Remuxer::writeMp4BoxesInSequence(VideoSequence& sequence) {
  Mp4Generator mp4Generator;
  ByteData* ftyp = mp4Generator.ftyp();
  ByteData* moov = mp4Generator.moov({&mp4Track}, mp4Track.duration, mp4Track.timescale);
  ByteData* moof = mp4Generator.moof(SEQUENCE_NUMBER, BASE_MEDIA_DECODE_TIME, &mp4Track);

  int dataSize = ftyp->length() + moov->length() + moof->length();
  LOGI("write mp4 boxes in sequence, data size:%d", dataSize);
  ByteArray boxesData(nullptr, dataSize);
  boxesData.setOrder(byteOrder);
  boxesData.writeBytes(ftyp->data(), ftyp->length());
  boxesData.writeBytes(moov->data(), moov->length());
  boxesData.writeBytes(moof->data(), moof->length());

  sequence.mp4Header = boxesData.release().release();
}

ByteData* H264Remuxer::getPayload() {
  ByteArray payload(nullptr, mp4Track.len);
  payload.setOrder(byteOrder);
  auto sampleDelta = std::floor(mp4Track.duration / samples.size());
  int count = 0;

  for (Sample* sample : samples) {
    Mp4Sample* mp4Sample = new Mp4Sample;
    mp4Sample->index = count;
    mp4Sample->size = sample->size;
    mp4Sample->duration = sampleDelta;
    mp4Sample->cts = mp4Track.pts.at(count) * sampleDelta + mp4Track.implicitOffset * sampleDelta -
                     count * sampleDelta;
    mp4Sample->flags.isLeading = 0;
    mp4Sample->flags.isDependedOn = 0;
    mp4Sample->flags.hasRedundancy = 0;
    mp4Sample->flags.degradPrio = 0;
    mp4Sample->flags.isNonSync = sample->keyFrame ? 0 : 1;
    mp4Sample->flags.dependsOn = sample->keyFrame ? 2 : 1;
    mp4Sample->flags.isKeyFrame = sample->keyFrame;

    for (auto unit : sample->units) {
      payload.writeBytes(unit->getData(), unit->getSize());
    }
    mp4Track.samples.emplace_back(mp4Sample);
    count += 1;
  }

  if (mp4Track.samples.empty()) {
    return nullptr;
  }

  return payload.release().release();
}

int H264Remuxer::getTrackID() {
  trackId++;
  return trackId;
}

int32_t H264Remuxer::getImplicitOffset(std::vector<int32_t> ptsList) {
  std::vector<int64_t> offsetList;
  int index = 0;
  for (int64_t pts : ptsList) {
    int64_t offset = pts - index;
    if (offset < 0) {
      offsetList.emplace_back(offset);
    }
    index++;
  }
  if (offsetList.empty()) {
    return 0;
  }
  return std::abs(*std::min_element(offsetList.begin(), offsetList.end()));
}

std::vector<H264Nalu*> H264Remuxer::filterBaseNalus(std::vector<H264Nalu*> units) {
  std::vector<H264Nalu*> result;
  for (H264Nalu* nalu : units) {
    switch (nalu->type) {
      case NaluType::NAL_SPS:
      case NaluType::NAL_PPS:
      case NaluType::NAL_SLICE_IDR:
      case NaluType::NAL_SLICE:
        result.emplace_back(nalu);
        continue;
      default:
        LOGI("drop nalu for type: %d\n", nalu->type);
        continue;
    }
  }
  return result;
}

int H264Remuxer::getCurrentFrameSize(std::vector<H264Nalu*> nalus) {
  int size = 0;
  for (H264Nalu* nalu : nalus) {
    size += nalu->getSize();
  }
  return size;
}

}  // namespace pag
