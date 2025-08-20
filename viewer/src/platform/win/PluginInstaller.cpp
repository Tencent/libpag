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
#include <shlobj.h>
#include <windows.h>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryFile>

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

  // Check common AE installation paths
  QStringList basePaths = {"C:/Program Files/Adobe", "C:/Program Files (x86)/Adobe",
                           "D:/Program Files/Adobe", "D:/Program Files (x86)/Adobe"};

  for (const QString& basePath : basePaths) {
    for (int year = 2017; year <= 2030; ++year) {
      QString aePath = QString("%1/Adobe After Effects %2/Support Files").arg(basePath).arg(year);
      if (QDir(aePath).exists() && QFile::exists(aePath + "/AfterFX.exe")) {
        paths << aePath;
      }

      // Also check CC versions
      QString ccPath =
          QString("%1/Adobe After Effects CC %2/Support Files").arg(basePath).arg(year);
      if (QDir(ccPath).exists() && QFile::exists(ccPath + "/AfterFX.exe")) {
        paths << ccPath;
      }
    }
  }

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
      return false;  // 保持原有返回值，由调用者处理错误类型
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
