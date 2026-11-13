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
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "cli/VideoEncoderProcessInternal.h"
#ifdef _WIN32
#include <accctrl.h>
#include <aclapi.h>
#include <memory>
#include <string>
#include <vector>
#include "cli/CliUtils.h"

namespace pagx::cli {

bool CreatePrivateDirectory(const std::string& path, DWORD* error) {
  *error = ERROR_SUCCESS;
  HANDLE token = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
    *error = GetLastError();
    return false;
  }
  DWORD tokenSize = 0;
  GetTokenInformation(token, TokenUser, nullptr, 0, &tokenSize);
  if (tokenSize == 0) {
    *error = GetLastError();
    CloseHandle(token);
    return false;
  }
  std::vector<uint8_t> tokenData(tokenSize);
  if (!GetTokenInformation(token, TokenUser, tokenData.data(), tokenSize, &tokenSize)) {
    *error = GetLastError();
    CloseHandle(token);
    return false;
  }
  auto* tokenUser = reinterpret_cast<TOKEN_USER*>(tokenData.data());
  EXPLICIT_ACCESS_W access = {};
  access.grfAccessPermissions = GENERIC_ALL;
  access.grfAccessMode = SET_ACCESS;
  access.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
  BuildTrusteeWithSidW(&access.Trustee, static_cast<PSID>(tokenUser->User.Sid));
  ACL* acl = nullptr;
  auto aclResult = SetEntriesInAclW(1, &access, nullptr, &acl);
  if (aclResult != ERROR_SUCCESS) {
    *error = aclResult;
    CloseHandle(token);
    return false;
  }
  SECURITY_DESCRIPTOR descriptor = {};
  SECURITY_ATTRIBUTES security = {};
  security.nLength = sizeof(security);
  security.lpSecurityDescriptor = &descriptor;
  if (!InitializeSecurityDescriptor(&descriptor, SECURITY_DESCRIPTOR_REVISION) ||
      !SetSecurityDescriptorDacl(&descriptor, TRUE, acl, FALSE)) {
    *error = GetLastError();
    LocalFree(acl);
    CloseHandle(token);
    return false;
  }
  auto widePath = UTF8ToWide(path);
  if (widePath.empty()) {
    *error = ERROR_INVALID_DATA;
    LocalFree(acl);
    CloseHandle(token);
    return false;
  }
  auto created = CreateDirectoryW(widePath.c_str(), &security) != 0;
  if (!created) {
    *error = GetLastError();
  }
  LocalFree(acl);
  CloseHandle(token);
  return created;
}

void InitializeVideoEncoderProcess() {
}

bool VideoEncoder::spawnChildWindows(const std::vector<std::string>& arguments,
                                     std::string* errorMsg) {
  auto process = std::make_unique<ChildProcess>();
  auto applicationName = UTF8ToWide(arguments.front());
  if (applicationName.empty()) {
    SetError(errorMsg, "failed to encode the ffmpeg path as UTF-16");
    return false;
  }
  std::wstring commandLine = {};
  for (const auto& argument : arguments) {
    auto wide = UTF8ToWide(argument);
    if (wide.empty() && !argument.empty()) {
      SetError(errorMsg, "failed to encode the ffmpeg command line as UTF-16");
      return false;
    }
    if (!commandLine.empty()) {
      commandLine += L' ';
    }
    commandLine += QuoteWindowsArg(wide);
  }
  SECURITY_ATTRIBUTES security = {};
  security.nLength = sizeof(security);
  security.bInheritHandle = TRUE;
  HANDLE readPipe = nullptr;
  HANDLE writePipe = nullptr;
  if (!CreatePipe(&readPipe, &writePipe, &security, 0)) {
    SetError(errorMsg, "failed to create the ffmpeg input pipe (Windows error " +
                           std::to_string(GetLastError()) + ")");
    return false;
  }
  if (!SetHandleInformation(writePipe, HANDLE_FLAG_INHERIT, 0)) {
    SetError(errorMsg, "failed to configure the ffmpeg input pipe (Windows error " +
                           std::to_string(GetLastError()) + ")");
    CloseHandle(readPipe);
    CloseHandle(writePipe);
    return false;
  }
  STARTUPINFOW startup = {};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = readPipe;
  startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
  startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  PROCESS_INFORMATION info = {};
  if (!CreateProcessW(applicationName.c_str(), commandLine.data(), nullptr, nullptr, TRUE, 0,
                      nullptr, nullptr, &startup, &info)) {
    SetError(errorMsg,
             "failed to spawn ffmpeg (Windows error " + std::to_string(GetLastError()) + ")");
    CloseHandle(readPipe);
    CloseHandle(writePipe);
    return false;
  }
  CloseHandle(readPipe);
  process->process = info.hProcess;
  process->thread = info.hThread;
  process->stdinPipe = writePipe;
  child = std::move(process);
  return true;
}

}  // namespace pagx::cli
#endif
