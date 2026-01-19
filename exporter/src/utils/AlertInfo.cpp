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

#include "AlertInfo.h"
#include <QString>
#include <QtCore/QObject>
#include <iostream>
#include "AEHelper.h"
#include "base/utils/EnumClassHash.h"
#include "ui/WindowManager.h"

namespace exporter {

#define FUNC_GETINFO(errorType) GetInfo##errorType
#define DEFINE_GETINFO(errorType)                                                          \
  static void GetInfo##errorType(std::string& info, [[maybe_unused]] std::string& suggest, \
                                 [[maybe_unused]] const std::string& addInfo)

DEFINE_GETINFO(UnknownWarning) {
  info = addInfo;
}

auto strAdjustTagLevel = QObject::tr(
    "Increase TagLevel: Go to Preferences->PAG Config->General, change \"Export version control\" "
    "to \"Beta\". (Generated PAG files may only be supported by newer SDK versions)");

auto unknownErrorInfo = QObject::tr("Undefined error message.");

DEFINE_GETINFO(UnsupportedEffects) {
  auto infoData = QObject::tr("Effect not supported: \"%1\".");
  auto suggestData =
      QObject::tr("Recommend removing this effect or redesigning with alternative methods.");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(UnsupportedLayerStyle) {
  auto infoData = QObject::tr("Layer style not supported: \"%1\".");
  auto suggestData =
      QObject::tr("Recommend removing this layer style or redesigning with alternative methods.");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(Expression) {
  auto infoData = QObject::tr("Expressions are not supported.");
  auto suggestData = QObject::tr(
      "Recommend removing expressions or converting them to keyframes (Right-click "
      "property->Keyframe Assistant->Convert Expression to Keyframes).");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(VideoSequenceNoContent) {
  auto infoData = QObject::tr("BMP composition is empty.");
  auto suggestData = QObject::tr("Recommend removing this composition.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(ContinuousSequence) {
  auto infoData = QObject::tr("Multiple adjacent BMP compositions detected.");
  auto suggestData = QObject::tr(
      "Recommend merging into a single BMP composition for better rendering performance.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(BmpLayerButVectorComp) {
  auto infoData = QObject::tr("Layer name contains '_bmp' but points to a non-BMP composition.");
  auto suggestData = QObject::tr(
      "To export this pre-composition as BMP, rename the pre-composition instead of the layer.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(NoPrecompLayerWithBmpName) {
  auto infoData = QObject::tr("Non-precomposition layer uses '_bmp' suffix.");
  auto suggestData = QObject::tr(
      "To export this layer as BMP, pre-compose it and add '_bmp' suffix to the new "
      "pre-composition name.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(SameSequence) {
  auto infoData = QObject::tr("Multiple identical BMP compositions detected.");
  auto suggestData = QObject::tr("Recommend keeping only one.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(StaticVideoSequence) {
  auto infoData = QObject::tr("Static video sequence composition detected.");
  auto suggestData = QObject::tr("Recommend using image layers for better playback performance.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(AudioEncodeFail) {
  auto infoData = QObject::tr("Audio encoding failed. Music has been ignored.");
  auto suggestData = QObject::tr(
      "Ignore music or submit an issue on GitHub (https://github.com/Tencent/libpag/issues).");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(TextBackgroundOnlyTextLayer) {
  auto infoData = QObject::tr("TextBackground effect can only be added to text layers.");
  auto suggestData = QObject::tr("Recommend adding TextBackground to text layers.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(ImageFillRuleOnlyImageLayer) {
  auto infoData = QObject::tr("ImageFillRule effect can only be added to image layers.");
  auto suggestData = QObject::tr("Recommend adding ImageFillRule to image layers.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(ImageFillRuleOnlyOne) {
  auto infoData = QObject::tr(
      "Multiple ImageFillRule effects detected on a single layer. Only the first one is applied.");
  auto suggestData = QObject::tr("Recommend adding only one ImageFillRule effect per image layer.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(TagLevelImageFillRule) {
  auto infoData = QObject::tr("ImageFillRule effect is not supported at current TagLevel.");
  auto suggestData = QObject::tr("Recommend %1");
  info = infoData.toStdString();
  suggest = suggestData.arg(strAdjustTagLevel).toStdString();
}

DEFINE_GETINFO(TagLevelImageFillRuleV2) {
  auto infoData = QObject::tr(
      "Current TagLevel only supports linear interpolation for ImageFillRule. Other interpolation "
      "methods will be ignored.");
  auto suggestData = QObject::tr("Recommend %1");
  info = infoData.toStdString();
  suggest = suggestData.arg(strAdjustTagLevel).toStdString();
}

DEFINE_GETINFO(VideoTrackLayerRepeatRef) {
  auto infoData = QObject::tr("VideoTrack layer is referenced multiple times.");
  auto suggestData = QObject::tr("Recommend referencing it only once.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(VideoTrackRefSameSource) {
  auto infoData = QObject::tr("Multiple VideoTrack layers reference the same asset (\"%1\").");
  auto suggestData = QObject::tr("Recommend using different assets.");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(VideoTrackPeakNum) {
  auto infoData = QObject::tr(
      "More than 2 VideoTrack layers exist at the same time (frame %1), which may impact "
      "performance.");
  auto suggestData =
      QObject::tr("Recommend reducing the number of VideoTrack layers at the same time.");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(VideoPlayBackward) {
  auto infoData = QObject::tr("ImageFillRule does not support reverse playback.");
  auto suggestData = QObject::tr("Recommend disabling reverse playback.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(VideoSpeedTooFast) {
  auto infoData = QObject::tr("ImageFillRule fast playback exceeds 8x speed.");
  auto suggestData = QObject::tr("Recommend keeping fast playback speed below 8x.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(VideoTimeTooShort) {
  auto infoData =
      QObject::tr("Total duration of resources in VideoTrack segment is less than 0.5 seconds.");
  auto suggestData = QObject::tr("Recommend extending to more than 0.5 seconds.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(VideoTrackTimeCover) {
  auto infoData =
      QObject::tr("VideoTrack layer overlaps another VideoTrack layer in timeline (%1).");
  auto suggestData =
      QObject::tr("Recommend avoiding complete overlap between VideoTrack layers (%1).");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.arg(QString::fromStdString(addInfo)).toStdString();
}

DEFINE_GETINFO(TagLevelVerticalText) {
  auto infoData = QObject::tr("Vertical text is not supported at current TagLevel.");
  auto suggestData = QObject::tr("Recommend using horizontal text or %1");
  info = infoData.toStdString();
  suggest = suggestData.arg(strAdjustTagLevel).toStdString();
}

DEFINE_GETINFO(TagLevelMotionBlur) {
  auto infoData = QObject::tr("MotionBlur is not supported at current TagLevel.");
  auto suggestData = QObject::tr("Recommend removing MotionBlur or %1");
  info = infoData.toStdString();
  suggest = suggestData.arg(strAdjustTagLevel).toStdString();
}

DEFINE_GETINFO(AdjustmentLayer) {
  auto infoData = QObject::tr("Adjustment layers are not supported.");
  auto suggestData = QObject::tr("Please apply effects directly to target layers.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(CameraLayer) {
  auto infoData = QObject::tr("Camera layers are not supported at current TagLevel.");
  auto suggestData = QObject::tr("Please redesign using alternative methods or %1");
  info = infoData.toStdString();
  suggest = suggestData.arg(strAdjustTagLevel).toStdString();
}

DEFINE_GETINFO(LayerTimeRemapping) {
  auto infoData = QObject::tr("Time remapping is not supported.");
  auto suggestData = QObject::tr("Please disable time remapping.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(EffectAndStylePickNum) {
  auto infoData = QObject::tr(
      "More than 3 effects/layer styles exist at the same time (frame %1), which may impact "
      "performance.");
  auto suggestData =
      QObject::tr("Recommend reducing the number of effects/layer styles at the same time.");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(LayerNum) {
  auto infoData = QObject::tr("Total layer count exceeds 60 (may impact performance).");
  auto suggestData = QObject::tr("Recommend reducing the number of layers.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(VideoSequenceInUiScene) {
  auto infoData = QObject::tr("Video sequences are not recommended in UI scenes.");
  auto suggestData = QObject::tr("Recommend reducing the number of video sequences.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(MarkerJsonHasChinese) {
  auto infoData = QObject::tr("Detected potential Chinese characters in marker JSON: %1");
  auto suggestData = QObject::tr("Please verify the correctness of JSON text in markers.");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(MarkerJsonGrammar) {
  auto infoData = QObject::tr("Detected potential JSON syntax errors in markers: %1");
  auto suggestData = QObject::tr("Please verify JSON syntax in markers.");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(RangeSelectorUnitsIndex) {
  auto infoData =
      QObject::tr("\"Text Animation - Range Selector - Units\" does not support \"Index\".");
  auto suggestData = QObject::tr("Please use \"Percentage\" option instead.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(RangeSelectorBasedOn) {
  auto infoData =
      QObject::tr("\"Text Animation - Range Selector - Based On\" does not support \"%1\".");
  auto suggestData = QObject::tr("Please use \"Characters\" option instead.");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(RangeSelectorSmoothness) {
  auto infoData = QObject::tr(
      "\"Text Animation - Range Selector - Smoothness\" only supports default value 100%.");
  auto suggestData = QObject::tr("Please reset smoothness to default value 100%.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(WigglySelectorBasedOn) {
  auto infoData =
      QObject::tr("\"Text Animation - Wiggly Selector - Based On\" does not support \"%1\".");
  auto suggestData = QObject::tr("Please use \"Characters\" option instead.");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(Layer3D) {
  auto infoData = QObject::tr(
      "PAG supports 3D properties, but your current TagLevel is too low to enable them.");
  auto suggestData = QObject::tr("If you still want to use 3D properties, please %1");
  info = infoData.toStdString();
  suggest = suggestData.arg(strAdjustTagLevel).toStdString();
}

DEFINE_GETINFO(GraphicsMemory) {
  auto infoData = QObject::tr("Preview GPU memory usage is too high (\"%1\").");
  auto suggestData = QObject::tr("Recommend optimizing to below 80MB.");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(GraphicsMemoryUI) {
  auto infoData = QObject::tr("Preview GPU memory usage is too high (\"%1\").");
  auto suggestData = QObject::tr("For UI scenes, recommend optimizing to below 30MB.");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(ImageNum) {
  auto infoData = QObject::tr("Too many images (%1).");
  auto suggestData = QObject::tr("Recommend reducing the number of images (<=30).");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(BmpCompositionNum) {
  auto infoData = QObject::tr("Too many BMP compositions (\"%1\").");
  auto suggestData = QObject::tr("Recommend reducing BMP compositions (<=3).");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(FontSmallAndScaleLarge) {
  auto infoData = QObject::tr(
      "Text layer font size is too small with excessive scaling (\"%1\"x), which may cause "
      "rendering distortion.");
  auto suggestData = QObject::tr("Recommend adjusting font size and scaling to normal values.");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(FontFileTooBig) {
  auto infoData =
      QObject::tr("Exported font file exceeds 30MB (\"%1\"MB), which may prevent web upload.");
  auto suggestData = QObject::tr(
      "Recommendation: Ignore if font library size is not critical; otherwise use alternative "
      "fonts.");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(TextPathParamPerpendicularToPath) {
  auto infoData = QObject::tr("Text Path option \"Perpendicular To Path\" does not support false.");
  auto suggestData = QObject::tr("Recommend using default value true.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(TextPathParamForceAlignment) {
  auto infoData = QObject::tr("Text Path option \"Force Alignment\" does not support true.");
  auto suggestData = QObject::tr("Recommend using default value false.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(TextPathVertial) {
  auto infoData = QObject::tr("Vertical text is not supported in Text Path.");
  auto suggestData = QObject::tr("Recommend using horizontal text or removing Text Path.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(TextPathBoxText) {
  auto infoData = QObject::tr("Box text is not supported in Text Path.");
  auto suggestData = QObject::tr("Recommend using point text or removing Text Path.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(TextPathAnimator) {
  auto infoData = QObject::tr("Text Path and Text Animator are currently incompatible.");
  auto suggestData = QObject::tr("Recommend removing Text Path or Text Animator.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(VideoCompositionOverlap) {
  auto infoData = QObject::tr(
      "There are overlapping time intervals for references to video composition \"%1\".");
  auto suggestData = QObject::tr(
      "Recommend adjusting the start time and duration of the layers referencing video composition "
      "\"%1\".");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.arg(QString::fromStdString(addInfo)).toStdString();
}

DEFINE_GETINFO(OtherWarning) {
  info = addInfo;
}

DEFINE_GETINFO(UnknownError) {
  info = addInfo;
}

DEFINE_GETINFO(ExportAEError) {
  auto infoData = QObject::tr("AE export error.");
  info = infoData.toStdString();
}

DEFINE_GETINFO(ExportBitmapSequenceError) {
  auto infoData = QObject::tr("Bitmap sequence export error.");
  info = infoData.toStdString();
}

DEFINE_GETINFO(ExportVideoSequenceError) {
  auto infoData = QObject::tr("Video sequence export error.");
  info = infoData.toStdString();
}

DEFINE_GETINFO(ExportAudioError) {
  auto infoData = QObject::tr("Audio export error.");
  info = infoData.toStdString();
}

DEFINE_GETINFO(WebpEncodeError) {
  auto infoData = QObject::tr("WebP encoding error.");
  info = infoData.toStdString();
}

DEFINE_GETINFO(ExportRenderError) {
  auto infoData = QObject::tr("Export rendering error.");
  info = infoData.toStdString();
}

DEFINE_GETINFO(CompositionHandleNotFound) {
  auto infoData = QObject::tr(
      "Composition handle not found: The AE project handle for this composition (ID: %1) is not "
      "registered. This may occur if the composition was added after export initialization or if "
      "the project structure changed.");
  auto suggestData = QObject::tr(
      "Try re-exporting the project. If the issue persists, restart After Effects and try again.");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(DisplacementMapRefSelf) {
  auto infoData = QObject::tr("DisplacementMap does not support referencing its own layer.");
  auto suggestData = QObject::tr(
      "Recommend removing this effect or redirecting the displacement layer to another layer.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(ExportRangeSlectorError) {
  auto infoData = QObject::tr("[Text Animation - Selector] export error.");
  info = infoData.toStdString();
}

DEFINE_GETINFO(PAGVerifyError) {
  auto infoData = QObject::tr("PAG file verification error.");
  info = infoData.toStdString();
}

DEFINE_GETINFO(OtherError) {
  info = addInfo;
  suggest = addInfo;
}

using GetInfoHandler = void(std::string& info, std::string& suggest, std::string addInfo);

#define LINE_GETINFO(errorType) \
  { AlertInfoType::errorType, FUNC_GETINFO(errorType) }

static const std::unordered_map<AlertInfoType, std::function<GetInfoHandler>, pag::EnumClassHash>
    GetInfoByTypeMap = {LINE_GETINFO(UnknownWarning),
                        LINE_GETINFO(UnsupportedEffects),
                        LINE_GETINFO(UnsupportedLayerStyle),
                        LINE_GETINFO(Expression),
                        LINE_GETINFO(VideoSequenceNoContent),
                        LINE_GETINFO(ContinuousSequence),
                        LINE_GETINFO(BmpLayerButVectorComp),
                        LINE_GETINFO(NoPrecompLayerWithBmpName),
                        LINE_GETINFO(SameSequence),
                        LINE_GETINFO(StaticVideoSequence),
                        LINE_GETINFO(AudioEncodeFail),
                        LINE_GETINFO(TextBackgroundOnlyTextLayer),
                        LINE_GETINFO(ImageFillRuleOnlyImageLayer),
                        LINE_GETINFO(ImageFillRuleOnlyOne),
                        LINE_GETINFO(TagLevelImageFillRule),
                        LINE_GETINFO(TagLevelImageFillRuleV2),
                        LINE_GETINFO(VideoTrackLayerRepeatRef),
                        LINE_GETINFO(VideoTrackRefSameSource),
                        LINE_GETINFO(VideoTrackPeakNum),
                        LINE_GETINFO(VideoPlayBackward),
                        LINE_GETINFO(VideoSpeedTooFast),
                        LINE_GETINFO(VideoTimeTooShort),
                        LINE_GETINFO(VideoTrackTimeCover),
                        LINE_GETINFO(TagLevelVerticalText),
                        LINE_GETINFO(TagLevelMotionBlur),
                        LINE_GETINFO(AdjustmentLayer),
                        LINE_GETINFO(CameraLayer),
                        LINE_GETINFO(Layer3D),
                        LINE_GETINFO(LayerTimeRemapping),
                        LINE_GETINFO(EffectAndStylePickNum),
                        LINE_GETINFO(LayerNum),
                        LINE_GETINFO(VideoSequenceInUiScene),
                        LINE_GETINFO(MarkerJsonHasChinese),
                        LINE_GETINFO(MarkerJsonGrammar),
                        LINE_GETINFO(RangeSelectorUnitsIndex),
                        LINE_GETINFO(RangeSelectorBasedOn),
                        LINE_GETINFO(RangeSelectorSmoothness),
                        LINE_GETINFO(WigglySelectorBasedOn),
                        LINE_GETINFO(GraphicsMemory),
                        LINE_GETINFO(GraphicsMemoryUI),
                        LINE_GETINFO(ImageNum),
                        LINE_GETINFO(BmpCompositionNum),
                        LINE_GETINFO(FontSmallAndScaleLarge),
                        LINE_GETINFO(FontFileTooBig),
                        LINE_GETINFO(TextPathParamPerpendicularToPath),
                        LINE_GETINFO(TextPathParamForceAlignment),
                        LINE_GETINFO(TextPathVertial),
                        LINE_GETINFO(TextPathBoxText),
                        LINE_GETINFO(TextPathAnimator),
                        LINE_GETINFO(VideoCompositionOverlap),
                        LINE_GETINFO(OtherWarning),
                        LINE_GETINFO(UnknownError),
                        LINE_GETINFO(ExportAEError),
                        LINE_GETINFO(ExportAudioError),
                        LINE_GETINFO(ExportBitmapSequenceError),
                        LINE_GETINFO(ExportVideoSequenceError),
                        LINE_GETINFO(WebpEncodeError),
                        LINE_GETINFO(ExportRenderError),
                        LINE_GETINFO(CompositionHandleNotFound),
                        LINE_GETINFO(DisplacementMapRefSelf),
                        LINE_GETINFO(ExportRangeSlectorError),
                        LINE_GETINFO(PAGVerifyError),
                        LINE_GETINFO(OtherError)};

#undef LINE_GETINFO
#undef FUNC_GETINFO
#undef DEFINE_GETINFO

AlertInfo::AlertInfo(AlertInfoType type, AEGP_ItemH itemHandle, AEGP_LayerH layerHandle,
                     const std::string& addInfo)
    : itemHandle(itemHandle), layerHandle(layerHandle), type(type) {
  auto pair = GetInfoByTypeMap.find(type);
  if (pair != GetInfoByTypeMap.end()) {
    pair->second(info, suggest, addInfo);
  } else {
    info = addInfo;
    suggest = unknownErrorInfo.toStdString();
  }

  compName = GetItemName(itemHandle);
  layerName = GetLayerName(layerHandle);

  isError = (type > AlertInfoType::OtherWarning);
}

std::string AlertInfo::getMessage() {
  return "[" + compName + "] > [" + layerName + "]:" + info + " " + suggest;
}

void AlertInfo::select() {
  SelectLayer(itemHandle, layerHandle);
}

static std::vector<AlertInfo> GetInfoList(std::vector<AlertInfo>& warningList, bool bWarning,
                                          bool bError) {
  std::vector<AlertInfo> messages;
  for (auto alert : warningList) {
    if ((bWarning && !alert.isError) || (bError && alert.isError) || (bWarning && bError)) {
      messages.emplace_back(alert);
    }
  }
  return messages;
}

bool AlertInfoManager::showAlertInfo(bool showWarning, bool showError) {
  auto errors = GetInfoList(warningList, false, true);
  auto warnings = GetInfoList(warningList, true, false);
  for (auto info : errors) {
    saveWarnings.emplace_back(info);
  }
  for (auto info : warnings) {
    saveWarnings.emplace_back(info);
  }
  bool ret = true;
  if (!errors.empty() && showError) {
    ret = WindowManager::GetInstance().showErrors(errors);
  }
  if (ret && showWarning) {
    ret = WindowManager::GetInstance().showWarnings(warnings);
  }
  warningList.clear();
  return ret;
}

template <typename T>
T GetHandleById(const std::unordered_map<pag::ID, T>& list, pag::ID id) {
  static_assert(std::is_pointer<T>::value, "T must be a pointer type.");
  auto pair = list.find(id);
  if (pair != list.end()) {
    return pair->second;
  } else {
    return nullptr;
  }
}

AlertInfoManager& AlertInfoManager::GetInstance() {
  static AlertInfoManager instance;
  return instance;
}

void AlertInfoManager::pushWarning(const AEGP_ItemH& itemHandle, const AEGP_LayerH& layerHandle,
                                   AlertInfoType type, const std::string& addInfo) {
  warningList.emplace_back(AlertInfo(type, itemHandle, layerHandle, addInfo));
}

std::vector<AlertInfo> AlertInfoManager::GetAlertList(AEGP_ItemH /*itemHandle*/) {

  return {};
}

}  // namespace exporter
