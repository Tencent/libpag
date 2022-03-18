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

#include "FFmpegDemuxer.h"
#include <list>

namespace pag {
std::string AVCodecIDToString(AVCodecID codecId) {
  std::string codecIDString;
  switch (codecId) {
    case AVCodecID::AV_CODEC_ID_H264:
      codecIDString = MIMETYPE_VIDEO_AVC;
      break;
    case AVCodecID::AV_CODEC_ID_HEVC:
      codecIDString = MIMETYPE_VIDEO_HEVC;
      break;

    default:
      break;
  }
  return codecIDString;
}

std::unique_ptr<FFmpegDemuxer> FFmpegDemuxer::Make(const std::string& filePath,
                                                   NALUType startCodeType) {
  auto demuxer = std::unique_ptr<FFmpegDemuxer>(new FFmpegDemuxer());
  if (!demuxer->open(filePath)) {
    return nullptr;
  }
  av_init_packet(&(demuxer->avPacket));
  demuxer->avPacket.data = nullptr;
  demuxer->avPacket.size = 0;
  demuxer->naluStartCodeType = startCodeType;
  return demuxer;
}

FFmpegDemuxer::~FFmpegDemuxer() {
  av_packet_unref(&avPacket);
  if (formatContext != nullptr) {
    avformat_close_input(&formatContext);
  }
  av_bsf_free(&avbsfContext);
  for (const auto& iter : formats) {
    delete iter.second;
  }
}

void FFmpegDemuxer::seekTo(int64_t timeUs) {
  if (!formatContext || currentStreamIndex < 0) {
    return;
  }
  // flag 是 AVSEEK_FLAG_BACKWARD 时，seek 传入关键帧的时间，得到的时间小于不等于传入的时间。
  // flag 是 0 时，seek 传入关键帧的时间，得到的时间大于等于传入的时间。
  // '+1' 确保可以 seek 到指定的时间
  int64_t pos = av_rescale_q_rnd(timeUs + 1, AVRational{1, AV_TIME_BASE},
                                 formatContext->streams[currentStreamIndex]->time_base,
                                 AVRounding::AV_ROUND_ZERO);
  int ret = av_seek_frame(formatContext, currentStreamIndex, pos, AVSEEK_FLAG_BACKWARD);
  if (ret < 0) {
    LOGE("Error in seeking!!!\n");
  }
}

bool FFmpegDemuxer::selectTrack(int index) {
  if (0 <= index && index < getTrackCount()) {
    currentStreamIndex = index;
    return true;
  }
  return false;
}

int64_t FFmpegDemuxer::getSampleTime() {
  return currentTime;
}

int FFmpegDemuxer::getTrackCount() {
  if (formatContext) {
    return static_cast<int>(formatContext->nb_streams);
  }
  return 0;
}

bool FFmpegDemuxer::advance() {
  av_packet_unref(&avPacket);
  if (formatContext && currentStreamIndex >= 0) {
    while (av_read_frame(formatContext, &avPacket) >= 0) {
      if (avPacket.stream_index == currentStreamIndex) {
        currentTime = av_rescale_q_rnd(avPacket.pts, avRational, AVRational{1, AV_TIME_BASE},
                                       AVRounding::AV_ROUND_ZERO);
        if (naluStartCodeType == NALUType::AnnexB) {
          if (av_bsf_send_packet(avbsfContext, &avPacket) != 0) {
            LOGE("AnnexB conversion send packet exception!!! \n");
          }
          while (av_bsf_receive_packet(avbsfContext, &avPacket) == 0) {
          }
        }
        afterAdvance(avPacket.flags & AVINDEX_KEYFRAME);
        return true;
      }
      av_packet_unref(&avPacket);
    }
  }
  return false;
}

SampleData FFmpegDemuxer::readSampleData() {
  SampleData sampleData = {};
  sampleData.data = avPacket.data;
  sampleData.length = avPacket.size;
  return sampleData;
}

MediaFormat* FFmpegDemuxer::getTrackFormat(int index) {
  auto iter = formats.find(index);
  if (iter != formats.end()) {
    return iter->second;
  }
  MediaFormat* fomat = getTrackFormatInternal(index);
  if (fomat) {
    formats.insert(std::make_pair(index, fomat));
  }
  return fomat;
}

bool FFmpegDemuxer::open(const std::string& filePath) {
  auto path = static_cast<const char*>(filePath.data());
  if (avformat_open_input(&formatContext, path, nullptr, nullptr) < 0) {
    LOGE("Could not open source file %s\n", path);
    return false;
  }
  if (avformat_find_stream_info(formatContext, nullptr) < 0) {
    LOGE("Could not find stream information\n");
    return false;
  }
  return true;
}

MediaFormat* FFmpegDemuxer::getTrackFormatInternal(int index) {
  if (index < 0 || static_cast<uint32_t>(index) > formatContext->nb_streams - 1) {
    return nullptr;
  }
  AVStream* avStream = formatContext->streams[index];
  AVMediaType type = avStream->codecpar->codec_type;
  if (type != AVMEDIA_TYPE_VIDEO && type != AVMEDIA_TYPE_AUDIO) {
    return nullptr;
  }

  auto trackFormat = new MediaFormat();
  auto duration = 1000000 * avStream->duration * avStream->time_base.num / avStream->time_base.den;
  trackFormat->setLong(KEY_DURATION, duration);
  trackFormat->setInteger(KEY_TRACK_ID, index);

  if (type == AVMEDIA_TYPE_VIDEO) {
    videoStreamIndex = index;
    trackFormat->setInteger(KEY_WIDTH, avStream->codecpar->width);
    trackFormat->setInteger(KEY_HEIGHT, avStream->codecpar->height);
    auto frameRate = 1.0 * avStream->avg_frame_rate.num / avStream->avg_frame_rate.den;
    trackFormat->setFloat(KEY_FRAME_RATE, frameRate);

    if (avStream->codecpar->color_primaries == AVCOL_PRI_BT2020) {
      trackFormat->setString(KEY_COLOR_SPACE, COLORSPACE_REC2020);
    } else if (avStream->codecpar->color_primaries == AVCOL_PRI_BT709) {
      trackFormat->setString(KEY_COLOR_SPACE, COLORSPACE_REC709);
    } else {
      trackFormat->setString(KEY_COLOR_SPACE, COLORSPACE_REC601);
    }

    if (avStream->codecpar->color_range == AVCOL_RANGE_JPEG) {
      trackFormat->setString(KEY_COLOR_RANGE, COLORRANGE_JPEG);
    } else {
      trackFormat->setString(KEY_COLOR_RANGE, COLORRANGE_MPEG);
    }

    avRational.den = avStream->time_base.den;
    avRational.num = avStream->time_base.num;

    trackFormat->setString(KEY_MIME, AVCodecIDToString(avStream->codecpar->codec_id));

    AVDictionaryEntry* tag = nullptr;
    tag = av_dict_get(avStream->metadata, "rotate", tag, 0);
    if (tag) {
      trackFormat->setInteger(KEY_ROTATION, static_cast<int>(std::strtol(tag->value, nullptr, 10)));
    }
    getNALUInfoWithVideoStream(trackFormat, avStream);

  } else if (type == AVMEDIA_TYPE_AUDIO) {
    trackFormat->setString(KEY_MIME, "audio");
  }

  return trackFormat;
}

void SetH264StartCodeIndex(int i, int* startCodeSPSIndex, int* startCodeFPPSIndex) {
  if (*startCodeSPSIndex == 0) {
    *startCodeSPSIndex = i;
  }
  if (i > *startCodeSPSIndex) {
    *startCodeFPPSIndex = i;
  }
}

void SetH265StartCodeIndex(int i, int* startCodeVPSIndex, int* startCodeSPSIndex,
                           int* startCodeFPPSIndex, int* startCodeRPPSIndex) {
  if (*startCodeVPSIndex == 0) {
    *startCodeVPSIndex = i;
    return;
  }
  if (i > *startCodeVPSIndex && *startCodeSPSIndex == 0) {
    *startCodeSPSIndex = i;
    return;
  }
  if (i > *startCodeSPSIndex && *startCodeFPPSIndex == 0) {
    *startCodeFPPSIndex = i;
    return;
  }
  if (i > *startCodeFPPSIndex && *startCodeRPPSIndex == 0) {
    *startCodeRPPSIndex = i;
  }
}

void SetStartCodeIndex(uint8_t* extradata, int extradataSize, AVCodecID avCodecId,
                       int* startCodeVPSIndex, int* startCodeSPSIndex, int* startCodeFPPSIndex,
                       int* startCodeRPPSIndex) {
  for (int i = 0; i < extradataSize; i++) {
    if (i >= 3) {
      if (extradata[i] == 0x01 && extradata[i - 1] == 0x00 && extradata[i - 2] == 0x00 &&
          extradata[i - 3] == 0x00) {
        if (avCodecId == AV_CODEC_ID_H264) {
          SetH264StartCodeIndex(i, startCodeSPSIndex, startCodeFPPSIndex);
        } else if (avCodecId == AV_CODEC_ID_HEVC) {
          SetH265StartCodeIndex(i, startCodeVPSIndex, startCodeSPSIndex, startCodeFPPSIndex,
                                startCodeRPPSIndex);
        }
      }
    }
  }
}

void SetH264Headers(uint8_t* extradata, int extradataSize,
                    std::vector<std::shared_ptr<ByteData>>& headers, int spsSize,
                    int startCodeSPSIndex, int startCodeFPPSIndex, int naluType) {
  int ppsSize = extradataSize - (startCodeFPPSIndex + 1);

  naluType = (uint8_t)extradata[startCodeSPSIndex + 1] & 0x1Fu;
  if (naluType == 0x07) {
    auto sps = &extradata[startCodeSPSIndex + 1];
    uint8_t* data;
    data = new uint8_t[spsSize + 4];
    memcpy(data, sps - 4, spsSize + 4);
    headers.push_back(std::shared_ptr<ByteData>(ByteData::MakeAdopted(data, spsSize + 4)));
  }

  naluType = (uint8_t)extradata[startCodeFPPSIndex + 1] & 0x1Fu;
  if (naluType == 0x08) {
    auto pps = &extradata[startCodeFPPSIndex + 1];
    uint8_t* data;
    data = new uint8_t[ppsSize + 4];
    memcpy(data, pps - 4, ppsSize + 4);
    headers.push_back(std::shared_ptr<ByteData>(ByteData::MakeAdopted(data, ppsSize + 4)));
  }
}

void SetH265Headers(uint8_t* extradata, int extradataSize,
                    std::vector<std::shared_ptr<ByteData>>& headers, int spsSize,
                    int startCodeVPSIndex, int startCodeSPSIndex, int startCodeFPPSIndex,
                    int startCodeRPPSIndex, int naluType) {
  int vpsSize = startCodeSPSIndex - startCodeVPSIndex - 4;
  int ppsSize;
  if (startCodeRPPSIndex > 0) {
    ppsSize = startCodeRPPSIndex - startCodeFPPSIndex - 4;
  } else {
    ppsSize = extradataSize - (startCodeFPPSIndex + 1);
  }

  naluType = (uint8_t)extradata[startCodeVPSIndex + 1] & 0x4Fu;
  if (naluType == 0x40) {
    auto vps = &extradata[startCodeVPSIndex + 1];
    uint8_t* data;
    data = new uint8_t[vpsSize + 4];
    memcpy(data, vps - 4, vpsSize + 4);
    headers.push_back(std::shared_ptr<ByteData>(ByteData::MakeAdopted(data, vpsSize + 4)));
  }

  naluType = (uint8_t)extradata[startCodeSPSIndex + 1] & 0x4Fu;
  if (naluType == 0x42) {
    auto sps = &extradata[startCodeSPSIndex + 1];
    uint8_t* data;
    data = new uint8_t[spsSize + 4];
    memcpy(data, sps - 4, spsSize + 4);
    headers.push_back(std::shared_ptr<ByteData>(ByteData::MakeAdopted(data, spsSize + 4)));
  }

  naluType = (uint8_t)extradata[startCodeFPPSIndex + 1] & 0x4Fu;
  if (naluType == 0x44) {
    auto pps = &extradata[startCodeFPPSIndex + 1];
    uint8_t* data;
    data = new uint8_t[ppsSize + 4];
    memcpy(data, pps - 4, ppsSize + 4);
    headers.push_back(std::shared_ptr<ByteData>(ByteData::MakeAdopted(data, ppsSize + 4)));
  }
  if (startCodeRPPSIndex > 0) {
    int rppsSize = extradataSize - (startCodeRPPSIndex + 1);
    naluType = (uint8_t)extradata[startCodeRPPSIndex + 1] & 0x4Fu;
    if (naluType == 0x44) {
      uint8_t* rpps = &extradata[startCodeRPPSIndex + 1];
      uint8_t* data;
      data = new uint8_t[rppsSize + 4];
      memcpy(data, rpps - 4, rppsSize + 4);
      headers.push_back(std::shared_ptr<ByteData>(ByteData::MakeAdopted(data, rppsSize + 4)));
    }
  }
}

void FFmpegDemuxer::getNALUInfoWithVideoStream(MediaFormat* trackFormat, AVStream* avStream) {
  AVPacket pkt;
  av_init_packet(&pkt);

  int size = av_read_frame(formatContext, &pkt);
  if (size < 0 || pkt.size < 0) {
    LOGE("can not get sps or pps information!!!\n");
    av_packet_unref(&pkt);
    return;
  }

  auto avCodecId = avStream->codecpar->codec_id;
  if (avCodecId == AV_CODEC_ID_H264) {
    absFilter = av_bsf_get_by_name("h264_mp4toannexb");
  } else if (avCodecId == AV_CODEC_ID_HEVC) {
    absFilter = av_bsf_get_by_name("hevc_mp4toannexb");
  }
  if (absFilter == nullptr) {
    LOGE("AVBSFContext init failed!!!\n");
    av_packet_unref(&pkt);
    return;
  }

  av_bsf_alloc(absFilter, &avbsfContext);
  avcodec_parameters_copy(avbsfContext->par_in, avStream->codecpar);
  av_bsf_init(avbsfContext);

  uint8_t* extradata = avbsfContext->par_out->extradata;
  int extradataSize = avbsfContext->par_out->extradata_size;

  int startCodeVPSIndex = 0;
  int startCodeSPSIndex = 0;
  int startCodeFPPSIndex = 0;

  int startCodeRPPSIndex = 0;
  int naluType = 0;

  SetStartCodeIndex(extradata, extradataSize, avCodecId, &startCodeVPSIndex, &startCodeSPSIndex,
                    &startCodeFPPSIndex, &startCodeRPPSIndex);

  std::vector<std::shared_ptr<ByteData>> headers{};
  int spsSize = startCodeFPPSIndex - startCodeSPSIndex - 4;

  if (avCodecId == AV_CODEC_ID_H264) {
    SetH264Headers(extradata, extradataSize, headers, spsSize, startCodeSPSIndex,
                   startCodeFPPSIndex, naluType);
  } else if (avCodecId == AV_CODEC_ID_HEVC) {
    SetH265Headers(extradata, extradataSize, headers, spsSize, startCodeVPSIndex, startCodeSPSIndex,
                   startCodeFPPSIndex, startCodeRPPSIndex, naluType);
  }
  av_packet_unref(&pkt);

  trackFormat->setHeaders(headers);
}

std::shared_ptr<PTSDetail> FFmpegDemuxer::createPTSDetail() {
  std::vector<int> keyframeIndexVector{};
  AVStream* avStream = formatContext->streams[videoStreamIndex];
  std::list<int64_t> ptsList{};
  for (int i = avStream->nb_index_entries - 1; i >= 0; --i) {
    auto entry = avStream->index_entries[i];
    auto pts = av_rescale_q_rnd(entry.pts, avStream->time_base, AVRational{1, AV_TIME_BASE},
                                AVRounding::AV_ROUND_ZERO);
    int index = 0;
    auto it = ptsList.begin();
    for (; it != ptsList.end(); ++it) {
      if (*it < pts) {
        ++index;
        continue;
      }
      break;
    }
    ptsList.insert(it, pts);
    if (entry.flags & AVINDEX_KEYFRAME) {
      index = avStream->nb_index_entries - (static_cast<int>(ptsList.size()) - index);
      keyframeIndexVector.insert(keyframeIndexVector.begin(), index);
    }
  }
  auto duration = 1000000 * avStream->duration * avStream->time_base.num / avStream->time_base.den;
  auto ptsVector = std::vector<int64_t>{std::make_move_iterator(ptsList.begin()),
                                        std::make_move_iterator(ptsList.end())};
  return std::make_shared<PTSDetail>(duration, std::move(ptsVector),
                                     std::move(keyframeIndexVector));
}
}  // namespace pag

#endif
