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

#include "ExportTimeStretchModel.h"
#include "export/Marker.h"
#include "utils/AEHelper.h"

namespace exporter {

std::unordered_map<pag::PAGTimeStretchMode, QString> timeStretchModeMap = {
    {pag::PAGTimeStretchMode::None, "None"},
    {pag::PAGTimeStretchMode::Scale, "Stretch"},
    {pag::PAGTimeStretchMode::Repeat, "Repeat"},
    {pag::PAGTimeStretchMode::RepeatInverted, "Repeat(Inverted)"}};

ExportTimeStretchModel::ExportTimeStretchModel(QObject* parent) : QObject(parent) {
}

int ExportTimeStretchModel::getDuration() const {
  return static_cast<int>(duration);
}

int ExportTimeStretchModel::getFrameRate() const {
  return static_cast<int>(frameRate);
}

int ExportTimeStretchModel::getStretchStartTime() const {
  return static_cast<int>(resource->stretchStartTime);
}

int ExportTimeStretchModel::getStretchDuration() const {
  return static_cast<int>(resource->stretchDuration);
}

QString ExportTimeStretchModel::getTimeStretchMode() const {
  return timeStretchModeMap[resource->stretchMode];
}

QStringList ExportTimeStretchModel::getTimeStretchModes() const {
  QStringList timeStretchModes;
  for (const auto& iter : timeStretchModeMap) {
    timeStretchModes.append(iter.second);
  }
  return timeStretchModes;
}

void ExportTimeStretchModel::setTimeStretchMode(const QString& timeStretchMode) {
  for (const auto& iter : timeStretchModeMap) {
    if (iter.second == timeStretchMode) {
      resource->stretchMode = iter.first;
      break;
    }
  }
  TimeStretchInfo timeStretchInfo = {resource->stretchMode, resource->stretchStartTime,
                                     resource->stretchDuration};
  SetTimeStretchInfo(timeStretchInfo, resource->itemHandle);
  Q_EMIT timeStretchModeChanged(timeStretchMode);
}

void ExportTimeStretchModel::setStretchStartTime(int stretchStartTime) {
  resource->stretchStartTime = static_cast<pag::Frame>(stretchStartTime);
  TimeStretchInfo timeStretchInfo = {resource->stretchMode, resource->stretchStartTime,
                                     resource->stretchDuration};
  SetTimeStretchInfo(timeStretchInfo, resource->itemHandle);
  Q_EMIT stretchStartTimeChanged(stretchStartTime);
}

void ExportTimeStretchModel::setStretchDuration(int stretchDuation) {
  resource->stretchDuration = static_cast<pag::Frame>(stretchDuation);
  TimeStretchInfo timeStretchInfo = {resource->stretchMode, resource->stretchStartTime,
                                     resource->stretchDuration};
  SetTimeStretchInfo(timeStretchInfo, resource->itemHandle);
  Q_EMIT stretchDurationChanged(stretchDuation);
}

void ExportTimeStretchModel::setAEResource(std::shared_ptr<AEResource> newResource) {
  resource = std::move(newResource);
  duration = GetItemDuration(resource->itemHandle);
  frameRate = GetItemFrameRate(resource->itemHandle);
  std::optional<TimeStretchInfo> timeStretchInfo = GetTimeStretchInfo(resource->itemHandle);
  if (timeStretchInfo.has_value()) {
    const auto& info = *timeStretchInfo;
    resource->stretchMode = info.mode;
    resource->stretchStartTime = info.start;
    resource->stretchDuration = info.duration;
    Q_EMIT timeStretchModeChanged(timeStretchModeMap[info.mode]);
    Q_EMIT stretchStartTimeChanged(static_cast<int>(info.start));
    Q_EMIT stretchDurationChanged(static_cast<int>(info.duration));
  }
  Q_EMIT durationChanged(static_cast<int>(duration));
  Q_EMIT frameRateChanged(static_cast<int>(frameRate));
}

}  // namespace exporter
