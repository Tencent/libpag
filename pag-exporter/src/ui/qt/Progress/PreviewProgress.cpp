/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "PreviewProgress.h"
#include <QtCore/QDebug>
#include "src/ui/qt/EnvConfig.h"

PreviewProgress::PreviewProgress(QWidget* parent) : QObject(parent) {
  init(nullptr);
}

PreviewProgress::PreviewProgress(pagexporter::Context* context, QWidget* parent) : QObject(parent) {
  init(context);
}

void PreviewProgress::translate() {
//  if (translator) {
//    CurrentUseQtApp()->removeTranslator(translator);
//  }
//  if (ExportUtils::isEnglish()) {
//    translator = new QTranslator();
//    bool result = translator->load(":/language/english.qm");
//    bool install = CurrentUseQtApp()->installTranslator(translator);
//    qDebug() << "translator result:" << result << ", install result:" << install;
//  }
//  engine->retranslate();
}

void PreviewProgress::init(pagexporter::Context* context) {
  this->context = context;
  QApplication::setOverrideCursor(Qt::WaitCursor);

  engine = new QQmlApplicationEngine(CurrentUseQtApp());
  QQmlContext* ctx = engine->rootContext();
  ctx->setContextProperty("progressDialog", this);

  translate();
  engine->load(QUrl(QStringLiteral("qrc:/qml/ProgressPage.qml")));
  window = static_cast<QQuickWindow*>(engine->rootObjects().at(0));
  window->setTitle(tr("PAG导出进度"));

  window->setTextRenderType(QQuickWindow::TextRenderType::NativeTextRendering);
#ifdef ENABLE_OLD_API
  window->setPersistentOpenGLContext(true);
#else
  window->setPersistentGraphics(true);
#endif
  window->setPersistentSceneGraph(true);
  window->show();
}

void PreviewProgress::windowClose() {
  if (context != nullptr) {
    context->bEarlyExit = true;
  }
}

PreviewProgress::~PreviewProgress() {
  QApplication::restoreOverrideCursor();
  if (window) {
    window->close();
  }
}

void PreviewProgress::setProgress(double progressValue) {
  if (progressValue < 0.0) {
    this->progress = 0.0;
  } else if (progressValue > 100.0) {
    this->progress = 100.0;
  } else {
    this->progress = progressValue;
  }
  QApplication::processEvents();  // QEventLoop::ExcludeUserInputEvents
  QMetaObject::invokeMethod(window, "setProgress", Qt::DirectConnection, Q_ARG(QVariant, progressValue / 100));
}

void PreviewProgress::setProgressTips(const QString& tip) {
  QMetaObject::invokeMethod(window, "setProgressTips", Qt::DirectConnection, Q_ARG(QVariant, tip));
}

void PreviewProgress::setProgressTitle(const QString& title) {
  if (window != nullptr) {
    window->setTitle(title);
  }
}
