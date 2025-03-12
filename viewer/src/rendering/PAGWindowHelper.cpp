#include "PAGWindowHelper.h"
#if defined(__APPLE__)
#include "PAGUpdater.h"
#endif

PAGWindowHelper::PAGWindowHelper (QObject* parent) : QObject(parent) {

}

PAGWindowHelper::~PAGWindowHelper() = default;

auto PAGWindowHelper::setupWindowStyle(QQuickWindow *window) -> void {
  if (TopFlag && (window != nullptr)) {
    window->setFlags(window->flags() | Qt::WindowStaysOnTopHint);
  }
#if defined(__APPLE__)
  if (window != nullptr) {
    PAGUpdater::changeTitleBarColor(window->winId(), 0.125, 0.125, 0.164);
  }
#endif
}

auto PAGWindowHelper::SetTopWindow(bool flag) -> void {
  TopFlag = flag;
}

bool PAGWindowHelper::TopFlag = false;