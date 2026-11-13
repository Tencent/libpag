/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "cli/VideoEncoder.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#endif

namespace pagx::cli {

void SetError(std::string* errorMsg, const std::string& text);

struct VideoEncoder::ChildProcess {
#ifdef _WIN32
  HANDLE process = nullptr;
  HANDLE thread = nullptr;
  HANDLE stdinPipe = nullptr;
#else
  pid_t processId = -1;
  int stdinPipe = -1;
  int execErrorPipe = -1;
  int execError = 0;
#endif
  bool reaped = false;
  bool exitStatusValid = false;
  int exitCode = 0;
};

#ifdef _WIN32
bool CreatePrivateDirectory(const std::string& path, DWORD* error);
#else
bool CreatePrivateDirectory(const std::string& path, int* error);
#endif

}  // namespace pagx::cli
