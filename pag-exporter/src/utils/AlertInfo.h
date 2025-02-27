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
#ifndef ALERTINFO_H
#define ALERTINFO_H
#include <pag/file.h>
#include <string>
#include <unordered_set>
#include <vector>
#include "AEGP_SuiteHandler.h"

namespace pagexporter {
enum class AlertInfoType {
  // warning
  UnknownWarning,               // 未定义的警告
  UnsupportedEffects,           // 不支持的效果
  UnsupportedLayerStyle,        // 不支持的图层样式
  Expression,                   // 表达式
  VideoSequenceNoContent,       // 视频序列帧内容为空
  ContinuousSequence,           // 相邻的BMP合成
  BmpLayerButVectorComp,        // 图层使用了BMP后缀却指向了非BMP合成
  NoPrecompLayerWithBmpName,    // 非预合成图层使用了BMP后缀
  SameSequence,                 // 多个序列帧内容完全相同
  StaticVideoSequence,          // 含有全静止的视频序列帧
  AudioEncodeFail,              // 音频编码失败
  TextBackgroundOnlyTextLayer,  // 只有文本图层可以添加文本背景
  ImageFillRuleOnlyImageLayer,  // 只有图片图层可以添加ImageFillRule
  ImageFillRuleOnlyOne,         // 一个图层只能添加一个ImageFillRule
  TagLevelImageFillRule,        // TagLevel太低不支持运动模糊MotionBlur
  TagLevelImageFillRuleV2,      // TagLevel太低不支持ImageFillRule的非线性插值
  VideoTrackLayerRepeatRef,     // VideoTrack图层被重复引用 （业务相关，已摒弃）
  VideoTrackRefSameSource,      // 两个VideoTrack图层引用同一个素材资源 （业务相关，已摒弃）
  VideoTrackPeakNum,            // 同一时刻（峰值）VideoTrack图层数目过多
  VideoPlayBackward,            // ImageFillRule倒放
  VideoSpeedTooFast,            // 快放超过8倍数
  VideoTimeTooShort,            // 填充的视频资源太短  （业务相关，已摒弃）
  VideoTrackTimeCover,  // VideoTrack图层时间完全覆盖另一个VideoTrack图层的时间 （业务相关，已摒弃）
  TagLevelVerticalText,              // TagLevel太低不支持竖排文本
  TagLevelMotionBlur,                // TagLevel太低不支持运动模糊MotionBlur
  AdjustmentLayer,                   // 调整图层
  CameraLayer,                       // 摄像机图层
  Layer3D,                           // 3D属性
  LayerTimeRemapping,                // 时间重映射
  EffectAndStylePickNum,             // 效果和图层样式（同一时刻）的数目过多
  LayerNum,                          // 图层数目过多
  VideoSequenceInUiScene,            // UI场景使用了视频序列帧
  MarkerJsonHasChinese,              // maker中可能含有中文字符
  MarkerJsonGrammar,                 // marker json语法不正确
  RangeSelectorUnitsIndex,           // 文本动画-范围选择器-单位-索引
  RangeSelectorBasedOn,              // 文本动画-范围选择器-依据 必须为“字符”
  RangeSelectorSmoothness,           // 文本动画-范围选择器-平滑度 必须为 100%
  WigglySelectorBasedOn,             // 文本动画-摆动选择器-依据 必须为“字符”
  GraphicsMemory,                    // 显存太大
  GraphicsMemoryUI,                  // 显存太大(UI场景)
  ImageNum,                          // 图片数目太多
  BmpCompositionNum,                 // BMP合成太多
  FontSmallAndScaleLarge,            // 字体太小而缩放很大
  FontFileTooBig,                    // 字体文件太大
  TextPathParamPerpendicularToPath,  // 文本路径参数"垂直于路径"只支持默认值true
  TextPathParamForceAlignment,       // 文本路径参数"强制对齐"只支持默认值false
  TextPathVertial,                   // 文本路径暂不支持竖排文本
  TextPathBoxText,                   // 文本路径暂不支持框文本（已经支持，已摈弃）
  TextPathAnimator,                  // 文本路径和文本动画暂不兼容

  OtherWarning,  // 其它警告

  // error
  UnknownError,               // 为定义的错误
  ExportAEError,              // AE导出错误
  ExportAudioError,           // 音乐导出错误
  ExportBitmapSequenceError,  // 位图序列帧导出错误
  ExportVideoSequenceError,   // 视频序列帧导出错误
  WebpEncodeError,            // webp编码错误
  ExportRenderError,          // 导出渲染错误
  DisplacementMapRefSelf,     // DisplacementMap引用自身
  ExportRangeSlectorError,    // 范围选择器导出错误
  PAGVerifyError,             // PAG文件校验错误

  OtherError  // 其它错误
};

class Context;

class AlertInfo {
 public:
  AlertInfo(AlertInfoType type, AEGP_ItemH itemH, AEGP_LayerH layerH, std::string info = "");
  void select();  // 定位到问题合成/图层

  AEGP_ItemH itemH = nullptr;
  AEGP_LayerH layerH = nullptr;
  AlertInfoType type = AlertInfoType::OtherWarning;
  std::string compName = "";
  std::string layerName = "";
  std::string info = "";
  std::string suggest = "";
  bool isError = false;
  bool isFold = false;  // 是否展开
  int itemHeight = 36;
  bool isFoldStatusChange = true;

  std::string getMessage();
};

class AlertInfos {
 public:
  static std::vector<AlertInfo> GetAlertList(AEGP_ItemH itemH);  // 供错误列表面板调用

  AlertInfos(pagexporter::Context* context) : context(context) {
  }

  bool show(bool showWarning = true, bool showError = true);

  std::vector<AlertInfo> warningList;
  std::vector<AlertInfo> saveWarnings;  // 将警告保存到pag文件中
  std::unordered_set<AEGP_ItemH> itemHList;
  std::unordered_set<AEGP_LayerH> layerHList;

  void pushWarning(AlertInfoType type, std::string addInfo = "");
  void pushWarning(AlertInfoType type, pag::ID compId, pag::ID layerId, std::string addInfo = "");

  void eraseUnusedInfo();

 private:
  pagexporter::Context* context = nullptr;
};

void PrintAlertList(std::vector<AlertInfo>& list);
}  // namespace pagexporter

#endif  //ALERTINFO_H
