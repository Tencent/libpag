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

#include "platform/PluginInstaller.h"
#include <shlobj.h>
#include <windows.h>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QRegularExpression>

namespace pag {

bool PluginInstaller::checkAeRunning() {
  QProcess process;
  process.start("tasklist", QStringList() << "/FI"
                                          << "IMAGENAME eq AfterFX.exe");
  process.waitForFinished(3000);  // 3 second timeout
  QString output = process.readAllStandardOutput();
  return output.contains("AfterFX.exe");
}

bool PluginInstaller::requestConfirmation(const QString& title, const QString& message) {
  return QMessageBox::question(nullptr, title, message, QMessageBox::Yes | QMessageBox::No) ==
         QMessageBox::Yes;
}

void PluginInstaller::showMessage(const QString& title, const QString& message, bool isWarning) {
  QMessageBox msgBox;
  msgBox.setWindowTitle(title);
  msgBox.setText(message);
  msgBox.setIcon(isWarning ? QMessageBox::Warning : QMessageBox::Information);
  msgBox.setStandardButtons(QMessageBox::Ok);
  msgBox.exec();
}

QStringList PluginInstaller::getAeInstallPaths() {
  QStringList paths;

  // Primary method: Check Windows Registry for accurate AE installations
  const QStringList registryPaths = {
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\Adobe",
    "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Adobe",
    "HKEY_CURRENT_USER\\SOFTWARE\\Adobe"
  };

  for (const QString& regPath : registryPaths) {
    QSettings registry(regPath, QSettings::NativeFormat);
    const QStringList adobeKeys = registry.childGroups();
    
    for (const QString& key : adobeKeys) {
      if (key.contains("After Effects", Qt::CaseInsensitive)) {
        QSettings aeRegistry(regPath + "\\" + key, QSettings::NativeFormat);
        QString installPath = aeRegistry.value("InstallPath").toString();
        if (!installPath.isEmpty()) {
          QString supportFilesPath = QDir::cleanPath(installPath + "/Support Files");
          if (QDir(supportFilesPath).exists() && QFile::exists(supportFilesPath + "/AfterFX.exe")) {
            paths << supportFilesPath;
          }
        }
      }
    }
  }

  // Fallback: Check Creative Cloud Uninstall registry
  QSettings ccRegistry("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", 
                      QSettings::NativeFormat);
  const QStringList uninstallKeys = ccRegistry.childGroups();
  
  for (const QString& key : uninstallKeys) {
    QSettings appRegistry("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + key, 
                         QSettings::NativeFormat);
    QString displayName = appRegistry.value("DisplayName").toString();
    if (displayName.contains("Adobe After Effects", Qt::CaseInsensitive)) {
      QString installLocation = appRegistry.value("InstallLocation").toString();
      if (!installLocation.isEmpty()) {
        QString supportFilesPath = QDir::cleanPath(installLocation + "/Support Files");
        if (QDir(supportFilesPath).exists() && QFile::exists(supportFilesPath + "/AfterFX.exe")) {
          if (!paths.contains(supportFilesPath)) {
            paths << supportFilesPath;
          }
        }
      }
    }
  }

  // Remove duplicates and sort by version
  paths.removeDuplicates();
  
  // Sort by version number extracted from path
  std::sort(paths.begin(), paths.end(), [](const QString& a, const QString& b) {
    QRegularExpression versionRegex("After Effects(?: CC)? (\\d+)");
    QRegularExpressionMatch matchA = versionRegex.match(a);
    QRegularExpressionMatch matchB = versionRegex.match(b);
    
    int versionA = matchA.hasMatch() ? matchA.captured(1).toInt() : 0;
    int versionB = matchB.hasMatch() ? matchB.captured(1).toInt() : 0;
    
    return versionA > versionB;
  });

  return paths;
}

QString PluginInstaller::getPluginSourcePath(const QString& pluginName) const {
  QString appDir = QCoreApplication::applicationDirPath();

  if (pluginName == "com.tencent.pagconfig") {
    return appDir + "/" + pluginName;
  } else if (pluginName == "H264EncoderTools") {
    return appDir + "/" + pluginName + ".exe";
  } else {
    return appDir + "/" + pluginName + ".aex";
  }
}

QString PluginInstaller::getPluginInstallPath(const QString& pluginName) const {
  if (pluginName == "com.tencent.pagconfig") {
    QString roaming = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return roaming + "/Adobe/CEP/extensions/" + pluginName;
  } else if (pluginName == "H264EncoderTools") {
    QString roaming = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return roaming + "/H264EncoderTools/" + pluginName + ".exe";
  } else {
    return "C:/Program Files/Adobe/Common/Plug-ins/7.0/MediaCore/" + pluginName + ".aex";
  }
}

QString PluginInstaller::getPluginVersionString(const QString& pluginPath) const {
  // For Windows, we might need to read version from file properties
  // For now, return empty string - this would need platform-specific implementation
  // to read version info from .aex files
  Q_UNUSED(pluginPath);
  return QString();
}

bool PluginInstaller::executeWithPrivileges(const QString& command) const {
  // Create temporary batch file
  QTemporaryFile tempFile;
  tempFile.setFileTemplate(QDir::tempPath() + "/install_plugin_XXXXXX.bat");
  tempFile.setAutoRemove(false);  // We'll remove it manually

  if (!tempFile.open()) {
    qDebug() << "Failed to create temporary batch file";
    return false;
  }

  QString batPath = tempFile.fileName();
  tempFile.write("@echo off\n");
  tempFile.write("chcp 65001 > nul\n");  // Set UTF-8 encoding
  tempFile.write(command.toUtf8());
  tempFile.write("\necho.\necho Press any key to continue...\npause > nul\n");
  tempFile.close();

  // Execute with elevated privileges
  HINSTANCE result = ShellExecuteW(
      nullptr, L"runas", L"cmd.exe",
      QString("/c \"%1\"").arg(QDir::toNativeSeparators(batPath)).toStdWString().c_str(), nullptr,
      SW_SHOW);

  // Clean up
  QFile::remove(batPath);

  return reinterpret_cast<intptr_t>(result) > 32;
}

bool PluginInstaller::copyPluginFiles(const QStringList& plugins, bool force) const {
  Q_UNUSED(force);
  QStringList commands;

  for (const QString& plugin : plugins) {
    QString source = getPluginSourcePath(plugin);
    QString target = getPluginInstallPath(plugin);

    if (!QFile::exists(source)) {
      qDebug() << "Plugin source not found:" << source;
      return false;
    }

    // Create target directory
    QString targetDir = QFileInfo(target).absolutePath();
    commands
        << QString("if not exist \"%1\" mkdir \"%1\"").arg(QDir::toNativeSeparators(targetDir));

    if (plugin == "com.tencent.pagconfig") {
      // Copy directory recursively
      commands << QString("xcopy /Y /E /I \"%1\" \"%2\"")
                      .arg(QDir::toNativeSeparators(source))
                      .arg(QDir::toNativeSeparators(target));
    } else {
      // Copy single file
      commands << QString("copy /Y \"%1\" \"%2\"")
                      .arg(QDir::toNativeSeparators(source))
                      .arg(QDir::toNativeSeparators(target));
    }
  }

  if (commands.isEmpty()) {
    return true;
  }

  QString fullCommand = commands.join("\n");
  return executeWithPrivileges(fullCommand);
}

bool PluginInstaller::removePluginFiles(const QStringList& plugins) const {
  QStringList commands;

  for (const QString& plugin : plugins) {
    QString target = getPluginInstallPath(plugin);

    if (plugin == "com.tencent.pagconfig") {
      // Remove directory
      commands
          << QString("if exist \"%1\" rmdir /S /Q \"%1\"").arg(QDir::toNativeSeparators(target));
    } else {
      // Remove file
      commands << QString("if exist \"%1\" del /F /Q \"%1\"").arg(QDir::toNativeSeparators(target));
    }
  }

  if (commands.isEmpty()) {
    return true;
  }

  QString fullCommand = commands.join("\n");
  return executeWithPrivileges(fullCommand);
}

}  // namespace pag
