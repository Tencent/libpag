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
#ifndef _WIN32
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace pagx::cli {

bool CreatePrivateDirectory(const std::string& path, int* error) {
  if (mkdir(path.c_str(), 0700) == 0) {
    return true;
  }
  *error = errno;
  return false;
}

void InitializeVideoEncoderProcess() {
  struct sigaction action = {};
  action.sa_handler = SIG_IGN;
  sigemptyset(&action.sa_mask);
  sigaction(SIGPIPE, &action, nullptr);
}

namespace {

bool CreateCloseOnExecPipe(int fds[2], const char* name, std::string* errorMsg) {
  if (pipe(fds) != 0) {
    SetError(errorMsg, "failed to create the " + std::string(name) + ": " +
                           std::string(std::strerror(errno)));
    return false;
  }
  if (fcntl(fds[0], F_SETFD, FD_CLOEXEC) != 0 || fcntl(fds[1], F_SETFD, FD_CLOEXEC) != 0) {
    auto error = errno;
    close(fds[0]);
    close(fds[1]);
    SetError(errorMsg, "failed to configure the " + std::string(name) + ": " +
                           std::string(std::strerror(error)));
    return false;
  }
  return true;
}

}  // namespace

bool VideoEncoder::spawnChildPosix(const std::vector<std::string>& arguments,
                                   std::string* errorMsg) {
  auto process = std::make_unique<ChildProcess>();
  int fds[2] = {-1, -1};
  if (!CreateCloseOnExecPipe(fds, "ffmpeg input pipe", errorMsg)) {
    return false;
  }
  int execErrors[2] = {-1, -1};
  if (!CreateCloseOnExecPipe(execErrors, "exec error pipe", errorMsg)) {
    close(fds[0]);
    close(fds[1]);
    return false;
  }
  std::vector<char*> argv = {};
  argv.reserve(arguments.size() + 1);
  for (const auto& argument : arguments) {
    argv.emplace_back(const_cast<char*>(argument.c_str()));
  }
  argv.emplace_back(nullptr);
  auto processId = fork();
  if (processId < 0) {
    auto error = errno;
    close(fds[0]);
    close(fds[1]);
    close(execErrors[0]);
    close(execErrors[1]);
    SetError(errorMsg, "failed to fork the ffmpeg process: " + std::string(std::strerror(error)));
    return false;
  }
  if (processId == 0) {
    close(execErrors[0]);
    struct sigaction defaultPipe = {};
    defaultPipe.sa_handler = SIG_DFL;
    sigemptyset(&defaultPipe.sa_mask);
    bool inputReady = dup2(fds[0], STDIN_FILENO) >= 0 && fcntl(STDIN_FILENO, F_SETFD, 0) >= 0;
    if (!inputReady || sigaction(SIGPIPE, &defaultPipe, nullptr) != 0) {
      int error = errno;
      write(execErrors[1], &error, sizeof(error));
      _exit(127);
    }
    if (fds[0] != STDIN_FILENO) {
      close(fds[0]);
    }
    close(fds[1]);
    execv(argv[0], argv.data());
    int error = errno;
    write(execErrors[1], &error, sizeof(error));
    _exit(127);
  }
  close(fds[0]);
  close(execErrors[1]);
  process->processId = processId;
  process->stdinPipe = fds[1];
  process->execErrorPipe = execErrors[0];
  child = std::move(process);
  return true;
}

}  // namespace pagx::cli
#endif
