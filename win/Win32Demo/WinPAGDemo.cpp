/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "WinPAGDemo.h"
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include "framework.h"
#include "win32_example_window.h"

#define BUFSIZE MAX_PATH

namespace {
bool RedirectStdioToLogFile(const std::filesystem::path& logPath) {
  int logDescriptor = -1;
  if (_wsopen_s(&logDescriptor, logPath.c_str(), _O_WRONLY | _O_CREAT | _O_TRUNC | _O_TEXT,
                _SH_DENYNO, _S_IREAD | _S_IWRITE) != 0) {
    return false;
  }
  FILE* stdoutStream = nullptr;
  FILE* stderrStream = nullptr;
  // Open the log before replacing either stream, so an unwritable path preserves existing output.
  // GUI streams may start without usable descriptors; initialize them before sharing one position.
  auto initialized = _wfreopen_s(&stdoutStream, L"NUL", L"w", stdout) == 0 &&
                     _wfreopen_s(&stderrStream, L"NUL", L"w", stderr) == 0;
  auto redirected = initialized && _dup2(logDescriptor, _fileno(stdoutStream)) == 0 &&
                    _dup2(logDescriptor, _fileno(stderrStream)) == 0;
  _close(logDescriptor);
  return redirected;
}

bool InitializeLogging(std::filesystem::path* logPath) {
  if (RedirectStdioToLogFile(*logPath)) {
    return true;
  }
  wchar_t tempDirectory[MAX_PATH] = {};
  auto length = GetTempPathW(MAX_PATH, tempDirectory);
  if (length == 0 || length >= MAX_PATH) {
    return false;
  }
  auto filename = L"WinPAGDemo-" + std::to_wstring(GetCurrentProcessId()) + L".log";
  auto fallbackPath = std::filesystem::path(tempDirectory) / filename;
  if (!RedirectStdioToLogFile(fallbackPath)) {
    return false;
  }
  *logPath = std::move(fallbackPath);
  auto message = L"[PAGDemo] Using fallback log: " + logPath->wstring() + L"\n";
  OutputDebugStringW(message.c_str());
  return true;
}
}  // namespace

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
  wchar_t modulePath[MAX_PATH] = {};
  auto modulePathLength = ::GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
  std::filesystem::path logPath = "WinPAGDemo.log";
  if (modulePathLength != 0 && modulePathLength < MAX_PATH) {
    logPath = std::filesystem::path(modulePath).parent_path() / "WinPAGDemo.log";
  }
  auto loggingReady = InitializeLogging(&logPath);
  if (!loggingReady) {
    OutputDebugStringW(L"[PAGDemo] Log initialization failed; continuing without a log file.\n");
    MessageBoxW(nullptr, L"Cannot open a log file. The demo will continue without file logging.",
                L"PAG Viewer", MB_OK | MB_ICONWARNING);
  } else {
    auto utf8Path = logPath.u8string();
    std::cout << "[PAGDemo] starting, log file: "
              << std::string(utf8Path.begin(), utf8Path.end())
              << std::endl;
  }

  int width = 600;
  int height = 800;
  Win32Window::Point origin(0, 0);
  Win32Window::Size size(width, height);

  Win32ExampleWindow client_window;
  if (!client_window.CreateAndShow(L"PAG Viewer", origin, size)) {
    std::cout << "[PAGDemo] failed to create the window" << std::endl;
    return -1;
  }
  std::cout << "[PAGDemo] window created" << std::endl;

  HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WINPAGDEMO));

  MSG msg;

  // 主消息循环:
  while (GetMessage(&msg, nullptr, 0, 0)) {
    if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  return static_cast<int>(msg.wParam);
}
