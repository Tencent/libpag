#include "PAGWindowHelper.h"
#if defined(PAG_MACOS)
#include "macos/MacUpdater.h"
#endif

PAGWindowHelper::PAGWindowHelper (QObject* parent) : QObject(parent) {

}

PAGWindowHelper::~PAGWindowHelper() = default;

auto PAGWindowHelper::setupWindowStyle(QQuickWindow *window) -> void {
  if (TopFlag && (window != nullptr)) {
    window->setFlags(window->flags() | Qt::WindowStaysOnTopHint);
  }
#if defined(PAG_MACOS)
  if (window != nullptr) {
    MacUpdater::changeTitleBarColor(window->winId(), 0.125, 0.125, 0.164);
  }
#endif
}

auto PAGWindowHelper::SetTopWindow(bool flag) -> void {
  TopFlag = flag;
}

bool PAGWindowHelper::TopFlag = false;