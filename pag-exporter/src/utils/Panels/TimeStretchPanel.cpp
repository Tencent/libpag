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
#include "TimeStretchPanel.h"
#include "AEUtils.h"
#include "src/exports/Composition/Composition.h"
#include "src/exports/Effect/Effect.h"
#include "src/exports/PAGExporter/PAGExporter.h"
#include "src/exports/AEMarker/AEMarker.h"
#include "src/exports/Stream/StreamValue.h"
#include "src/utils/PAGFilterUtil.h"

TimeStretchPanel::TimeStretchPanel(AEGP_ItemH mainCompItemH) : itemH(mainCompItemH) {
  auto& suites = SUITES();
  auto compHandle = AEUtils::GetCompFromItem(itemH);
  A_FpLong longFrameRate = 24;
  suites.CompSuite6()->AEGP_GetCompFramerate(compHandle, &longFrameRate);
  frameRate = static_cast<float>(longFrameRate);

  A_Time time = {};
  suites.ItemSuite6()->AEGP_GetItemDuration(mainCompItemH, &time);
  compositionDuration = static_cast<pag::Frame>(round(time.value * frameRate / time.scale));

  readInfoFromeMarker();
}

void TimeStretchPanel::readInfoFromeMarker() {
  pag::Enum mode = pag::PAGTimeStretchMode::Repeat;
  pag::Frame start = 0;
  pag::Frame duration = 0;

  auto finded = AEMarker::GetTimeStretchInfo(mode, start, duration, itemH);
  if (!finded) {
    finded = AEMarker::GetTimeStretchInfoOld(mode, start, duration, itemH);
  }

  timeStretchMode = static_cast<TimeStretchMode>(mode);
  timeStretchStart = start;
  timeStretchDuration = duration;

  validStartAndDuration();
}

void TimeStretchPanel::validStartAndDuration() {
  if (timeStretchStart > compositionDuration) {
    timeStretchStart = compositionDuration;
  }
  if (timeStretchStart + timeStretchDuration > compositionDuration) {
    timeStretchDuration = compositionDuration - timeStretchStart;
  }
}

void TimeStretchPanel::setTimeStretchMode(TimeStretchMode mode) {
  if (mode == timeStretchMode) {
    return;
  }
  timeStretchMode = mode;
  validStartAndDuration();
  AEMarker::SetTimeStretchInfo(static_cast<pag::Enum>(timeStretchMode), timeStretchStart, timeStretchDuration, itemH);
}

void TimeStretchPanel::setTimeStretchStart(pag::Frame start) {
  if (start == timeStretchStart) {
    return;
  }
  timeStretchStart = start;
  validStartAndDuration();
  AEMarker::SetTimeStretchInfo(static_cast<pag::Enum>(timeStretchMode), timeStretchStart, timeStretchDuration, itemH);
}

void TimeStretchPanel::setTimeStretchDuration(pag::Frame duration) {
  if (duration == timeStretchDuration) {
    return;
  }
  timeStretchDuration = duration;
  validStartAndDuration();
  AEMarker::SetTimeStretchInfo(static_cast<pag::Enum>(timeStretchMode), timeStretchStart, timeStretchDuration, itemH);
}
