#ifndef RENDERING_PAG_WINDOW_HELPER_H_
#define RENDERING_PAG_WINDOW_HELPER_H_

#include <QObject>
#include <QQuickWindow>

class PAGWindowHelper: public QObject {
  Q_OBJECT
 public:
  explicit PAGWindowHelper(QObject* parent = nullptr);
  ~PAGWindowHelper() override;

  Q_INVOKABLE void setupWindowStyle(QQuickWindow *window);

  static auto SetTopWindow(bool flag) -> void;

private:
  static bool TopFlag;
};

#endif // RENDERING_PAG_WINDOW_HELPER_H_