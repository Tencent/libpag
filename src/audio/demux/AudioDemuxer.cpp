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

#include "AudioDemuxer.h"

namespace pag {

static const AVRational TimeBaseQ = {1, AV_TIME_BASE};

std::unique_ptr<AudioDemuxer> AudioDemuxer::Make(const AudioSource& source) {
  auto demuxer = std::unique_ptr<AudioDemuxer>(new AudioDemuxer());
  if (demuxer->openFile(source) < 0) {
    return nullptr;
  }
  return demuxer;
}

AudioDemuxer::~AudioDemuxer() {
  av_packet_unref(&avPacket);
  avformat_close_input(&fmtCtx);
  if (avioCtx) {
    // note: the internal buffer could have changed, and be != buffer
    av_freep(&avioCtx->buffer);
    avio_context_free(&avioCtx);
  }
  for (const auto& iter : formats) {
    delete iter.second;
  }
}

bool AudioDemuxer::selectTrack(unsigned int index) {
  if (index >= fmtCtx->nb_streams) {
    return false;
  }
  currentStreamIndex = static_cast<int>(index);
  return true;
}

unsigned int AudioDemuxer::getTrackCount() {
  return fmtCtx->nb_streams;
}

MediaFormat* AudioDemuxer::getTrackFormat(unsigned int index) {
  auto iter = formats.find(index);
  if (iter != formats.end()) {
    return iter->second;
  }
  auto format = getTrackFormatInternal(index);
  if (format) {
    formats.insert(std::make_pair(index, format));
  }
  return format;
}

AVStream* AudioDemuxer::getStream(unsigned int index) {
  return fmtCtx->streams[index];
}

bool AudioDemuxer::seekTo(int64_t timestamp) {
  if (currentStreamIndex < 0) {
    return false;
  }
  auto time_base = fmtCtx->streams[currentStreamIndex]->time_base;
  auto pos = av_rescale_q(timestamp, TimeBaseQ, time_base);
  auto ret = av_seek_frame(fmtCtx, currentStreamIndex, pos, AVSEEK_FLAG_ANY | AVSEEK_FLAG_BACKWARD);
  if (ret < 0) {
    LOGE("Error in seeking\n");
    return false;
  }
  return true;
}

bool AudioDemuxer::advance() {
  if (currentStreamIndex < 0) {
    return false;
  }
  av_packet_unref(&avPacket);
  while (av_read_frame(fmtCtx, &avPacket) >= 0) {
    if (avPacket.stream_index != currentStreamIndex) {
      av_packet_unref(&avPacket);
      continue;
    }
    auto time_base = fmtCtx->streams[currentStreamIndex]->time_base;
    currentTime = av_rescale_q(avPacket.pts, time_base, TimeBaseQ);
    return true;
  }
  return false;
}

SampleData AudioDemuxer::readSampleData() const {
  if (avPacket.data == nullptr) {
    return {};
  }
  return {avPacket.data, avPacket.size};
}

int64_t AudioDemuxer::sampleTime() const {
  return currentTime;
}

int readPacket(void* opaque, uint8_t* buf, int bufSize) {
  auto bufferData = (AudioDemuxer::BufferData*)opaque;
  bufSize = FFMIN(bufSize, static_cast<int>(bufferData->size));
  if (bufSize <= 0) {
    return AVERROR_EOF;
  }
  memcpy(buf, bufferData->ptr, bufSize);
  bufferData->ptr += bufSize;
  bufferData->size -= bufSize;
  return bufSize;
}

int64_t seekInBuffer(void* opaque, int64_t offset, int whence) {
  auto bufferData = static_cast<AudioDemuxer::BufferData*>(opaque);
  int64_t ret = -1;
  switch (whence) {
    case AVSEEK_SIZE:
      ret = bufferData->fileSize;
      break;
    case SEEK_SET:
      bufferData->ptr = bufferData->originPtr + offset;
      bufferData->size = bufferData->fileSize - offset;
      ret = reinterpret_cast<int64_t>(bufferData->ptr);
      break;
    default:
      break;
  }
  return ret;
}

int AudioDemuxer::openFile(const AudioSource& source) {
  if (source.filePath.empty()) {
    bufferData.originPtr = source.data->data();
    bufferData.fileSize = source.data->length();
    bufferData.ptr = source.data->data();
    bufferData.size = source.data->length();
    size_t bufferSize = 32768;
    auto buffer = static_cast<unsigned char*>(av_malloc(bufferSize));
    avioCtx = avio_alloc_context(buffer, static_cast<int>(bufferSize), 0, &bufferData, readPacket,
                                 nullptr, seekInBuffer);
    if (avioCtx == nullptr) {
      av_freep(&buffer);
      return -1;
    }

    fmtCtx = avformat_alloc_context();
    fmtCtx->pb = avioCtx;
    fmtCtx->avio_flags = AVFMT_FLAG_CUSTOM_IO;
    if (avformat_open_input(&fmtCtx, "", nullptr, nullptr) < 0) {
      LOGE("Could not open source data.\n");
      return -2;
    }
  } else {
    auto path = static_cast<const char*>(source.filePath.data());
    auto ret = avformat_open_input(&fmtCtx, path, nullptr, nullptr);
    if (ret < 0) {
      // LOGE("Could not open source file %s, %s\n", path, av_err2str(ret));
      return -2;
    }
  }
  if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
    LOGE("Could not find stream information\n");
    return -3;
  }
  mediaSource = source;
  return 0;
}

std::string AVCodecIDToStringAudio(AVCodecID codecId) {
  std::string codecIDString;
  switch (codecId) {
    case AVCodecID::AV_CODEC_ID_AAC:
      codecIDString = MIMETYPE_AUDIO_AAC;
      break;
    case AVCodecID::AV_CODEC_ID_MP3:
      codecIDString = MIMETYPE_AUDIO_MP3;
      break;
    default:
      codecIDString = MIMETYPE_UNKNOWN;
      break;
  }
  return codecIDString;
}

MediaFormat* AudioDemuxer::getTrackFormatInternal(unsigned int index) {
  auto avStream = fmtCtx->streams[index];
  auto mediaType = avStream->codecpar->codec_type;
  if (mediaType != AVMEDIA_TYPE_AUDIO) {
    return nullptr;
  }
  auto trackFormat = new MediaFormat();
  trackFormat->setInteger(KEY_TRACK_ID, static_cast<int>(index));
  auto duration = av_rescale_q(avStream->duration, avStream->time_base, TimeBaseQ);
  trackFormat->setLong(KEY_DURATION, duration);
  trackFormat->setString(KEY_MIME, AVCodecIDToStringAudio(avStream->codecpar->codec_id));
  return trackFormat;
}
}  // namespace pag
#endif