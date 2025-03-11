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
#ifndef EXPORTPROGRESSLISTWINDOW_H
#define EXPORTPROGRESSLISTWINDOW_H
#include <QtCore/QEventLoop>
#include <QtQml/QQmlApplicationEngine>
#include <QtQuick/QQuickWindow>
#include <QtWidgets/QListView>
#include <QtWidgets/QTableView>
#include "src/ui/qt/ExportProgressListWindow/ProgressListModel.h"
#include "src/ui/qt/Progress/ProgressBase.h"
#include "src/utils/Context.h"

class ExportProgressListWindow final : public QObject {
  Q_OBJECT

 public:
  explicit ExportProgressListWindow(std::vector<std::shared_ptr<ExportProgressItem>> progressItems,
                                    QWidget* parent = nullptr);
  ~ExportProgressListWindow() override;
  void showProgressListWindow();
  void startExport();
  void hide() const;
  void setProgress(int index, float progressValue) const;
  void exportSuccess(int index);
  void exportWrong(int index, const std::string& errorInfo) const;
  void exportAllProgressFinish();
  int progressCount() const;
  Q_INVOKABLE void show() const;
  Q_INVOKABLE void exit();

  Q_SIGNAL void successProgressCountChanged();
  Q_SIGNAL void allProgressFinished();

  bool isActive();

 public Q_SLOTS:
  void goToPreviewDialog(const QModelIndex& index);  // now mock
  void onCancelBtnClick();

 private:
  QApplication* app = nullptr;
  QQmlApplicationEngine* engine = nullptr;
  QQuickWindow* window = nullptr;
  pagexporter::Context* context = nullptr;
  std::shared_ptr<AEResource> aeResource;
  ProgressListModel* progressListModel = nullptr;
  bool exportFinish = false;
  bool earlyExit = false;
  ProgressBase* currentExportProgress = nullptr;
  std::vector<std::shared_ptr<ExportProgressItem>> progressItems;

  void initProgressListView();  // now mock
  void registerConnect();
};

class ProgressItemWrapper final : public ProgressBase {
 public:
  ProgressItemWrapper(ExportProgressListWindow* progressWindow, int index);
  ~ProgressItemWrapper() override;
  void setProgress(double progressValue) override;
  void setProgressTips(const QString& tip) override;
  void setProgressTitle(const QString& title) override;

  int index = -1;
  ExportProgressListWindow* progressWindow;
};

#endif  //EXPORTPROGRESSLISTWINDOW_H
