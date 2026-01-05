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

#include "PluginInstallerModel.h"
#include "platform/PluginInstaller.h"

namespace pag {

PluginInstallerModel::PluginInstallerModel(QObject* parent)
    : QObject(parent), installer(std::make_unique<PluginInstaller>(this)) {

  connect(installer.get(), &PluginInstaller::updateChecked, this,
          &PluginInstallerModel::updateCheckCompleted);
  connect(installer.get(), &PluginInstaller::installCompleted, this,
          &PluginInstallerModel::installationCompleted);
  connect(installer.get(), &PluginInstaller::uninstallCompleted, this,
          &PluginInstallerModel::uninstallationCompleted);
}

bool PluginInstallerModel::hasUpdate() const {
  return installer->hasUpdate();
}

InstallResult PluginInstallerModel::installPlugin() {
  return installer->installPlugin();
}

InstallResult PluginInstallerModel::uninstallPlugin() {
  return installer->uninstallPlugin();
}

QString PluginInstallerModel::getInstalledVersion() const {
  return installer->getInstalledVersion();
}

bool PluginInstallerModel::isPluginInstalled() const {
  return installer->isPluginInstalled();
}

}  // namespace pag
