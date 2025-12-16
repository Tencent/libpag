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
#include <pag/file.h>
#include <string>
#include <unordered_set>
#include <vector>
#include "AEGP_SuiteHandler.h"

namespace exporter {
enum class AlertInfoType {
  // warning
  UnknownWarning,               // Undefined warning
  UnsupportedEffects,           // Unsupported effects
  UnsupportedLayerStyle,        // Unsupported layer style
  Expression,                   // Expression
  VideoSequenceNoContent,       // Video sequence frame content is empty
  ContinuousSequence,           // Adjacent BMP composites
  BmpLayerButVectorComp,        // Layer uses BMP suffix but points to non-BMP composite
  NoPrecompLayerWithBmpName,    // Non-precomposite layer uses BMP suffix
  SameSequence,                 // Multiple sequence frames have identical content
  StaticVideoSequence,          // Contains fully static video sequence frames
  AudioEncodeFail,              // Audio encoding failed
  TextBackgroundOnlyTextLayer,  // Only text layers can add text backgrounds
  ImageFillRuleOnlyImageLayer,  // Only image layers can add ImageFillRule
  ImageFillRuleOnlyOne,         // One layer can only add one ImageFillRule
  TagLevelImageFillRule,        // TagLevel too low to support motion blur MotionBlur
  TagLevelImageFillRuleV2,  // TagLevel too low to support non-linear interpolation of ImageFillRule
  VideoTrackLayerRepeatRef,  // VideoTrack layer is repeatedly referenced （Business-related, abandoned）
  VideoTrackRefSameSource,  // Two VideoTrack layers reference the same source material （Business-related, abandoned）
  VideoTrackPeakNum,    // Too many VideoTrack layers at the same time (peak)
  VideoPlayBackward,    // ImageFillRule is played backward
  VideoSpeedTooFast,    // Playback speed exceeds 8x
  VideoTimeTooShort,    // Filled video resource is too short  （Business-related, abandoned）
  VideoTrackTimeCover,  // VideoTrack layer time completely covers another VideoTrack layer's time （Business-related, abandoned）
  TagLevelVerticalText,     // TagLevel too low to support vertical text
  TagLevelMotionBlur,       // TagLevel too low to support motion blur MotionBlur
  AdjustmentLayer,          // Adjustment layer
  CameraLayer,              // Camera layer
  Layer3D,                  // 3D properties
  LayerTimeRemapping,       // Time remapping
  EffectAndStylePickNum,    // Too many effects and layer styles (at the same time)
  LayerNum,                 // Too many layers
  VideoSequenceInUiScene,   // UI scene uses video sequence frames
  MarkerJsonHasChinese,     // Maker may contain Chinese characters
  MarkerJsonGrammar,        // Marker json syntax is incorrect
  RangeSelectorUnitsIndex,  // Text animation - range selector - unit - index
  RangeSelectorBasedOn,     // Text animation - range selector - based on must be "characters"
  RangeSelectorSmoothness,  // Text animation - range selector - smoothness must be 100%
  WigglySelectorBasedOn,    // Text animation - wiggly selector - based on must be "characters"
  GraphicsMemory,           // Graphics memory is too large
  GraphicsMemoryUI,         // Graphics memory is too large (UI scene)
  ImageNum,                 // Too many images
  BmpCompositionNum,        // Too many BMP composites
  FontSmallAndScaleLarge,   // Font is too small while scaling is too large
  FontFileTooBig,           // Font file is too large
  TextPathParamPerpendicularToPath,  // Text path parameter "perpendicular to path" only supports the default value true
  TextPathParamForceAlignment,  // Text path parameter "force alignment" only supports the default value false
  TextPathVertial,              // Text path does not support vertical text
  TextPathBoxText,   // Text path does not support box text (already supported, abandoned)
  TextPathAnimator,  // Text path and text animation are not compatible

  OtherWarning,  // Other warnings

  // error
  UnknownError,               // Undefined error
  ExportAEError,              // AE export error
  ExportAudioError,           // Audio export error
  ExportBitmapSequenceError,  // Bitmap sequence frame export error
  ExportVideoSequenceError,   // Video sequence frame export error
  WebpEncodeError,            // Webp encoding error
  ExportRenderError,          // Export render error
  CompositionHandleNotFound,  // Composition handle not found in itemHandleMap
  DisplacementMapRefSelf,     // DisplacementMap references itself
  ExportRangeSlectorError,    // Range selector export error
  PAGVerifyError,             // PAG file verification error

  OtherError  // Other errors
};

class AlertInfo {
 public:
  AlertInfo(AlertInfoType type, AEGP_ItemH itemHandle, AEGP_LayerH layerHandle,
            const std::string& info = "");
  void select();  // Navigate to the problematic composition/layer

  AEGP_ItemH itemHandle = nullptr;
  AEGP_LayerH layerHandle = nullptr;
  AlertInfoType type = AlertInfoType::OtherWarning;
  std::string compName = "";
  std::string layerName = "";
  std::string info = "";
  std::string suggest = "";
  bool isError = false;
  bool isFold = false;  // Whether expanded
  int itemHeight = 36;
  bool isFoldStatusChange = true;

  std::string getMessage();
};

class AlertInfoManager {
 public:
  static AlertInfoManager& GetInstance();
  std::vector<AlertInfo> GetAlertList(AEGP_ItemH itemHandle);

  bool showAlertInfo(bool showWarning = true, bool showError = true);

  std::vector<AlertInfo> warningList = {};
  std::vector<AlertInfo> saveWarnings = {};

  void pushWarning(const AEGP_ItemH& itemHandle, const AEGP_LayerH& layerHandle, AlertInfoType type,
                   const std::string& addInfo = "");

  AlertInfoManager(const AlertInfoManager&) = delete;
  AlertInfoManager& operator=(const AlertInfoManager&) = delete;

  AlertInfoManager(AlertInfoManager&&) = delete;
  AlertInfoManager& operator=(AlertInfoManager&&) = delete;

 private:
  AlertInfoManager() = default;
  ~AlertInfoManager() = default;
};

}  // namespace exporter
