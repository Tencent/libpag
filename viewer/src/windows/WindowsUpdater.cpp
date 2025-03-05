#include "WindowsUpdater.h"
#include <QWindow>
#include <winsparkle.h>
#include "rendering/PAGWindow.h"

static bool ShowUI = false;

static bool UpdateDidFindCallBack() {
  for (int i = 0; i < PAGWindow::AllWindows.count(); i++)
  {
    auto window = PAGWindow::AllWindows[i];
    auto root = window->getEngine()->rootObjects().first();
    if (root) {
      QMetaObject::invokeMethod(root, "setHasNewVersion", Q_ARG(QVariant, true));
    }
  }
  return ShowUI;
}

auto WindowsUpdater::initUpdater(const wchar_t* version) -> void {
  win_sparkle_set_appcast_url("http://www.tencent.com/appcast.xml");
  win_sparkle_set_app_details(L"pag.art", L"PAGViewer", version);
  win_sparkle_set_automatic_check_for_updates(0);
  win_sparkle_init();
}

auto WindowsUpdater::checkUpdates(bool showUI, std::string feedUrl) -> void {
  ShowUI = showUI;
  win_sparkle_set_did_find_update_callback2(UpdateDidFindCallBack);
  win_sparkle_set_appcast_url(feedUrl.data());
  if (showUI) {
    win_sparkle_check_update_with_ui();
  } else {
    win_sparkle_check_update_without_ui();
  }
}