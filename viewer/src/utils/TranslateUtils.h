#include <string>
#include <QObject>
#include <QApplication>

static std::string Translate(const char *sourceText, const char *arg = nullptr) {
  auto translateWord = QCoreApplication::translate("QObject", sourceText);
  if (arg != nullptr) {
    translateWord = translateWord.arg(arg);
  }

  return translateWord.toStdString();
}
