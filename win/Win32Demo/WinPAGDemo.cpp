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

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

#include "framework.h"
#include "win32_example_window.h"

#define BUFSIZE MAX_PATH

namespace {
void RedirectStdioToLogFile(const std::filesystem::path& logPath) {
  FILE* stdoutStream = nullptr;
  _wfreopen_s(&stdoutStream, logPath.c_str(), L"w", stdout);
  FILE* stderrStream = nullptr;
  _wfreopen_s(&stderrStream, logPath.c_str(), L"a", stderr);
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
  RedirectStdioToLogFile(logPath);
  std::cout << "[PAGDemo] starting, log file: " << logPath.string() << std::endl;

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
