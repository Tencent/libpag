#ifndef RENDERING_WINDOWHELPER_H_
#define RENDERING_WINDOWHELPER_H_

#include <QObject>
#include <QQuickWindow>

class WindowHelper: public QObject {
  Q_OBJECT
 public:
  explicit WindowHelper(QObject* parent = nullptr);
  ~WindowHelper() override;

  Q_INVOKABLE void setupWindowStyle(QQuickWindow *window);

  static auto SetTopWindow(bool flag) -> void;

private:
  static bool TopFlag;
};

#endif // RENDERING_WINDOWHELPER_H_