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

#include "win32_window.h"

#include "shellscalingapi.h"

namespace {

// the Windows DPI system is based on this
// constant for machines running at 100% scaling.
// constexpr int kBaseDpi = 96;

constexpr LPCWSTR kClassName = L"CLASSNAME";

// Scale helper to convert logical scaler values to physical using passed in
// scale factor
int Scale(int source, double scale_factor) { return static_cast<int>(source * scale_factor); }

}  // namespace

Win32Window::Win32Window() {}

Win32Window::~Win32Window() { Destroy(); }

void Win32Window::DoPaint(HDC hdc) {}

void Win32Window::OnCreate() {}

void Win32Window::OnTimer(int timer_id) {}

Win32Window::Size Win32Window::GetWindowSize() const {
  RECT client_rect = {0};
  ::GetClientRect(window_handle_, &client_rect);

  return Win32Window::Size(client_rect.right - client_rect.left,
                           client_rect.bottom - client_rect.top);
}

void Win32Window::SetWindowSize(int width, int height) {
  ::SetWindowPos(window_handle_, nullptr, 0, 0, width, height,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOREDRAW);
}

bool Win32Window::Invalidate() {
  RECT rect_window = {0};
  ::GetClientRect(window_handle_, &rect_window);
  BOOL ret = ::InvalidateRect(window_handle_, nullptr, FALSE);
  return ret;
}

bool Win32Window::CreateAndShow(const std::wstring& title, const Point& origin, const Size& size) {
  Destroy();

  WNDCLASS window_class = RegisterWindowClass();

  double scale_factor = 1.0f;

  DWORD ws_ex_flag = WS_EX_APPWINDOW;
  if (test_layered_window) {
    ws_ex_flag = WS_EX_LAYERED;
  }

  HWND window = CreateWindowEx(
      ws_ex_flag, window_class.lpszClassName, title.c_str(), WS_OVERLAPPEDWINDOW,
      Scale(origin.x, scale_factor), Scale(origin.y, scale_factor), Scale(size.width, scale_factor),
      Scale(size.height, scale_factor), nullptr, nullptr, window_class.hInstance, this);

  RECT rcClient = {0};
  ::GetClientRect(window_handle_, &rcClient);
  int width = rcClient.right - rcClient.left;
  int height = rcClient.bottom - rcClient.top;
  int offset_x = width - (rcClient.right - rcClient.left);
  int offset_y = height - (rcClient.bottom - rcClient.top);
  width += offset_x;
  height += offset_y;
  SetWindowPos(window_handle_, nullptr, 0, 0, width, height, SWP_NOZORDER | SWP_NOACTIVATE);

  CenterAndShow();
  return window != nullptr;
}

WNDCLASS Win32Window::RegisterWindowClass() {
  WNDCLASS window_class{};
  window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
  window_class.lpszClassName = kClassName;
  window_class.style = CS_HREDRAW | CS_VREDRAW;
  window_class.cbClsExtra = 0;
  window_class.cbWndExtra = 0;
  window_class.hInstance = GetModuleHandle(nullptr);
  window_class.hIcon = nullptr;
  window_class.hbrBackground = 0;
  window_class.lpszMenuName = nullptr;
  window_class.lpfnWndProc = WndProc;
  RegisterClass(&window_class);
  return window_class;
}

LRESULT CALLBACK Win32Window::WndProc(HWND const window, UINT const message, WPARAM const wparam,
                                      LPARAM const lparam) noexcept {
  if (message == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCT*>(lparam);
    SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));

    auto* that = static_cast<Win32Window*>(cs->lpCreateParams);

    that->window_handle_ = window;
  } else if (Win32Window* that = GetThisFromHandle(window)) {
    auto* win32Window = reinterpret_cast<Win32Window*>(GetWindowLongPtr(window, GWLP_USERDATA));
    if (win32Window == nullptr) {
      return 0;
    }
    return that->MessageHandler(window, message, wparam, lparam);
  }

  return DefWindowProc(window, message, wparam, lparam);
}

LRESULT
Win32Window::MessageHandler(HWND hwnd, UINT const message, WPARAM const wparam,
                            LPARAM const lparam) noexcept {
  switch (message) {
    case WM_CREATE:
      OnCreate();
      break;
    case WM_DESTROY:
      window_handle_ = nullptr;
      Destroy();
      OnDestroy();
      return 0;

    case WM_SIZE:
      RECT rect;
      GetClientRect(hwnd, &rect);
      OnSize(rect.right - rect.left, rect.bottom - rect.top);
      return 0;

    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      DoPaint(hdc);
      EndPaint(hwnd, &ps);
    } break;
    case WM_TIMER:
      OnTimer(static_cast<int>(wparam));
      break;

    case WM_ACTIVATE:
      if (child_content_ != nullptr) {
        SetFocus(child_content_);
      }
      return 0;

      // Messages that are directly forwarded to embedding.
    case WM_FONTCHANGE:
      SendMessage(child_content_, WM_FONTCHANGE, NULL, NULL);
      return 0;
    case WM_REQUEST_RESIZE_WINDOW:
      int width = wparam;
      int height = lparam;
      MoveWindow(child_content_, 0, 0, width, height, TRUE);
      break;
  }

  return DefWindowProc(window_handle_, message, wparam, lparam);
}

void Win32Window::Destroy() {
  if (window_handle_) {
    DestroyWindow(window_handle_);
    window_handle_ = nullptr;
  }

  UnregisterClass(kClassName, nullptr);
}

Win32Window* Win32Window::GetThisFromHandle(HWND const window) noexcept {
  return reinterpret_cast<Win32Window*>(GetWindowLongPtr(window, GWLP_USERDATA));
}

void Win32Window::SetChildContent(HWND content) {
  child_content_ = content;
  SetParent(content, window_handle_);
  RECT frame;
  GetClientRect(window_handle_, &frame);

  MoveWindow(content, frame.left, frame.top, frame.right - frame.left, frame.bottom - frame.top,
             true);

  SetFocus(child_content_);
}

HWND Win32Window::GetHandle() { return window_handle_; }

void Win32Window::OnDestroy() {}

void Win32Window::OnSize(int w, int h) {}

void Win32Window::CenterAndShow() {
  HWND window_handle = GetHandle();
  if ((GetWindowStyle(window_handle) & WS_CHILD) != 0) return;

  RECT rcDlg = {0};
  ::GetWindowRect(window_handle, &rcDlg);
  RECT rcArea = {0};
  RECT rcCenter = {0};
  HWND hWnd = window_handle;
  ::GetParent(window_handle);
  HWND hWndCenter = ::GetWindowOwner(window_handle);
  if (hWndCenter != NULL) hWnd = hWndCenter;

  MONITORINFO oMonitor = {};
  oMonitor.cbSize = sizeof(oMonitor);
  ::GetMonitorInfo(::MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), &oMonitor);
  rcArea = oMonitor.rcWork;

  if (hWndCenter == NULL)
    rcCenter = rcArea;
  else
    ::GetWindowRect(hWndCenter, &rcCenter);

  int DlgWidth = rcDlg.right - rcDlg.left;
  int DlgHeight = rcDlg.bottom - rcDlg.top;

  // Find dialog's upper left based on rcCenter
  int xLeft = (rcCenter.left + rcCenter.right) / 2 - DlgWidth / 2;
  int yTop = (rcCenter.top + rcCenter.bottom) / 2 - DlgHeight / 2;

  // The dialog is outside the screen, move it inside
  if (xLeft < rcArea.left) {
    if (xLeft < 0) {
      xLeft = GetSystemMetrics(SM_CXSCREEN) / 2 - DlgWidth / 2;
    } else {
      xLeft = rcArea.left;
    }
  } else if (xLeft + DlgWidth > rcArea.right) {
    xLeft = rcArea.right - DlgWidth;
  }

  if (yTop < rcArea.top) {
    if (yTop < 0) {
      yTop = GetSystemMetrics(SM_CYSCREEN) / 2 - DlgHeight / 2;
    } else {
      yTop = rcArea.top;
    }

  } else if (yTop + DlgHeight > rcArea.bottom) {
    yTop = rcArea.bottom - DlgHeight;
  }
  ::SetWindowPos(window_handle, NULL, xLeft, yTop, -1, -1,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}
