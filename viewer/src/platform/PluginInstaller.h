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

enum class InstallResult {
  Success,
  SourceNotFound,
  PermissionDenied,
  AeRunning,
  UserCancelled,
  UnknownError
};

enum class VersionResult {
  Success,
  FileNotFound,
  DirectoryNotFound,
  NoPluginFiles,
  VersionInfoUnavailable
};

/// Indicates the privilege level required to install/uninstall a plugin.
/// Each value represents a distinct privilege model. Do not introduce hybrid values —
/// callers rely on a binary User-vs-Admin decision.
enum class PluginPermission {
  /// The plugin installs to user-writable directories and needs no elevation.
  User,
  /// The plugin installs to system directories and requires admin privileges.
  Admin
};

struct PluginInfo {
  QString name;
  PluginPermission permission;
};

class PluginInstaller : public QObject {
  Q_OBJECT

 public:
  static constexpr int DefaultMinYear = 2017;
  static constexpr int DefaultMaxYear = 2030;

  explicit PluginInstaller(QObject* parent = nullptr);

  bool hasUpdate() const;
  InstallResult installPlugin();
  InstallResult uninstallPlugin();

  /// Run once at app startup to keep the AE plugin in sync with the bundled version.
  /// Behavior depends on the installed state:
  ///   - Not installed: prompts the user to install.
  ///   - Installed but version differs: prompts the user to update.
  ///   - Installed and versions match: no-op.
  /// Idempotent: subsequent calls within the same session are silent.
  void checkPluginOnStartup();

  QString getInstalledVersion() const;
  bool isPluginInstalled() const;

  void setYearRange(int minYear, int maxYear);

  static QString GetH264EncoderToolsExePath();

 Q_SIGNALS:
  void updateChecked(bool hasUpdate);
  void installCompleted(InstallResult result, const QString& message);
  void uninstallCompleted(InstallResult result, const QString& message);

 private:
  /// The plugin used as the source of truth for "is anything installed?" and version
  /// comparison during startup checks. Must be present in pluginInfoList().
  static constexpr const char* PrimaryPluginName = "PAGExporter";

  InstallResult performInstall();
  void promptAndInstall(const QString& title, const QString& message);
  QString getPluginVersionAt(const QString& path) const;
  bool checkAeRunning();
  bool requestConfirmation(const QString& title, const QString& message);
  void showMessage(const QString& title, const QString& message, bool isWarning = false);

  /// Single source of truth for the install/update prompts. Shared between the manual
  /// installPlugin() path and the startup checkPluginOnStartup() path so wording stays in
  /// sync — keep this list aligned with the matching messages in Chinese.ts.
  static QString InstallPromptTitle();
  static QString InstallPromptMessage();
  static QString UpdatePromptTitle();
  static QString UpdatePromptMessage();

  QStringList getAeInstallPaths();
  QString getPluginSourcePath(const QString& pluginName) const;
  QString getPluginInstallPath(const QString& pluginName) const;
  QString getPluginFullName(const QString& pluginName) const;
  PluginPermission getPluginPermission(const QString& pluginName) const;
  int getAeVersionForPath(const QString& aePath) const;

  VersionResult getPluginVersionString(const QString& pluginPath, QString& version) const;
  bool executeWithPrivileges(const QString& command) const;

  bool copyPluginFiles(const QStringList& plugins) const;
  bool removePluginFiles(const QStringList& plugins) const;

  int64_t getPluginVersion(const QString& pluginPath) const;
  QStringList getPluginList() const;
  static const QList<PluginInfo>& pluginInfoList();

  QString getQtResourceDir() const;
  bool shouldExcludeFile(const QString& fileName) const;
  bool shouldExcludeDir(const QString& dirName) const;

  void storeViewerPathForPlugin() const;

  bool copyUserPluginWithRetry(const QString& plugin, int maxRetries = 5) const;

  void CopyQtResource(char cmd[], int cmdSize) const;
  void DeleteQtResource(char cmd[], int cmdSize) const;

  struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;
    int build = 0;

    explicit Version(const QString& versionStr);
    bool operator>(const Version& other) const;
    QString toString() const;
  };

  int minSupportedYear = DefaultMinYear;
  int maxSupportedYear = DefaultMaxYear;
  bool startupCheckTriggered = false;
};

}  // namespace pag
