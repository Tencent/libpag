/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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
#ifndef TIMESTRETCHMODEL_H
#define TIMESTRETCHMODEL_H
#include <QtCore/QObject>
#include "src/utils/Panels/TimeStretchPanel.h"

class TimeStretchModel : public QObject {
  Q_OBJECT
 public:
  TimeStretchModel(AEGP_ItemH const& currentAEItem, QObject* parent = nullptr);
  ~TimeStretchModel();

  Q_INVOKABLE int getStretchStartTime();
  Q_INVOKABLE void setStretchStartTime(int frame);
  Q_INVOKABLE int getStretchDuration();
  Q_INVOKABLE void setStretchDuration(int frame);
  Q_INVOKABLE int getCompositionDuration();
  Q_INVOKABLE int getStretchMode();
  Q_INVOKABLE void setStretchMode(int mode);
  Q_INVOKABLE int getFps();

private:
  TimeStretchPanel* timeStretchPanel;
};

#endif //TIMESTRETCHMODEL_H
