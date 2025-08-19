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

bool PluginInstaller::checkAeRunning() {
  QProcess process;
  process.start("pgrep", QStringList() << "-f"
                                       << "Adobe After Effects");
  process.waitForFinished(3000);
  return process.exitCode() == 0 && !process.readAllStandardOutput().isEmpty();
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

  // Check common AE installation paths on macOS
  for (int year = 2017; year <= 2030; ++year) {
    QString aePath =
        QString("/Applications/Adobe After Effects %1/Adobe After Effects %1.app").arg(year);
    if (QDir(aePath).exists()) {
      paths << aePath;
    }

    // Also check CC versions
    QString ccPath =
        QString("/Applications/Adobe After Effects CC %1/Adobe After Effects CC %1.app").arg(year);
    if (QDir(ccPath).exists()) {
      paths << ccPath;
    }
  }

  return paths;
}

QString PluginInstaller::getPluginSourcePath(const QString& pluginName) const {
  QString resourcesPath;

  // Use bundle to get resources path
  NSBundle* bundle = [NSBundle mainBundle];
  NSString* bundleResourcesPath = [bundle resourcePath];
  if (bundleResourcesPath) {
    resourcesPath = QString::fromNSString(bundleResourcesPath);
    qDebug() << "PluginInstaller: Bundle resources path:" << resourcesPath;
  } else {
    // Fallback to relative path if bundle method fails
    resourcesPath = QDir(QCoreApplication::applicationDirPath()).filePath("../Resources");
    qDebug() << "PluginInstaller: Using fallback path:" << resourcesPath;
  }

  QString fullPath;
  if (pluginName == "com.tencent.pagconfig") {
    fullPath = resourcesPath + "/" + pluginName;
  } else if (pluginName == "H264EncoderTools") {
    fullPath = resourcesPath + "/" + pluginName;
  } else {
    fullPath = resourcesPath + "/" + pluginName + ".plugin";
  }

  qDebug() << "PluginInstaller: Plugin" << pluginName << "source path:" << fullPath;
  qDebug() << "PluginInstaller: Path exists:" << QFile::exists(fullPath);

  return fullPath;
}

QString PluginInstaller::getPluginInstallPath(const QString& pluginName) const {
  // 测试模式开关 - 取消下面这行的注释来启用测试目录
//#define PAG_PLUGIN_TEST_MODE

#ifdef PAG_PLUGIN_TEST_MODE
  // 硬编码的测试目录 - 请修改为你实际的测试路径
  static const QString kTestBasePath = "/Users/codywwang/Desktop/pag_plugin_test";

  if (pluginName == "com.tencent.pagconfig") {
    return kTestBasePath + "/Adobe/CEP/extensions/" + pluginName;
  } else if (pluginName == "H264EncoderTools") {
    return kTestBasePath + "/H264EncoderTools/" + pluginName;
  } else {
    return kTestBasePath + "/Adobe/MediaCore/" + pluginName + ".plugin";
  }
#else
  // 原有的系统目录逻辑
  if (pluginName == "com.tencent.pagconfig") {
    return "/Library/Application Support/Adobe/CEP/extensions/" + pluginName;
  } else if (pluginName == "H264EncoderTools") {
    QString roaming = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return roaming + "/H264EncoderTools/" + pluginName;
  } else {
    return "/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/" + pluginName +
           ".plugin";
  }
#endif
}

QString PluginInstaller::getPluginVersionString(const QString& pluginPath) const {
  QString plistPath = pluginPath.endsWith(".plugin") ? pluginPath + "/Contents/Info.plist"
                                                     : pluginPath + ".plugin/Contents/Info.plist";

  if (!QFile::exists(plistPath)) {
    return QString();
  }

  QSettings plist(plistPath, QSettings::NativeFormat);
  return plist.value("CFBundleVersion").toString();
}

bool PluginInstaller::executeWithPrivileges(const QString& command) const {
  QProcess process;
  QStringList args;

  // 添加详细的调试日志
  qDebug() << "PluginInstaller: Executing command with privileges:" << command;

  // 使用ditto代替cp，并在复制后清除扩展属性
  QString enhancedCommand = command;
  enhancedCommand.replace("cp -r ", "ditto ");

  // 在每个复制操作后添加清除扩展属性的命令
  // 修复：对复制的目标文件执行xattr，而不是目标目录
  QStringList commandParts = enhancedCommand.split(" && ");
  for (int i = 0; i < commandParts.size(); ++i) {
    if (commandParts[i].startsWith("ditto ")) {
      // 提取源文件和目标目录
      QRegularExpression re("ditto '([^']+)' '([^']+)'");
      QRegularExpressionMatch match = re.match(commandParts[i]);
      if (match.hasMatch()) {
        QString sourcePath = match.captured(1);
        QString targetDir = match.captured(2);

        // 构建完整的目标路径
        QString sourceFileName = QFileInfo(sourcePath).fileName();
        QString fullTargetPath = targetDir + "/" + sourceFileName;

        // 对复制后的文件清除扩展属性
        commandParts[i] += QString(" && xattr -cr '%1' 2>/dev/null || true").arg(fullTargetPath);
      }
    }
  }
  enhancedCommand = commandParts.join(" && ");
  qDebug() << "PluginInstaller: Enhanced command:" << enhancedCommand;

  // Escape the command properly for AppleScript
  QString escapedCommand = enhancedCommand;
  escapedCommand.replace("\"", "\\\"");
  escapedCommand.replace("\\", "\\\\");

  QString appleScriptCommand =
      QString("do shell script \"%1\" with administrator privileges").arg(escapedCommand);
  qDebug() << "PluginInstaller: AppleScript command:" << appleScriptCommand;

  args << "-e" << appleScriptCommand;

  process.start("osascript", args);
  bool finished = process.waitForFinished(300000);  // 30 second timeout

  qDebug() << "PluginInstaller: Process finished:" << finished;
  qDebug() << "PluginInstaller: Exit code:" << process.exitCode();
  qDebug() << "PluginInstaller: Standard output:" << process.readAllStandardOutput();
  qDebug() << "PluginInstaller: Standard error:" << process.readAllStandardError();

  if (process.exitCode() != 0) {
    QString error = process.readAllStandardError();
    qDebug() << "AppleScript execution failed:" << error;

    // 如果ditto失败，尝试使用cp作为备选方案
    qDebug() << "PluginInstaller: Trying fallback command...";
    QString fallbackCommand = command;
    fallbackCommand.replace("cp -r ", "cp -r ");

    escapedCommand = fallbackCommand;
    escapedCommand.replace("\"", "\\\"");
    escapedCommand.replace("\\", "\\\\");

    QString fallbackAppleScript =
        QString("do shell script \"%1\" with administrator privileges").arg(escapedCommand);
    qDebug() << "PluginInstaller: Fallback AppleScript command:" << fallbackAppleScript;

    args.clear();
    args << "-e" << fallbackAppleScript;

    process.start("osascript", args);
    bool fallbackFinished = process.waitForFinished(300000);

    qDebug() << "PluginInstaller: Fallback process finished:" << fallbackFinished;
    qDebug() << "PluginInstaller: Fallback exit code:" << process.exitCode();
    qDebug() << "PluginInstaller: Fallback standard output:" << process.readAllStandardOutput();
    qDebug() << "PluginInstaller: Fallback standard error:" << process.readAllStandardError();

    if (process.exitCode() != 0) {
      error = process.readAllStandardError();
      qDebug() << "Fallback command also failed:" << error;
      return false;
    }
  }

  qDebug() << "PluginInstaller: Command executed successfully";
  return true;
}

bool PluginInstaller::copyPluginFiles(const QStringList& plugins, bool force) const {
  Q_UNUSED(force);
  QStringList commands;

  qDebug() << "PluginInstaller: Starting to copy" << plugins.size() << "plugins";

  for (const QString& plugin : plugins) {
    QString source = getPluginSourcePath(plugin);
    QString target = getPluginInstallPath(plugin);

    // 添加调试信息输出
    qDebug() << "PluginInstaller: Installing plugin:" << plugin;
    qDebug() << "  Source path:" << source;
    qDebug() << "  Target path:" << target;

    if (!QFile::exists(source)) {
      qDebug() << "PluginInstaller: ERROR - Plugin source not found:" << source;
      return false;
    } else {
      qDebug() << "PluginInstaller: Source file/directory exists";
    }

    QString targetDir = QFileInfo(target).absolutePath();
    qDebug() << "PluginInstaller: Target directory:" << targetDir;

    // 构建命令
    QString mkdirCmd = QString("mkdir -p '%1'").arg(targetDir);
    QString rmCmd = QString("rm -rf '%1'").arg(target);
    QString cpCmd;

    // Copy new plugin
    if (plugin == "com.tencent.pagconfig") {
      // Copy directory recursively
      cpCmd = QString("cp -r '%1' '%2'").arg(source, targetDir);
      qDebug() << "PluginInstaller: Copying directory for" << plugin;
    } else {
      // Copy single file/plugin
      cpCmd = QString("cp -r '%1' '%2'").arg(source, targetDir);
      qDebug() << "PluginInstaller: Copying file/plugin for" << plugin;
    }

    qDebug() << "PluginInstaller: Adding commands:";
    qDebug() << "  mkdir:" << mkdirCmd;
    qDebug() << "  rm:" << rmCmd;
    qDebug() << "  cp:" << cpCmd;

    commands << mkdirCmd;
    commands << rmCmd;
    commands << cpCmd;
  }

  if (commands.isEmpty()) {
    qDebug() << "PluginInstaller: No commands to execute";
    return true;
  }

  // Add Qt resource copying command
  char qtResourceCmd[4096] = {0};
  CopyQtResource(qtResourceCmd, sizeof(qtResourceCmd));
  if (strlen(qtResourceCmd) > 0) {
    commands << QString::fromUtf8(qtResourceCmd).trimmed();
    qDebug() << "PluginInstaller: Added Qt resource copying command";
  }

  QString fullCommand = commands.join(" && ");
  qDebug() << "PluginInstaller: Full command to execute:" << fullCommand;

  bool result = executeWithPrivileges(fullCommand);
  qDebug() << "PluginInstaller: Copy operation result:" << result;

  if (result) {
    qDebug() << "PluginInstaller: Verifying installation (note: verification may fail due to "
                "system directory permissions)...";
    for (const QString& plugin : plugins) {
      QString target = getPluginInstallPath(plugin);
      bool exists = QFile::exists(target);
      qDebug() << "PluginInstaller: Plugin" << plugin << "exists at target:" << exists << "("
               << target << ")";
      if (!exists) {
        qDebug() << "PluginInstaller: Note - QFile::exists returned false, but this may be due to "
                    "permission restrictions on system directories";
        qDebug() << "PluginInstaller: The actual installation status should be verified by "
                    "checking the Adobe After Effects plugin menu";
      }
    }
  }

  return result;
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

  // Add Qt resource deletion command
  char deleteQtResourceCmd[4096] = {0};
  DeleteQtResource(deleteQtResourceCmd, sizeof(deleteQtResourceCmd));
  if (strlen(deleteQtResourceCmd) > 0) {
    commands << QString::fromUtf8(deleteQtResourceCmd).trimmed();
    qDebug() << "PluginInstaller: Added Qt resource deletion command";
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
      NSString* deleteShellPath = [[NSBundle mainBundle] pathForResource:@"delete_qt_resource" ofType:@"sh"];
      if (deleteShellPath != nil) {
        snprintf(cmd + strlen(cmd), cmdSize - strlen(cmd), "sh '%s'\n", [deleteShellPath UTF8String]);
      }
}


}  // namespace pag
