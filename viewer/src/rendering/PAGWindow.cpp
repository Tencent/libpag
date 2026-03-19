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
  bool result = contentView->setFile(path);
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

void PAGWindow::onContentViewChanged(ContentView* newContentView) {
  if (newContentView == contentView) {
    return;
  }

  disconnectContentViewSignals();
  contentView = newContentView;
  pagView = qobject_cast<PAGView*>(contentView);
  connectContentViewSignals();
}

void PAGWindow::disconnectContentViewSignals() {
  if (contentView == nullptr) {
    return;
  }
  auto* taskFactory = window->findChild<PAGTaskFactory*>("taskFactory");
  if (taskFactory) {
    disconnect(contentView, &ContentView::filePathChanged, taskFactory,
               &PAGTaskFactory::setFilePath);
  }
  disconnect(contentView, &ContentView::fileChanged, treeViewModel.get(),
             &PAGTreeViewModel::setFile);
  disconnect(contentView, &ContentView::pagxDocumentChanged, treeViewModel.get(),
             &PAGTreeViewModel::setPAGXDocument);
  disconnect(contentView, &ContentView::pagFileChanged, editAttributeModel.get(),
             &PAGEditAttributeModel::setPAGFile);
  disconnect(contentView, &ContentView::pagFileChanged, runTimeDataModel.get(),
             &PAGRunTimeDataModel::setPAGFile);
  disconnect(contentView, &ContentView::pagxDocumentChanged, runTimeDataModel.get(),
             &PAGRunTimeDataModel::setPAGXDocument);
  disconnect(contentView, &ContentView::pagFileChanged, textLayerModel.get(),
             &PAGTextLayerModel::setPAGFile);
  disconnect(contentView, &ContentView::pagFileChanged, imageLayerModel.get(),
             &PAGImageLayerModel::setPAGFile);

  if (pagView != nullptr) {
    RenderThread* renderThread = pagView->getRenderThread();
    disconnect(textLayerModel.get(), &PAGTextLayerModel::textChanged, renderThread,
               &RenderThread::flush);
    disconnect(imageLayerModel.get(), &PAGImageLayerModel::imageChanged, renderThread,
               &RenderThread::flush);
    disconnect(renderThread, SIGNAL(frameTimeMetricsReady(int64_t, int64_t, int64_t, int64_t)),
               runTimeDataModel.get(), SLOT(updateData(int64_t, int64_t, int64_t, int64_t)));
    disconnect(window, &QQuickWindow::afterRendering, pagView, &PAGView::flush);
  }

  auto* pagxView = qobject_cast<PAGXView*>(contentView);
  if (pagxView != nullptr) {
    RenderThread* renderThread = pagxView->getRenderThread();
    disconnect(renderThread, SIGNAL(renderTimeReady(int64_t, int64_t, int64_t)),
               runTimeDataModel.get(), SLOT(updatePAGXRenderTime(int64_t, int64_t, int64_t)));
    disconnect(window, &QQuickWindow::afterRendering, pagxView,
               static_cast<void (PAGXView::*)() const>(&PAGXView::flush));
  }
}

void PAGWindow::connectBaseContentViewSignals() {
  if (contentView == nullptr) {
    return;
  }
  auto* taskFactory = window->findChild<PAGTaskFactory*>("taskFactory");
  if (taskFactory) {
    connect(contentView, &ContentView::filePathChanged, taskFactory, &PAGTaskFactory::setFilePath);
  }
  connect(contentView, &ContentView::fileChanged, treeViewModel.get(), &PAGTreeViewModel::setFile,
          Qt::QueuedConnection);
  connect(contentView, &ContentView::pagxDocumentChanged, treeViewModel.get(),
          &PAGTreeViewModel::setPAGXDocument, Qt::QueuedConnection);
  connect(contentView, &ContentView::pagFileChanged, editAttributeModel.get(),
          &PAGEditAttributeModel::setPAGFile);
  connect(contentView, &ContentView::pagFileChanged, runTimeDataModel.get(),
          &PAGRunTimeDataModel::setPAGFile);
  connect(contentView, &ContentView::pagxDocumentChanged, runTimeDataModel.get(),
          &PAGRunTimeDataModel::setPAGXDocument);
  connect(contentView, &ContentView::pagFileChanged, textLayerModel.get(),
          &PAGTextLayerModel::setPAGFile);
  connect(contentView, &ContentView::pagFileChanged, imageLayerModel.get(),
          &PAGImageLayerModel::setPAGFile);
}

void PAGWindow::connectFormatSpecificSignals() {
  if (contentView == nullptr) {
    return;
  }

  if (pagView != nullptr) {
    RenderThread* renderThread = pagView->getRenderThread();
    connect(textLayerModel.get(), &PAGTextLayerModel::textChanged, renderThread,
            &RenderThread::flush);
    connect(imageLayerModel.get(), &PAGImageLayerModel::imageChanged, renderThread,
            &RenderThread::flush);
    connect(renderThread, SIGNAL(frameTimeMetricsReady(int64_t, int64_t, int64_t, int64_t)),
            runTimeDataModel.get(), SLOT(updateData(int64_t, int64_t, int64_t, int64_t)));
    connect(window, &QQuickWindow::afterRendering, pagView, &PAGView::flush);
  }

  auto* pagxView = qobject_cast<PAGXView*>(contentView);
  if (pagxView != nullptr) {
    RenderThread* renderThread = pagxView->getRenderThread();
    connect(renderThread, SIGNAL(renderTimeReady(int64_t, int64_t, int64_t)),
            runTimeDataModel.get(), SLOT(updatePAGXRenderTime(int64_t, int64_t, int64_t)));
    connect(window, &QQuickWindow::afterRendering, pagxView,
            static_cast<void (PAGXView::*)() const>(&PAGXView::flush));
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
  pagView = qobject_cast<PAGView*>(contentView);

  connect(window, SIGNAL(closing(QQuickCloseEvent*)), this, SLOT(onPAGViewerDestroyed()),
          Qt::QueuedConnection);

  connect(windowHelper.get(), &PAGWindowHelper::contentViewChanged, this,
          &PAGWindow::onContentViewChanged);

  connectBaseContentViewSignals();
  connectFormatSpecificSignals();
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
