/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "base/utils/TimeUtil.h"
#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/utils/LockGuard.h"
#include "rendering/utils/ScopedLock.h"

namespace pag {
uint16_t PAGFile::MaxSupportedTagLevel() {
  return File::MaxSupportedTagLevel();
}

std::shared_ptr<PAGFile> PAGFile::Load(const void* bytes, size_t length,
                                       const std::string& filePath) {
  auto file = File::Load(bytes, length, filePath);
  return MakeFrom(file);
}

std::shared_ptr<PAGFile> PAGFile::Load(const std::string& filePath) {
  auto file = File::Load(filePath);
  return MakeFrom(file);
}

std::shared_ptr<PAGFile> PAGFile::MakeFrom(std::shared_ptr<File> file) {
  if (file == nullptr) {
    return nullptr;
  }
  auto pagLayer = BuildPAGLayer(file, file->getRootLayer());
  auto locker = std::make_shared<std::mutex>();
  pagLayer->updateRootLocker(locker);
  if (pagLayer->layerType() != LayerType::PreCompose) {
    return nullptr;
  }
  pagLayer->gotoTime(0);
  auto pagFile = std::static_pointer_cast<PAGFile>(pagLayer);
  pagFile->onAddToRootFile(pagFile.get());
  pagFile->_stretchedFrameDuration = pagLayer->layer->duration;
  pagFile->_timeStretchMode = file->timeStretchMode;
  return pagFile;
}

std::shared_ptr<PAGLayer> PAGFile::BuildPAGLayer(std::shared_ptr<File> file, Layer* layer) {
  PAGLayer* pagLayer;
  switch (layer->type()) {
    case LayerType::Solid: {
      pagLayer = new PAGSolidLayer(file, static_cast<SolidLayer*>(layer));
    } break;
    case LayerType::Text: {
      pagLayer = new PAGTextLayer(file, static_cast<TextLayer*>(layer));
      pagLayer->_editableIndex = file->getEditableIndex(static_cast<TextLayer*>(layer));
    } break;
    case LayerType::Shape: {
      pagLayer = new PAGShapeLayer(file, static_cast<ShapeLayer*>(layer));
    } break;
    case LayerType::Image: {
      pagLayer = new PAGImageLayer(file, static_cast<ImageLayer*>(layer));
      pagLayer->_editableIndex = file->getEditableIndex(static_cast<ImageLayer*>(layer));
    } break;
    case LayerType::PreCompose: {
      if (layer == file->getRootLayer()) {
        pagLayer = new PAGFile(file, static_cast<PreComposeLayer*>(layer));
      } else {
        pagLayer = new PAGComposition(file, static_cast<PreComposeLayer*>(layer));
      }

      auto composition = static_cast<PreComposeLayer*>(layer)->composition;
      if (composition->type() == CompositionType::Vector) {
        auto& layers = static_cast<VectorComposition*>(composition)->layers;
        // The index order of PAGLayers is different from Layers in File.
        for (int i = static_cast<int>(layers.size()) - 1; i >= 0; i--) {
          auto childLayer = layers[i];
          auto childPAGLayer = BuildPAGLayer(file, childLayer);
          static_cast<PAGComposition*>(pagLayer)->layers.push_back(childPAGLayer);
          childPAGLayer->_parent = static_cast<PAGComposition*>(pagLayer);
          if (childLayer->trackMatteLayer) {
            childPAGLayer->_trackMatteLayer = BuildPAGLayer(file, childLayer->trackMatteLayer);
            childPAGLayer->_trackMatteLayer->trackMatteOwner = childPAGLayer.get();
          }
        }
      }
    } break;
    default:
      pagLayer = new PAGLayer(file, layer);
      break;
  }
  auto shared = std::shared_ptr<PAGLayer>(pagLayer);
  pagLayer->weakThis = shared;
  return shared;
}

PAGFile::PAGFile(std::shared_ptr<File> file, pag::PreComposeLayer* layer)
    : PAGComposition(file, layer) {
}

uint16_t PAGFile::tagLevel() {
  return file->tagLevel();
}

int PAGFile::numTexts() {
  return file->numTexts();
}

int PAGFile::numImages() {
  return file->numImages();
}

int PAGFile::numVideos() {
  return file->numVideos();
}

std::string PAGFile::path() const {
  return file->path;
}

std::shared_ptr<TextDocument> PAGFile::getTextData(int editableTextIndex) {
  return file->getTextData(editableTextIndex);
}

void PAGFile::replaceText(int editableTextIndex, std::shared_ptr<TextDocument> textData) {
  LockGuard autoLock(rootLocker);
  auto textLayers = getLayersByEditableIndexInternal(editableTextIndex, LayerType::Text);
  replaceTextInternal(textLayers, textData);
}

void PAGFile::replaceImage(int editableImageIndex, std::shared_ptr<PAGImage> image) {
  LockGuard autoLock(rootLocker);
  auto imageLayers = getLayersByEditableIndexInternal(editableImageIndex, LayerType::Image);
  replaceImageInternal(imageLayers, image);
}

void PAGFile::replaceImageByName(const std::string& layerName, std::shared_ptr<PAGImage> image) {
  if (layerName.empty()) {
    return;
  }
  auto imageLayers = getLayersByName(layerName);
  if (imageLayers.size() == 0) {
    return;
  }
  LockGuard autoLock(rootLocker);
  replaceImageInternal(imageLayers, image);
}

std::vector<std::shared_ptr<PAGLayer>> PAGFile::getLayersByEditableIndex(int editableIndex,
                                                                         LayerType layerType) {
  LockGuard autoLock(rootLocker);
  return getLayersByEditableIndexInternal(editableIndex, layerType);
}

std::vector<std::shared_ptr<PAGLayer>> PAGFile::getLayersByEditableIndexInternal(
    int editableIndex, LayerType layerType) {
  std::vector<std::shared_ptr<PAGLayer>> result = getLayersBy([=](PAGLayer* pagLayer) -> bool {
    return pagLayer->layerType() == layerType && pagLayer->_editableIndex == editableIndex &&
           pagLayer->file == file;
  });
  return result;
}

PAGTimeStretchMode PAGFile::timeStretchMode() const {
  LockGuard autoLock(rootLocker);
  return _timeStretchMode;
}

PAGTimeStretchMode PAGFile::timeStretchModeInternal() const {
  return _timeStretchMode;
}

void PAGFile::setTimeStretchMode(PAGTimeStretchMode value) {
  LockGuard autoLock(rootLocker);
  _timeStretchMode = value;
}

void PAGFile::setDuration(int64_t duration) {
  LockGuard autoLock(rootLocker);
  setDurationInternal(duration);
}

void PAGFile::setDurationInternal(int64_t duration) {
  auto totalFrames = TimeToFrame(duration, frameRateInternal());
  if (totalFrames <= 0) {
    totalFrames = layer->duration;
  }
  if (_stretchedFrameDuration == totalFrames) {
    return;
  }
  _stretchedFrameDuration = totalFrames;
  if (_parent && _parent->emptyComposition) {
    _parent->updateDurationAndFrameRate();
  }
  onTimelineChanged();
  notifyModified(true);
}

std::shared_ptr<PAGFile> PAGFile::copyOriginal() {
  return MakeFrom(file);
}

bool PAGFile::isPAGFile() const {
  return true;
}

std::vector<int> PAGFile::getEditableIndices(LayerType layerType) {
  switch (layerType) {
    case LayerType::Image:
      if (file->editableImages != nullptr) {
        return *file->editableImages;
      }
      break;
    case LayerType::Text:
      if (file->editableTexts != nullptr) {
        return *file->editableTexts;
      }
      break;
    default:
      break;
  }
  return {};
}

Frame PAGFile::stretchedFrameDuration() const {
  return _stretchedFrameDuration;
}

Frame PAGFile::stretchedContentFrame() const {
  return _stretchedContentFrame;
}

bool PAGFile::gotoTime(int64_t layerTime) {
  _stretchedContentFrame = TimeToFrame(layerTime, frameRateInternal()) - startFrame;
  if (_stretchedFrameDuration == layer->duration) {
    return PAGComposition::gotoTime(layerTime);
  }
  auto fileTime = stretchedTimeToFileTime(layerTime);
  return PAGComposition::gotoTime(fileTime);
}

Frame PAGFile::childFrameToLocal(pag::Frame childFrame, float childFrameRate) const {
  childFrame = PAGComposition::childFrameToLocal(childFrame, childFrameRate);
  if (_stretchedFrameDuration != layer->duration) {
    childFrame = fileFrameToStretchedFrame(childFrame);
  }
  return childFrame;
}

Frame PAGFile::localFrameToChild(pag::Frame localFrame, float childFrameRate) const {
  if (_stretchedFrameDuration != layer->duration) {
    localFrame = stretchedFrameToFileFrame(localFrame);
  }
  return PAGComposition::localFrameToChild(localFrame, childFrameRate);
}

static void HandleTimeStretch_Repeat(int64_t* fileContent, int64_t layerDuration) {
  if (*fileContent >= layerDuration) {
    *fileContent = *fileContent % layerDuration;
  }
}

static void HandleTimeStretch_RepeatInverted(int64_t* fileContent, int64_t layerDuration) {
  if (*fileContent >= layerDuration) {
    auto count = static_cast<int>(ceil(static_cast<double>(*fileContent + 1) / layerDuration));
    *fileContent = *fileContent % layerDuration;
    if (count % 2 == 0) {
      *fileContent = layerDuration - 1 - *fileContent;
    }
  }
}

Frame PAGFile::stretchedFrameToFileFrame(Frame stretchedFrame) const {
  auto fileContentFrame = stretchedFrame - startFrame;
  if (fileContentFrame <= 0) {
    return stretchedFrame;
  }
  auto layerDuration = frameDuration();
  if (fileContentFrame >= _stretchedFrameDuration) {
    return stretchedFrame - _stretchedContentFrame + layerDuration;
  }

  switch (_timeStretchMode) {
    case PAGTimeStretchMode::Scale: {
      if (file->hasScaledTimeRange()) {
        fileContentFrame = scaledFrameToFileFrame(fileContentFrame, file->scaledTimeRange);
      } else {
        auto progress = FrameToProgress(fileContentFrame, _stretchedFrameDuration);
        fileContentFrame = ProgressToFrame(progress, layerDuration);
      }
    } break;
    case PAGTimeStretchMode::Repeat: {
      HandleTimeStretch_Repeat(&fileContentFrame, layerDuration);
    } break;
    case PAGTimeStretchMode::RepeatInverted: {
      HandleTimeStretch_RepeatInverted(&fileContentFrame, layerDuration);
    } break;
    default: {
      if (fileContentFrame >= layerDuration) {
        fileContentFrame = layerDuration - 1;
      }
    } break;
  }
  return startFrame + fileContentFrame;
}

int64_t PAGFile::stretchedTimeToFileTime(int64_t stretchedTime) const {
  auto fileContentTime = stretchedTime - startTimeInternal();
  if (fileContentTime <= 0) {
    return stretchedTime;
  }
  auto layerDuration = FrameToTime(frameDuration(), frameRateInternal());
  auto stretchedTimeDuration = FrameToTime(_stretchedFrameDuration, frameRateInternal());
  if (fileContentTime >= stretchedTimeDuration) {
    return stretchedTime - stretchedTimeDuration + layerDuration;
  }

  switch (_timeStretchMode) {
    case PAGTimeStretchMode::Scale: {
      if (file->hasScaledTimeRange()) {
        auto fileContentFrame = scaledFrameToFileFrame(
            TimeToFrame(fileContentTime, frameRateInternal()), file->scaledTimeRange);
        fileContentTime = FrameToTime(fileContentFrame, frameRateInternal());
      } else {
        auto progress = TimeToProgress(fileContentTime, stretchedTimeDuration);
        fileContentTime = ProgressToTime(progress, layerDuration);
      }
    } break;
    case PAGTimeStretchMode::Repeat: {
      HandleTimeStretch_Repeat(&fileContentTime, layerDuration);
    } break;
    case PAGTimeStretchMode::RepeatInverted: {
      HandleTimeStretch_RepeatInverted(&fileContentTime, layerDuration);
    } break;
    default: {
      if (fileContentTime >= layerDuration) {
        fileContentTime = layerDuration - 1;
      }
    } break;
  }
  return startTimeInternal() + fileContentTime;
}

Frame PAGFile::scaledFrameToFileFrame(Frame scaledFrame, const TimeRange& scaledTimeRange) const {
  if (scaledFrame < scaledTimeRange.start) {
    return scaledFrame;
  }
  auto layerDuration = frameDuration();
  auto minDuration = scaledTimeRange.start + layerDuration - scaledTimeRange.end;
  if (_stretchedFrameDuration <= minDuration) {
    return scaledFrame + scaledTimeRange.end - scaledTimeRange.start;
  }
  auto offsetFrame = _stretchedFrameDuration - layerDuration;
  if (scaledFrame >= scaledTimeRange.end + offsetFrame) {
    return scaledFrame - offsetFrame;
  }
  auto progress = (scaledFrame * 1.0 - scaledTimeRange.start) /
                  static_cast<float>(_stretchedFrameDuration - minDuration - 1);
  auto targetFrame =
      static_cast<Frame>(round(progress * static_cast<double>(layerDuration - minDuration - 1)));
  return targetFrame + scaledTimeRange.start;
}

Frame PAGFile::fileFrameToStretchedFrame(Frame fileFrame) const {
  auto stretchedContentFrame = fileFrame - startFrame;
  if (stretchedContentFrame <= 0) {
    return fileFrame;
  }
  auto layerDuration = frameDuration();
  if (stretchedContentFrame >= layerDuration) {
    return fileFrame + _stretchedFrameDuration - layerDuration;
  }
  if (_timeStretchMode == PAGTimeStretchMode::Scale) {
    if (file->hasScaledTimeRange()) {
      stretchedContentFrame = fileFrameToScaledFrame(stretchedContentFrame, file->scaledTimeRange);
    } else {
      auto progress = FrameToProgress(stretchedContentFrame, layerDuration);
      stretchedContentFrame = ProgressToFrame(progress, _stretchedFrameDuration);
    }
  }
  return startFrame + stretchedContentFrame;
}

Frame PAGFile::fileFrameToScaledFrame(Frame fileFrame, const TimeRange& scaledTimeRange) const {
  if (fileFrame < scaledTimeRange.start) {
    return fileFrame;
  }
  auto layerDuration = frameDuration();
  auto minDuration = scaledTimeRange.start + layerDuration - scaledTimeRange.end;
  if (_stretchedFrameDuration <= minDuration) {
    return fileFrame - scaledTimeRange.end + scaledTimeRange.start;
  }
  if (fileFrame >= scaledTimeRange.end) {
    return fileFrame + _stretchedFrameDuration - layerDuration;
  }
  auto progress = FrameToProgress(fileFrame - scaledTimeRange.start,
                                  scaledTimeRange.end - scaledTimeRange.start);
  auto targetFrame = ProgressToFrame(progress, _stretchedFrameDuration - minDuration);
  return targetFrame + scaledTimeRange.start;
}

void PAGFile::replaceTextInternal(const std::vector<std::shared_ptr<PAGLayer>>& textLayers,
                                  std::shared_ptr<TextDocument> textData) {
  for (auto& pagLayer : textLayers) {
    if (pagLayer->layerType() == LayerType::Text) {
      auto pagTextLayer = std::static_pointer_cast<PAGTextLayer>(pagLayer);
      pagTextLayer->replaceTextInternal(textData);
    }
  }
}

void PAGFile::replaceImageInternal(const std::vector<std::shared_ptr<PAGLayer>>& imageLayers,
                                   std::shared_ptr<PAGImage> image) {
  for (auto& pagLayer : imageLayers) {
    if (pagLayer->layerType() == LayerType::Image) {
      auto pagImageLayer = std::static_pointer_cast<PAGImageLayer>(pagLayer);
      pagImageLayer->setImageInternal(image);
    }
  }
}

}  // namespace pag
