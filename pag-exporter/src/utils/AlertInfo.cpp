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
#include "AlertInfo.h"
#include <QtCore/QObject>
#include "AEUtils.h"
#include "CommonMethod.h"
#include "EnumClassHash.h"
#include "src/exports/ExportVerify/ExportVerify.h"
#include "src/exports/PAGExporter/PAGExporter.h"
#include "src/ui/qt/ErrorList/AlertInfoUI.h"

namespace pagexporter {
std::string AlertInfo::getMessage() {
  return "[" + compName + "] > [" + layerName + "]:" + info + " " + suggest;
}

#define FUNC_GETINFO(errorType) GetInfo##errorType
#define DEFINE_GETINFO(errorType) \
  static void GetInfo##errorType(std::string& info, std::string& suggest, std::string addInfo)

DEFINE_GETINFO(UnknownWarning) {
  info = addInfo;
}

DEFINE_GETINFO(UnsupportedEffects) {
  auto infoData = QObject::tr("暂不支持效果:\"%1\".");
  auto suggestData = QObject::tr("建议去掉该效果，或改用其它方式设计。");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(UnsupportedLayerStyle) {
  auto infoData = QObject::tr("暂不支持图层样式:\"%1\".");
  auto suggestData = QObject::tr("建议去掉该图层样式，或改用其它方式设计。");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(Expression) {
  auto infoData = QObject::tr("暂不支持表达式.");
  auto suggestData = QObject::tr(
      "建议去掉表达式，或改为用关键帧实现(选中相应属性，点击鼠标右键->关键帧辅助->"
      "将表达式转化为关键帧).");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(VideoSequenceNoContent) {
  auto infoData = QObject::tr("BMP合成内容为空");
  auto suggestData = QObject::tr("建议去掉该合成。");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(ContinuousSequence) {
  auto infoData = QObject::tr("含有相邻的多个BMP合成.");
  auto suggestData = QObject::tr("建议合并为一个BMP合成. 合并后可以提升渲染性能.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(BmpLayerButVectorComp) {
  auto infoData = QObject::tr("图层名含有_bmp但指向了一个非BMP合成.");
  auto suggestData =
      QObject::tr("如果想将此预合成导出为BMP合成, 请修改预合成的名字, 而不是修改图层名字.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(NoPrecompLayerWithBmpName) {
  auto infoData = QObject::tr("非预合成图层使用了_bmp后缀.");
  auto suggestData =
      QObject::tr("如果想将此图层以BMP导出, 请将此图层打入预合成，并将新的预合成名字添加_bmp后缀.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(SameSequence) {
  auto infoData = QObject::tr("有多个内容完全相同的BMP合成.");
  auto suggestData = QObject::tr("建议只保留一个");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(StaticVideoSequence) {
  auto infoData = QObject::tr("含有全静止的视频序列帧合成.");
  auto suggestData = QObject::tr("建议用图片图层制作(播放性能更优).");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(AudioEncodeFail) {
  auto infoData = QObject::tr("音乐编码失败，已忽略音乐.");
  auto suggestData =
      QObject::tr("忽略音乐，或在Github提交Issue  (https://github.com/Tencent/libpag/issues).");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(TextBackgroundOnlyTextLayer) {
  auto infoData = QObject::tr("只有文本图层可以添加效果TextBackground.");
  auto suggestData = QObject::tr("建议将TextBackground添加到文本图层");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(ImageFillRuleOnlyImageLayer) {
  auto infoData = QObject::tr("只有图片图层可以添加效果ImageFillRule.");
  auto suggestData = QObject::tr("建议将ImageFillRule添加到图片图层");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(ImageFillRuleOnlyOne) {
  auto infoData = QObject::tr("一个图层添加了多个ImageFillRule效果, 第2个开始已忽略.");
  auto suggestData = QObject::tr("建议每个图片图层最多只添加一个ImageFillRule效果.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(TagLevelImageFillRule) {
  auto infoData = QObject::tr("当前TagLevel不支持效果ImageFillRule.");
  auto suggestData = QObject::tr("建议%1");
  auto strAdjustTagLevel = QObject::tr(
      "调高TagLevel：首选项->PAG Config->通用, 将/“导出版本控制/”改为/“Beta/”. "
      "(调高后生成的pag文件可能只有较新版本SDK才能支持)");
  info = infoData.toStdString();
  suggest = suggestData.arg(strAdjustTagLevel).toStdString();
}

DEFINE_GETINFO(TagLevelImageFillRuleV2) {
  auto infoData =
      QObject::tr("当前TagLevel只支持线性插值的ImageFillRule效果, 其它插值方式将被忽略.");
  auto suggestData = QObject::tr("建议%1");
  auto strAdjustTagLevel = QObject::tr(
      "调高TagLevel：首选项->PAG Config->通用, 将/“导出版本控制/”改为/“Beta/”. "
      "(调高后生成的pag文件可能只有较新版本SDK才能支持)");
  info = infoData.toStdString();
  suggest = suggestData.arg(strAdjustTagLevel).toStdString();
}

DEFINE_GETINFO(VideoTrackLayerRepeatRef) {
  auto infoData = QObject::tr("VideoTrack图层被多次引用.");
  auto suggestData = QObject::tr("建议只引用一次.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(VideoTrackRefSameSource) {
  auto infoData = QObject::tr("多个VideoTrack图层引用相同的素材.(\"%1\").");
  auto suggestData = QObject::tr("建议引用不同的素材.");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(VideoTrackPeakNum) {
  auto infoData = QObject::tr("videoTrack图层在同一时间(第\"%1\"帧)存在的数目超过了2个(影响性能)");
  auto suggestData = QObject::tr("建议减少同一时间videoTrack图层的数目.");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(VideoPlayBackward) {
  auto infoData = QObject::tr("ImageFillRule暂不支持倒放");
  auto suggestData = QObject::tr("建议去掉倒放.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(VideoSpeedTooFast) {
  auto infoData = QObject::tr("ImageFillRule快放超过8倍速");
  auto suggestData = QObject::tr("建议快放倍数不要超过8倍速.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(VideoTimeTooShort) {
  auto infoData = QObject::tr("VideoTrack段落中需要填充的资源总长小于0.5秒.");
  auto suggestData = QObject::tr("建议大于0.5秒.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(VideoTrackTimeCover) {
  auto infoData = QObject::tr("VideoTrack图层在时间上的覆盖了另一个VideoTrack图层(%1).");
  auto suggestData =
      QObject::tr("建议VideoTrack图层在时间上的不要完全覆盖了另一个VideoTrack图层(%1).");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.arg(QString::fromStdString(addInfo)).toStdString();
}

DEFINE_GETINFO(TagLevelVerticalText) {
  auto infoData = QObject::tr("当前TagLevel不支持竖排文本.");
  auto suggestData = QObject::tr("建议改用横排文本，或者%1");
  auto strAdjustTagLevel = QObject::tr(
      "调高TagLevel：首选项->PAG Config->通用, 将/“导出版本控制/”改为/“Beta/”. "
      "(调高后生成的pag文件可能只有较新版本SDK才能支持)");
  info = infoData.toStdString();
  suggest = suggestData.arg(strAdjustTagLevel).toStdString();
}

DEFINE_GETINFO(TagLevelMotionBlur) {
  auto infoData = QObject::tr("当前TagLevel不支持MotionBlur.");
  auto suggestData = QObject::tr("建议去掉MotionBlur， 或者%1");
  auto strAdjustTagLevel = QObject::tr(
      "调高TagLevel：首选项->PAG Config->通用, 将/“导出版本控制/”改为/“Beta/”. "
      "(调高后生成的pag文件可能只有较新版本SDK才能支持)");
  info = infoData.toStdString();
  suggest = suggestData.arg(strAdjustTagLevel).toStdString();
}

DEFINE_GETINFO(AdjustmentLayer) {
  auto infoData = QObject::tr("暂不支持调整图层.");
  auto suggestData = QObject::tr("请将效果直接添加到目标图层.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(CameraLayer) {
  auto infoData = QObject::tr("当前TagLevel不支持摄像机图层.");
  auto suggestData = QObject::tr("请改用其它方式设计， 或者%1");
  auto strAdjustTagLevel = QObject::tr(
      "调高TagLevel：首选项->PAG Config->通用, 将/“导出版本控制/”改为/“Beta/”. "
      "(调高后生成的pag文件可能只有较新版本SDK才能支持)");
  info = infoData.toStdString();
  suggest = suggestData.arg(strAdjustTagLevel).toStdString();
}

DEFINE_GETINFO(EffectAndStylePickNum) {
  auto infoData = QObject::tr("同一时间(第\"%1\"帧)存在的效果和图层样式超过了3个(影响性能)");
  auto suggestData = QObject::tr("建议减少同一时间的效果和图层样式的数目.");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(LayerNum) {
  auto infoData = QObject::tr("图层总数超过了 60 个(可能影响性能).");
  auto suggestData = QObject::tr("建议减少图层数目");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(VideoSequenceInUiScene) {
  auto infoData = QObject::tr("UI场景下建议不包含视频序列帧.");
  auto suggestData = QObject::tr("建议减少视频序列帧数目");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(MarkerJsonHasChinese) {
  auto infoData = QObject::tr("检测到marker中的JSON可能含有中文字符: %1");
  auto suggestData = QObject::tr("请检查Marker中JSON文本的正确性.");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(MarkerJsonGrammar) {
  auto infoData = QObject::tr("检测到marker中的JSON可能语法不正确: %1");
  auto suggestData = QObject::tr("请检查Marker中JSON语法的正确性.");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(RangeSelectorUnitsIndex) {
  auto infoData = QObject::tr("\"文本动画-范围选择器-单位\"暂不支持\"索引\".");
  auto suggestData = QObject::tr("请改用选项\"百分比\".");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(RangeSelectorBasedOn) {
  auto infoData = QObject::tr("\"文本动画-范围选择器-依据\"暂不支持\"%1\".");
  auto suggestData = QObject::tr("请改用选项\"字符\".");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(RangeSelectorSmoothness) {
  auto infoData = QObject::tr("\"文本动画-范围选择器-平滑度\"暂只支持默认值100%");
  auto suggestData = QObject::tr("请将平滑度改回默认值100%");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(WigglySelectorBasedOn) {
  auto infoData = QObject::tr("\"文本动画-摆动选择器-依据\"暂不支持\"%1\".");
  auto suggestData = QObject::tr("请改用选项\"字符\".");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(Layer3D) {
  auto infoData = QObject::tr("PAG已经支持3D属性，但您设置的TagLevel太低不支持3D属性.");
  auto suggestData = QObject::tr("如果仍想使用3D属性， 请%1");
  auto strAdjustTagLevel = QObject::tr(
      "调高TagLevel：首选项->PAG Config->通用, 将/“导出版本控制/”改为/“Beta/”. "
      "(调高后生成的pag文件可能只有较新版本SDK才能支持)");
  info = infoData.toStdString();
  suggest = suggestData.arg(strAdjustTagLevel).toStdString();
}

DEFINE_GETINFO(LayerTimeRemapping) {
  auto infoData = QObject::tr("暂不支持时间重映射.");
  auto suggestData = QObject::tr("请关闭时间重映射.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(GraphicsMemory) {
  auto infoData = QObject::tr("预览显存占用太大(\"%1\").");
  auto suggestData = QObject::tr("建议优化到80M以内.");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(GraphicsMemoryUI) {
  auto infoData = QObject::tr("预览显存占用太大(\"%1\").");
  auto suggestData = QObject::tr("UI场景下建议优化到30M以内.");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(ImageNum) {
  auto infoData = QObject::tr("图片数目太多(%1张).");
  auto suggestData = QObject::tr("建议减少图片数目(<=30张).");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(BmpCompositionNum) {
  auto infoData = QObject::tr("BMP合成数目太多(\"%1\").");
  auto suggestData = QObject::tr("建议减少BMP合成数目(<=3).");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(FontSmallAndScaleLarge) {
  auto infoData = QObject::tr("文本图层的字体太小而缩放比例过大(\"%1\"倍)，可能导致文本渲染失真.");
  auto suggestData = QObject::tr("建议将字体大小和缩放比例调整到正常大小.");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(FontFileTooBig) {
  auto infoData = QObject::tr("导出的字体文件大于30M(\"%1\"M), 可能导致web端无法上传.");
  auto suggestData = QObject::tr("建议：如果对字体库大小不敏感，可忽略；否则请改用其它字体.");
  info = infoData.arg(QString::fromStdString(addInfo)).toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(TextPathParamPerpendicularToPath) {
  auto infoData = QObject::tr("文本路径选项\"垂直于路径\"暂不支持false.");
  auto suggestData = QObject::tr("建议改为默认值true.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(TextPathParamForceAlignment) {
  auto infoData = QObject::tr("文本路径选项\"强制对齐\"暂不支持true.");
  auto suggestData = QObject::tr("建议改为默认值false.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(TextPathVertial) {
  auto infoData = QObject::tr("文本路径暂不支持竖排文本.");
  auto suggestData = QObject::tr("建议改为横排文本或去掉文本路径.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(TextPathBoxText) {
  auto infoData = QObject::tr("文本路径暂不支持框文本.");
  auto suggestData = QObject::tr("建议改为点文本或去掉文本路径.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(TextPathAnimator) {
  auto infoData = QObject::tr("文本路径和文本动画暂不兼容.");
  auto suggestData = QObject::tr("建议去掉文本路径或去掉文本动画.");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(OtherWarning) {
  info = addInfo;
}

DEFINE_GETINFO(UnknownError) {
  info = addInfo;
}

DEFINE_GETINFO(ExportAEError) {
  auto infoData = QObject::tr("AE导出错误");
  info = infoData.toStdString();
}

DEFINE_GETINFO(ExportBitmapSequenceError) {
  auto infoData = QObject::tr("位图序列帧导出错误");
  info = infoData.toStdString();
}

DEFINE_GETINFO(ExportVideoSequenceError) {
  auto infoData = QObject::tr("视频序列帧导出错误");
  info = infoData.toStdString();
}

DEFINE_GETINFO(ExportAudioError) {
  auto infoData = QObject::tr("音频导出错误.");
  info = infoData.toStdString();
}

DEFINE_GETINFO(WebpEncodeError) {
  auto infoData = QObject::tr("Webp编码错误.");
  info = infoData.toStdString();
}

DEFINE_GETINFO(ExportRenderError) {
  auto infoData = QObject::tr("导出渲染错误.");
  info = infoData.toStdString();
}

DEFINE_GETINFO(DisplacementMapRefSelf) {
  auto infoData = QObject::tr("置换图DisplacementMap暂不支持指向自身图层.");
  auto suggestData = QObject::tr("建议去掉该效果，或修改该置换图层指向其它图层");
  info = infoData.toStdString();
  suggest = suggestData.toStdString();
}

DEFINE_GETINFO(ExportRangeSlectorError) {
  auto infoData = QObject::tr("[文本动画-选择器]导出错误.");
  info = infoData.toStdString();
}

DEFINE_GETINFO(PAGVerifyError) {
  auto infoData = QObject::tr("PAG文件校验错误.");
  info = infoData.toStdString();
}

DEFINE_GETINFO(OtherError) {
  info = addInfo;
  suggest = addInfo;
}

using GetInfoHandler = void(std::string& info, std::string& suggest, std::string addInfo);

#define LINE_GETINFO(errorType) {AlertInfoType::errorType, FUNC_GETINFO(errorType)}

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
                        LINE_GETINFO(OtherWarning),
                        LINE_GETINFO(UnknownError),
                        LINE_GETINFO(ExportAEError),
                        LINE_GETINFO(ExportAudioError),
                        LINE_GETINFO(ExportBitmapSequenceError),
                        LINE_GETINFO(ExportVideoSequenceError),
                        LINE_GETINFO(WebpEncodeError),
                        LINE_GETINFO(ExportRenderError),
                        LINE_GETINFO(DisplacementMapRefSelf),
                        LINE_GETINFO(ExportRangeSlectorError),
                        LINE_GETINFO(PAGVerifyError),
                        LINE_GETINFO(OtherError)};

#undef LINE_GETINFO
#undef FUNC_GETINFO
#undef DEFINE_GETINFO

AlertInfo::AlertInfo(AlertInfoType type, AEGP_ItemH itemH, AEGP_LayerH layerH, std::string addInfo)
    : type(type), itemH(itemH), layerH(layerH) {
  auto pair = GetInfoByTypeMap.find(type);
  if (pair != GetInfoByTypeMap.end()) {
    pair->second(info, suggest, addInfo);
  } else {
    info = addInfo;
    suggest = "开发漏处理的错误.";  // 不应该执行到这里
  }

  compName = AEUtils::GetItemName(itemH);
  layerName = AEUtils::GetLayerName(layerH);

  isError = (type > AlertInfoType::OtherWarning);
}

void AlertInfo::select() {
  AEUtils::SelectItem(itemH, layerH);
}

static std::vector<AlertInfo> GetInfoList(std::vector<AlertInfo>& warningList, bool bWarning,
                                          bool bError) {
  std::vector<AlertInfo> messages;
  for (auto alert : warningList) {
    if ((bWarning && !alert.isError) || (bError && alert.isError) || (bWarning && bError)) {
      messages.push_back(alert);
    }
  }
  return messages;
}

static std::vector<std::string> AlertInfosToStrings(std::vector<AlertInfo>& alertList) {
  std::vector<std::string> list;
  for (auto alert : alertList) {
    list.push_back(alert.getMessage());
  }
  return list;
}

bool AlertInfos::show(bool showWarning, bool showError) {
  //eraseUnusedInfo();
  auto errors = GetInfoList(warningList, false, true);
  auto warnings = GetInfoList(warningList, true, false);
  for (auto info : errors) {
    saveWarnings.push_back(info);
  }
  for (auto info : warnings) {
    saveWarnings.push_back(info);
  }
  auto ret = (errors.size() > 0);
  if (ret && showError) {
    auto infos = AlertInfosToStrings(errors);
    // ErrorAlert(infos[0]);
  }
  if (!ret && showWarning) {
    auto infos = AlertInfosToStrings(warnings);
    // ret = WarningsAlert(infos);
  }
  warningList.clear();
  return ret;
}

template <typename T>
T GetHandleById(std::unordered_map<pag::ID, T> list, pag::ID id) {
  auto pair = list.find(id);
  if (pair != list.end()) {
    return pair->second;
  } else {
    return nullptr;
  }
}

void AlertInfos::pushWarning(AlertInfoType type, std::string addInfo) {
  auto itemH = GetHandleById(context->compItemHList, context->curCompId);
  auto layerH = GetHandleById(context->layerHList, context->curLayerId);
  warningList.push_back(AlertInfo(type, itemH, layerH, addInfo));
}

void AlertInfos::pushWarning(AlertInfoType type, pag::ID compId, pag::ID layerId,
                             std::string addInfo) {
  AssignRecover<pag::ID> arCI(context->curCompId, compId);
  AssignRecover<pag::ID> arLI(context->curLayerId, layerId);
  pushWarning(type, addInfo);
}

void AlertInfos::eraseUnusedInfo() {
  for (int i = static_cast<int>(warningList.size()) - 1; i >= 0; i--) {
    auto& alert = warningList[i];
    if (alert.type != AlertInfoType::UnknownWarning) {
      if ((alert.itemH != nullptr && itemHList.count(alert.itemH) == 0) ||
          (alert.layerH != nullptr && layerHList.count(alert.layerH) == 0)) {
        warningList.erase(warningList.begin() + i);
      }
    }
  }
}

static void GetAlertInfoList(Context& context, std::vector<AlertInfo>& alertList, bool isError) {
  for (auto& info : context.alertInfos.warningList) {
    if (info.isError == isError) {
      alertList.push_back(info);
    }
  }
}

std::vector<AlertInfo> AlertInfos::GetAlertList(AEGP_ItemH itemH) {
  std::vector<AlertInfo> alertList;
  Context context("./tmp.pag");
  context.enableRunScript = false;
  context.enableFontFile = false;
  context.enableAudio = false;
  context.enableForceStaticBMP = false;
  AssignRecover<pag::ID> arCI(context.curCompId, AEUtils::GetItemId(itemH));  // 暂存当前compItemId
  ExportComposition(&context, itemH);
  pag::Codec::InstallReferences(context.compositions);

  auto compositions = PAGExporter::ResortCompositions(context);

  if (compositions.size() == 0) {
    return alertList;
  }

  auto images = PAGExporter::ResortImages(context, compositions);

  ExportVerifyBefore(context, compositions, images);

  //context.alertInfos.eraseUnusedInfo();

  GetAlertInfoList(context, alertList, true);
  GetAlertInfoList(context, alertList, false);

  return alertList;
}

void PrintAlertList(std::vector<AlertInfo>& list) {
  for (auto alert : list) {
    printf("\n");
    printf("%s：%s\n", alert.isError ? "错误" : "警告", alert.info.c_str());
    printf("解决：%s\n", alert.suggest.c_str());
  }
}
}  // namespace pagexporter
