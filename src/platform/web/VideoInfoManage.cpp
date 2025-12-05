/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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
#include "VideoInfoManage.h"
#include "WebVideoSequenceDemuxer.h"
#include "pag/file.h"

namespace pag {

void getPAGFileFromPAGComposition(std::shared_ptr<PAGComposition>& pagComposition,
                                  std::vector<std::shared_ptr<PAGFile>>& pagFiles) {
  if (pagComposition != nullptr) {
    if (pagComposition->isPAGFile() && pagComposition->hasVideo()) {
      pagFiles.push_back(std::static_pointer_cast<PAGFile>(pagComposition));
    }
    auto layerSize = pagComposition->numChildren();
    for (int i = 0; i < layerSize; ++i) {
      auto childLayer = pagComposition->getLayerAt(i);
      if (childLayer->isPAGFile()) {
        auto pagFile = std::static_pointer_cast<PAGFile>(childLayer);
        if (pagFile->hasVideo()) {
          pagFiles.push_back(pagFile);
        }
      }
    }
  }
}

VideoInfoManage::VideoInfoManage(std::shared_ptr<PAGComposition> pagComposition)
    : pagComposition(pagComposition) {
  getPAGFileFromPAGComposition(pagComposition, pagFiles);
  for (auto pagFile : pagFiles) {
    for (auto composition : pagFile->getFile()->compositions) {
      if (composition->type() == CompositionType::Video) {
        VideoInfo videoInfo;
        videoInfo.pagFile = pagFile;
        videoInfo.videoComposition = static_cast<VideoComposition*>(composition);
        videoInfo.videoSequence = static_cast<VideoSequence*>(Sequence::Get(composition));
        videoInfo.width = videoInfo.videoSequence->getVideoWidth();
        videoInfo.height = videoInfo.videoSequence->getVideoHeight();
        videoInfo.frameRate = videoInfo.videoSequence->frameRate;
        videoInfo.preLayer =
            pagFile->getPreLayerByComposition(pagFile->getFile()->getRootLayer(), composition);
        videoIds.push_back(composition->uniqueID);
        videoInfo.pagComposition =
            pagFile->findCompositionByPreComposeLayer(pagFile, videoInfo.preLayer);
        videoInfoMap.emplace(composition->uniqueID, std::move(videoInfo));
      }
    }
  }
}

emscripten::val VideoInfoManage::getMp4DataById(const ID id) {

  auto iter = videoInfoMap.find(id);

  if (iter != videoInfoMap.end()) {
    if (iter->second.videoSequence == nullptr) {
      return emscripten::val::null();
    }

    auto demuxer = std::make_unique<WebVideoSequenceDemuxer>(
        iter->second.pagFile->getFile(), iter->second.videoSequence, iter->second.pagFile.get());
    iter->second.mp4Data = demuxer->getMp4Data();

    if (iter->second.mp4Data == nullptr) {
      return emscripten::val::null();
    }
    return emscripten::val(
        typed_memory_view(iter->second.mp4Data->length(), iter->second.mp4Data->data()));
  }

  return emscripten::val::null();
}

int32_t VideoInfoManage::getWidthById(ID id) {
  auto iter = videoInfoMap.find(id);

  if (iter != videoInfoMap.end()) {
    return iter->second.width;
  }

  return 0;
}

int32_t VideoInfoManage::getHeightById(ID id) {
  auto iter = videoInfoMap.find(id);

  if (iter != videoInfoMap.end()) {
    return iter->second.height;
  }

  return 0;
}

float VideoInfoManage::getFrameRateById(ID id) {
  auto iter = videoInfoMap.find(id);

  if (iter != videoInfoMap.end()) {
    return iter->second.frameRate;
  }

  return 0;
}

emscripten::val VideoInfoManage::getStaticTimeRangesById(ID id) {
  auto iter = videoInfoMap.find(id);

  if (iter != videoInfoMap.end()) {
    if (iter->second.videoSequence == nullptr) {
      return emscripten::val::null();
    }
    auto demuxer = std::make_unique<WebVideoSequenceDemuxer>(
        iter->second.pagFile->getFile(), iter->second.videoSequence, iter->second.pagFile.get());
    return demuxer->getStaticTimeRanges();
  }

  return emscripten::val::null();
}

int VideoInfoManage::getTargetFrameById(ID id) {
  auto iter = videoInfoMap.find(id);

  if (iter != videoInfoMap.end()) {
    auto pagComposition = iter->second.pagComposition;
    if (pagComposition == nullptr || pagComposition->parent() == nullptr ||
        iter->second.preLayer == nullptr) {
      return -1;
    }
    if (pagComposition->parent()->getContentFrame() < 0) {
      return -1;
    }
    Frame targetFrame = pagComposition->getTargetFrame(iter->second.preLayer);
    return static_cast<int>(targetFrame);
  }
  return -1;
}

float VideoInfoManage::getPlaybackRateById(ID id) {
  float playbackRate = 1;
  auto iter = videoInfoMap.find(id);

  if (iter != videoInfoMap.end()) {
    if (iter->second.pagFile != nullptr &&
        iter->second.pagFile->timeStretchModeInternal() == PAGTimeStretchMode::Scale) {
      playbackRate =
          iter->second.pagFile->getFile()->duration() /
          ((iter->second.pagFile->duration() / 1000000) * iter->second.pagFile->frameRate());
    }
  }

  return playbackRate;
}

emscripten::val VideoInfoManage::getVideoIds() {
  int size = videoIds.size();
  emscripten::val jsArray = emscripten::val::array();
  for (int i = 0; i < size; ++i) {
    jsArray.call<void>("push", videoIds[i]);
  }
  return jsArray;
}

}  // namespace pag