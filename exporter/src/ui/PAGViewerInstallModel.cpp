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

#include "PAGViewerInstallModel.h"
#include <QApplication>
#include <QDebug>
#include <QQmlContext>
#include <QThread>
#include <QtConcurrent/QtConcurrent>
#include <memory>
#include <string>
#include "platform/PAGViewerInstaller.h"
#include "platform/PlatformHelper.h"

namespace exporter {

PAGViewerInstallModel::PAGViewerInstallModel(QObject* parent) : QObject(parent) {
  updateStage(Confirm);
}

PAGViewerInstallModel::~PAGViewerInstallModel() {
  if (window) {
    window->close();
  }
}

bool PAGViewerInstallModel::showInstallDialog(const std::string& pagFilePath) {
  pendingFilePath = pagFilePath;
  dialogResult = false;

  int argc = 0;
  app = std::make_unique<QApplication>(argc, nullptr);
  app->setObjectName("PAGViewer-Install");
  engine = std::make_unique<QQmlApplicationEngine>(app.get());

  auto context = engine->rootContext();
  context->setContextProperty("installModel", this);

  engine->load(QUrl(QStringLiteral("qrc:/qml/PAGViewerInstall.qml")));

  if (engine->rootObjects().isEmpty()) {
    return false;
  }

  window = qobject_cast<QQuickWindow*>(engine->rootObjects().first());
  if (!window) {
    return false;
  }

  window->setPersistentGraphics(true);
  window->setPersistentSceneGraph(true);
  window->setTextRenderType(QQuickWindow::TextRenderType::NativeTextRendering);
  window->show();

  if (!QCoreApplication::instance()->property("_eventLoopRunning").toBool()) {
    QCoreApplication::instance()->setProperty("_eventLoopRunning", true);
    app->exec();
    QCoreApplication::instance()->setProperty("_eventLoopRunning", false);
  }
  return dialogResult;
}

void PAGViewerInstallModel::onConfirmInstall() {
  updateStage(Installing);
  executeInstallation();
}

void PAGViewerInstallModel::onCancel() {
  dialogResult = false;
  if (window) {
    window->close();
  }
}

void PAGViewerInstallModel::onComplete() {
  if (currentStage == Success) {
    dialogResult = true;
    QThreadPool::globalInstance()->start([this]() { startPreview(pendingFilePath); });
  } else {
    dialogResult = false;
  }

  if (window) {
    window->close();
  }
}

void PAGViewerInstallModel::updateStage(InstallStage stage) {
  currentStage = stage;

  switch (stage) {
    case Confirm:
      title = tr("PAGViewer is not installed");
      message =
          tr("PAGViewer is used to preview PAG files.\nWould you like to download and install it "
             "now?");
      buttonText = tr("Confirm");
      showCancelButton = true;
      break;

    case Installing:
      title = tr("Installing PAGViewer");
      message = tr("Please wait, downloading and installing PAGViewer...");
      buttonText = "";
      showCancelButton = false;
      break;

    case Success:
      title = tr("Installation Complete");
      message = tr("PAGViewer installed successfully!\nPreviewing your PAG file now.");
      buttonText = tr("Preview");
      showCancelButton = false;
      break;

    case Failed:
      title = tr("Installation Failed");
      message =
          tr("PAGViewer installation failed\nPlease check your network connection or install "
             "manually.");
      buttonText = tr("Retry");
      showCancelButton = true;
      break;
  }

  Q_EMIT titleChanged();
  Q_EMIT messageChanged();
  Q_EMIT buttonTextChanged();
  Q_EMIT showCancelButtonChanged();
  Q_EMIT stageChanged();
}

void PAGViewerInstallModel::executeInstallation() {
  QThreadPool::globalInstance()->start([this]() {
    try {
      auto config = std::make_shared<AppConfig>();
      config->setAppName("PAGViewer");
      auto installer = std::make_unique<PAGViewerInstaller>(config);
      installer->setProgressCallback([this](int progress) {
        QMetaObject::invokeMethod(
            this,
            [this, progress]() {
              QString progressMessage =
                  tr("Installing PAGViewer... ") + QString("(%1%)").arg(progress);
              message = progressMessage;
              Q_EMIT messageChanged();
            },
            Qt::QueuedConnection);
      });

      InstallStatus result = installer->installPAGViewer();

      QMetaObject::invokeMethod(
          this,
          [this, result]() {
            if (!result.isSuccess()) {
              updateStage(Success);
            } else {
              QString errorMsg = tr("PAGViewer installation failed");
              if (!result.errorMessage.empty()) {
                errorMsg += "\n" + QString::fromStdString(result.errorMessage);
              }
              message = errorMsg;
              updateStage(Failed);
            }
          },
          Qt::QueuedConnection);

    } catch (const std::exception& e) {
      QMetaObject::invokeMethod(
          this,
          [this, e]() {
            message =
                tr("An error occurred during installation: ") + QString::fromStdString(e.what());
            updateStage(Failed);
          },
          Qt::QueuedConnection);
    }
  });
}

void PAGViewerInstallModel::startPreview(const std::string& filePath) {
  if (QThread::currentThread() != QApplication::instance()->thread()) {
    QMetaObject::invokeMethod(
        QApplication::instance(), [filePath]() { PreviewPAGFile(filePath); }, Qt::QueuedConnection);
  }
}

}  // namespace exporter
