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
    if (timeRange.contains(targetFrame)) {
      return true;
    }
  }
  return false;
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

bool VideoInfoManager::hasTimeRangeOverlap() const {
  // Iterate through each VideoInfo in videoInfoMap
  for (const auto& videoPair : videoInfoMap) {
    const auto& pagCompositions = videoPair.second.pagCompositions;

    if (pagCompositions.size() <= 1) {
      continue;
    }

    // Check each pair of pagCompositions for time range overlap
    for (size_t i = 0; i < pagCompositions.size() - 1; ++i) {
      const auto& comp1 = pagCompositions[i];
      if (comp1 == nullptr || comp1->parent() == nullptr) {
        continue;
      }

      Frame startFrame1 = comp1->parent()->startFrame;
      Frame endFrame1 = startFrame1 + comp1->parent()->frameDuration();

      for (size_t j = i + 1; j < pagCompositions.size(); ++j) {
        const auto& comp2 = pagCompositions[j];
        if (comp2 == nullptr || comp2->parent() == nullptr) {
          continue;
        }

        Frame startFrame2 = comp2->parent()->startFrame;
        Frame endFrame2 = startFrame2 + comp2->parent()->frameDuration();

        // Check if time ranges [startFrame1, endFrame1) and [startFrame2, endFrame2) overlap
        if (startFrame1 < endFrame2 && startFrame2 < endFrame1) {
          return true;
        }
      }
    }
  }

  return false;
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

}  // namespace pag