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

#include "PAGUtilsImpl.h"
#include <shlobj_core.h>
#include <windows.h>
#include <QDir>

namespace pag::Utils {

void openFileInFinder(QFileInfo& fileInfo) {
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(hr)) {
    return;
  }

  QString folderPath = QDir::toNativeSeparators(fileInfo.absolutePath());
  QString filePath = QDir::toNativeSeparators(fileInfo.absoluteFilePath());

  std::wstring folderWPath = folderPath.toStdWString();
  std::wstring fileWPath = filePath.toStdWString();

  PIDLIST_ABSOLUTE pidlFolder(ILCreateFromPathW(folderWPath.c_str()));
  PIDLIST_ABSOLUTE pidlFile(ILCreateFromPathW(fileWPath.c_str()));

  if (!pidlFolder || !pidlFile) {
    if (pidlFolder) {
      ILFree(pidlFolder);
    }
    if (pidlFile) {
      ILFree(pidlFile);
    }
    CoUninitialize();
    return;
  }

  PCUITEMID_CHILD pidlChild = ILFindChild(pidlFolder, pidlFile);
  if (!pidlChild) {
    ILFree(pidlFolder);
    ILFree(pidlFile);
    CoUninitialize();
    return;
  }

  PCUITEMID_CHILD_ARRAY pidls = const_cast<PCUITEMID_CHILD_ARRAY>(&pidlChild);
  SHOpenFolderAndSelectItems(pidlFolder, 1, pidls, 0);

  ILFree(pidlFolder);
  ILFree(pidlFile);
  CoUninitialize();
}

}  // namespace pag::Utils
