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
#include "TimeStretchModel.h"

TimeStretchModel::TimeStretchModel(AEGP_ItemH const& currentAEItem, QObject* parent) : QObject(parent) {
  timeStretchPanel = new TimeStretchPanel(currentAEItem);
}

TimeStretchModel::~TimeStretchModel() noexcept {
  delete timeStretchPanel;
  timeStretchPanel = nullptr;
}

int TimeStretchModel::getStretchStartTime() const {
  return timeStretchPanel->timeStretchStart;
}

void TimeStretchModel::setStretchStartTime(const int frame) const {
  timeStretchPanel->setTimeStretchStart(frame);
}

int TimeStretchModel::getStretchDuration() const {
  return timeStretchPanel->timeStretchDuration;
}

void TimeStretchModel::setStretchDuration(const int frame) const {
  timeStretchPanel->setTimeStretchDuration(frame);
}

int TimeStretchModel::getCompositionDuration() const {
  return timeStretchPanel->compositionDuration;
}

int TimeStretchModel::getStretchMode() const {
  const int modeIndex = static_cast<int>(timeStretchPanel->timeStretchMode);
  return modeIndex;
}

void TimeStretchModel::setStretchMode(int mode) const {
  const auto stretchMode = static_cast<TimeStretchMode>(mode);
  timeStretchPanel->setTimeStretchMode(stretchMode);
}

int TimeStretchModel::getFps() {
  return timeStretchPanel->frameRate;
}
