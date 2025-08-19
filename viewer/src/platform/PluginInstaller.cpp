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

#include "PluginInstaller.h"
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QStandardPaths>

namespace pag {

PluginInstaller::PluginInstaller(QObject* parent) : QObject(parent) {
  // Initialize any required resources or settings here
}

PluginInstaller::~PluginInstaller() = default;

bool PluginInstaller::hasUpdate() const {
  QString pluginName = "PAGExporter";
  QString sourcePath = getPluginSourcePath(pluginName);
  QString installPath = getPluginInstallPath(pluginName);

  if (!QFile::exists(sourcePath)) {
    return false;
  }

  int64_t sourceVersion = getPluginVersion(sourcePath);
  int64_t installedVersion = getPluginVersion(installPath);

  return sourceVersion > installedVersion;
}

InstallResult PluginInstaller::installPlugins(bool force) {
  if (!force && !hasUpdate()) {
    Q_EMIT updateChecked(false);
    showMessage(tr("No Updates"), tr("All plugins are already up to date."));
    return InstallResult::Success;
  }

  if (!force) {
    if (!requestConfirmation(
            tr("Update Adobe After Effects Plug-in"),
            tr("New plugin versions are available. Do you want to install them?"))) {
      return InstallResult::UnknownError;
    }
  }

  // Check if AE is running and wait for it to close
  while (checkAeRunning()) {
    if (!requestConfirmation(tr("Please close Adobe After Effects"),
                             tr("Adobe After Effects is currently running. Please close it and "
                                "click OK to continue, or Cancel to abort."))) {
      return InstallResult::AeRunning;
    }
  }

  QStringList plugins = getPluginList();

  // Check if all source files exist before attempting installation
  for (const QString& plugin : plugins) {
    QString sourcePath = getPluginSourcePath(plugin);
    if (!QFile::exists(sourcePath)) {
      showMessage(tr("Installation Failed"), tr("Plugin source not found: %1").arg(sourcePath),
                  true);
      return InstallResult::SourceNotFound;
    }
  }

  bool success = copyPluginFiles(plugins, force);

  if (success) {
    showMessage(tr("Installation Complete"),
                tr("Adobe After Effects plugins have been successfully installed!"));
    Q_EMIT installCompleted(InstallResult::Success,
                            tr("Adobe After Effects plug-in installed successfully."));
    return InstallResult::Success;
  } else {
    showMessage(tr("Installation Failed"),
                tr("Failed to install Adobe After Effects plugins. Please check permissions."),
                true);
    Q_EMIT installCompleted(
        InstallResult::PermissionDenied,
        tr("Failed to install Adobe After Effects plug-in due to permission issues."));
    return InstallResult::PermissionDenied;
  }
}

InstallResult PluginInstaller::uninstallPlugins() {
  if (!requestConfirmation(tr("Uninstall Plugins"),
                           tr("Are you sure you want to uninstall the selected plugins?"))) {
    return InstallResult::UnknownError;
  }

  // Check if AE is running and wait for it to close
  while (checkAeRunning()) {
    if (!requestConfirmation(tr("Please close Adobe After Effects"),
                             tr("Adobe After Effects is currently running. Please close it and "
                                "click OK to continue, or Cancel to abort."))) {
      return InstallResult::AeRunning;
    }
  }

  QStringList plugins = getPluginList();
  bool success = removePluginFiles(plugins);

  if (success) {
    showMessage(tr("Uninstallation Complete"), tr("Plugins have been successfully removed!"));
    Q_EMIT uninstallCompleted(InstallResult::Success, tr("Uninstalled AE Plug-in Successfully"));
    return InstallResult::Success;
  } else {
    showMessage(tr("Uninstallation Failed"),
                tr("Failed to uninstall plugins. Please check permissions."), true);
    Q_EMIT uninstallCompleted(InstallResult::PermissionDenied,
                              tr("Failed to uninstall AE Plug-in due to permission issues."));
    return InstallResult::PermissionDenied;
  }
}

QString PluginInstaller::getInstalledVersion() const {
  QString pluginName = "PAGExporter";
  QString installPath = getPluginInstallPath(pluginName);
  if (!QFile::exists(installPath)) {
    return QString();
  }
  return getPluginVersionString(installPath);
}

bool PluginInstaller::isPluginInstalled() const {
  QString pluginName = "PAGExporter";
  return QFile::exists(getPluginInstallPath(pluginName));
}

QStringList PluginInstaller::getPluginList() const {
  return {"PAGExporter", "ImageFillRule", "TextBackground", "com.tencent.pagconfig",
          "H264EncoderTools"};
}

int64_t PluginInstaller::getPluginVersion(const QString& pluginPath) const {
  QString versionString = getPluginVersionString(pluginPath);
  if (versionString.isEmpty()) {
    return 0;
  }

  Version version(versionString);
  return (static_cast<int64_t>(version.major) << 48) | (static_cast<int64_t>(version.minor) << 32) |
         (static_cast<int64_t>(version.patch) << 16) | (static_cast<int64_t>(version.build) << 0);
}

// Version struct implementation
PluginInstaller::Version::Version(const QString& versionStr) {
  QStringList parts = versionStr.split('.');
  if (parts.size() >= 1) major = parts[0].toInt();
  if (parts.size() >= 2) minor = parts[1].toInt();
  if (parts.size() >= 3) patch = parts[2].toInt();
  if (parts.size() >= 4) build = parts[3].toInt();
}

bool PluginInstaller::Version::operator>(const Version& other) const {
  if (major != other.major) return major > other.major;
  if (minor != other.minor) return minor > other.minor;
  if (patch != other.patch) return patch > other.patch;
  return build > other.build;
}

QString PluginInstaller::Version::toString() const {
  return QString("%1.%2.%3.%4").arg(major).arg(minor).arg(patch).arg(build);
}

}  // namespace pag
