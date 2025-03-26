#pragma once

#include <QQuickWindow>

namespace pag {

class PAGWindowHelper : public QObject {
  Q_OBJECT
 public:
  explicit PAGWindowHelper(QObject* parent = nullptr);
  Q_INVOKABLE void setWindowStyle(QQuickWindow* quickWindow, double red, double green, double blue);
};

}  // namespace pag
