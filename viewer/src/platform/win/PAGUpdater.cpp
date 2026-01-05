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

#include "PAGUpdater.h"
#include <windows.h>
#include <winsparkle/winsparkle.h>
#include "rendering/PAGWindow.h"
#include "version.h"

namespace pag {

static void WinSparkleDidFindUpdateCallback() {
  for (auto window : PAGWindow::AllWindows) {
    auto root = window->getEngine()->rootObjects().first();
    if (root) {
      QMetaObject::invokeMethod(root, "updateAvailable", Q_ARG(QVariant, true));
    }
  }
}

void InitUpdater() {
  int size = MultiByteToWideChar(CP_UTF8, 0, AppVersion.c_str(), -1, nullptr, 0);
  auto wVersion = std::vector<wchar_t>(size);
  MultiByteToWideChar(CP_UTF8, 0, AppVersion.c_str(), -1, wVersion.data(), size);
  win_sparkle_set_appcast_url("");
  win_sparkle_set_app_details(L"pag.art", L"PAGViewer", wVersion.data());
  win_sparkle_set_automatic_check_for_updates(0);
  win_sparkle_init();
}

void CheckForUpdates(bool keepSilent, const std::string& url) {
  win_sparkle_set_appcast_url(url.c_str());
  win_sparkle_set_did_find_update_callback(WinSparkleDidFindUpdateCallback);
  if (keepSilent) {
    win_sparkle_check_update_without_ui();
  } else {
    win_sparkle_check_update_with_ui();
  }
}

}  // namespace pag
