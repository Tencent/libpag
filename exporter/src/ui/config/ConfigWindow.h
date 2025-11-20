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

#pragma once
#include <QVariantMap>
#include "config/ConfigParam.h"
#include "ui/BaseWindow.h"

namespace exporter {

class ConfigWindow : public BaseWindow {
  Q_OBJECT
 public:
  explicit ConfigWindow(QApplication* app, QObject* parent = nullptr);

  void initConfigWindow();

  Q_INVOKABLE void saveConfig();
  Q_INVOKABLE void resetToDefault();
  Q_INVOKABLE void setLanguage(int value);
  Q_INVOKABLE void updateConfigFromQML(const QVariantMap& configData);
  Q_INVOKABLE QVariantMap getDefaultConfig() const;
  Q_INVOKABLE QVariantMap getCurrentConfig() const;

 private:
  ConfigParam currentConfig = {};
};

}  // namespace exporter