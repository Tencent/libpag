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
#include <memory>
#include "platform/PluginInstaller.h"

namespace pag {

class PluginInstaller;

class PluginInstallerModel : public QObject {
  Q_OBJECT

 public:
  explicit PluginInstallerModel(QObject* parent = nullptr);

  Q_INVOKABLE bool hasUpdate() const;

  Q_INVOKABLE InstallResult installPlugin();

  Q_INVOKABLE InstallResult uninstallPlugin();

  Q_INVOKABLE QString getInstalledVersion() const;

  Q_INVOKABLE bool isPluginInstalled() const;

 Q_SIGNALS:
  void updateCheckCompleted(bool hasUpdate);
  void installationCompleted(InstallResult result, const QString& message);
  void uninstallationCompleted(InstallResult result, const QString& message);

 private:
  std::unique_ptr<PluginInstaller> installer = nullptr;
};

}  // namespace pag
