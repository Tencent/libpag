/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

Win32ExampleWindow::Win32ExampleWindow() {}

Win32ExampleWindow::~Win32ExampleWindow() {}

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
  pag_engine_->InitOnscreenRender(init_data);

  std::string test_pag_file_path = "../../assets/particle_video.pag";
  auto byte_data = pag::ByteData::FromPath(test_pag_file_path);
  if (byte_data == nullptr) {
    return;
  }
  pag_engine_->SetPagFileBuffer(byte_data->data(), byte_data->length());
  SetTimer(kRenderTimerId, 20);
}

void Win32ExampleWindow::OnDestroy() { PostQuitMessage(0); }

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

void Win32ExampleWindow::OnPagPlayEnd() {}
