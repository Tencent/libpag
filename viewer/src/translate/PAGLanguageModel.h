#ifndef TRANSLATE_PAGLANGUAGEMODEL_H
#define TRANSLATE_PAGLANGUAGEMODEL_H

#include <QObject>
#include <QTranslator>
#include <QApplication>

enum Language : int8_t {
  Unknown = -1,
  Chinese = 0,
  English = 1
};

class PAGLanguageModel : public QObject {
  Q_OBJECT
 public:
  static void Init(QCoreApplication* app);
  static void ToEnglish();
  static void ToChinese();
  static bool SystemIsEnglish();

  Q_INVOKABLE void setLanguage(bool isEnglish);
  Q_INVOKABLE bool getSystemLanguage();

private:
  static bool IsEnglish;
  static Language SystemLanguage;
  static QTranslator Translator;
  static QCoreApplication* App;
};

#endif // TRANSLATE_PAGLANGUAGEMODEL_H