#include "PAGWindowHelper.h"

namespace pag {
PAGWindowHelper::PAGWindowHelper(QObject* parent) : QObject(parent) {
}

auto PAGWindowHelper::setWindowStyle(QQuickWindow* quickWindow, double red, double green,
                                     double blue) -> void {
  return;
}

}  // namespace pag