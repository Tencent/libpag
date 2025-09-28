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
#include <QString>
#include <QStringList>
#include <cstdint>

namespace pag {

enum class InstallResult { Success, SourceNotFound, PermissionDenied, AeRunning, UnknownError };

class PluginInstaller : public QObject {
  Q_OBJECT

 public:
  static constexpr int DefaultMinYear = 2017;
  static constexpr int DefaultMaxYear = 2030;

  explicit PluginInstaller(QObject* parent = nullptr);
  ~PluginInstaller() override;

  bool hasUpdate() const;
  InstallResult installPlugins(bool force = false);
  InstallResult uninstallPlugins();

  QString getInstalledVersion() const;
  bool isPluginInstalled() const;

  void setYearRange(int minYear, int maxYear);

 Q_SIGNALS:
  void updateChecked(bool hasUpdate);
  void installCompleted(InstallResult result, const QString& message);
  void uninstallCompleted(InstallResult result, const QString& message);

 private:
  bool checkAeRunning();
  bool requestConfirmation(const QString& title, const QString& message);
  void showMessage(const QString& title, const QString& message, bool isWarning = false);

  QStringList getAeInstallPaths();
  QString getPluginSourcePath(const QString& pluginName) const;
  QString getPluginInstallPath(const QString& pluginName) const;

  QString getPluginVersionString(const QString& pluginPath) const;
  bool executeWithPrivileges(const QString& command) const;

  bool copyPluginFiles(const QStringList& plugins, bool force) const;
  bool removePluginFiles(const QStringList& plugins) const;

  int64_t getPluginVersion(const QString& pluginPath) const;
  QStringList getPluginList() const;

  void CopyQtResource(char cmd[], int cmdSize) const;
  void DeleteQtResource(char cmd[], int cmdSize) const;

  struct Version {
    int major = 0, minor = 0, patch = 0, build = 0;

    Version() = default;
    explicit Version(const QString& versionStr);
    bool operator>(const Version& other) const;
    QString toString() const;
  };

  int minSupportedYear = DefaultMinYear;
  int maxSupportedYear = DefaultMaxYear;
};

}  // namespace pag
