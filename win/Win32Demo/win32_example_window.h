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

#pragma once

#include "pag_engine/PAGEngine.h"
#include "win32_window.h"

static const int kRenderTimerId = 1234565;

class Win32ExampleWindow : public Win32Window, public pagengine::PAGEngineCallback {
 public:
  Win32ExampleWindow();
  ~Win32ExampleWindow();

  void SetTimer(int timer_id, int elapse);

  void OnCreate() override;
  void OnDestroy() override;
  void DoPaint(HDC hdc) override;
  void OnTimer(int timer_id) override;
  void OnSize(int w, int h) override;

  // pagengine::PAGEngineCallback
  void OnPagPlayEnd() override;

 private:
  pagengine::PAGEngine* pag_engine_ = nullptr;
};
