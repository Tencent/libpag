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

#include "PAGWindow.h"
#include <QQmlContext>
#include <QSettings>
#include "PAGRenderThread.h"
#include "PAGViewer.h"
#include "PAGWindowHelper.h"
#include "profiling/PAGRunTimeDataModel.h"
#include "task/PAGTaskFactory.h"

namespace pag {

QList<PAGWindow*> PAGWindow::AllWindows;

PAGWindow::PAGWindow(QObject* parent) : QObject(parent) {
}

void PAGWindow::openFile(QString path) {
  bool result = pagView->setFile(path);
  if (!result) {
    return;
  }
  filePath = path;
  window->raise();
  window->requestActivate();
}

void PAGWindow::onPAGViewerDestroyed() {
  Q_EMIT destroyWindow(this);
}

void PAGWindow::open() {
  translator = std::make_unique<QTranslator>();
  engine = std::make_unique<QQmlApplicationEngine>();
  windowHelper = std::make_unique<PAGWindowHelper>();
  treeViewModel = std::make_unique<PAGTreeViewModel>();
  runTimeDataModel = std::make_unique<PAGRunTimeDataModel>();
  editAttributeModel = std::make_unique<PAGEditAttributeModel>();
  textLayerModel = std::make_unique<PAGTextLayerModel>();
  imageLayerModel = std::make_unique<PAGImageLayerModel>();
  benchmarkModel = std::make_unique<PAGBenchmarkModel>();

  bool result = translator->load(":translation/Chinese.qm");
  if (result && !isUseEnglish() && qApp != nullptr) {
    qApp->installTranslator(translator.get());
  }

  auto context = engine->rootContext();
  context->setContextProperty("windowHelper", windowHelper.get());
  context->setContextProperty("treeViewModel", treeViewModel.get());
  context->setContextProperty("runTimeDataModel", runTimeDataModel.get());
  context->setContextProperty("editAttributeModel", editAttributeModel.get());
  context->setContextProperty("textLayerModel", textLayerModel.get());
  context->setContextProperty("imageLayerModel", imageLayerModel.get());
  context->setContextProperty("benchmarkModel", benchmarkModel.get());

  auto viewer = static_cast<PAGViewer*>(qApp);
  context->setContextProperty("checkUpdateModel", viewer->getCheckUpdateModel());

  // Image Provider will be managed by QML
  auto imageProvider = new PAGImageProvider();
  imageProvider->setImageLayerModel(imageLayerModel.get());
  engine->addImageProvider(QLatin1String("PAGImageProvider"), imageProvider);
  engine->load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

  window = static_cast<QQuickWindow*>(engine->rootObjects().at(0));
  window->setPersistentGraphics(true);
  window->setPersistentSceneGraph(true);
  window->setTextRenderType(QQuickWindow::TextRenderType::NativeTextRendering);
  auto surfaceFormat = window->format();
  surfaceFormat.setSwapInterval(1);
  window->setFormat(surfaceFormat);

  pagView = window->findChild<pag::PAGView*>("pagView");
  auto* taskFactory = window->findChild<PAGTaskFactory*>("taskFactory");
  PAGRenderThread* renderThread = pagView->getRenderThread();

  connect(window, SIGNAL(closing(QQuickCloseEvent*)), this, SLOT(onPAGViewerDestroyed()),
          Qt::QueuedConnection);
  connect(window, &QQuickWindow::afterRendering, pagView, &PAGView::flush);
  connect(pagView, &PAGView::filePathChanged, taskFactory, &PAGTaskFactory::setFilePath);
  connect(pagView, &PAGView::fileChanged, treeViewModel.get(), &PAGTreeViewModel::setFile);
  connect(pagView, &PAGView::pagFileChanged, editAttributeModel.get(),
          &PAGEditAttributeModel::setPAGFile);
  connect(pagView, &PAGView::pagFileChanged, runTimeDataModel.get(),
          &PAGRunTimeDataModel::setPAGFile);
  connect(pagView, &PAGView::pagFileChanged, textLayerModel.get(), &PAGTextLayerModel::setPAGFile);
  connect(textLayerModel.get(), &PAGTextLayerModel::textChanged, renderThread,
          &PAGRenderThread::flush);
  connect(pagView, &PAGView::pagFileChanged, imageLayerModel.get(),
          &PAGImageLayerModel::setPAGFile);
  connect(imageLayerModel.get(), &PAGImageLayerModel::imageChanged, renderThread,
          &PAGRenderThread::flush);
  connect(renderThread, &PAGRenderThread::frameTimeMetricsReady, runTimeDataModel.get(),
          &PAGRunTimeDataModel::updateData);
}

bool PAGWindow::isUseEnglish() {
  QSettings settings;
  auto value = settings.value("isUseEnglish");
  if (!value.isNull()) {
    return value.toBool();
  }
  return true;
}

QString PAGWindow::getFilePath() {
  return filePath;
}

QQmlApplicationEngine* PAGWindow::getEngine() {
  return engine.get();
}

}  // namespace pag
