#include "PAGUpdater.h"
#include <QWindow>
#if !defined(PAG_DEBUG)
#include <winsparkle.h>
#endif
#include "rendering/PAGWindow.h"

static bool ShowUI = false;

static auto UpdateDidFindCallBack() -> void {
#if defined(PAG_DEBUG)
  return;
#else
  for (int i = 0; i < PAGWindow::AllWindows.count(); i++)
  {
    auto window = PAGWindow::AllWindows[i];
    auto root = window->getEngine()->rootObjects().first();
    if (root) {
      QMetaObject::invokeMethod(root, "setHasNewVersion", Q_ARG(QVariant, true));
    }
  }
#endif
}

auto PAGUpdater::initUpdater(const wchar_t* version) -> void {
#if defined(PAG_DEBUG)
  return;
#else
  win_sparkle_set_appcast_url("http://www.tencent.com/appcast.xml");
  win_sparkle_set_app_details(L"pag.art", L"PAGViewer", version);
  win_sparkle_set_automatic_check_for_updates(0);
  win_sparkle_init();
#endif
}

auto PAGUpdater::checkUpdates(bool showUI, std::string feedUrl) -> void {
#if defined(PAG_DEBUG)
  return;
#else
  ShowUI = showUI;
  win_sparkle_set_did_find_update_callback(UpdateDidFindCallBack);
  // win_sparkle_set_did_find_update_callback2(UpdateDidFFindCallBack);
  win_sparkle_set_appcast_url(feedUrl.data());
  if (showUI) {
    win_sparkle_check_update_with_ui();
  } else {
    win_sparkle_check_update_without_ui();
  }
#endif
}