#pragma once

#include <QQuickWindow>

namespace pag {

class PAGWindowHelper : public QObject {
  Q_OBJECT
 public:
  explicit PAGWindowHelper(QObject* parent = nullptr);
  auto setWindowStyle(QQuickWindow* quickWindow, double red, double green, double blue) -> void;
};

}  // namespace pag
