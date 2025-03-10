#ifndef UTILS_TRANSLATE_H_
#define UTILS_TRANSLATE_H_

#include <string>
#include <QObject>
#include <QApplication>

namespace Utils {

static std::string translate(const char *sourceText, const char *arg = nullptr) {
  auto translateWord = QCoreApplication::translate("QObject", sourceText);
  if (arg != nullptr) {
    translateWord = translateWord.arg(arg);
  }

  return translateWord.toStdString();
}

} // namespace Utils

#endif // UTILS_TRANSLATE_H_
