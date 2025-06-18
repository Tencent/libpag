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

#pragma once

#include <pag/file.h>
#include "AEGP_SuiteHandler.h"

namespace pag {
namespace exporter {

enum class PresetTagLevel {
  TagLevelStable = static_cast<uint16_t>(TagCode::TextAnimatorPropertiesStrokeColor),
  TagLevelMin = static_cast<uint16_t>(TagCode::DropShadowStyle),
  TagLevelMax = static_cast<uint16_t>(TagCode::Count) - 1,
};

enum class TagMode {
  TagModeStable = 0,
  TagModeBeta = 1,
  TagModeCustom = 2,
};

enum class ExportLanguage {
  Auto = 0,
  Chinese = 1,
  English = 2,
};

enum class ExportScenes {
  GeneralScenes = 0,
  UIScenes = 1,
  VideoEditScenes = 2,
};

struct ConfigParam {
  int bitmapKeyFrameInterval = 60;
  int bitmapMaxResolution = 720;
  float frameRate = 24.0;
  TagMode tagMode = TagMode::TagModeStable;
  ExportScenes scenes = ExportScenes::GeneralScenes;
  uint16_t exportTagLevel = 1023;
  int imageQuality = 80;
  float imagePixelRatio = 2.0;
  std::string sequenceSuffix = "_bmp";
  bool enableLayerName = true;
  bool enableFontFile = false;
  int sequenceQuality = 80;
  bool enableCompressionPanel = false;
  pag::CompositionType sequenceType = CompositionType::Video;
  bool enableFontAudio = true;
  ExportLanguage language = ExportLanguage::Auto;
};
}  // namespace exporter
}  // namespace pag
