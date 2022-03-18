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

#pragma once

#include <unordered_map>
#include "pag/types.h"

namespace pag {
#define KEY_MIME "mime"
#define KEY_COLOR_SPACE "colorSpace"
#define KEY_COLOR_RANGE "colorRange"
#define KEY_WIDTH "width"
#define KEY_HEIGHT "height"
#define KEY_DURATION "durationUs"
#define KEY_FRAME_RATE "frame-rate"
#define KEY_ROTATION "rotation-degrees"
#define KEY_TRACK_ID "track-id"
#define MIMETYPE_UNKNOWN "unknown"
#define MIMETYPE_VIDEO_AVC "video/avc"
#define MIMETYPE_VIDEO_HEVC "video/hevc"
#define MIMETYPE_AUDIO_AAC "audio/aac"
#define MIMETYPE_AUDIO_MP3 "audio/mp3"
#define COLORSPACE_REC601 "Rec601"
#define COLORSPACE_REC709 "Rec709"
#define COLORSPACE_REC2020 "Rec2020"
#define COLORRANGE_MPEG "MPEG"
#define COLORRANGE_JPEG "JPEG"

class MediaFormat {
 public:
  MediaFormat() = default;
  ~MediaFormat();

  int getInteger(const std::string& name);
  int64_t getLong(const std::string& name);
  float getFloat(const std::string& name);
  std::string getString(const std::string& name);
  // Get video header information, AnnexB format
  std::vector<std::shared_ptr<ByteData>> headers();

 private:
  std::unordered_map<std::string, std::string> trackFormatMap{};
  std::vector<std::shared_ptr<ByteData>> _headers;

  void setInteger(std::string name, int value);
  void setLong(std::string name, int64_t value);
  void setFloat(std::string name, float value);
  void setString(std::string name, std::string value);
  void setHeaders(std::vector<std::shared_ptr<ByteData>> headers);

  friend class FFmpegDemuxer;

  friend class AudioDemuxer;
};
}  // namespace pag

#endif