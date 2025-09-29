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

#pragma once

#include <QQmlApplicationEngine>
#include <QString>
#include <QTranslator>
#include "PAGView.h"
#include "PAGWindowHelper.h"
#include "editing/PAGEditAttributeModel.h"
#include "editing/PAGImageLayerModel.h"
#include "editing/PAGImageProvider.h"
#include "editing/PAGTextLayerModel.h"
#include "editing/PAGTreeViewModel.h"
#include "profiling/PAGBenchmarkModel.h"
#include "profiling/PAGRunTimeDataModel.h"

namespace pag {
class PAGWindow : public QObject {
  Q_OBJECT
 public:
  explicit PAGWindow(QObject* parent = nullptr);

  Q_SIGNAL void destroyWindow(PAGWindow* window);

  Q_SLOT void openFile(QString path);
  Q_SLOT void onPAGViewerDestroyed();

  void open();
  QString getFilePath();
  QQmlApplicationEngine* getEngine();
  bool isUseEnglish();

  static QList<PAGWindow*> AllWindows;

 private:
  QString filePath = "";
  QQuickWindow* window = nullptr;
  PAGView* pagView = nullptr;
  std::unique_ptr<QTranslator> translator = nullptr;
  std::unique_ptr<PAGWindowHelper> windowHelper = nullptr;
  std::unique_ptr<QQmlApplicationEngine> engine = nullptr;
  std::unique_ptr<PAGTreeViewModel> treeViewModel = nullptr;
  std::unique_ptr<PAGRunTimeDataModel> runTimeDataModel = nullptr;
  std::unique_ptr<PAGEditAttributeModel> editAttributeModel = nullptr;
  std::unique_ptr<PAGTextLayerModel> textLayerModel = nullptr;
  std::unique_ptr<PAGImageLayerModel> imageLayerModel = nullptr;
  std::unique_ptr<PAGBenchmarkModel> benchmarkModel = nullptr;
};

}  // namespace pag
