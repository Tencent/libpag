#include "WindowHelper.h"

WindowHelper::WindowHelper (QObject* parent) : QObject(parent) {

}

WindowHelper::~WindowHelper() = default;

auto WindowHelper::setupWindowStyle(QQuickWindow *window) -> void {
  if (TopFlag && window != nullptr) {
    window->setFlags(window->flags() | Qt::WindowStaysOnTopHint);
  }
#if defined(PAG_MACOS)
  if (window) {
    // TODO
    // MacUpdater::changeTitleBarColor(window->winId(), 0.125, 0.125, 0.164);
  }
#endif
}

auto WindowHelper::SetTopWindow(bool flag) -> void {
  TopFlag = flag;
}

bool WindowHelper::TopFlag = false;