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
#ifndef TIMESTRETCHPANEL_H
#define TIMESTRETCHPANEL_H
#include "AEGP_SuiteHandler.h"
#include "AE_GeneralPlug.h"
#include "src/exports/PAGDataTypes.h"
#include <QtCore/QMetaType>
#include <string>
#include <vector>

// 填充模式
enum class TimeStretchMode {
  None = 0, // 不缩放
  Scale, // 伸缩
  Repeat, // 重复
  RepeatInverted, // 重复(倒序)
  Default = Repeat
};
Q_DECLARE_METATYPE(TimeStretchMode)

class TimeStretchPanel {
public:
  TimeStretchPanel(AEGP_ItemH mainCompItemH);
  ~TimeStretchPanel() {};

  void setTimeStretchMode(TimeStretchMode mode);  // 设置伸缩模式
  void setTimeStretchStart(pag::Frame start);
  void setTimeStretchDuration(pag::Frame duration);

  // 方便UI读取才写成public，但上层不要修改下列数据
  AEGP_ItemH itemH;  // 合成资源句柄
  TimeStretchMode timeStretchMode = TimeStretchMode::Default;  // 伸缩模式
  pag::Frame timeStretchStart = 0;
  pag::Frame timeStretchDuration = 0;
  pag::Frame compositionDuration = 0;
  float frameRate = 24.0f;

private:
  void readInfoFromeMarker();
  void validStartAndDuration();
};

#endif //TIMESTRETCHPANEL_H
