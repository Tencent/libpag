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

#include "base/utils/TimeUtil.h"
#include "pag/file.h"
#include "rendering/Drawable.h"
#include "rendering/FileReporter.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/layers/PAGStage.h"
#include "rendering/utils/ApplyScaleMode.h"
#include "rendering/utils/LockGuard.h"
#include "rendering/utils/ScopedLock.h"
#include "tgfx/core/Clock.h"

namespace pag {
PAGPlayer::PAGPlayer() {
  stage = PAGStage::Make(0, 0);
  rootLocker = stage->rootLocker;
  renderCache = new RenderCache(stage.get());
}

PAGPlayer::~PAGPlayer() {
  delete renderCache;
  setSurface(nullptr);
  stage->removeAllLayers();
  delete reporter;
}

std::shared_ptr<PAGComposition> PAGPlayer::getComposition() {
  LockGuard autoLock(rootLocker);
  return stage->getRootComposition();
}

void PAGPlayer::setComposition(std::shared_ptr<PAGComposition> newComposition) {
  LockGuard autoLock(rootLocker);
  auto pagComposition = stage->getRootComposition();
  if (pagComposition == newComposition) {
    return;
  }
  if (pagComposition) {
    auto index = stage->getLayerIndexInternal(pagComposition);
    if (index >= 0) {
      stage->doRemoveLayer(index);
    }
    delete reporter;
    reporter = nullptr;
  }
  pagComposition = newComposition;
  if (pagComposition) {
    stage->doAddLayer(pagComposition, 0);
    reporter = FileReporter::Make(pagComposition).release();
    updateScaleModeIfNeed();
  }
}

std::shared_ptr<PAGSurface> PAGPlayer::getSurface() {
  LockGuard autoLock(rootLocker);
  return pagSurface;
}

void PAGPlayer::setSurface(std::shared_ptr<PAGSurface> newSurface) {
  auto locker = newSurface ? newSurface->rootLocker : nullptr;
  ScopedLock autoLock(rootLocker, locker);
  setSurfaceInternal(newSurface);
}

void PAGPlayer::setSurfaceInternal(std::shared_ptr<PAGSurface> newSurface) {
  if (pagSurface == newSurface) {
    return;
  }
  if (newSurface && newSurface->pagPlayer != nullptr) {
    LOGE("PAGPlayer.setSurface(): The new surface is already set to another PAGPlayer!");
    return;
  }
  if (pagSurface) {
    pagSurface->pagPlayer = nullptr;
    pagSurface->rootLocker = std::make_shared<std::mutex>();
  }
  pagSurface = newSurface;
  if (pagSurface) {
    pagSurface->pagPlayer = this;
    pagSurface->contentVersion = 0;
    pagSurface->rootLocker = rootLocker;
    updateStageSize();
  } else {
    stage->setContentSizeInternal(0, 0);
  }
}

bool PAGPlayer::videoEnabled() {
  LockGuard autoLock(rootLocker);
  return renderCache->videoEnabled();
}

void PAGPlayer::setVideoEnabled(bool value) {
  LockGuard autoLock(rootLocker);
  renderCache->setVideoEnabled(value);
}

bool PAGPlayer::cacheEnabled() {
  LockGuard autoLock(rootLocker);
  return renderCache->snapshotEnabled();
}

void PAGPlayer::setCacheEnabled(bool value) {
  LockGuard autoLock(rootLocker);
  renderCache->setSnapshotEnabled(value);
}

float PAGPlayer::cacheScale() {
  LockGuard autoLock(rootLocker);
  return stage->cacheScale();
}

void PAGPlayer::setCacheScale(float value) {
  LockGuard autoLock(rootLocker);
  stage->setCacheScale(value);
}

float PAGPlayer::maxFrameRate() {
  LockGuard autoLock(rootLocker);
  return _maxFrameRate;
}

void PAGPlayer::setMaxFrameRate(float value) {
  LockGuard autoLock(rootLocker);
  if (_maxFrameRate == value) {
    return;
  }
  _maxFrameRate = value;
}

int PAGPlayer::scaleMode() {
  LockGuard autoLock(rootLocker);
  return _scaleMode;
}

void PAGPlayer::setScaleMode(int mode) {
  LockGuard autoLock(rootLocker);
  _scaleMode = mode;
  auto pagComposition = stage->getRootComposition();
  if (_scaleMode == PAGScaleMode::None && pagComposition) {
    pagComposition->setMatrixInternal(Matrix::I());
  }
  updateScaleModeIfNeed();
}

Matrix PAGPlayer::matrix() {
  LockGuard autoLock(rootLocker);
  auto pagComposition = stage->getRootComposition();
  return pagComposition ? pagComposition->layerMatrix : Matrix::I();
}

void PAGPlayer::setMatrix(const Matrix& matrix) {
  LockGuard autoLock(rootLocker);
  _scaleMode = PAGScaleMode::None;
  auto pagComposition = stage->getRootComposition();
  if (pagComposition) {
    pagComposition->setMatrixInternal(matrix);
  }
}

int64_t PAGPlayer::duration() {
  LockGuard autoLock(rootLocker);
  return durationInternal();
}

int64_t PAGPlayer::durationInternal() {
  auto pagComposition = stage->getRootComposition();
  return pagComposition ? pagComposition->durationInternal() : 0;
}

void PAGPlayer::nextFrame() {
  LockGuard autoLock(rootLocker);
  auto pagComposition = stage->getRootComposition();
  if (pagComposition) {
    pagComposition->nextFrameInternal();
  }
}

void PAGPlayer::preFrame() {
  LockGuard autoLock(rootLocker);
  auto pagComposition = stage->getRootComposition();
  if (pagComposition) {
    pagComposition->preFrameInternal();
  }
}

double PAGPlayer::getProgress() {
  LockGuard autoLock(rootLocker);
  auto pagComposition = stage->getRootComposition();
  return pagComposition ? pagComposition->getProgressInternal() : 0;
}

void PAGPlayer::setProgress(double percent) {
  LockGuard autoLock(rootLocker);
  auto pagComposition = stage->getRootComposition();
  if (pagComposition == nullptr) {
    return;
  }
  auto realProgress = percent;
  auto frameRate = pagComposition->frameRateInternal();
  if (_maxFrameRate < frameRate && _maxFrameRate > 0) {
    auto duration = pagComposition->durationInternal();
    auto totalFrames = TimeToFrame(duration, frameRate);
    auto numFrames = ceilf(totalFrames * _maxFrameRate / frameRate);
    // 首先计算在maxFrameRate的帧号，之后重新计算progress
    auto targetFrame = ProgressToFrame(realProgress, numFrames);
    realProgress = FrameToProgress(targetFrame, numFrames);
  }
  pagComposition->setProgressInternal(realProgress);
}

bool PAGPlayer::autoClear() {
  LockGuard autoLock(rootLocker);
  return _autoClear;
}

void PAGPlayer::setAutoClear(bool value) {
  LockGuard autoLock(rootLocker);
  if (_autoClear == value) {
    return;
  }
  _autoClear = value;
  stage->notifyModified(true);
}

void PAGPlayer::prepare() {
  LockGuard autoLock(rootLocker);
  prepareInternal();
}

void PAGPlayer::prepareInternal() {
#ifdef PAG_BUILD_FOR_WEB
  auto distance = durationInternal();
  renderCache->prepareLayers(distance);
#else
  renderCache->prepareLayers();
#endif
  if (contentVersion != stage->getContentVersion()) {
    contentVersion = stage->getContentVersion();
    Recorder recorder = {};
    stage->draw(&recorder);
    lastGraphic = recorder.makeGraphic();
  }
  if (lastGraphic) {
    lastGraphic->prepare(renderCache);
  }
}

bool PAGPlayer::wait(const BackendSemaphore& waitSemaphore) {
  LockGuard autoLock(rootLocker);
  if (pagSurface == nullptr) {
    return false;
  }
  return pagSurface->wait(waitSemaphore);
}

bool PAGPlayer::flushAndSignalSemaphore(BackendSemaphore* signalSemaphore) {
  LockGuard autoLock(rootLocker);
  return flushInternal(signalSemaphore);
}

bool PAGPlayer::flush() {
  LockGuard autoLock(rootLocker);
  return flushInternal(nullptr);
}

bool PAGPlayer::flushInternal(BackendSemaphore* signalSemaphore) {
  if (pagSurface == nullptr) {
    return false;
  }
  renderCache->beginFrame();
  updateStageSize();
  tgfx::Clock clock = {};
  prepareInternal();
  clock.mark("rendering");
  if (!pagSurface->draw(renderCache, lastGraphic, signalSemaphore, _autoClear)) {
    return false;
  }
  clock.mark("presenting");
  renderCache->renderingTime = clock.measure("", "rendering");
  renderCache->presentingTime = clock.measure("rendering", "presenting");
  auto knownTime = renderCache->imageDecodingTime + renderCache->textureUploadingTime +
                   renderCache->programCompilingTime + renderCache->hardwareDecodingTime +
                   renderCache->softwareDecodingTime;
  renderCache->presentingTime -= knownTime;
  renderCache->totalTime = clock.measure("", "presenting");
  //  auto composition = stage->getRootComposition();
  //  if (composition) {
  //    renderCache->printPerformance(composition->currentFrameInternal());
  //  }
  if (reporter) {
    reporter->recordPerformance(renderCache);
  }
  return true;
}

Rect PAGPlayer::getBounds(std::shared_ptr<PAGLayer> pagLayer) {
  if (pagLayer == nullptr) {
    return Rect::MakeEmpty();
  }
  LockGuard autoLock(rootLocker);
  updateStageSize();
  tgfx::Rect bounds = {};
  pagLayer->measureBounds(&bounds);
  auto layer = pagLayer.get();
  bool contains = false;
  while (layer) {
    if (layer == stage.get()) {
      contains = true;
      break;
    }
    auto layerMatrix = ToTGFX(layer->getTotalMatrixInternal());
    layerMatrix.mapRect(&bounds);
    if (layer->_parent == nullptr && layer->trackMatteOwner) {
      layer = layer->trackMatteOwner->_parent;
    } else {
      layer = layer->_parent;
    }
  }
  return contains ? ToPAG(bounds) : Rect::MakeEmpty();
}

std::vector<std::shared_ptr<PAGLayer>> PAGPlayer::getLayersUnderPoint(float surfaceX,
                                                                      float surfaceY) {
  LockGuard autoLock(rootLocker);
  updateStageSize();
  std::vector<std::shared_ptr<PAGLayer>> results;
  stage->getLayersUnderPointInternal(surfaceX, surfaceY, &results);
  return results;
}

bool PAGPlayer::hitTestPoint(std::shared_ptr<PAGLayer> pagLayer, float surfaceX, float surfaceY,
                             bool pixelHitTest) {
  LockGuard autoLock(rootLocker);
  updateStageSize();
  auto local = pagLayer->globalToLocalPoint(surfaceX, surfaceY);
  if (!pixelHitTest) {
    tgfx::Rect bounds = {};
    pagLayer->measureBounds(&bounds);
    return bounds.contains(local.x, local.y);
  }

  if (pagSurface == nullptr || pagLayer->getStage() != stage.get()) {
    return false;
  }
  Recorder recorder = {};
  pagLayer->draw(&recorder);
  auto graphic = recorder.makeGraphic();
  return pagSurface->hitTest(renderCache, graphic, local.x, local.y);
}

int64_t PAGPlayer::getTimeStampInternal() {
  auto pagComposition = stage->getRootComposition();
  if (pagComposition == nullptr) {
    return 0;
  }
  auto duration = pagComposition->durationInternal();
  auto progress = pagComposition->getProgressInternal();
  return static_cast<int64_t>(ceil(progress * duration));
}

int64_t PAGPlayer::renderingTime() {
  LockGuard autoLock(rootLocker);
  // TODO(domrjchen): update the performance monitoring panel of PAGViewer to display the new
  // properties
  return renderCache->totalTime - renderCache->presentingTime - renderCache->imageDecodingTime;
}

int64_t PAGPlayer::imageDecodingTime() {
  LockGuard autoLock(rootLocker);
  return renderCache->imageDecodingTime;
}

int64_t PAGPlayer::presentingTime() {
  LockGuard autoLock(rootLocker);
  return renderCache->presentingTime;
}

int64_t PAGPlayer::graphicsMemory() {
  LockGuard autoLock(rootLocker);
  return renderCache->memoryUsage();
}

void PAGPlayer::updateStageSize() {
  if (pagSurface == nullptr) {
    return;
  }
  auto surfaceWidth = pagSurface->drawable->width();
  auto surfaceHeight = pagSurface->drawable->height();
  if (surfaceWidth != stage->widthInternal() || surfaceHeight != stage->heightInternal()) {
    stage->setContentSizeInternal(surfaceWidth, surfaceHeight);
    updateScaleModeIfNeed();
  }
}

void PAGPlayer::updateScaleModeIfNeed() {
  auto pagComposition = stage->getRootComposition();
  if (!pagComposition) {
    return;
  }
  if (stage->widthInternal() > 0 && stage->heightInternal() > 0 &&
      _scaleMode != PAGScaleMode::None) {
    auto matrix = ApplyScaleMode(_scaleMode, pagComposition->widthInternal(),
                                 pagComposition->heightInternal(), stage->widthInternal(),
                                 stage->heightInternal());
    pagComposition->setMatrixInternal(matrix);
  }
}
}  // namespace pag
