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

#include "PAGTask.h"
#include "base/utils/TimeUtil.h"
#include "utils/FileUtils.h"

namespace pag {

PAGTask::PAGTask() : QObject(nullptr) {
}

auto PAGTask::getVisible() const -> bool {
  return visible;
}

auto PAGTask::getProgress() const -> double {
  return progress;
}

PAGPlayTask::PAGPlayTask(std::shared_ptr<PAGFile>& pagFile, const QString& filePath)
    : filePath(filePath), pagFile(pagFile) {
  QObject::connect(&workerThread, &QThread::started, this, &PAGPlayTask::startInternal,
                   Qt::DirectConnection);
}

PAGPlayTask::~PAGPlayTask() {
  if (workerThread.isRunning()) {
    workerThread.quit();
  }
  QObject::disconnect(&workerThread, &QThread::started, this, &PAGPlayTask::startInternal);
  releaseResource();
}

auto PAGPlayTask::start() -> void {
  if (pagFile == nullptr) {
    return;
  }
  isWorking = true;
  onBegin();
  workerThread.start();
}

auto PAGPlayTask::stop() -> void {
  this->isWorking = false;
  currentFrame = 0;
}

auto PAGPlayTask::onBegin() -> void {
  initOpenGLEnvironment();
  visible = true;
  Q_EMIT visibleChanged(visible);
}

auto PAGPlayTask::onFinish() -> int {
  visible = false;
  Q_EMIT visibleChanged(visible);
  return 0;
}

auto PAGPlayTask::onFrameFlush(double progress) -> void {
  this->progress = progress;
  Q_EMIT progressChanged(progress);
}

auto PAGPlayTask::isNeedRenderCurrentFrame() -> bool {
  return currentFrame >= 0;
}

auto PAGPlayTask::releaseResource() -> void {
  pagPlayer.reset();
  surface.reset();
  if (context != nullptr) {
    if (frameBuffer != nullptr) {
      context->makeCurrent(offscreenSurface.get());
      frameBuffer->release();
      frameBuffer.reset();
      context->doneCurrent();
    }
    context.reset();
  }
  offscreenSurface.reset();
}

auto PAGPlayTask::startInternal() -> void {
  float frameRate = pagFile->frameRate();
  Frame totalFrame = TimeToFrame(pagFile->duration(), frameRate);
  while (currentFrame < totalFrame) {
    if (isNeedRenderCurrentFrame()) {
      pagFile->setCurrentTime(FrameToTime(currentFrame, frameRate));
      pagPlayer->flush();
      onFrameFlush(pagPlayer->getProgress());
    }
    currentFrame++;
    if (isWorking != true) {
      releaseResource();
      workerThread.exit(0);
      return;
    }
  }
  isWorking = false;
  int result = onFinish();
  currentFrame = 0;
  releaseResource();
  Q_EMIT taskFinished(result, filePath);
  if (openAfterExport) {
    Utils::openInFinder(filePath, true);
  }
  workerThread.exit(0);
}

auto PAGPlayTask::initOpenGLEnvironment() -> void {
  if (surface != nullptr) {
    return;
  }
  pagPlayer = std::make_unique<PAGPlayer>();
  context = std::make_unique<QOpenGLContext>();
  context->create();
  offscreenSurface = std::make_unique<QOffscreenSurface>();
  offscreenSurface->setFormat(context->format());
  offscreenSurface->create();
  context->makeCurrent(offscreenSurface.get());
  frameBuffer = std::make_unique<QOpenGLFramebufferObject>(
      QSize(pagFile->width(), pagFile->height()), GL_TEXTURE_2D);
  GLFrameBufferInfo frameBufferInfo;
  frameBufferInfo.id = frameBuffer->handle();
  BackendRenderTarget renderTarget =
      BackendRenderTarget(frameBufferInfo, frameBuffer->width(), frameBuffer->height());
  surface = pag::PAGSurface::MakeFrom(renderTarget, pag::ImageOrigin::TopLeft);
  pagPlayer->setSurface(surface);
  pagPlayer->setComposition(pagFile);
  context->doneCurrent();
  context->moveToThread(&workerThread);
  offscreenSurface->moveToThread(&workerThread);
}

}  // namespace pag