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

#include "win32_example_window.h"
#include <filesystem>
#include <iostream>
#include <vector>

namespace {
std::filesystem::path ExecutableDirectory() {
  wchar_t buffer[MAX_PATH] = {};
  auto length = ::GetModuleFileNameW(nullptr, buffer, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) {
    return {};
  }
  return std::filesystem::path(buffer).parent_path();
}

std::string PathToUtf8(const std::filesystem::path& path) {
  auto value = path.u8string();
  return std::string(value.begin(), value.end());
}

std::string StripLeadingParents(const std::string& path) {
  auto result = path;
  while (result.rfind("../", 0) == 0) {
    result.erase(0, 3);
  }
  return result;
}

std::string ResolveResourcePath(const std::string& relativePath) {
  auto exeDirectory = ExecutableDirectory();
  if (!exeDirectory.empty()) {
    // WinPAGDemo.exe lives in win/<arch>/<configuration>, three levels below the repository root.
    // The callers pass paths relative to win/Win32Demo, so the leading "../" segments are already
    // consumed by walking up to the repository root.
    auto candidate = exeDirectory / ".." / ".." / ".." / StripLeadingParents(relativePath);
    std::error_code error = {};
    if (std::filesystem::exists(candidate, error) && !error) {
      return PathToUtf8(candidate.lexically_normal());
    }
  }
  std::error_code error = {};
  if (std::filesystem::exists(relativePath, error) && !error) {
    return relativePath;
  }
  return {};
}
}  // namespace

Win32ExampleWindow::Win32ExampleWindow() {
}

Win32ExampleWindow::~Win32ExampleWindow() {
}

void Win32ExampleWindow::SetTimer(int timer_id, int elapse) {
  ::SetTimer(window_handle_, timer_id, elapse, nullptr);
}

void Win32ExampleWindow::OnCreate() {
  Size size = GetWindowSize();
  pag_engine_ = pagengine::CreatePAGEngine();
  pagengine::PAGEngineInitData init_data;
  init_data.hwnd = window_handle_;
  init_data.pag_engine_callback_ = this;
  init_data.width = size.width;
  init_data.height = size.height;
  if (!pag_engine_->InitOnscreenRender(init_data)) {
    std::cout << "[PAGDemo] failed to initialize the onscreen renderer" << std::endl;
    return;
  }
  std::vector<std::string> fontPaths = {};
  std::vector<int> ttcIndices = {};
  for (const auto& fontName :
       {"../../resources/font/NotoSansSC-Regular.otf", "../../resources/font/NotoColorEmoji.ttf"}) {
    auto fontPath = ResolveResourcePath(fontName);
    if (fontPath.empty()) {
      std::cout << "[PAGDemo] fallback font not found: " << fontName << std::endl;
      continue;
    }
    fontPaths.push_back(fontPath);
    ttcIndices.push_back(0);
  }
  if (!fontPaths.empty()) {
    pag::PAGFont::SetFallbackFontPaths(fontPaths, ttcIndices);
  }
  auto test_pag_file_path = ResolveResourcePath("../../assets/test2.pag");
  if (test_pag_file_path.empty()) {
    std::cout << "[PAGDemo] assets/test2.pag not found next to the executable or in the working "
                 "directory"
              << std::endl;
    return;
  }
  auto byte_data = pag::ByteData::FromPath(test_pag_file_path);
  if (byte_data == nullptr) {
    std::cout << "[PAGDemo] failed to read " << test_pag_file_path << std::endl;
    return;
  }
  if (!pag_engine_->SetPagFileBuffer(byte_data->data(), byte_data->length())) {
    std::cout << "[PAGDemo] failed to load " << test_pag_file_path << std::endl;
    return;
  }
  std::cout << "[PAGDemo] loaded " << test_pag_file_path << std::endl;
  SetTimer(kRenderTimerId, 20);
}

void Win32ExampleWindow::OnDestroy() {
  PostQuitMessage(0);
}

void Win32ExampleWindow::DoPaint(HDC hdc) {
  bool ret = pag_engine_->Flush(true);
}

void Win32ExampleWindow::OnTimer(int timer_id) {
  if (timer_id == kRenderTimerId) {
    RECT client_rect = {0};
    ::GetClientRect(window_handle_, &client_rect);
    ::InvalidateRect(window_handle_, &client_rect, TRUE);
  }
}

void Win32ExampleWindow::OnSize(int w, int h) {
  pag_engine_->Resize(w, h);
}

void Win32ExampleWindow::OnPagPlayEnd() {
}
