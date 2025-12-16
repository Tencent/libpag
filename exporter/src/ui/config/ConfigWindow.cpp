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

#include "ConfigWindow.h"
#include <QApplication>
#include <QFont>
#include <QQmlContext>
#include <QThread>
#include "config/ConfigFile.h"

namespace exporter {

static QVariantMap ConfigParamToVariantMap(const ConfigParam& config) {
  QVariantMap map;
  map["language"] = static_cast<int>(config.language);
  map["exportUseCase"] = static_cast<int>(config.scenes);
  map["exportVersionControl"] = static_cast<int>(config.tagMode);
  map["tagLevel"] = config.exportTagLevel;
  map["bitmapCompressionQuality"] = config.sequenceQuality;
  map["imageScaleRatio"] = config.imagePixelRatio;
  map["exportLayerName"] = config.exportLayerName;
  map["exportFonts"] = config.exportFontFile;
  map["bitmapQuality"] = static_cast<int>(config.sequenceType);
  map["imageQuality"] = config.imageQuality;
  map["exportSizeLimit"] = config.bitmapMaxResolution;
  map["maximumFrameRate"] = config.frameRate;
  map["keyframeInterval"] = config.bitmapKeyFrameInterval;
  return map;
}

ConfigWindow::ConfigWindow(QApplication* app, QObject* parent) : BaseWindow(app, parent) {
  ReadConfigFile(&currentConfig);
  initConfigWindow();
}

void ConfigWindow::initConfigWindow() {
  if (QThread::currentThread() != app->thread()) {
    qCritical() << "Must call initConfigWindow() in main thread";
    return;
  }

  auto context = engine->rootContext();
  context->setContextProperty("configWindow", this);

  engine->load(QUrl(QStringLiteral("qrc:/qml/ConfigWindow.qml")));

  window = qobject_cast<QQuickWindow*>(engine->rootObjects().first());
  window->setPersistentGraphics(true);
  window->setPersistentSceneGraph(true);
  window->setTextRenderType(QQuickWindow::TextRenderType::NativeTextRendering);
}

QVariantMap ConfigWindow::getDefaultConfig() const {
  ConfigParam defaultConfig{};
  return ConfigParamToVariantMap(defaultConfig);
}

void ConfigWindow::saveConfig() {
  WriteConfigFile(&currentConfig);
}

void ConfigWindow::setLanguage(int value) {
  currentConfig.language = static_cast<Language>(value);
}

void ConfigWindow::updateConfigFromQML(const QVariantMap& configData) {
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

  if (configData.contains("imageScaleRatio")) {
    currentConfig.imagePixelRatio = configData["imageScaleRatio"].toFloat();
  }

  if (configData.contains("exportLayerName")) {
    currentConfig.exportLayerName = configData["exportLayerName"].toBool();
  }

  if (configData.contains("exportFonts")) {
    currentConfig.exportFontFile = configData["exportFonts"].toBool();
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
  saveConfig();
}

void ConfigWindow::resetToDefault() {
  currentConfig = ConfigParam();
}

QVariantMap ConfigWindow::getCurrentConfig() const {
  return ConfigParamToVariantMap(currentConfig);
}
}  // namespace exporter
