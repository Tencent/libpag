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
#include "VideoInfoManager.h"
#include "WebVideoSequenceDemuxer.h"
#include "pag/file.h"

namespace pag {
bool IsFrameInStaticRange(Frame targetFrame, std::vector<TimeRange>& staticTimeRanges) {
  for (const auto& timeRange : staticTimeRanges) {
    if (timeRange.start < targetFrame && targetFrame <= timeRange.end) {
      return true;
    }
  }
  return false;
}

void CollectVideoCompositionReferences(const Composition* composition,
                                       const VideoComposition* targetVideo, float mainCompFrameRate,
                                       Frame mainCompClipStart, Frame localClipStart,
                                       Frame localClipEnd,
                                       std::vector<TimeRange>* referenceRanges) {
  if (composition->type() != CompositionType::Vector) {
    return;
  }

  float currentFrameRate = composition->frameRate;
  for (auto layer : static_cast<const VectorComposition*>(composition)->layers) {
    if (layer->type() != LayerType::PreCompose) {
      continue;
    }

    auto preComposeLayer = static_cast<PreComposeLayer*>(layer);
    Frame offset = preComposeLayer->compositionStartTime;
    Frame visibleStart = std::max(layer->startTime, localClipStart);
    Frame visibleEnd = std::min(layer->startTime + layer->duration, localClipEnd);
    if (visibleStart >= visibleEnd) {
      continue;
    }

    // Convert frame offset from current composition's frame rate to main composition's frame rate
    float toMainFactor = mainCompFrameRate / currentFrameRate;
    Frame absoluteStart =
        mainCompClipStart + static_cast<Frame>((visibleStart - localClipStart) * toMainFactor);
    Frame absoluteEnd =
        mainCompClipStart + static_cast<Frame>((visibleEnd - localClipStart) * toMainFactor);

    if (preComposeLayer->composition == targetVideo) {
      referenceRanges->push_back({absoluteStart, absoluteEnd});
    } else if (preComposeLayer->composition->type() == CompositionType::Vector) {
      auto subComposition = preComposeLayer->composition;
      float toSubFactor = subComposition->frameRate / currentFrameRate;
      Frame newLocalClipStart = static_cast<Frame>((visibleStart - offset) * toSubFactor);
      Frame newLocalClipEnd = static_cast<Frame>((visibleEnd - offset) * toSubFactor);
      if (newLocalClipStart < newLocalClipEnd) {
        CollectVideoCompositionReferences(subComposition, targetVideo, mainCompFrameRate,
                                          absoluteStart, newLocalClipStart, newLocalClipEnd,
                                          referenceRanges);
      }
    }
  }
}

size_t GetMaxOverlapCount(const std::vector<TimeRange>& referenceRanges) {
  if (referenceRanges.empty()) {
    return 0;
  }

  std::vector<Frame> startTimes, endTimes;
  for (const auto& range : referenceRanges) {
    startTimes.push_back(range.start);
    endTimes.push_back(range.end);
  }

  std::sort(startTimes.begin(), startTimes.end());
  std::sort(endTimes.begin(), endTimes.end());

  size_t startIndex = 0, endIndex = 0, maxCount = 0, count = 0;
  while (startIndex < startTimes.size()) {
    if (startTimes[startIndex] < endTimes[endIndex]) {
      maxCount = std::max(maxCount, ++count);
      startIndex++;
    } else {
      count--;
      endIndex++;
    }
  }
  return maxCount;
}

VideoInfoManager::VideoInfoManager(std::shared_ptr<PAGComposition> pagComposition)
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
            getPreLayerByComposition(pagFile->getFile()->getRootLayer(), composition);
        videoInfo.pagCompositions = findCompositionByPreComposeLayer(pagFile, videoInfo.preLayer);
        videoInfoMap.emplace(composition->uniqueID, std::move(videoInfo));
        videoIDs.push_back(composition->uniqueID);
      }
    }
  }
}

emscripten::val VideoInfoManager::getMp4DataByID(const ID id) {

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

int32_t VideoInfoManager::getWidthByID(ID id) {
  auto iter = videoInfoMap.find(id);

  if (iter != videoInfoMap.end()) {
    return iter->second.width;
  }

  return 0;
}

int32_t VideoInfoManager::getHeightByID(ID id) {
  auto iter = videoInfoMap.find(id);

  if (iter != videoInfoMap.end()) {
    return iter->second.height;
  }

  return 0;
}

float VideoInfoManager::getFrameRateByID(ID id) {
  auto iter = videoInfoMap.find(id);

  if (iter != videoInfoMap.end()) {
    return iter->second.frameRate;
  }

  return 0;
}

emscripten::val VideoInfoManager::getStaticTimeRangesByID(ID id) {
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

int VideoInfoManager::getTargetFrameByID(ID id) {
  auto iter = videoInfoMap.find(id);

  if (iter != videoInfoMap.end()) {
    for (auto pagComposition : iter->second.pagCompositions) {
      if (pagComposition == nullptr || pagComposition->parent() == nullptr ||
          iter->second.preLayer == nullptr) {
        continue;
      }
      if (pagComposition->parent()->contentFrame < 0) {
        continue;
      }
      Frame targetFrame = getTargetFrame(pagComposition, iter->second.preLayer);
      if (iter->second.pagCompositionLastFrames.count(pagComposition->uniqueID())) {
        if (iter->second.pagCompositionLastFrames[pagComposition->uniqueID()] == targetFrame) {
          continue;
        }
      }
      iter->second.pagCompositionLastFrames[pagComposition->uniqueID()] = targetFrame;
      if (IsFrameInStaticRange(targetFrame, iter->second.videoSequence->staticTimeRanges)) {
        continue;
      }
      if (iter->second.lastFrame == targetFrame) {
        continue;
      }
      iter->second.lastFrame = targetFrame;
      return static_cast<int>(targetFrame);
    }
  }
  return -1;
}

float VideoInfoManager::getPlaybackRateByID(ID id) {
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

emscripten::val VideoInfoManager::getVideoIDs() {
  int size = videoIDs.size();
  emscripten::val jsArray = emscripten::val::array();
  for (int i = 0; i < size; ++i) {
    jsArray.call<void>("push", videoIDs[i]);
  }
  return jsArray;
}

bool VideoInfoManager::HasVideo(std::shared_ptr<PAGComposition>& pagComposition) {
  // Check if current composition is a PAGFile with video
  if (pagComposition->isPAGFile() && pagComposition->file) {
    for (const auto& composition : pagComposition->file->compositions) {
      if (composition->type() == CompositionType::Video) {
        return true;
      }
    }
  }

  // Recursively check all child layers
  for (const auto& childLayer : pagComposition->layers) {
    if (childLayer->layerType() == LayerType::PreCompose) {
      // Recursively check nested PAGComposition
      auto childComposition = std::static_pointer_cast<PAGComposition>(childLayer);
      if (HasVideo(childComposition)) {
        return true;
      }
    }
  }

  return false;
}

void VideoInfoManager::getPAGFileFromPAGComposition(
    std::shared_ptr<PAGComposition>& pagComposition,
    std::vector<std::shared_ptr<PAGFile>>& pagFiles) {
  if (pagComposition == nullptr) {
    return;
  }

  // Check if current composition is a PAGFile with video
  if (pagComposition->isPAGFile() && HasVideo(pagComposition)) {
    pagFiles.push_back(std::static_pointer_cast<PAGFile>(pagComposition));
  }

  // Recursively check all child layers
  auto layerSize = pagComposition->numChildren();
  for (int i = 0; i < layerSize; ++i) {
    auto childLayer = pagComposition->getLayerAt(i);

    // Only process if child layer is a PAGComposition (check type first)
    if (childLayer->layerType() == LayerType::PreCompose) {
      auto childComposition = std::static_pointer_cast<PAGComposition>(childLayer);
      getPAGFileFromPAGComposition(childComposition, pagFiles);
    }
  }
}

PreComposeLayer* VideoInfoManager::getPreLayerByComposition(PreComposeLayer* preLayer,
                                                            Composition* com) {
  if (preLayer == nullptr || com == nullptr) {
    return nullptr;
  }

  if (preLayer->composition == com) {
    return preLayer;
  }

  if (preLayer->composition != nullptr &&
      preLayer->composition->type() == CompositionType::Vector) {
    auto* vectorCom = static_cast<VectorComposition*>(preLayer->composition);
    for (Layer* layer : vectorCom->layers) {
      if (layer == nullptr) continue;
      if (layer->type() == LayerType::PreCompose) {
        auto* childPre = static_cast<PreComposeLayer*>(layer);
        PreComposeLayer* result = getPreLayerByComposition(childPre, com);
        if (result != nullptr) {
          return result;
        }
      }
    }
  }
  return nullptr;
}

std::vector<std::shared_ptr<PAGComposition>> VideoInfoManager::findCompositionByPreComposeLayer(
    std::shared_ptr<PAGComposition> composition, PreComposeLayer* targetPreLayer) {
  std::vector<std::shared_ptr<PAGComposition>> results;

  if (composition == nullptr || targetPreLayer == nullptr) {
    return results;
  }

  if (composition->layer == targetPreLayer) {
    results.push_back(composition);
  }

  for (const auto& childLayer : composition->layers) {
    if (childLayer->layerType() == LayerType::PreCompose) {
      auto childComposition = std::static_pointer_cast<PAGComposition>(childLayer);
      auto childResults = findCompositionByPreComposeLayer(childComposition, targetPreLayer);
      results.insert(results.end(), childResults.begin(), childResults.end());
    }
  }

  return results;
}

Frame VideoInfoManager::getTargetFrame(std::shared_ptr<PAGComposition>& pagComposition,
                                       PreComposeLayer* preLayer) {
  Frame targetFrame = -1;
  if (preLayer == nullptr) {
    return targetFrame;
  }
  auto composition = preLayer->composition;
  if (composition->type() == CompositionType::Bitmap ||
      composition->type() == CompositionType::Video) {
    auto layerFrame = preLayer->startTime + pagComposition->contentFrame;
    targetFrame = preLayer->getCompositionFrame(layerFrame);
  }

  auto sequence = Sequence::Get(composition);
  if (sequence == nullptr) {
    return targetFrame;
  }
  targetFrame = sequence->toSequenceFrame(targetFrame);

  return targetFrame;
}

bool VideoInfoManager::hasTimeRangeOverlap() {

  for (const auto& videoPair : videoInfoMap) {
    auto compositions = videoPair.second.pagFile->getFile()->compositions;
    if (compositions.size() <= 1) {
      continue;
    }
    constexpr size_t maxAllowedOverlap = 1;
    Composition* mainComposition = compositions.back();

    if (mainComposition == nullptr || mainComposition->type() != CompositionType::Vector) {
      continue;
    }

    auto preLayer = getPreLayerByComposition(videoPair.second.pagFile->getFile()->getRootLayer(),
                                             mainComposition);

    Frame clipStart = -preLayer->compositionStartTime;
    Frame clipEnd = clipStart + preLayer->duration;

    for (auto composition : compositions) {
      if (composition->type() != CompositionType::Video) {
        continue;
      }

      auto videoComposition = static_cast<VideoComposition*>(composition);
      std::vector<TimeRange> referenceRanges;
      CollectVideoCompositionReferences(mainComposition, videoComposition,
                                        mainComposition->frameRate, clipStart, clipStart, clipEnd,
                                        &referenceRanges);

      size_t maxOverlap = GetMaxOverlapCount(referenceRanges);
      if (maxOverlap > maxAllowedOverlap) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace pag
