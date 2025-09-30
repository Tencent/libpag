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
#include <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>
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
@interface NSString (QtBridge)
+ (NSString*)fromQString:(const QString&)qstr;
@end

@implementation NSString (QtBridge)
+ (NSString*)fromQString:(const QString&)qstr {
  return [NSString stringWithUTF8String:qstr.toUtf8().constData()];
}
@end

namespace pag {
static constexpr int cmdBufSize = 4096;

bool PluginInstaller::checkAeRunning() {
  NSArray<NSRunningApplication*>* runningApps = [[NSWorkspace sharedWorkspace] runningApplications];

  if (!runningApps) {
    return false;
  }
  for (NSRunningApplication* app in runningApps) {
    NSString* bundleIdentifier = [app bundleIdentifier];

    if (bundleIdentifier) {
      QString bundleId = QString::fromNSString(bundleIdentifier);

      if (bundleId == "com.adobe.AfterEffects" || bundleId.startsWith("com.adobe.AfterEffects")) {
        return true;
      }
    }
  }

  return false;
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

QStringList detectSpecialVersion() {
  QStringList specialPaths;

  QString latestPath = "/Applications/Adobe After Effects/Adobe After Effects.app";
  if (QDir(latestPath).exists()) {
    specialPaths << latestPath;
  }

  QString betaPath = "/Applications/Adobe After Effects (Beta)/Adobe After Effects (Beta).app";
  if (QDir(betaPath).exists()) {
    specialPaths << betaPath;
  }

  return specialPaths;
}

QStringList PluginInstaller::getAeInstallPaths() {
  QStringList paths;
  int currentYear = QDate::currentDate().year();
  int startYear = qMax(minSupportedYear, currentYear - 10);
  int endYear = qMin(maxSupportedYear, currentYear + 2);

  for (int year = startYear; year <= endYear; ++year) {
    QString aePath =
        QString("/Applications/Adobe After Effects %1/Adobe After Effects %1.app").arg(year);
    if (QDir(aePath).exists()) {
      paths << aePath;
    }

    QString ccPath =
        QString("/Applications/Adobe After Effects CC %1/Adobe After Effects CC %1.app").arg(year);
    if (QDir(ccPath).exists()) {
      paths << ccPath;
    }
  }

  paths << detectSpecialVersion();
  return paths;
}

QString PluginInstaller::getPluginFullName(const QString& pluginName) const {
  if (pluginName == "com.tencent.pagconfig") {
    return pluginName; // Directory, no extension
  } else if (pluginName == "H264EncoderTools") {
    return pluginName; // Directory, no extension
  } else {
    return pluginName + ".plugin";
  }
}

QString PluginInstaller::getPluginSourcePath(const QString& pluginName) const {
  QString resourcesPath;
  NSBundle* bundle = [NSBundle mainBundle];
  NSString* bundleResourcesPath = [bundle resourcePath];
  if (bundleResourcesPath) {
    resourcesPath = QString::fromNSString(bundleResourcesPath);
  } else {
    resourcesPath = QDir(QCoreApplication::applicationDirPath()).filePath("../Resources");
  }

  QString fullName = getPluginFullName(pluginName);
  return resourcesPath + "/" + fullName;
}

QString PluginInstaller::getPluginInstallPath(const QString& pluginName) const {
  QString fullName = getPluginFullName(pluginName);

  if (pluginName == "com.tencent.pagconfig") {
    return "/Library/Application Support/Adobe/CEP/extensions/" + fullName;
  } else if (pluginName == "H264EncoderTools") {
    QString roaming = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return roaming + "/H264EncoderTools/" + fullName;
  } else {
    return "/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/" + fullName;
  }
}

QString PluginInstaller::getPluginVersionString(const QString& pluginPath) const {
  QString plistPath;

  // Handle different plugin types
  if (pluginPath.endsWith(".plugin")) {
    plistPath = pluginPath + "/Contents/Info.plist";
  } else {
    // For directory-based plugins like com.tencent.pagconfig, check if it has a .plugin subdirectory
    QString pluginDirPath = pluginPath + ".plugin";
    if (QDir(pluginDirPath).exists()) {
      plistPath = pluginDirPath + "/Contents/Info.plist";
    } else {
      // Fallback: try to find any .plugin directory in the path
      QDir pluginDir(pluginPath);
      QStringList entries = pluginDir.entryList(QDir::Dirs);
      for (const QString& entry : entries) {
        if (entry.endsWith(".plugin")) {
          plistPath = pluginPath + "/" + entry + "/Contents/Info.plist";
          break;
        }
      }
    }
  }

  if (!QFile::exists(plistPath)) {
    return QString();
  }

  QSettings plist(plistPath, QSettings::NativeFormat);
  return plist.value("CFBundleVersion").toString();
}

bool PluginInstaller::executeWithPrivileges(const QString& command) const {
  QProcess process;
  QStringList args;

  QString enhancedCommand = command;
  enhancedCommand.replace("cp -r ", "ditto ");

  QStringList commandParts = enhancedCommand.split(" && ");
  for (int i = 0; i < commandParts.size(); ++i) {
    if (commandParts[i].startsWith("ditto ")) {
      QRegularExpression re("ditto '([^']+)' '([^']+)'");
      QRegularExpressionMatch match = re.match(commandParts[i]);
      if (match.hasMatch()) {
        QString sourcePath = match.captured(1);
        QString targetDir = match.captured(2);
        QString sourceFileName = QFileInfo(sourcePath).fileName();
        QString fullTargetPath = targetDir + "/" + sourceFileName;
        commandParts[i] += QString(" && xattr -cr '%1' 2>/dev/null || true").arg(fullTargetPath);
      }
    }
  }
  enhancedCommand = commandParts.join(" && ");

  QString escapedCommand = enhancedCommand;
  escapedCommand.replace("\"", "\\\"");
  escapedCommand.replace("\\", "\\\\");

  QString appleScriptCommand =
      QString("do shell script \"%1\" with administrator privileges").arg(escapedCommand);

  args << "-e" << appleScriptCommand;

  process.start("osascript", args);
  process.waitForFinished(30000);
  if (process.exitCode() != 0) {
    QString error = process.readAllStandardError();

    QString fallbackCommand = command;
    // fallbackCommand.replace("cp -r ", "cp -r ");

    escapedCommand = fallbackCommand;
    escapedCommand.replace("\"", "\\\"");
    escapedCommand.replace("\\", "\\\\");

    QString fallbackAppleScript =
        QString("do shell script \"%1\" with administrator privileges").arg(escapedCommand);

    args.clear();
    args << "-e" << fallbackAppleScript;

    process.start("osascript", args);

    if (process.exitCode() != 0) {
      error = process.readAllStandardError();
      return false;
    }
  }

  return true;
}

bool PluginInstaller::copyPluginFiles(const QStringList& plugins, bool force) const {
  Q_UNUSED(force);
  QStringList commands;

  for (const QString& plugin : plugins) {
    QString source = getPluginSourcePath(plugin);
    QString target = getPluginInstallPath(plugin);

    if (!QFile::exists(source)) {
      return false;
    }

    QString targetDir = QFileInfo(target).absolutePath();
    QString mkdirCmd = QString("mkdir -p '%1'").arg(targetDir);
    QString rmCmd = QString("rm -rf '%1'").arg(target);
    QString cpCmd;

    if (plugin == "com.tencent.pagconfig") {
      cpCmd = QString("cp -r '%1' '%2'").arg(source, targetDir);
    } else {
      cpCmd = QString("cp -r '%1' '%2'").arg(source, targetDir);
    }

    commands << mkdirCmd;
    commands << rmCmd;
    commands << cpCmd;
  }

  if (commands.isEmpty()) {
    return true;
  }

  char qtResourceCmd[cmdBufSize] = {0};
  CopyQtResource(qtResourceCmd, sizeof(qtResourceCmd));
  if (strlen(qtResourceCmd) > 0) {
    commands << QString::fromUtf8(qtResourceCmd).trimmed();
  }

  QString fullCommand = commands.join(" && ");
  return executeWithPrivileges(fullCommand);
}

bool PluginInstaller::removePluginFiles(const QStringList& plugins) const {
  QStringList commands;

  for (const QString& plugin : plugins) {
    QString target = getPluginInstallPath(plugin);
    commands << QString("rm -rf '%1'").arg(target);
  }

  if (commands.isEmpty()) {
    return true;
  }

  char deleteQtResourceCmd[cmdBufSize] = {0};
  DeleteQtResource(deleteQtResourceCmd, sizeof(deleteQtResourceCmd));
  if (strlen(deleteQtResourceCmd) > 0) {
    commands << QString::fromUtf8(deleteQtResourceCmd).trimmed();
  }

  QString fullCommand = commands.join(" && ");
  return executeWithPrivileges(fullCommand);
}

void PluginInstaller::CopyQtResource(char cmd[], int cmdSize) const {
  NSString* copyQtShellPath = [[NSBundle mainBundle] pathForResource:@"copy_qt_resource"
                                                              ofType:@"sh"];
  if (copyQtShellPath != nil) {
    snprintf(cmd + strlen(cmd), cmdSize - strlen(cmd), "sh '%s'\n", [copyQtShellPath UTF8String]);
  }
}

void PluginInstaller::DeleteQtResource(char cmd[], int cmdSize) const {
  NSString* deleteShellPath = [[NSBundle mainBundle] pathForResource:@"delete_qt_resource"
                                                              ofType:@"sh"];
  if (deleteShellPath != nil) {
    snprintf(cmd + strlen(cmd), cmdSize - strlen(cmd), "sh '%s'\n", [deleteShellPath UTF8String]);
  }
}

void PluginInstaller::setYearRange(int minYear, int maxYear) {
  if (minYear > maxYear) {
    qWarning() << "Invalid year range:" << minYear << "-" << maxYear;
    return;
  }

  minSupportedYear = qMax(2010, minYear);
  maxSupportedYear = qMin(2050, maxYear);
}

}  // namespace pag
