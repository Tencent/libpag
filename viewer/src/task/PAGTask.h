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

#pragma once

#include <QObject>
#include <QOffscreenSurface>
#include <QOpenGLContext>
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

  virtual bool getVisible() const;
  virtual double getProgress() const;

  Q_SIGNAL void taskFinished(int result, QString filePath);
  Q_SIGNAL void visibleChanged(bool visible);
  Q_SIGNAL void progressChanged(double progress);

  Q_INVOKABLE virtual void start() = 0;
  Q_INVOKABLE virtual void stop() = 0;

 protected:
  virtual void onBegin() = 0;
  virtual int onFinish() = 0;

 private:
  virtual void startInternal() = 0;

 protected:
  bool visible = false;
  double progress = 0.0;
};

class PAGPlayTask : public PAGTask {
  Q_OBJECT
 public:
  explicit PAGPlayTask(std::shared_ptr<PAGFile> pagFile, const QString& filePath);
  ~PAGPlayTask() override;

  void start() override;
  void stop() override;

 protected:
  void onBegin() override;
  int onFinish() override;
  virtual void onFrameFlush(double progress);
  virtual bool isNeedRenderCurrentFrame();
  void releaseResource();

 private:
  void startInternal() override;
  void initOpenGLEnvironment();

 protected:
  bool openAfterExport = true;
  Frame currentFrame = 0;
  QString filePath = "";
  std::shared_ptr<PAGFile> pagFile = nullptr;
  std::unique_ptr<PAGPlayer> pagPlayer = nullptr;
  std::shared_ptr<PAGSurface> surface = nullptr;
  std::unique_ptr<QOpenGLContext> context = nullptr;
  std::unique_ptr<QOffscreenSurface> offscreenSurface = nullptr;
  std::unique_ptr<QOpenGLFramebufferObject> frameBuffer = nullptr;

 private:
  QThread workerThread;
  std::atomic_bool isWorking = false;
};

}  // namespace pag
