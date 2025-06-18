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

namespace exporter {

enum class PresetTagLevel {
  TagLevelStable = static_cast<uint16_t>(pag::TagCode::TextAnimatorPropertiesStrokeColor),
  TagLevelMin = static_cast<uint16_t>(pag::TagCode::DropShadowStyle),
  TagLevelMax = (static_cast<uint16_t>(pag::TagCode::Count) - 1),
};

/* export version control */
enum class ExportTagMode {
  TagModeStable = 0,
  TagModeBeta = 1,
  TagModeCustom = 2,
};

/* language */
enum class ExportLanguage {
  Auto = 0,
  Chinese = 1,
  English = 2,
};

/*export use case*/
enum class ExportScenes {
  GeneralScenes = 0,
  UIScenes = 1,
  VideoEditScenes = 2,
};

/*keyframe interval*/
#define DEFAULT_BITMAP_KEYFRAME_INTERVAL 60

/*export size limit*/
#define DEFAULT_BITMAP_MAX_RESOLUTION 720

/*TAG level*/
#define DEFAULT_TAG_LEVEL 1023

/*image quality*/
#define DEFAULT_IMAGE_QUALITY 80

/*bitmap pxiel density*/
#define DEFAULT_IMAGE_PIXEL_RATIO 2.0

/*export layer name*/
#define DEFAULT_ENABLE_LAYERNAME true

/*export font file*/
#define DEFAULT_ENABLE_FONTFILE false

/*language*/
#define DEFAULT_LANGUAGE ExportLanguage::Auto

/*suffix*/
#define DEFAULT_SEQUENCE_SUFFIX "_bmp"

/* export scenes */
#define DEFAULT_SCENES ExportScenes::GeneralScenes

/* sequence quality */
#define DEFAULT_SEQUENCE_QUALITY 80

/* export tag mode */
#define DEFAULT_TAG_MODE ExportTagMode::TagModeStable

/* sequence type */
#define DEFAULT_SEQUENCE_TYPE pag::CompositionType::Video

/* enable compression panel */
#define DEFAULT_ENABLE_COMPRESS_PANEL false

/* enable font audio */
#define DEFAULT_ENABLE_AUDIO true

/* enable compression panel */
#define DEFAULT_ENABLE_COMPRESS_PANEL false

struct ScaleAndFps {
  int32_t width;
  int32_t height;
  float frameRate;
};

struct ConfigParam {
  int bitmapKeyFrameInterval = DEFAULT_BITMAP_KEYFRAME_INTERVAL;
  int bitmapMaxResolution = DEFAULT_BITMAP_MAX_RESOLUTION;
  std::vector<std::pair<float, float>> scaleAndFpsList;
  ExportTagMode tagMode = DEFAULT_TAG_MODE;
  ExportScenes scenes = DEFAULT_SCENES;
  uint16_t exportTagLevel = DEFAULT_TAG_LEVEL;
  int imageQuality = DEFAULT_IMAGE_QUALITY;
  float imagePixelRatio = DEFAULT_IMAGE_PIXEL_RATIO;
  std::string sequenceSuffix = DEFAULT_SEQUENCE_SUFFIX;
  bool enableLayerName = DEFAULT_ENABLE_LAYERNAME;
  bool enableFontFile = DEFAULT_ENABLE_FONTFILE;
  int sequenceQuality = DEFAULT_SEQUENCE_QUALITY;
  bool enableCompressionPanel = DEFAULT_ENABLE_COMPRESS_PANEL;
  pag::CompositionType sequenceType = DEFAULT_SEQUENCE_TYPE;
  bool enableFontAudio = DEFAULT_ENABLE_AUDIO;
  ExportLanguage language = DEFAULT_LANGUAGE;
};

/*maximum frame rate*/
void ScaleAndFpsListDefault(std::vector<std::pair<float, float>>& scaleAndFpsList);

}  // namespace exporter
