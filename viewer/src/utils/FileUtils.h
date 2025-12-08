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

#include <QString>
#include "pag/file.h"

namespace pag::Utils {

void OpenInFinder(const QString& path, bool select = true);

bool DeleteFile(const QString& path);

bool DeleteDir(const QString& path);

bool MakeDir(const QString& path, bool isDir = true);

bool WriteFileToDisk(const std::shared_ptr<File>& file, const QString& filePath);

bool WriteDataToDisk(const QString& filePath, const void* data, size_t length);

}  // namespace pag::Utils
