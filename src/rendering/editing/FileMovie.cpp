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

#include "FileMovie.h"
#include "audio/model/AudioClip.h"
#include "base/utils/MatrixUtil.h"
#include "base/utils/TimeUtil.h"
#include "platform/Platform.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/graphics/Picture.h"
#include "rendering/graphics/Recorder.h"
#include "rendering/readers/MovieReader.h"
#include "video/FFmpegDemuxer.h"

namespace pag {

class MovieProxy : public TextureProxy {
 public:
  MovieProxy(MovieInfo* movieInfo, int64_t timestamp, ID movieID)
      : TextureProxy(movieInfo->videoWidth(), movieInfo->videoHeight()),
        movieInfo(movieInfo),
        timestamp(timestamp),
        movieID(movieID) {
  }

  bool cacheEnabled() const override {
    return true;
  }

  void prepare(RenderCache* cache) const override {
    cache->prepareMovieReader(movieID, movieInfo, timestamp, DecodingPolicy::SoftwareToHardware);
  }

  std::shared_ptr<tgfx::Texture> getTexture(RenderCache* cache) const override {
    auto reader = cache->getMovieReader(movieID, movieInfo);
    if (reader == nullptr) {
      return nullptr;
    }
    return reader->readTexture(timestamp, static_cast<RenderCache*>(cache));
  }

 private:
  MovieInfo* movieInfo = nullptr;
  int64_t timestamp = 0;
  ID movieID = 0;
};

static std::mutex mutex = {};
static std::unordered_map<std::string, std::shared_ptr<MovieInfo>> movieInfoMap = {};

std::shared_ptr<MovieInfo> MovieInfo::Load(const std::string& filePath) {
  std::lock_guard<std::mutex> locker(mutex);
  auto iter = movieInfoMap.find(filePath);
  if (iter != movieInfoMap.end()) {
    return iter->second;
  }
  auto info = std::make_shared<MovieInfo>();
  auto path = filePath;
  auto index = path.rfind('?');
  if (index != std::string::npos) {
    path = path.substr(0, index);
    info->shareDecoder = true;
  }
  info->_filePath = path;
  auto demuxer = FFmpegDemuxer::Make(path, Platform::Current()->naluType());
  if (demuxer == nullptr) {
    return nullptr;
  }
  for (int i = 0; i < demuxer->getTrackCount(); ++i) {
    auto format = demuxer->getTrackFormat(i);
    if (!format) {
      continue;
    }
    info->_duration = std::max(info->_duration, format->getLong(KEY_DURATION));
    if (format->getString(KEY_MIME).rfind("video", 0) == 0) {
      info->_frameRate = format->getFloat(KEY_FRAME_RATE);
      info->_rotation = format->getInteger(KEY_ROTATION);
      info->_videoWidth = format->getInteger(KEY_WIDTH);
      info->_videoHeight = format->getInteger(KEY_HEIGHT);
      info->_trackID = format->getInteger(KEY_TRACK_ID);
      break;
    }
  }
  if (info->videoWidth() <= 0 || info->videoHeight() <= 0 || info->duration() <= 0) {
    return nullptr;
  }
  movieInfoMap.insert(std::make_pair(filePath, info));
  return info;
}

std::shared_ptr<PAGMovie> PAGMovie::FromVideoPath(const std::string& filePath) {
  return PAGMovie::FromVideoPath(filePath, -1, -1);
}

std::shared_ptr<PAGMovie> PAGMovie::FromVideoPath(const std::string& filePath, int64_t startTime,
                                                  int64_t duration, float speed, int16_t trackID) {
  if (filePath.empty() || speed < 0) {
    return nullptr;
  }
  std::string path;
  if (trackID != 0) {
    path = filePath + "?trackID=" + std::to_string(trackID);
  } else {
    path = filePath;
  }
  auto movieInfo = MovieInfo::Load(path);
  if (movieInfo == nullptr) {
    return nullptr;
  }
  auto movie = std::shared_ptr<FileMovie>(new FileMovie(movieInfo));
  if (startTime >= movie->info->duration()) {
    return nullptr;
  }
  movie->speed = speed;
  auto endTime = startTime + duration;
  if (startTime == -1 && duration == -1) {
    endTime = movie->info->duration();
  }
  movie->startTime = std::max(static_cast<int64_t>(0), startTime);
  movie->_duration = std::min(endTime, movie->info->duration()) - movie->startTime;
  if (movie->_duration < 0) {
    return nullptr;
  }
  if (speed == 0) {
    movie->_duration = 0;
  } else {
    movie->_duration = movie->_duration / speed;
  }
  return movie;
}

FileMovie::FileMovie(std::shared_ptr<MovieInfo> movieInfo) : info(std::move(movieInfo)) {
  _duration = info->duration();
  int r = std::abs(info->rotation() / 90);
  if (r == 1 || r == 3) {
    _width = info->videoHeight();
    _height = info->videoWidth();
  } else {
    _width = info->videoWidth();
    _height = info->videoHeight();
  }
  if (info->rotation() != 0) {
    matrix.setRotate(static_cast<float>(info->rotation()),
                     static_cast<float>(info->videoWidth()) / 2,
                     static_cast<float>(info->videoHeight()) / 2);
    matrix.postTranslate(static_cast<float>(_width - info->videoWidth()) / 2,
                         static_cast<float>(_height - info->videoHeight()) / 2);
  }
}

tgfx::Rect FileMovie::getContentSize() const {
  return tgfx::Rect::MakeWH(_width, _height);
}

int64_t FileMovie::duration() {
  return durationInternal();
}

void FileMovie::setVolumeRamp(float fromStartVolume, float toEndVolume,
                              const TimeRange& forTimeRange) {
  volumeRanges.emplace_back(forTimeRange, fromStartVolume, toEndVolume);
}

int64_t FileMovie::durationInternal() {
  return _duration;
}

void FileMovie::draw(Recorder* recorder) {
  auto content = getGraphic();
  if (content) {
    recorder->drawGraphic(content, matrix);
  }
}

void FileMovie::measureBounds(tgfx::Rect* bounds) {
  bounds->setXYWH(0, 0, static_cast<float>(_width), static_cast<float>(_height));
}

bool FileMovie::setCurrentTime(int64_t time) {
  contentTime = time;
  movieTime = time;
  if (movieTime < 0) {
    movieTime = 0;
  } else if (movieTime >= _duration) {
    movieTime = std::max(static_cast<int64_t>(0), _duration - 1);
  }
  movieTime = static_cast<int64_t>(movieTime * speed) + startTime;
  auto movieFrame = TimeToFrame(movieTime, info->frameRate());
  if (movieFrame == lastMovieFrame) {
    return false;
  }
  lastMovieFrame = movieFrame;
  // 仅当时间戳对应的视频帧真的发生变化时才重置 graphic。
  graphic = nullptr;
  // 定格
  return !(0 <= startTime && _duration == 0) && speed != 0;
}

std::shared_ptr<Graphic> FileMovie::getGraphic() {
  if (graphic == nullptr && movieTime >= 0) {
    auto proxy = new MovieProxy(info.get(), movieTime, uniqueID());
    graphic = Picture::MakeFrom(uniqueID(), std::unique_ptr<MovieProxy>(proxy));
  }
  return graphic;
}

std::vector<AudioClip> FileMovie::generateAudioClips() {
  if (_duration == 0 || speed == 0) {
    return {};
  }
  auto sourceTimeRange = TimeRange{startTime, startTime + static_cast<Frame>(_duration * speed)};
  auto targetTimeRange = TimeRange{0, _duration};
  return {AudioClip(AudioSource(info->filePath()), sourceTimeRange, targetTimeRange, volumeRanges)};
}
}  // namespace pag

#else

#include "FileMovie.h"
#include "pag/pag.h"

namespace pag {
std::shared_ptr<PAGMovie> PAGMovie::FromVideoPath(const std::string&) {
  return nullptr;
}

std::shared_ptr<PAGMovie> PAGMovie::FromVideoPath(const std::string&, int64_t, int64_t, float,
                                                  int16_t) {
  return nullptr;
}
}  // namespace pag

#endif
