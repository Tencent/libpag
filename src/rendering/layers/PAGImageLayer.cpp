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

#include "base/keyframes/SingleEaseKeyframe.h"
#include "base/utils/TimeUtil.h"
#include "pag/pag.h"
#include "rendering/caches/LayerCache.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/editing/ImageReplacement.h"
#include "rendering/layers/PAGStage.h"
#include "rendering/utils/LockGuard.h"
#include "rendering/utils/ScopedLock.h"

namespace pag {
std::shared_ptr<PAGImageLayer> PAGImageLayer::Make(int width, int height, int64_t duration) {
  if (width <= 0 || height <= 0 || duration <= 0) {
    return nullptr;
  }
  auto imageLayer = std::shared_ptr<PAGImageLayer>(new PAGImageLayer(width, height, duration));
  imageLayer->weakThis = imageLayer;
  return imageLayer;
}

PAGImageLayer::PAGImageLayer(std::shared_ptr<pag::File> file, ImageLayer* layer)
    : PAGLayer(std::move(file), layer) {
}

PAGImageLayer::PAGImageLayer(int width, int height, int64_t duration) : PAGLayer(nullptr, nullptr) {
  emptyImageLayer = new ImageLayer();
  emptyImageLayer->transform = Transform2D::MakeDefault();
  emptyImageLayer->imageBytes = new ImageBytes();
  emptyImageLayer->imageBytes->width = width;
  emptyImageLayer->imageBytes->height = height;
  emptyImageLayer->imageBytes->fileBytes = ByteData::Make(0).release();
  emptyImageLayer->duration = TimeToFrame(duration, 60);

  layer = emptyImageLayer;
  layerCache = LayerCache::Get(layer);
  rootLocker = std::make_shared<std::mutex>();
  contentVersion = 1;
  _editableIndex = 0;
}

PAGImageLayer::~PAGImageLayer() {
  delete replacement;
  if (emptyImageLayer) {
    delete emptyImageLayer->imageBytes;
    delete emptyImageLayer;
  }
}

template <typename T>
Frame CalculateMaxFrame(const std::vector<Keyframe<T>*>& keyframes) {
  Frame maxFrame = 0;
  for (auto& keyframe : keyframes) {
    if (keyframe->startValue > maxFrame) {
      maxFrame = static_cast<Frame>(keyframe->startValue);
    }
    if (keyframe->endValue > maxFrame) {
      maxFrame = static_cast<Frame>(keyframe->endValue);
    }
  }
  return maxFrame;
}

int64_t PAGImageLayer::contentDuration() {
  LockGuard autoLock(rootLocker);
  Frame maxFrame = 0;
  float frameRate = 60;
  if (rootFile) {
    frameRate = rootFile->frameRateInternal();
    auto property = getContentTimeRemap();
    if (!property->animatable()) {
      return 0;
    }
    auto timeRemap = static_cast<AnimatableProperty<float>*>(property);
    maxFrame = CalculateMaxFrame(timeRemap->keyframes);
  } else {
    auto imageFillRule = static_cast<ImageLayer*>(layer)->imageFillRule;
    if (imageFillRule == nullptr || imageFillRule->timeRemap == nullptr ||
        !imageFillRule->timeRemap->animatable()) {
      return FrameToTime(layer->duration, frameRate);
    }
    frameRate = frameRateInternal();
    auto timeRemap = static_cast<AnimatableProperty<Frame>*>(imageFillRule->timeRemap);
    // timeRemap编码过程中是开区间，实际使用过程中是闭区间，计算完后需要-1
    maxFrame = CalculateMaxFrame(timeRemap->keyframes) - 1;
  }

  return FrameToTime(maxFrame + 1, frameRate);
}

class FrameRange {
 public:
  FrameRange(pag::Frame startFrame, pag::Frame endFrame, pag::Frame duration)
      : startFrame(startFrame), endFrame(endFrame), duration(duration) {
  }

  static std::vector<FrameRange> FromTimeRemap(std::vector<Keyframe<Frame>*> keyframes,
                                               Layer* layer) {
    std::vector<FrameRange> frameRanges;
    if (keyframes[0]->startTime > layer->startTime) {
      auto startTime = keyframes[0]->startValue;
      auto duration = keyframes[0]->startTime - layer->startTime;
      frameRanges.emplace_back(startTime, startTime, duration);
    }
    for (auto& keyframe : keyframes) {
      auto startTime = keyframe->startValue;
      auto endTime = keyframe->endValue;
      auto duration = keyframe->endTime - keyframe->startTime;
      frameRanges.emplace_back(startTime, endTime, duration);
    }
    auto keyframe = keyframes.back();
    if (keyframe->endTime < layer->startTime + layer->duration) {
      auto endTime = keyframe->endValue;
      auto duration = layer->startTime + layer->duration - keyframe->endTime;
      frameRanges.emplace_back(endTime, endTime, duration);
    }

    for (auto index = frameRanges.size() - 1; index >= 1; index--) {
      if (frameRanges[index].startFrame == frameRanges[index].endFrame &&
          frameRanges[index - 1].startFrame == frameRanges[index - 1].endFrame &&
          frameRanges[index].startFrame == frameRanges[index - 1].startFrame) {
        frameRanges[index - 1].duration += frameRanges[index].duration;
        frameRanges.erase(frameRanges.begin() + index);
      }
    }

    for (auto index = frameRanges.size() - 1; index >= 1; index--) {
      if (frameRanges[index].duration == 1) {
        frameRanges[index - 1].duration++;
        frameRanges.erase(frameRanges.begin() + index);
      }
    }
    return frameRanges;
  }

  pag::Frame startFrame = 0;
  pag::Frame endFrame = 0;
  pag::Frame duration = 0;
};

std::vector<PAGVideoRange> PAGImageLayer::getVideoRanges() const {
  LockGuard autoLock(rootLocker);
  auto frameRate = frameRateInternal();
  auto imageFillRule = static_cast<ImageLayer*>(layer)->imageFillRule;
  auto timeRemap = (imageFillRule == nullptr) ? nullptr : imageFillRule->timeRemap;
  if (timeRemap == nullptr || !timeRemap->animatable()) {
    auto duration = FrameToTime(layer->duration, frameRate);
    PAGVideoRange videoRange = {0, duration, duration};
    return {videoRange};
  }

  auto keyframes = static_cast<AnimatableProperty<Frame>*>(timeRemap)->keyframes;
  auto frameRanges = FrameRange::FromTimeRemap(keyframes, layer);

  std::vector<PAGVideoRange> ranges;
  for (auto frameRange : frameRanges) {
    auto startTime = FrameToTime(frameRange.startFrame, frameRate);
    auto endTime = FrameToTime(frameRange.endFrame, frameRate);
    auto duration = FrameToTime(frameRange.duration, frameRate);
    ranges.emplace_back(startTime, endTime, duration);
  }
  return ranges;
}

bool PAGImageLayer::gotoTime(int64_t layerTime) {
  auto changed = PAGLayer::gotoTime(layerTime);
  if (replacement != nullptr) {
    auto pagImage = replacement->getImage();
    if (!pagImage->isStill() && pagImage->getOwner() == this) {
      auto contentTime = getCurrentContentTime(layerTime);
      if (pagImage->setContentTime(contentTime)) {
        changed = true;
      }
    }
  }
  return changed;
}

void PAGImageLayer::replaceImage(std::shared_ptr<pag::PAGImage> image) {
  LockGuard autoLock(rootLocker);
  if (rootFile != nullptr) {
    rootFile->replaceImageInternal(_editableIndex, image);
    if (image != nullptr) {
      image->setOwner(this);
    }
  } else {
    setImageInternal(image);
  }
}

void PAGImageLayer::setImage(std::shared_ptr<PAGImage> image) {
  LockGuard autoLock(rootLocker);
  setImageInternal(image);
}

void PAGImageLayer::setImageInternal(std::shared_ptr<PAGImage> image) {
  if (stage) {
    if (replacement != nullptr) {
      auto oldPAGImage = replacement->getImage();
      stage->removeReference(oldPAGImage.get(), this);
    }
    if (image) {
      stage->addReference(image.get(), this);
    }
  }
  delete replacement;
  if (image != nullptr) {
    replacement = new ImageReplacement(static_cast<ImageLayer*>(layer), image);
    image->setOwner(this);
  } else {
    replacement = nullptr;
  }
  notifyModified(true);
  invalidateCacheScale();
}

Content* PAGImageLayer::getContent() {
  return hasPAGImage() ? replacement : layerCache->getContent(contentFrame);
}

bool PAGImageLayer::contentModified() const {
  return hasPAGImage();
}

bool PAGImageLayer::cacheFilters() const {
  return layerCache->cacheFilters() && !hasPAGImage();
}

void PAGImageLayer::onRemoveFromRootFile() {
  PAGLayer::onRemoveFromRootFile();
  contentTimeRemap = nullptr;
}

void PAGImageLayer::onTimelineChanged() {
  contentTimeRemap = nullptr;
}

bool PAGImageLayer::contentVisible() {
  if (rootFile) {
    auto timeRange = getVisibleRangeInFile();
    auto stretchedContentFrame = rootFile->stretchedContentFrame();
    return timeRange.start <= stretchedContentFrame && stretchedContentFrame <= timeRange.end;
  } else {
    auto frame = TimeToFrame(contentDuration(), frameRateInternal());
    return 0 <= contentFrame && contentFrame < frame;
  }
}

int64_t PAGImageLayer::getCurrentContentTime(int64_t layerTime) {
  int64_t replacementTime = 0;
  if (rootFile) {
    if (contentVisible()) {
      auto timeRemap = getContentTimeRemap();
      auto replacementFrame = timeRemap->getValueAt(rootFile->stretchedContentFrame());
      replacementTime =
          static_cast<Frame>(ceil(replacementFrame * 1000000.0 / rootFile->frameRateInternal()));
    } else {
      replacementTime =
          FrameToTime(rootFile->stretchedContentFrame(), rootFile->frameRateInternal());
    }
  } else {
    replacementTime = layerTime - startTimeInternal();
  }
  return replacementTime;
}

static Keyframe<float>* CreateKeyframe(float startTime, float endTime, float startValue,
                                       float endValue) {
  auto newKeyframe = new SingleEaseKeyframe<float>();
  newKeyframe->startTime = startTime;
  newKeyframe->endTime = endTime;
  newKeyframe->startValue = startValue;
  newKeyframe->endValue = endValue;
  return newKeyframe;
}

static void CutKeyframe(Keyframe<float>* keyframe, Frame position, bool cutLeft) {
  if (keyframe->interpolationType == KeyframeInterpolationType::Bezier) {
    auto t = static_cast<double>(position - keyframe->startTime) /
             static_cast<double>(keyframe->endTime - keyframe->startTime);
    auto p1 = Interpolate(Point::Zero(), keyframe->bezierOut[0], t);
    auto bc = Interpolate(keyframe->bezierOut[0], keyframe->bezierIn[0], t);
    auto p5 = Interpolate(keyframe->bezierIn[0], Point::Make(1, 1), t);
    auto p2 = Interpolate(p1, bc, t);
    auto p4 = Interpolate(bc, p5, t);
    if (cutLeft) {
      keyframe->bezierOut[0] = p4;
      keyframe->bezierIn[0] = p5;
    } else {
      keyframe->bezierOut[0] = p1;
      keyframe->bezierIn[0] = p2;
    }
  }
  auto midValue = keyframe->getValueAt(position);
  if (cutLeft) {
    keyframe->startValue = midValue;
    keyframe->startTime = position;
  } else {
    keyframe->endValue = midValue;
    keyframe->endTime = position;
  }
}

bool PAGImageLayer::hasPAGImage() const {
  return replacement != nullptr;
}

std::shared_ptr<PAGImage> PAGImageLayer::getPAGImage() const {
  if (replacement == nullptr) {
    return nullptr;
  }
  return replacement->getImage();
}

// 输出的时间轴不包含startTime
std::unique_ptr<AnimatableProperty<float>> PAGImageLayer::copyContentTimeRemap() {
  std::vector<Keyframe<float>*> keyframes = {};
  auto imageFillRule = static_cast<ImageLayer*>(layer)->imageFillRule;
  if (imageFillRule == nullptr || imageFillRule->timeRemap == nullptr ||
      !imageFillRule->timeRemap->animatable()) {
    auto newKeyframe = CreateKeyframe(layer->startTime, layer->startTime + layer->duration - 1,
                                      layer->startTime, layer->startTime + layer->duration - 1);
    keyframes.push_back(newKeyframe);
    newKeyframe->interpolationType = KeyframeInterpolationType::Linear;

  } else {
    auto timeRemap = static_cast<AnimatableProperty<Frame>*>(imageFillRule->timeRemap);
    for (auto& keyFrame : timeRemap->keyframes) {
      Keyframe<float>* newKeyframe = nullptr;
      if (keyFrame->interpolationType == KeyframeInterpolationType::Hold) {
        newKeyframe = new Keyframe<float>();
      } else {
        newKeyframe = new SingleEaseKeyframe<float>();
      }
      newKeyframe->startValue = static_cast<float>(keyFrame->startValue);
      newKeyframe->endValue = static_cast<float>(keyFrame->endValue);
      newKeyframe->startTime = keyFrame->startTime;
      newKeyframe->endTime = keyFrame->endTime;
      newKeyframe->interpolationType = keyFrame->interpolationType;
      newKeyframe->bezierIn.assign(keyFrame->bezierIn.begin(), keyFrame->bezierIn.end());
      newKeyframe->bezierOut.assign(keyFrame->bezierOut.begin(), keyFrame->bezierOut.end());
      newKeyframe->spatialIn = keyFrame->spatialIn;
      newKeyframe->spatialOut = keyFrame->spatialOut;
      keyframes.push_back(newKeyframe);
    }
  }

  return std::unique_ptr<AnimatableProperty<float>>(new AnimatableProperty<float>(keyframes));
}

Property<float>* PAGImageLayer::getContentTimeRemap() {
  if (contentTimeRemap == nullptr && rootFile) {
    auto visibleRange = getVisibleRangeInFile();
    if (visibleRange.start == visibleRange.end || visibleRange.end < 0 ||
        visibleRange.start > rootFile->stretchedFrameDuration() - 1) {
      // 在File层级不可见
      auto property = new Property<float>();
      property->value = 0;
      contentTimeRemap = std::unique_ptr<Property<float>>(property);
    } else {
      auto property = copyContentTimeRemap();
      if (property != nullptr) {
        for (auto& keyFrame : property->keyframes) {
          keyFrame->startTime -= layer->startTime;
          keyFrame->endTime -= layer->startTime;
        }
      }
      double frameScale =
          static_cast<double>(visibleRange.end - visibleRange.start + 1) / frameDuration();
      BuildContentTimeRemap(property.get(), rootFile, visibleRange, frameScale);
      contentTimeRemap = std::move(property);
    }
  }
  return contentTimeRemap.get();
}

Frame PAGImageLayer::ScaleTimeRemap(AnimatableProperty<float>* property,
                                    const TimeRange& visibleRange, double frameScale,
                                    Frame fileEndFrame) {
  float minValue = FLT_MAX;
  float maxValue = 0;
  auto& keyframes = property->keyframes;
  for (int i = static_cast<int>(keyframes.size() - 1); i >= 0; i--) {
    auto& keyframe = keyframes[i];
    keyframe->startTime =
        visibleRange.start + static_cast<Frame>(round(keyframe->startTime * frameScale));
    auto duration = keyframe->endTime - keyframe->startTime + 1;
    // 由于是闭区间，计算完后需要-1
    keyframe->endTime = visibleRange.start +
                        static_cast<Frame>(round((keyframe->startTime + duration) * frameScale)) -
                        1;
    keyframe->startValue = static_cast<Frame>(round(keyframe->startValue * frameScale));
    auto valueLength = keyframe->endValue - keyframe->startValue + 1;
    // 由于是闭区间，计算完后需要-1
    keyframe->endValue =
        static_cast<Frame>(round((keyframe->startValue + valueLength) * frameScale)) - 1;
    // 由于startTime和endTime都是帧号，因此当startTime == fileEndFrame 和 endTime == 0都有意义
    if (keyframe->startTime > fileEndFrame) {
      delete keyframe;
      keyframes.erase(keyframes.begin() + i);
      continue;
    }
    if (keyframe->endTime < 0) {
      delete keyframe;
      keyframes.erase(keyframes.begin() + i);
      continue;
    }
    if (keyframe->endTime > fileEndFrame) {
      CutKeyframe(keyframe, fileEndFrame, false);
    }
    if (keyframe->startTime < 0) {
      CutKeyframe(keyframe, 0, true);
    }
    minValue = std::floor(std::min(keyframe->startValue, minValue));
    minValue = std::floor(std::min(keyframe->endValue, minValue));
    maxValue = std::floor(std::max(keyframe->startValue, maxValue));
    maxValue = std::floor(std::max(keyframe->endValue, maxValue));
  }
  for (auto& keyframe : keyframes) {
    keyframe->startValue -= minValue;
    keyframe->endValue -= minValue;
  }
  // 当keyframes所有的区间都在File的显示区间之外时，补一个keyFrame，防止后续计算错误。
  if (keyframes.size() == 0) {
    auto newKeyframe = CreateKeyframe(visibleRange.start, visibleRange.end, 0,
                                      visibleRange.end - visibleRange.start);
    keyframes.push_back(newKeyframe);
    newKeyframe->initialize();
    maxValue = visibleRange.end - visibleRange.start;
    minValue = 0;
  }
  return (maxValue - minValue + 1);
}

// 输出的时间轴不包含startTime
void PAGImageLayer::BuildContentTimeRemap(AnimatableProperty<float>* property, PAGFile* fileOwner,
                                          const TimeRange& visibleRange, double frameScale) {
  auto fileDuration = fileOwner->frameDuration();
  auto fileStretchedDuration = fileOwner->stretchedFrameDuration();
  auto hasRepeat = (fileOwner->_timeStretchMode == PAGTimeStretchMode::Repeat ||
                    fileOwner->_timeStretchMode == PAGTimeStretchMode::RepeatInverted) &&
                   (fileDuration < fileStretchedDuration);
  auto fileEndFrame = hasRepeat ? fileDuration - 1 : fileStretchedDuration - 1;
  auto scaleDuration = ScaleTimeRemap(property, visibleRange, frameScale, fileEndFrame);
  auto& keyframes = property->keyframes;
  // 补齐头尾的空缺。
  auto firstKeyframe = keyframes.front();
  if (firstKeyframe->startTime > 0) {
    auto newKeyframe = CreateKeyframe(0, firstKeyframe->startTime, firstKeyframe->startValue,
                                      firstKeyframe->startValue);
    keyframes.insert(keyframes.begin(), newKeyframe);
    newKeyframe->initialize();
  }
  auto lastKeyframe = keyframes.back();
  if (lastKeyframe->endTime < fileEndFrame) {
    auto newKeyframe = CreateKeyframe(lastKeyframe->endTime, fileEndFrame, lastKeyframe->endValue,
                                      lastKeyframe->endValue);
    keyframes.push_back(newKeyframe);
    newKeyframe->initialize();
  }
  if (hasRepeat) {
    // 处理file存在repeat拉伸的情况。
    ExpandPropertyByRepeat(property, fileOwner, scaleDuration);
  }
}

TimeRange PAGImageLayer::getVisibleRangeInFile() {
  TimeRange timeRange = {};
  timeRange.start = startFrame;
  timeRange.end = startFrame + frameDuration() - 1;
  auto parent = _parent;
  auto childFrameRate = frameRateInternal();
  while (parent) {
    timeRange.start = parent->childFrameToLocal(timeRange.start, childFrameRate);
    timeRange.end = parent->childFrameToLocal(timeRange.end, childFrameRate);
    if (parent == rootFile) {
      break;
    }
    childFrameRate = parent->frameRateInternal();
    parent = parent->_parent;
  }
  timeRange.start -= rootFile->startFrame;
  timeRange.end -= rootFile->startFrame;
  if (timeRange.start > timeRange.end) {
    std::swap(timeRange.start, timeRange.end);
  }
  return timeRange;
}

void PAGImageLayer::ExpandPropertyByRepeat(AnimatableProperty<float>* property, PAGFile* fileOwner,
                                           Frame contentDuration) {
  std::vector<Keyframe<float>*> newKeyframes = {};
  auto fileStretchedDuration = fileOwner->stretchedFrameDuration();
  auto fileDuration = fileOwner->frameDuration();
  int count = 1;
  bool end = false;
  while (!end) {
    for (auto& keyframe : property->keyframes) {
      Keyframe<float>* newKeyframe = new SingleEaseKeyframe<float>();
      *newKeyframe = *keyframe;
      newKeyframe->startTime += fileDuration * count;
      newKeyframe->endTime += fileDuration * count;
      newKeyframe->startValue += contentDuration * count;
      newKeyframe->endValue += contentDuration * count;
      newKeyframes.push_back(newKeyframe);
      newKeyframe->initialize();
      if (newKeyframe->endTime > fileStretchedDuration - 1) {
        CutKeyframe(newKeyframe, fileStretchedDuration - 1, false);
      }
      if (newKeyframe->endTime == fileStretchedDuration - 1) {
        end = true;
        break;
      }
    }
    count++;
  }
  for (auto& keyframe : newKeyframes) {
    property->keyframes.push_back(keyframe);
  }
}

int64_t PAGImageLayer::contentTimeToLayer(int64_t replacementTime) {
  LockGuard autoLock(rootLocker);
  if (rootFile == nullptr) {
    return replacementTime;
  }
  auto replacementFrame = TimeToFrame(replacementTime, rootFile->frameRateInternal());
  auto fileFrame = getFrameFromTimeRemap(replacementFrame);
  auto localFrame = fileFrameToLocalFrame(fileFrame);
  if (localFrame > startFrame + stretchedFrameDuration()) {
    localFrame = startFrame + stretchedFrameDuration();
  }
  if (localFrame < startFrame) {
    localFrame = startFrame;
  }
  return FrameToTime(localFrame, frameRateInternal());
}

int64_t PAGImageLayer::layerTimeToContent(int64_t layerTime) {
  LockGuard autoLock(rootLocker);
  if (rootFile == nullptr) {
    return layerTime;
  }
  auto localFrame = TimeToFrame(layerTime, frameRateInternal());
  auto fileFrame = localFrameToFileFrame(localFrame);
  auto timeRemap = getContentTimeRemap();
  auto replacementFrame = timeRemap->getValueAt(fileFrame);
  return FrameToTime(replacementFrame, rootFile->frameRateInternal());
}

int64_t PAGImageLayer::localFrameToFileFrame(int64_t localFrame) const {
  if (rootFile == nullptr) {
    return localFrame;
  }
  auto owner = getTimelineOwner();
  auto childFrameRate = frameRateInternal();
  while (owner != nullptr) {
    localFrame = owner->childFrameToLocal(localFrame, childFrameRate);
    childFrameRate = owner->frameRateInternal();
    if (owner == rootFile) {
      break;
    }
    owner = owner->getTimelineOwner();
  }
  return localFrame;
}

int64_t PAGImageLayer::fileFrameToLocalFrame(int64_t fileFrame) const {
  std::vector<PAGLayer*> list = {};
  auto owner = getTimelineOwner();
  while (owner != nullptr) {
    list.push_back(owner);
    if (owner == rootFile) {
      break;
    }
    owner = owner->getTimelineOwner();
  }
  for (int i = static_cast<int>(list.size() - 1); i >= 0; i--) {
    auto childFrameRate = i > 0 ? list[i - 1]->frameRateInternal() : frameRateInternal();
    fileFrame = list[i]->localFrameToChild(fileFrame, childFrameRate);
  }
  return fileFrame;
}

Frame GetFrameFromBezierTimeRemap(Frame value, Keyframe<float>* frame,
                                  pag::AnimatableProperty<float>* property) {
  auto current = (frame->startTime + frame->endTime) / 2;
  auto start = frame->startTime;
  auto end = frame->endTime;
  // 二分查找
  while (start <= end) {
    auto currentValue = static_cast<Frame>(property->getValueAt(current));
    if (currentValue > value) {
      end = current - 1;
    } else if (currentValue < value) {
      start = current + 1;
    } else {
      break;
    }
    current = (start + end) / 2;
  }
  return current;
}

Frame PAGImageLayer::getFrameFromTimeRemap(pag::Frame value) {
  auto timeRemap = getContentTimeRemap();
  if (!timeRemap->animatable()) {
    return 0;
  }
  Frame result = 0;
  auto property = static_cast<pag::AnimatableProperty<float>*>(timeRemap);
  for (auto frame : property->keyframes) {
    if (frame->startValue > value) {
      break;
    } else if (frame->endValue <= value) {
      result = frame->endTime;
    } else {
      switch (frame->interpolationType) {
        case pag::KeyframeInterpolationType::Linear: {
          auto scale =
              (frame->endTime - frame->startTime) * 1.0 / (frame->endValue - frame->startValue);
          result = frame->startTime + ceil((value - frame->startValue) * scale);
        } break;
        case pag::KeyframeInterpolationType::Hold:
          result = frame->endTime;
          break;
        case pag::KeyframeInterpolationType::Bezier:
          result = GetFrameFromBezierTimeRemap(value, frame, property);
          break;
        default:
          break;
      }
      break;
    }
  }
  return result;
}

// ImageBytesV3场景下，图片的透明区域会发生裁剪，不能从图片解码中获取宽高
void PAGImageLayer::measureBounds(tgfx::Rect* bounds) {
  auto imageLayer = static_cast<ImageLayer*>(layer);
  bounds->setWH(imageLayer->imageBytes->width, imageLayer->imageBytes->height);
}

ByteData* PAGImageLayer::imageBytes() const {
  auto imageLayer = static_cast<ImageLayer*>(layer);
  if (imageLayer->imageBytes) {
    return imageLayer->imageBytes->fileBytes;
  }
  return nullptr;
}

}  // namespace pag
