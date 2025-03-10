#ifndef COMMON_PAGAPPLICATION_H_
#define COMMON_PAGAPPLICATION_H_

#include <QApplication>

class PAGWindow;

class PAGApplication : public QApplication {
  Q_OBJECT
public:
  PAGApplication(int& argc, char** argv);

  auto event(QEvent* event) -> bool override ;
  auto openFile(QString path) -> void;

  Q_SLOT void onWindowDestroyed(PAGWindow* window);
  Q_SLOT void applicationMessage(int instanceId, const QByteArray& message);

 public:
  QString waitToOpenFile;
};

#endif // COMMON_PAGAPPLICATION_H_