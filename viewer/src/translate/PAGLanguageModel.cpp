#include "PAGLanguageModel.h"
#include <QSettings>

bool PAGLanguageModel::IsEnglish = false;
Language PAGLanguageModel::SystemLanguage = Language::Unknown;
QTranslator PAGLanguageModel::Translator;
QCoreApplication* PAGLanguageModel::App = nullptr;

void PAGLanguageModel::Init(QCoreApplication* app) {
  App = app;
  QSettings settings;
  auto value = settings.value("isUseEnglish");
  if (value.isNull()) {
    if (SystemIsEnglish()) {
      qDebug() << "System Language: English";
      ToEnglish();
      IsEnglish = true;
    }
  } else {
    if (value.toBool()) {
      ToEnglish();
      IsEnglish = true;
    }
  }
}

void PAGLanguageModel::ToEnglish() {
  if (!IsEnglish) {
    bool result = Translator.load("qrc:/language/english.qm");
    bool install = App->installTranslator(&Translator);
    qDebug() << "Translator result:" << result << ", Install result:" << install;
    IsEnglish = true;
  }
}

void PAGLanguageModel::ToChinese() {
  if (IsEnglish) {
    App->removeTranslator(&Translator);
    IsEnglish = false;
  }
}

bool PAGLanguageModel::SystemIsEnglish() {
  if (SystemLanguage == Language::Chinese) {
    return false;
  } else if (SystemLanguage == Language::English) {
    return true;
  }

  QLocale locale;
  if (locale.language() == QLocale::Chinese) {
    qDebug() << "Chinese system";
    SystemLanguage = Language::Chinese;
    return false;
  } else {
    qDebug() << "English system";
    SystemLanguage = Language::English;
    return true;
  }
}

void PAGLanguageModel::setLanguage(bool isEnglish) {
  QSettings settings;
  settings.setValue("isUseEnglish", isEnglish);
}

bool PAGLanguageModel::getSystemLanguage() {
  return SystemIsEnglish();
}
