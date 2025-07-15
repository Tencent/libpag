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

#include "base/utils/MatrixUtil.h"
#include "base/utils/TimeUtil.h"
#include "base/utils/UniqueID.h"
#include "pag/pag.h"
#include "rendering/caches/LayerCache.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/layers/PAGStage.h"
#include "rendering/renderers/TrackMatteRenderer.h"
#include "rendering/utils/LockGuard.h"
#include "rendering/utils/ScopedLock.h"

namespace pag {
PAGLayer::PAGLayer(std::shared_ptr<File> file, Layer* layer)
    : layer(layer), file(std::move(file)), _uniqueID(UniqueID::Next()) {
  layerMatrix.setIdentity();
  if (layer != nullptr) {  // could be nullptr.
    layerCache = LayerCache::Get(layer);
    layerVisible = layer->isActive;
    startFrame = layer->startTime;
  }
}

PAGLayer::~PAGLayer() {
  if (_trackMatteLayer) {
    _trackMatteLayer->detachFromTree();
    _trackMatteLayer->trackMatteOwner = nullptr;
  }
}

uint32_t PAGLayer::uniqueID() const {
  return _uniqueID;
}

LayerType PAGLayer::layerType() const {
  return layer->type();
}

std::string PAGLayer::layerName() const {
  return layer->name;
}

Matrix PAGLayer::matrix() const {
  LockGuard autoLock(rootLocker);
  return layerMatrix;
}

void PAGLayer::setMatrix(const Matrix& value) {
  LockGuard autoLock(rootLocker);
  setMatrixInternal(value);
}

void PAGLayer::resetMatrix() {
  LockGuard autoLock(rootLocker);
  setMatrixInternal(Matrix::I());
}

Matrix PAGLayer::getTotalMatrix() {
  LockGuard autoLock(rootLocker);
  return getTotalMatrixInternal();
}

Matrix PAGLayer::getTotalMatrixInternal() {
  auto matrix = ToPAG(layerCache->getTransform(contentFrame)->matrix);
  matrix.postConcat(layerMatrix);
  return matrix;
}

float PAGLayer::alpha() const {
  LockGuard autoLock(rootLocker);
  return layerAlpha;
}

void PAGLayer::setAlpha(float alpha) {
  LockGuard autoLock(rootLocker);
  if (alpha == layerAlpha) {
    return;
  }
  layerAlpha = alpha;
  notifyModified();
}

bool PAGLayer::visible() const {
  LockGuard autoLock(rootLocker);
  return layerVisible;
}

void PAGLayer::setVisible(bool value) {
  LockGuard autoLock(rootLocker);
  setVisibleInternal(value);
}

void PAGLayer::setVisibleInternal(bool value) {
  if (value == layerVisible) {
    return;
  }
  layerVisible = value;
  notifyModified();
}

Rect PAGLayer::getBounds() {
  LockGuard autoLock(rootLocker);
  Rect bounds = {};
  measureBounds(ToTGFX(&bounds));
  return bounds;
}

int PAGLayer::editableIndex() const {
  return _editableIndex;
}

std::shared_ptr<PAGComposition> PAGLayer::parent() const {
  LockGuard autoLock(rootLocker);
  if (_parent) {
    return std::static_pointer_cast<PAGComposition>(_parent->weakThis.lock());
  }
  return nullptr;
}

std::vector<const Marker*> PAGLayer::markers() const {
  std::vector<const Marker*> result = {};
  for (auto marker : layer->markers) {
    result.push_back(marker);
  }
  return result;
}

int64_t PAGLayer::localTimeToGlobal(int64_t localTime) const {
  LockGuard autoLock(rootLocker);
  auto localFrame = TimeToFrame(localTime, frameRateInternal());
  auto globalFrame = localFrameToGlobal(localFrame);
  auto globalLayer = this;
  while (globalLayer) {
    auto owner = globalLayer->getTimelineOwner();
    if (!owner) {
      break;
    }
    globalLayer = owner;
  }
  return FrameToTime(globalFrame, globalLayer->frameRateInternal());
}

Frame PAGLayer::localFrameToGlobal(Frame localFrame) const {
  auto parent = getTimelineOwner();
  auto childFrameRate = frameRateInternal();
  while (parent) {
    localFrame = parent->childFrameToLocal(localFrame, childFrameRate);
    childFrameRate = parent->frameRateInternal();
    parent = parent->getTimelineOwner();
  }
  return localFrame;
}

int64_t PAGLayer::globalToLocalTime(int64_t globalTime) const {
  LockGuard autoLock(rootLocker);
  auto globalLayer = this;
  while (globalLayer) {
    auto owner = globalLayer->getTimelineOwner();
    if (!owner) {
      break;
    }
    globalLayer = owner;
  }
  auto globalFrame = TimeToFrame(globalTime, globalLayer->frameRateInternal());
  auto localFrame = globalToLocalFrame(globalFrame);
  return FrameToTime(localFrame, frameRateInternal());
}

Frame PAGLayer::globalToLocalFrame(Frame globalFrame) const {
  std::vector<PAGLayer*> list = {};
  auto owner = getTimelineOwner();
  while (owner) {
    list.push_back(owner);
    owner = owner->getTimelineOwner();
  }
  for (int i = static_cast<int>(list.size() - 1); i >= 0; i--) {
    auto childFrameRate = i > 0 ? list[i - 1]->frameRateInternal() : frameRateInternal();
    globalFrame = list[i]->localFrameToChild(globalFrame, childFrameRate);
  }
  return globalFrame;
}

Frame PAGLayer::localFrameToChild(Frame localFrame, float childFrameRate) const {
  auto timeScale = childFrameRate / frameRateInternal();
  return static_cast<Frame>(roundf((localFrame - startFrame) * timeScale));
}

Frame PAGLayer::childFrameToLocal(Frame childFrame, float childFrameRate) const {
  auto timeScale = frameRateInternal() / childFrameRate;
  return static_cast<Frame>(roundf(childFrame * timeScale)) + startFrame;
}

PAGLayer* PAGLayer::getTimelineOwner() const {
  if (_parent) {
    return _parent;
  }
  if (trackMatteOwner) {
    return trackMatteOwner->_parent;
  }
  return nullptr;
}

int64_t PAGLayer::startTime() const {
  LockGuard autoLock(rootLocker);
  return startTimeInternal();
}

int64_t PAGLayer::startTimeInternal() const {
  return FrameToTime(startFrame, frameRateInternal());
}

void PAGLayer::setStartTime(int64_t time) {
  LockGuard autoLock(rootLocker);
  setStartTimeInternal(time);
}

void PAGLayer::setStartTimeInternal(int64_t time) {
  auto targetStartFrame = TimeToFrame(time, frameRateInternal());
  if (startFrame == targetStartFrame) {
    return;
  }
  auto layerFrame = startFrame + contentFrame;
  startFrame = targetStartFrame;
  if (_parent && _parent->emptyComposition) {
    _parent->updateDurationAndFrameRate();
  }
  gotoTimeAndNotifyChanged(FrameToTime(layerFrame, frameRateInternal()));
  onTimelineChanged();
}

int64_t PAGLayer::duration() const {
  LockGuard autoLock(rootLocker);
  return durationInternal();
}

int64_t PAGLayer::durationInternal() const {
  return FrameToTime(stretchedFrameDuration(), frameRateInternal());
}

float PAGLayer::frameRate() const {
  LockGuard autoLock(rootLocker);
  return frameRateInternal();
}

float PAGLayer::frameRateInternal() const {
  return file ? file->frameRate() : 60;
}

int64_t PAGLayer::currentTime() const {
  LockGuard autoLock(rootLocker);
  return currentTimeInternal();
}

int64_t PAGLayer::currentTimeInternal() const {
  return FrameToTime(currentFrameInternal(), frameRateInternal());
}

void PAGLayer::setCurrentTime(int64_t time) {
  LockGuard autoLock(rootLocker);
  setCurrentTimeInternal(time);
}

bool PAGLayer::setCurrentTimeInternal(int64_t time) {
  return gotoTimeAndNotifyChanged(time);
}

Frame PAGLayer::currentFrameInternal() const {
  return startFrame + stretchedContentFrame();
}

double PAGLayer::getProgress() {
  LockGuard autoLock(rootLocker);
  return getProgressInternal();
}

double PAGLayer::getProgressInternal() {
  return FrameToProgress(stretchedContentFrame(), stretchedFrameDuration());
}

void PAGLayer::setProgress(double percent) {
  LockGuard autoLock(rootLocker);
  setProgressInternal(percent);
}

void PAGLayer::setProgressInternal(double percent) {
  gotoTimeAndNotifyChanged(startTimeInternal() + ProgressToTime(percent, durationInternal()));
}

void PAGLayer::preFrame() {
  LockGuard autoLock(rootLocker);
  preFrameInternal();
}

void PAGLayer::preFrameInternal() {
  auto totalFrames = stretchedFrameDuration();
  if (totalFrames <= 1) {
    return;
  }
  auto targetContentFrame = stretchedContentFrame();
  targetContentFrame--;
  if (targetContentFrame < 0) {
    targetContentFrame = totalFrames - 1;
  }
  gotoTimeAndNotifyChanged(FrameToTime(startFrame + targetContentFrame, frameRateInternal()));
}

void PAGLayer::nextFrame() {
  LockGuard autoLock(rootLocker);
  nextFrameInternal();
}

void PAGLayer::nextFrameInternal() {
  auto totalFrames = stretchedFrameDuration();
  if (totalFrames <= 1) {
    return;
  }
  auto targetContentFrame = stretchedContentFrame();
  targetContentFrame++;
  if (targetContentFrame >= totalFrames) {
    targetContentFrame = 0;
  }
  gotoTimeAndNotifyChanged(FrameToTime(startFrame + targetContentFrame, frameRateInternal()));
}

Frame PAGLayer::frameDuration() const {
  return layer->duration;
}

Frame PAGLayer::stretchedFrameDuration() const {
  return frameDuration();
}

Frame PAGLayer::stretchedContentFrame() const {
  return contentFrame;
}

bool PAGLayer::gotoTimeAndNotifyChanged(int64_t targetTime) {
  auto changed = gotoTime(targetTime);
  if (changed) {
    notifyModified();
  }
  return changed;
}

std::shared_ptr<PAGLayer> PAGLayer::trackMatteLayer() const {
  return _trackMatteLayer;
}

Point PAGLayer::globalToLocalPoint(float stageX, float stageY) {
  Matrix totalMatrix = Matrix::I();
  auto pagLayer = this;
  while (pagLayer) {
    auto matrix = pagLayer->getTotalMatrixInternal();
    totalMatrix.postConcat(matrix);
    pagLayer = pagLayer->_parent;
  }
  Point localPoint = {stageX, stageY};
  MapPointInverted(ToTGFX(totalMatrix), ToTGFX(&localPoint));
  return localPoint;
}

bool PAGLayer::excludedFromTimeline() const {
  LockGuard autoLock(rootLocker);
  return _excludedFromTimeline;
}

void PAGLayer::setExcludedFromTimeline(bool value) {
  LockGuard autoLock(rootLocker);
  _excludedFromTimeline = value;
}

void PAGLayer::notifyModified(bool contentChanged) {
  if (contentChanged) {
    contentVersion++;
  }
  auto parentLayer = getParentOrOwner();
  while (parentLayer) {
    parentLayer->contentVersion++;
    parentLayer = parentLayer->getParentOrOwner();
  }
}

void PAGLayer::notifyAudioModified() {
  audioVersion++;
  auto parentLayer = getParentOrOwner();
  while (parentLayer) {
    parentLayer->audioVersion++;
    parentLayer = parentLayer->getParentOrOwner();
  }
}

PAGLayer* PAGLayer::getParentOrOwner() const {
  if (_parent) {
    return _parent;
  }
  if (trackMatteOwner) {
    return trackMatteOwner;
  }
  return nullptr;
}

bool PAGLayer::contentModified() const {
  return contentVersion > 0;
}

bool PAGLayer::cacheFilters() const {
  return layerCache->cacheFilters();
}

const Layer* PAGLayer::getLayer() const {
  return layer;
}

const PAGStage* PAGLayer::getStage() const {
  return stage;
}

bool PAGLayer::gotoTime(int64_t layerTime) {
  auto changed = false;
  if (_trackMatteLayer != nullptr) {
    changed = _trackMatteLayer->gotoTime(layerTime);
  }
  auto layerFrame = TimeToFrame(layerTime, frameRateInternal());
  auto oldContentFrame = contentFrame;
  contentFrame = layerFrame - startFrame;
  if (!changed) {
    changed = layerCache->checkFrameChanged(contentFrame, oldContentFrame);
  }
  return changed;
}

void PAGLayer::draw(Recorder* recorder) {
  getContent()->draw(recorder);
}

void PAGLayer::measureBounds(tgfx::Rect* bounds) {
  getContent()->measureBounds(bounds);
}

bool PAGLayer::isPAGFile() const {
  return false;
}

Content* PAGLayer::getContent() {
  return layerCache->getContent(contentFrame);
}

void PAGLayer::invalidateCacheScale() {
  if (stage) {
    stage->invalidateCacheScale(this);
  }
}

void PAGLayer::onAddToStage(PAGStage* pagStage) {
  stage = pagStage;
  pagStage->addReference(this);
  if (_trackMatteLayer != nullptr) {
    _trackMatteLayer->onAddToStage(pagStage);
  }
}

void PAGLayer::onRemoveFromStage() {
  stage->removeReference(this);
  stage = nullptr;
  if (_trackMatteLayer != nullptr) {
    _trackMatteLayer->onRemoveFromStage();
  }
}

void PAGLayer::onAddToRootFile(PAGFile* pagFile) {
  if (_trackMatteLayer != nullptr && _trackMatteLayer->file == file) {
    _trackMatteLayer->onAddToRootFile(pagFile);
  }
  rootFile = pagFile;
}

void PAGLayer::onRemoveFromRootFile() {
  if (_trackMatteLayer != nullptr && _trackMatteLayer->file == file) {
    _trackMatteLayer->onRemoveFromRootFile();
  }
  rootFile = nullptr;
}

void PAGLayer::onTimelineChanged() {
  notifyAudioModified();
}

void PAGLayer::updateRootLocker(std::shared_ptr<std::mutex> newLocker) {
  if (_trackMatteLayer != nullptr) {
    _trackMatteLayer->updateRootLocker(newLocker);
  }
  rootLocker = newLocker;
}

void PAGLayer::setMatrixInternal(const Matrix& matrix) {
  if (matrix == layerMatrix) {
    return;
  }
  layerMatrix = matrix;
  notifyModified();
  invalidateCacheScale();
}

void PAGLayer::removeFromParentOrOwner() {
  if (_parent) {
    auto oldIndex = _parent->getLayerIndexInternal(weakThis.lock());
    if (oldIndex >= 0) {
      _parent->doRemoveLayer(oldIndex);
    }
  }
  if (trackMatteOwner) {
    detachFromTree();
    trackMatteOwner->_trackMatteLayer = nullptr;
    trackMatteOwner = nullptr;
  }
}

void PAGLayer::attachToTree(std::shared_ptr<std::mutex> newLocker, PAGStage* newStage) {
  updateRootLocker(newLocker);
  if (newStage) {
    onAddToStage(newStage);
  }
}

void PAGLayer::detachFromTree() {
  if (stage) {
    onRemoveFromStage();
  }
  auto locker = std::make_shared<std::mutex>();
  updateRootLocker(locker);
}

bool PAGLayer::getTransform(Transform* transform) {
  if (contentFrame < 0 || contentFrame >= frameDuration() || !layerMatrix.invertible() ||
      layerAlpha == 0.0f) {
    return false;
  }
  auto layerTransform = layerCache->getTransform(contentFrame);
  if (!layerTransform->visible()) {
    return false;
  }
  *transform = *layerTransform;
  transform->matrix.postConcat(ToTGFX(layerMatrix));
  transform->alpha *= layerAlpha;
  return true;
}

std::shared_ptr<File> PAGLayer::getFile() const {
  return file;
}

bool PAGLayer::frameVisible() const {
  return contentFrame >= 0 && contentFrame < frameDuration();
}

}  // namespace pag
