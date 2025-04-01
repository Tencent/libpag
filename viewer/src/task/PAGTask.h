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

#pragma once

#include <QObject>
#include <QOffscreenSurface>
#include <QOpenGLFramebufferObject>
#include <QThread>
#include <atomic>
#include "pag/pag.h"

namespace pag {

class PAGTask : public QObject {
  Q_OBJECT
 public:
  PAGTask();

  Q_PROPERTY(bool visible READ getVisible NOTIFY visibleChanged)
  Q_PROPERTY(double progress READ getProgress NOTIFY progressChanged)

  virtual auto getVisible() const -> bool;
  virtual auto getProgress() const -> double;

  Q_SIGNAL void taskFinished(int result, QString filePath);
  Q_SIGNAL void visibleChanged(bool visible);
  Q_SIGNAL void progressChanged(double progress);

  Q_INVOKABLE virtual void start() = 0;
  Q_INVOKABLE virtual void pause() = 0;
  Q_INVOKABLE virtual void stop() = 0;

 protected:
  virtual auto onBegin() -> void = 0;
  virtual auto onFinish() -> int = 0;

 private:
  virtual auto startInternal() -> void = 0;

 protected:
  bool visible = false;
  double progress = 0.0;
};

class PAGFileTask : public PAGTask {
  Q_OBJECT
 public:
  explicit PAGFileTask(std::shared_ptr<PAGFile>& pagFile, const QString& filePath);
  ~PAGFileTask() override;

  auto start() -> void override;
  auto pause() -> void override;
  auto stop() -> void override;

 protected:
  auto onBegin() -> void override;
  auto onFinish() -> int override;
  virtual auto onFrameFlush(double progress) -> void;
  virtual auto isNeedRenderCurrentFrame() -> bool;
  auto releaseResource() -> void;

 private:
  auto startInternal() -> void override;
  auto initOpenGLEnvironment() -> void;

 protected:
  Frame currentFrame = 0;
  QString filePath;
  PAGPlayer* pagPlayer = nullptr;
  QOpenGLContext* context = nullptr;
  QOffscreenSurface* offscreenSurface = nullptr;
  QOpenGLFramebufferObject* frameBuffer = nullptr;
  std::shared_ptr<PAGFile> pagFile = nullptr;
  std::shared_ptr<PAGSurface> surface = nullptr;

 private:
  QThread workerThread;
  std::atomic_bool isWorking = false;
};

}  // namespace pag