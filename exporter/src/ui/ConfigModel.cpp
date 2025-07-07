/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "ConfigModel.h"
#include <QApplication>
#include <QFont>
#include <QQmlContext>
#include <QThread>
#include "Config/ConfigFile.h"

namespace exporter {

ConfigModel::ConfigModel(QObject* parent) : QObject(parent) {
  ReadConfigFile(&currentConfig);
  int argc = 0;
  app = std::make_unique<QApplication>(argc, nullptr);
  app->setObjectName("PAG-Config");
  configEngine = std::make_unique<QQmlApplicationEngine>(app.get());
  initConfigWindow();
}

ConfigModel::~ConfigModel() {
}

void ConfigModel::initConfigWindow() {
  if (QThread::currentThread() != app->thread()) {
    qCritical() << "Must call initConfigWindow() in main thread";
    return;
  }

  QQmlContext* context = configEngine->rootContext();
  context->setContextProperty("configModel", this);

  configEngine->load(QUrl(QStringLiteral("qrc:/qml/ConfigWindow.qml")));

  configWindow = qobject_cast<QQuickWindow*>(configEngine->rootObjects().first());
  configWindow->setPersistentGraphics(true);
  configWindow->setPersistentSceneGraph(true);
  configWindow->setTextRenderType(QQuickWindow::TextRenderType::NativeTextRendering);
}

void ConfigModel::showConfig() const {
  if (configWindow == nullptr) {
    return;
  }
  configWindow->show();
  app->exec();
}

QVariantMap ConfigModel::getDefaultConfig() const {
  ConfigParam defaultConfig{};
  return ConfigParamToVariantMap(defaultConfig);
}

void ConfigModel::saveConfig() {
  WriteConfigFile(&currentConfig);
}

void ConfigModel::setLanguage(int value) {
  currentConfig.language = static_cast<Language>(value);
}

void ConfigModel::updateConfigFromQML(const QVariantMap& configData) {
  if (configData.contains("language")) {
    currentConfig.language = static_cast<Language>(configData["language"].toInt());
  }

  if (configData.contains("exportUseCase")) {
    currentConfig.scenes = static_cast<ExportScenes>(configData["exportUseCase"].toInt());
  }

  if (configData.contains("exportVersionControl")) {
    currentConfig.tagMode = static_cast<TagMode>(configData["exportVersionControl"].toInt());
  }

  if (configData.contains("tagLevel")) {
    currentConfig.exportTagLevel = static_cast<uint16_t>(configData["tagLevel"].toInt());
  }

  if (configData.contains("bitmapCompressionQuality")) {
    currentConfig.sequenceQuality = configData["bitmapCompressionQuality"].toInt();
  }

  if (configData.contains("bitmapPixelDensity")) {
    currentConfig.imagePixelRatio = configData["bitmapPixelDensity"].toFloat();
  }

  if (configData.contains("exportLayerName")) {
    currentConfig.enableLayerName = configData["exportLayerName"].toBool();
  }

  if (configData.contains("exportFonts")) {
    currentConfig.enableFontFile = configData["exportFonts"].toBool();
  }

  if (configData.contains("bitmapQuality")) {
    currentConfig.sequenceType =
        static_cast<pag::CompositionType>(configData["bitmapQuality"].toInt());
  }

  if (configData.contains("imageQuality")) {
    currentConfig.imageQuality = configData["imageQuality"].toInt();
  }

  if (configData.contains("exportSizeLimit")) {
    currentConfig.bitmapMaxResolution = configData["exportSizeLimit"].toInt();
  }

  if (configData.contains("maximumFrameRate")) {
    currentConfig.frameRate = configData["maximumFrameRate"].toFloat();
  }

  if (configData.contains("keyframeInterval")) {
    currentConfig.bitmapKeyFrameInterval = configData["keyframeInterval"].toInt();
  }
}

void ConfigModel::resetToDefault() {
  currentConfig = ConfigParam();
}

QVariantMap ConfigModel::ConfigParamToVariantMap(const ConfigParam& config) {
  QVariantMap map;
  map["language"] = static_cast<int>(config.language);
  map["exportUseCase"] = static_cast<int>(config.scenes);
  map["exportVersionControl"] = static_cast<int>(config.tagMode);
  map["tagLevel"] = config.exportTagLevel;
  map["bitmapCompressionQuality"] = config.sequenceQuality;
  map["bitmapPixelDensity"] = config.imagePixelRatio;
  map["exportLayerName"] = config.enableLayerName;
  map["exportFonts"] = config.enableFontFile;

  map["bitmapQuality"] = static_cast<int>(config.sequenceType);
  map["imageQuality"] = config.imageQuality;
  map["exportSizeLimit"] = config.bitmapMaxResolution;
  map["maximumFrameRate"] = config.frameRate;
  map["keyframeInterval"] = config.bitmapKeyFrameInterval;
  return map;
}

QVariantMap ConfigModel::getCurrentConfig() const {
  return ConfigParamToVariantMap(currentConfig);
}
}  // namespace exporter
