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

#include <QApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <memory>
#include <string>

namespace exporter {

class PAGViewerInstallModel : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString title READ getTitle NOTIFY titleChanged)
  Q_PROPERTY(QString message READ getMessage NOTIFY messageChanged)
  Q_PROPERTY(QString buttonText READ getButtonText NOTIFY buttonTextChanged)
  Q_PROPERTY(bool showCancelButton READ getShowCancelButton NOTIFY showCancelButtonChanged)
  Q_PROPERTY(InstallStage currentStage READ getCurrentStage NOTIFY stageChanged)

 public:
  enum InstallStage { Confirm, Installing, Success, Failed };
  Q_ENUM(InstallStage)

  explicit PAGViewerInstallModel(QObject* parent = nullptr);
  ~PAGViewerInstallModel();

  bool showInstallDialog(const std::string& pagFilePath);

  Q_INVOKABLE void onConfirmInstall();
  Q_INVOKABLE void onCancel();
  Q_INVOKABLE void onComplete();

  QString getTitle() const {
    return title;
  }
  QString getMessage() const {
    return message;
  }
  QString getButtonText() const {
    return buttonText;
  }

  bool getShowCancelButton() const {
    return showCancelButton;
  }
  InstallStage getCurrentStage() const {
    return currentStage;
  }

 protected:
  void updateStage(InstallStage stage);
  void executeInstallation();
  void startPreview(const std::string& filePath);

 Q_SIGNALS:
  void titleChanged();
  void messageChanged();
  void buttonTextChanged();
  void showCancelButtonChanged();
  void stageChanged();

 private:
  InstallStage currentStage = Confirm;
  QString title;
  QString message;
  QString buttonText;
  bool showCancelButton = true;
  std::string pendingFilePath = "";
  std::unique_ptr<QApplication> app = nullptr;
  std::unique_ptr<QQmlApplicationEngine> engine = nullptr;
  QQuickWindow* window = nullptr;
  bool dialogResult = false;
};

}  // namespace exporter
