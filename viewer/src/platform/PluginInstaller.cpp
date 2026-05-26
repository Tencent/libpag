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
}

bool PluginInstaller::hasUpdate() const {
  QStringList plugins = getPluginList();

  for (const QString& pluginName : plugins) {
    QString sourcePath = getPluginSourcePath(pluginName);
    QString installPath = getPluginInstallPath(pluginName);
    if (!QFile::exists(sourcePath)) {
      continue;
    }

    // Only flag an update when we can actually compare versions and the source is strictly
    // newer. A zero result from getPluginVersion() means either the file is missing or the
    // version string is unparseable (e.g. dev builds that ship a placeholder like ".." in
    // Info.plist) — in both cases we skip rather than risk spamming the user.
    auto sourceVersion = getPluginVersion(sourcePath);
    auto installedVersion = getPluginVersion(installPath);
    if (sourceVersion == 0 || installedVersion == 0) {
      continue;
    }
    if (sourceVersion > installedVersion) {
      return true;
    }
  }

  return false;
}

InstallResult PluginInstaller::installPlugin() {
  bool pluginInstalled = isPluginInstalled();
  bool hasUpdateAvailable = hasUpdate();

  if (!pluginInstalled) {
    if (!requestConfirmation(installPromptTitle(), installPromptMessage())) {
      return InstallResult::UserCancelled;
    }
  } else if (hasUpdateAvailable) {
    if (!requestConfirmation(updatePromptTitle(), updatePromptMessage())) {
      return InstallResult::UserCancelled;
    }
  } else {
    if (!requestConfirmation(tr("Reinstall Adobe After Effects Plug-in"),
                             tr("The plugins are already installed with the latest version. "
                                "Do you want to reinstall them?"))) {
      return InstallResult::UserCancelled;
    }
  }

  return performInstall();
}

void PluginInstaller::checkPluginOnStartup() {
  // Idempotent: only run on the first call per session. Each call shows a modal, so allowing
  // repeated calls would let any unintended QML re-trigger spam the user.
  if (startupCheckTriggered) {
    return;
  }
  startupCheckTriggered = true;

  // Read versions only for diagnostic logging — install/update decisions go through
  // isPluginInstalled() and hasUpdate() so the startup path stays consistent with
  // installPlugin() and automatically covers any plugin in pluginInfoList().
  QString sourceVersion = getPluginVersionAt(getPluginSourcePath(PrimaryPluginName));
  QString installedVersion = getPluginVersionAt(getPluginInstallPath(PrimaryPluginName));

  if (!isPluginInstalled()) {
    if (sourceVersion.isEmpty()) {
      // No bundled plugin to install — nothing we can do.
      qDebug() << "[PluginInstaller] startup: plugin not installed and no bundled source";
      return;
    }
    qDebug() << "[PluginInstaller] startup: plugin not installed, sourceVersion=" << sourceVersion;
    promptAndInstall(installPromptTitle(), installPromptMessage());
    return;
  }

  if (hasUpdate()) {
    qDebug() << "[PluginInstaller] startup: update available, source=" << sourceVersion
             << "installed=" << installedVersion;
    promptAndInstall(updatePromptTitle(), updatePromptMessage());
    return;
  }

  qDebug() << "[PluginInstaller] startup: plugin up to date, version=" << installedVersion;
}

void PluginInstaller::promptAndInstall(const QString& title, const QString& message) {
  if (!requestConfirmation(title, message)) {
    qDebug() << "[PluginInstaller] user declined prompt";
    return;
  }
  performInstall();
}

QString PluginInstaller::installPromptTitle() {
  return tr("Install Adobe After Effects Plug-in");
}

QString PluginInstaller::installPromptMessage() {
  return tr("Do you want to install the Adobe After Effects plugins?");
}

QString PluginInstaller::updatePromptTitle() {
  return tr("Update Adobe After Effects Plug-in");
}

QString PluginInstaller::updatePromptMessage() {
  return tr(
      "A new version of the Adobe After Effects plugin is available. "
      "Would you like to update it now?");
}

InstallResult PluginInstaller::performInstall() {
  while (checkAeRunning()) {
    if (!requestConfirmation(tr("Please close Adobe After Effects"),
                             tr("Adobe After Effects is currently running. Please close it and "
                                "click OK to continue, or Cancel to abort."))) {
      return InstallResult::AeRunning;
    }
  }

  QStringList plugins = getPluginList();
  for (const QString& plugin : plugins) {
    QString sourcePath = getPluginSourcePath(plugin);
    if (!QFile::exists(sourcePath)) {
      showMessage(tr("Installation Failed"), tr("Plugin source not found: %1").arg(sourcePath),
                  true);
      return InstallResult::SourceNotFound;
    }
  }

  bool success = copyPluginFiles(plugins);

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

InstallResult PluginInstaller::uninstallPlugin() {
  if (!requestConfirmation(tr("Uninstall Plugins"),
                           tr("Are you sure you want to uninstall the selected plugins?"))) {
    return InstallResult::UserCancelled;
  }

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
  return getPluginVersionAt(getPluginInstallPath(PrimaryPluginName));
}

QString PluginInstaller::getPluginVersionAt(const QString& path) const {
  if (!QFile::exists(path)) {
    return QString();
  }
  QString version;
  if (getPluginVersionString(path, version) == VersionResult::Success) {
    return version;
  }
  return QString();
}

bool PluginInstaller::isPluginInstalled() const {
  return QFile::exists(getPluginInstallPath(PrimaryPluginName));
}

const QList<PluginInfo>& PluginInstaller::pluginInfoList() {
  // The "PAGExporter" entry must stay in sync with PluginInstaller::PrimaryPluginName,
  // which is the plugin used for startup install/update detection.
  static const QList<PluginInfo> list = {
      {"PAGExporter", PluginPermission::Admin},
      {"H264EncoderTools", PluginPermission::User},
  };
  return list;
}

QStringList PluginInstaller::getPluginList() const {
  QStringList names;
  for (const auto& info : pluginInfoList()) {
    names << info.name;
  }
  return names;
}

PluginPermission PluginInstaller::getPluginPermission(const QString& pluginName) const {
  for (const auto& info : pluginInfoList()) {
    if (info.name == pluginName) {
      return info.permission;
    }
  }
  return PluginPermission::Admin;
}

int64_t PluginInstaller::getPluginVersion(const QString& pluginPath) const {
  QString versionString;
  if (getPluginVersionString(pluginPath, versionString) != VersionResult::Success) {
    return 0;
  }

  Version version(versionString);

  static constexpr int MaxVersionField = 0xFFFF;
  if (version.major < 0 || version.major > MaxVersionField || version.minor < 0 ||
      version.minor > MaxVersionField || version.patch < 0 || version.patch > MaxVersionField ||
      version.build < 0 || version.build > MaxVersionField) {
    return 0;
  }

  return (static_cast<int64_t>(version.major) << 48) | (static_cast<int64_t>(version.minor) << 32) |
         (static_cast<int64_t>(version.patch) << 16) | (static_cast<int64_t>(version.build) << 0);
}

PluginInstaller::Version::Version(const QString& versionStr) {
  QStringList parts = versionStr.split('.');
  if (parts.size() >= 1) {
    major = parts[0].toInt();
  }
  if (parts.size() >= 2) {
    minor = parts[1].toInt();
  }
  if (parts.size() >= 3) {
    patch = parts[2].toInt();
  }
  if (parts.size() >= 4) {
    build = parts[3].toInt();
  }
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
