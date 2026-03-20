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
#include "ContentViewModel.h"
#include "PAGViewer.h"
#include "PAGWindowHelper.h"
#include "RenderThread.h"
#include "profiling/PAGRunTimeDataModel.h"
#include "task/PAGTaskFactory.h"

namespace pag {

QList<PAGWindow*> PAGWindow::AllWindows;

PAGWindow::PAGWindow(QObject* parent) : QObject(parent) {
}

void PAGWindow::openFile(QString path) {
  if (contentView == nullptr) {
    return;
  }
  bool result = contentView->getViewModel()->loadFile(path);
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

void PAGWindow::notifyContentViewChanged(ContentView* newContentView) {
  if (newContentView == contentView) {
    return;
  }
  disconnectContentViewSignals();
  contentView = newContentView;
  connectContentViewSignals();
}

void PAGWindow::disconnectContentViewSignals() {
  if (contentView == nullptr) {
    return;
  }
  auto* viewModel = contentView->getViewModel();
  auto* hostWindow = contentView->window();
  auto* taskFactory = hostWindow ? hostWindow->findChild<PAGTaskFactory*>("taskFactory") : nullptr;
  if (taskFactory) {
    disconnect(viewModel, &ContentViewModel::filePathChanged, taskFactory,
               &PAGTaskFactory::setFilePath);
  }
  if (auto* pagVM = qobject_cast<PAGViewModel*>(viewModel)) {
    disconnect(pagVM, &PAGViewModel::fileChanged, treeViewModel.get(),
               &PAGTreeViewModel::setFile);
    disconnect(pagVM, &PAGViewModel::pagFileChanged, editAttributeModel.get(),
               &PAGEditAttributeModel::setPAGFile);
    disconnect(pagVM, &PAGViewModel::pagFileChanged, runTimeDataModel.get(),
               &PAGRunTimeDataModel::setPAGFile);
    disconnect(pagVM, &PAGViewModel::pagFileChanged, textLayerModel.get(),
               &PAGTextLayerModel::setPAGFile);
    disconnect(pagVM, &PAGViewModel::pagFileChanged, imageLayerModel.get(),
               &PAGImageLayerModel::setPAGFile);
  } else if (auto* pagxVM = qobject_cast<PAGXViewModel*>(viewModel)) {
    disconnect(pagxVM, &PAGXViewModel::pagxDocumentChanged, treeViewModel.get(),
               &PAGTreeViewModel::setPAGXDocument);
    disconnect(pagxVM, &PAGXViewModel::pagxDocumentChanged, runTimeDataModel.get(),
               &PAGRunTimeDataModel::setPAGXDocument);
  }

  auto* renderThread = contentView->getRenderThread();
  disconnect(textLayerModel.get(), &PAGTextLayerModel::textChanged, renderThread,
             &RenderThread::flush);
  disconnect(imageLayerModel.get(), &PAGImageLayerModel::imageChanged, renderThread,
             &RenderThread::flush);
  disconnect(renderThread, &RenderThread::renderMetricsReady, runTimeDataModel.get(),
             &PAGRunTimeDataModel::updateMetrics);
  if (hostWindow) {
    disconnect(hostWindow, &QQuickWindow::afterRendering, contentView, &ContentView::flush);
  }
}

void PAGWindow::connectContentViewSignals() {
  if (contentView == nullptr) {
    return;
  }
  auto* viewModel = contentView->getViewModel();
  auto* hostWindow = contentView->window();
  auto* taskFactory = hostWindow ? hostWindow->findChild<PAGTaskFactory*>("taskFactory") : nullptr;
  if (taskFactory) {
    connect(viewModel, &ContentViewModel::filePathChanged, taskFactory,
            &PAGTaskFactory::setFilePath);
  }
  if (auto* pagVM = qobject_cast<PAGViewModel*>(viewModel)) {
    connect(pagVM, &PAGViewModel::fileChanged, treeViewModel.get(),
            &PAGTreeViewModel::setFile, Qt::QueuedConnection);
    connect(pagVM, &PAGViewModel::pagFileChanged, editAttributeModel.get(),
            &PAGEditAttributeModel::setPAGFile);
    connect(pagVM, &PAGViewModel::pagFileChanged, runTimeDataModel.get(),
            &PAGRunTimeDataModel::setPAGFile);
    connect(pagVM, &PAGViewModel::pagFileChanged, textLayerModel.get(),
            &PAGTextLayerModel::setPAGFile);
    connect(pagVM, &PAGViewModel::pagFileChanged, imageLayerModel.get(),
            &PAGImageLayerModel::setPAGFile);
  } else if (auto* pagxVM = qobject_cast<PAGXViewModel*>(viewModel)) {
    connect(pagxVM, &PAGXViewModel::pagxDocumentChanged, treeViewModel.get(),
            &PAGTreeViewModel::setPAGXDocument, Qt::QueuedConnection);
    connect(pagxVM, &PAGXViewModel::pagxDocumentChanged, runTimeDataModel.get(),
            &PAGRunTimeDataModel::setPAGXDocument);
  }

  auto* renderThread = contentView->getRenderThread();
  connect(textLayerModel.get(), &PAGTextLayerModel::textChanged, renderThread,
          &RenderThread::flush);
  connect(imageLayerModel.get(), &PAGImageLayerModel::imageChanged, renderThread,
          &RenderThread::flush);
  connect(renderThread, &RenderThread::renderMetricsReady, runTimeDataModel.get(),
          &PAGRunTimeDataModel::updateMetrics);
  if (hostWindow) {
    connect(hostWindow, &QQuickWindow::afterRendering, contentView, &ContentView::flush);
  }
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
  context->setContextProperty("pagWindow", this);
  context->setContextProperty("treeViewModel", treeViewModel.get());
  context->setContextProperty("runTimeDataModel", runTimeDataModel.get());
  context->setContextProperty("editAttributeModel", editAttributeModel.get());
  context->setContextProperty("textLayerModel", textLayerModel.get());
  context->setContextProperty("imageLayerModel", imageLayerModel.get());
  context->setContextProperty("benchmarkModel", benchmarkModel.get());

  auto viewer = static_cast<PAGViewer*>(qApp);
  context->setContextProperty("checkUpdateModel", viewer->getCheckUpdateModel());

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

  contentView = window->findChild<ContentView*>("contentView");

  connect(window, SIGNAL(closing(QQuickCloseEvent*)), this, SLOT(onPAGViewerDestroyed()),
          Qt::QueuedConnection);

  connectContentViewSignals();
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
