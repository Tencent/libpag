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

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include "platform/PAGViewerInstaller.h"

#ifdef Q_OS_WIN
#include <shlobj.h>
#include <windows.h>

namespace exporter {

bool PAGViewerInstaller::IsPAGViewerInstallWin() {

  QString programFiles = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
  QString pagViewerPath = programFiles + "/PAGViewer/PAGViewer.exe";

  if (QFile::exists(pagViewerPath)) {
    return true;
  }

  QSettings registry("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
                     QSettings::NativeFormat);
  for (const QString& key : registry.childGroups()) {
    registry.beginGroup(key);
    QString displayName = registry.value("DisplayName").toString();
    if (displayName.contains("PAGViewer", Qt::CaseInsensitive)) {
      registry.endGroup();
      return true;
    }
    registry.endGroup();
  }

  return false;
}

bool PAGViewerInstaller::copyToApplicationsWindows(const QString& sourcePath) {
  Q_UNUSED(sourcePath)
  return true;
}

}  // namespace exporter

#endif