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
#include "ExportVerify.h"
#include <set>
#include <unordered_set>
#include "AEUtils.h"
#include "src/exports//ImageBytes/ImageBytes.h"
#include "ImageFillRuleVerify.h"
#include "src/utils//ImageData/ImageRawData.h"
#include "PAGFilterUtil.h"
#include "src/VideoEncoder/VideoEncoder.h"
#include "src/exports/Sequence/VideoSequence.h"
#include "src/cJSON/cJSON.h"
#include "src/utils/AEUtils.h"
#include "src/utils/StringUtil.h"

static bool HasVideoTrack(pag::Layer* layer) {
  for (auto marker : layer->markers) {
    auto pos1 = marker->comment.find("{\"videoTrack\" : 1}");
    auto pos2 = marker->comment.find("{\"videoTrack\":1}");
    if (pos1 != std::string::npos || pos2 != std::string::npos) {
      return true;
    }
  }
  return false;
}

static void ProcessLayerVideoTrack(pagexporter::Context& context, pag::Layer* layer, void* ctx) {
  if (!HasVideoTrack(layer)) {
    return;
  }

  auto layerIDs = static_cast<std::unordered_set<pag::ID>*>(ctx);
  if (layerIDs->count(layer->id) == 0) {
    layerIDs->insert(layer->id);
  } else {
    context.pushWarning(pagexporter::AlertInfoType::VideoTrackLayerRepeatRef);
  }
}

static void ProcessLayerVideoTrack2(pagexporter::Context& context, pag::Layer* layer,
                                    pag::Frame compositionStartTime, void* ctx) {
  if (!HasVideoTrack(layer)) {
    return;
  }

  auto timeRangeCounts = static_cast<std::vector<std::pair<pag::TimeRange, int64_t>>*>(ctx);
  pag::TimeRange timeRange = {layer->startTime + compositionStartTime,
                              layer->startTime + layer->duration + compositionStartTime};
  timeRangeCounts->push_back(std::make_pair(timeRange, 1));
}

static void ProcessLayerVideoTrackRecordLayerName(pagexporter::Context& context, pag::Layer* layer,
                                                  pag::Frame compositionStartTime, void* ctx) {
  if (!HasVideoTrack(layer)) {
    return;
  }
  if ((layer->startTime + layer->duration) < compositionStartTime ||
      layer->startTime > compositionStartTime) {
    return;
  }
  std::string* layerNames = static_cast<std::string*>(ctx);
  std::string name = "\"" + layer->name + "\"";
  if (layerNames->empty()) {
    *layerNames += name;
  } else {
    *layerNames = *layerNames + ", " + name;
  }
}

static void ProcessLayerEffectCount(pagexporter::Context& context, pag::Layer* layer,
                                    pag::Frame compositionStartTime, void* ctx) {
  int64_t count = static_cast<int64_t>(layer->layerStyles.size() + layer->effects.size());
  if (count == 0) {
    return;
  }

  auto timeRangeCounts = static_cast<std::vector<std::pair<pag::TimeRange, int64_t>>*>(ctx);
  pag::TimeRange timeRange = {layer->startTime + compositionStartTime,
                              layer->startTime + layer->duration + compositionStartTime};
  timeRangeCounts->push_back(std::make_pair(timeRange, count));
}

static bool TimeCompare(const std::pair<pag::Frame, int64_t> a,
                        const std::pair<pag::Frame, int64_t> b) {
  return a.first < b.first;
}

// 统计峰值数目
static int GetPeakCount(std::vector<std::pair<pag::TimeRange, int64_t>>& timeRangeCounts,
                        pag::Frame* pMaxCountFrame = nullptr) {

  if (timeRangeCounts.size() <= 0) {
    return 0;
  }

  std::vector<std::pair<pag::Frame, int64_t>> startTimeCounts;
  std::vector<std::pair<pag::Frame, int64_t>> endTimeCounts;

  for (auto timeRangeCount : timeRangeCounts) {
    startTimeCounts.push_back(std::make_pair(timeRangeCount.first.start, timeRangeCount.second));
    endTimeCounts.push_back(std::make_pair(timeRangeCount.first.end, timeRangeCount.second));
  }

  sort(startTimeCounts.begin(), startTimeCounts.end(), TimeCompare);
  sort(endTimeCounts.begin(), endTimeCounts.end(), TimeCompare);

  int si = 0;
  int ei = 0;
  int maxCount = 0;
  int count = 0;
  pag::Frame maxCountFrame = 0;
  while (si < startTimeCounts.size()) {
    if (startTimeCounts[si].first < endTimeCounts[ei].first) {
      count += startTimeCounts[si].second;
      if (maxCount < count) {
        maxCount = count;
        maxCountFrame = startTimeCounts[si].first;
      }
      si++;
    } else {
      count -= endTimeCounts[ei].second;
      ei++;
    }
  }

  if (pMaxCountFrame != nullptr) {
    *pMaxCountFrame = maxCountFrame;
  }
  return maxCount;
}

static void VideoTrackVerify(pagexporter::Context& context, pag::Composition* composition) {
  //std::unordered_set<pag::ID> layerIDs;
  //PAGFilterUtil::TraversalLayers(context, composition, pag::LayerType::Image,
  //                               &ProcessLayerVideoTrack, &layerIDs);

  std::vector<std::pair<pag::TimeRange, int64_t>> timeRangeCounts;
  PAGFilterUtil::TraversalLayers(context, composition, pag::LayerType::Image, 0,
                                 &ProcessLayerVideoTrack2, &timeRangeCounts);

  pag::Frame peakFrame;
  if (GetPeakCount(timeRangeCounts, &peakFrame) > 2) {
    std::string layerNames;
    PAGFilterUtil::TraversalLayers(context, composition, pag::LayerType::Image, peakFrame,
                                   &ProcessLayerVideoTrackRecordLayerName, &layerNames);
    context.pushWarning(pagexporter::AlertInfoType::VideoTrackPeakNum, std::to_string(peakFrame));
  }
}

static void EffectVerify(pagexporter::Context& context, pag::Composition* composition) {
  std::vector<std::pair<pag::TimeRange, int64_t>> timeRangeCounts;
  PAGFilterUtil::TraversalLayers(context, composition, pag::LayerType::Unknown, 0,
                                 &ProcessLayerEffectCount, &timeRangeCounts);
  pag::Frame peakFrame = 0;
  if (GetPeakCount(timeRangeCounts, &peakFrame) > 3) {
    context.pushWarning(pagexporter::AlertInfoType::EffectAndStylePickNum, std::to_string(peakFrame));
  }
}

static bool isStaticVideoSequence(pag::Composition* composition) {
  if (composition->type() == pag::CompositionType::Video) {
    auto sequences = static_cast<pag::VideoComposition*>(composition)->sequences;
    if (sequences.size() > 0) {
      auto staticTimeRanges = sequences[0]->staticTimeRanges;
      if (staticTimeRanges.size() == 1 &&
          staticTimeRanges[0].end - staticTimeRanges[0].start + 1 == sequences[0]->duration()) {
        return true;
      }
    }
  }
  return false;
}

//获取图层总数
static int GetNumberOfLayers(const std::vector<pag::Composition*>& compositions) {
  int numLayers = 0;
  for (auto composition : compositions) {
    if (composition->type() != pag::CompositionType::Vector) {
      numLayers++;
      continue;
    }
    for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
      if (layer->type() == pag::LayerType::PreCompose) {
        continue;
      }
      numLayers++;
    }
  }
  return numLayers;
}

// 相邻的多个序列帧(_bmp)建议合并为一个。
static void CheckContinuousSequenceComposition(pagexporter::Context& context, pag::Composition* composition) {
  if (composition->type() != pag::CompositionType::Vector) {
    return;
  }

  bool lastIsSequence = false;
  auto lastBlendMode = pag::BlendMode::Normal;
  pag::ID lastCompostionId = pag::ZeroID;

  for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
    if (layer->type() == pag::LayerType::PreCompose) {
      auto subCompostion = static_cast<pag::PreComposeLayer*>(layer)->composition;

      if (lastCompostionId != subCompostion->id) {
        lastCompostionId = subCompostion->id;
        if (subCompostion->type() != pag::CompositionType::Vector) {
          if (lastIsSequence && layer->blendMode == lastBlendMode) {
            context.pushWarning(pagexporter::AlertInfoType::ContinuousSequence, composition->id, layer->id);
          }
          lastIsSequence = true;
          lastBlendMode = layer->blendMode;
        } else {
          CheckContinuousSequenceComposition(context, subCompostion);
          lastIsSequence = false;
        }
      }
    } else {
      lastIsSequence = false;
    }
  }
}

// 非 video composition 命名为 _bmp 提示
static void BmpSuffixVerify(pagexporter::Context& context, pag::Composition* composition) {
  if (composition->type() != pag::CompositionType::Vector) {
    return;
  }
  for (auto layer : static_cast<pag::VectorComposition*>(composition)->layers) {
    if (layer->type() == pag::LayerType::PreCompose) {
      auto subCompostion = static_cast<pag::PreComposeLayer*>(layer)->composition;
      if (subCompostion->type() == pag::CompositionType::Vector) {
        if (context.exportTagLevel >= static_cast<uint16_t>(pag::TagCode::VideoSequence)) {
          if (StringUtil::IsSuffixContainsSequenceSuffix(layer->name, context)) {
            context.pushWarning(pagexporter::AlertInfoType::BmpLayerButVectorComp, composition->id, layer->id);
          }
        }
        BmpSuffixVerify(context, subCompostion);
      }
    } else {
      if (StringUtil::IsSuffixContainsSequenceSuffix(layer->name, context)) {
        context.pushWarning(pagexporter::AlertInfoType::NoPrecompLayerWithBmpName, composition->id, layer->id);
      }
    }
  }
}

// 验证图层总数是否超标
static void NumberOfLayersVerify(pagexporter::Context& context,
                                 const std::vector<pag::Composition*>& compositions) {
  if (GetNumberOfLayers(compositions) > 60) {
    context.pushWarning(pagexporter::AlertInfoType::LayerNum);
  }
}

//UI场景 只要有序列帧就警告
static void VideoCompositionInUIScenesVerfiy(pagexporter::Context& context,
                                             const std::vector<pag::Composition*>& compositions) {
  if (context.scenes != ExportScenes::UIScene) {
    return;
  }
  for (int i = 0; i < compositions.size(); ++i) {
    if (compositions[i]->type() == pag::CompositionType::Video) {
      context.pushWarning(pagexporter::AlertInfoType::VideoSequenceInUiScene, compositions[i]->id, 0);
    }
  }
}

//检查序列帧数
static void CheckBitmapAndVideoNum(pagexporter::Context& context, std::vector<pag::ImageBytes*>& images,
                                   std::vector<pag::Composition*>& compositions) {
  int sequenceCompositionNums = 0;
  int staticVideoSequencesNum = 0;
  for (auto composition : compositions) {
    if (composition->type() != pag::CompositionType::Vector) {
      if (isStaticVideoSequence(composition)) {
        staticVideoSequencesNum++;
      } else {
        sequenceCompositionNums++;
      }
    }
  }

  // 限制导出位图数目最大为30，超过30提示警告
  int imageNum = static_cast<int>(images.size()) + staticVideoSequencesNum;
  if (imageNum > 30) {
    context.pushWarning(pagexporter::AlertInfoType::ImageNum, std::to_string(imageNum));
    return;
  }

  // 序列帧数目过多时给出警告（可能导致卡顿）。
  if (sequenceCompositionNums > 3) {
    context.pushWarning(pagexporter::AlertInfoType::BmpCompositionNum, std::to_string(sequenceCompositionNums));
  }
}

// 获取某一帧data
static uint8_t* GetVideoFrameData(pagexporter::Context& context, const pag::VideoComposition* composition,
                                  uint8_t* data, A_u_long stride, int frame) {
  int width = composition->width;
  int height = composition->height;

  auto itemH = context.getCompItemHById(composition->id);
  if (itemH == nullptr) {
    return nullptr;
  }
  auto& suites = context.suites;
  AEGP_RenderOptionsH renderOptions = nullptr;

  suites.RenderOptionsSuite3()->AEGP_NewFromItem(context.pluginID, itemH, &renderOptions);
  suites.RenderOptionsSuite3()->AEGP_SetWorldType(renderOptions, AEGP_WorldType_8);

  SetRenderTime(&context, renderOptions, composition->frameRate, frame);  // 设置render时间
  int tmpWidth = 0;
  int tmpHeight = 0;
  GetRenderFrame(data, stride, tmpWidth, tmpHeight, suites, renderOptions);

  if (tmpWidth == width && tmpHeight == height) {
    return data;
  }
  return nullptr;
}

static bool CompareVideoCompositions(pagexporter::Context& context,
                                     const pag::VideoComposition* videoComposition1,
                                     const pag::VideoComposition* videoComposition2) {
  if (videoComposition1->width != videoComposition2->width ||
      videoComposition1->height != videoComposition2->height) {
    return false;
  }
  if (videoComposition1->duration != videoComposition2->duration) {
    return false;
  }
  if (videoComposition1->frameRate != videoComposition2->frameRate) {
    return false;
  }

  int width = videoComposition1->width;
  int height = videoComposition1->height;
  A_u_long stride = static_cast<A_u_long>(SIZE_ALIGN(width) * 4);
  auto data1 = new uint8_t[stride * SIZE_ALIGN(height) + stride * 2];
  auto data2 = new uint8_t[stride * SIZE_ALIGN(height) + stride * 2];
  bool isSame = true;

  for (int frame = 0; frame < videoComposition1->duration && !context.bEarlyExit; frame++) {
    GetVideoFrameData(context, videoComposition1, data1, stride, frame);
    GetVideoFrameData(context, videoComposition2, data2, stride, frame);
    if (!ImageIsStatic(data1, data2, width, height, stride)) {
      isSame = false;
      break;
    }
  }
  delete[] data1;
  delete[] data2;
  return isSame;
}

//判断序列帧是否相同，进行排重。
static void VideoSequenceRepeatVerify(pagexporter::Context& context,
                                      std::vector<pag::Composition*>& compositions) {
  if (!context.enableForceStaticBMP) {
    return;
  }
  std::vector<pag::VideoComposition*> videoCompositions;
  videoCompositions.clear();
  for (auto composition : compositions) {
    if (composition->type() == pag::CompositionType::Video) {
      videoCompositions.push_back(static_cast<pag::VideoComposition*>(composition));
    }
  }

  if (videoCompositions.size() < 2) {
    return;
  }

  for (int i = 0; i < videoCompositions.size(); i++) {
    for (int j = i + 1; j < videoCompositions.size() && !context.bEarlyExit; j++) {
      if (CompareVideoCompositions(context, videoCompositions[i], videoCompositions[j])) {
        // 视频序列帧相同 排重逻辑
        context.pushWarning(pagexporter::AlertInfoType::SameSequence, videoCompositions[j]->id, 0);
      }
    }
  }
}

//检查静态视频序列帧
static void CheckStaticVideoSequence(pagexporter::Context& context,
                                     std::vector<pag::Composition*>& compositions) {
  for (auto composition : compositions) {
    if (isStaticVideoSequence(composition)) {
      context.pushWarning(pagexporter::AlertInfoType::StaticVideoSequence, composition->id, 0);
    }
  }
}

static bool HasChineseCharacter(const char* data) {
  auto len = strlen(data);
  bool flag = false;  // 双引号是否有效状态
  for (int i = 0; i < len; i++) {
    if (data[i] == '\"') {
      flag = !flag;  // 更改双引号状态
    }
    if (data[i] & 0x80) {  // data[i] >= 128
      if (!flag) {         // 不在双引号内
        return true;
      }
      i++;
    }
  }

  return false;
}

static void CheckLayerMarkerCharacter(pagexporter::Context& context, pag::Layer* layer) {
  for (auto marker : layer->markers) {
    auto& comment = marker->comment;

    auto cjson = cJSON_Parse(comment.c_str());
    if (cjson != nullptr) {
      // 正常的json文本
      cJSON_Delete(cjson);
      continue;
    }

    // 检测是否包含在{}内。
    auto posStart = comment.find_first_of("{");  // 英文左大括号
    auto posEnd = comment.find_first_of("}");    // 英文右大括号
    if (posStart == std::string::npos || posEnd == std::string::npos) {
      continue;  // 没有左右大括号，算正常标注
    }

    // 有左右大括号，提示可能json错误
    if (HasChineseCharacter(comment.c_str())) {
      // 含有中文字符
      context.pushWarning(pagexporter::AlertInfoType::MarkerJsonHasChinese, comment);
    } else {
      // json语法不正确
      context.pushWarning(pagexporter::AlertInfoType::MarkerJsonGrammar, comment);
    }
  }
}

static void CheckLayerDisplacementMap(pagexporter::Context& context, pag::Layer* layer) {
  for (auto effect : layer->effects) {
    if (effect->type() == pag::EffectType::DisplacementMap) {
      auto displacementMapLayer =
          static_cast<pag::DisplacementMapEffect*>(effect)->displacementMapLayer;
      if (displacementMapLayer != nullptr && displacementMapLayer->id == layer->id) {
        context.pushWarning(pagexporter::AlertInfoType::DisplacementMapRefSelf);
      }
    }
  }
}

static const std::string NameOfBasedOn[] = {"字符", "不包含空格的字符", "词", "行"};

// 检查图层中文本动画的有效性
static void CheckLayerTextAnimator(pagexporter::Context& context, pag::Layer* layer) {
  for (auto animator : static_cast<pag::TextLayer*>(layer)->animators) {
    for (auto selector : animator->selectors) {
      if (!selector->verify()) {
        context.pushWarning(pagexporter::AlertInfoType::ExportRangeSlectorError);
      }
      if (selector->type() == pag::TextSelectorType::Range) {
        auto rangeSelector = static_cast<pag::TextRangeSelector*>(selector);
        if (rangeSelector->units != pag::TextRangeSelectorUnits::Percentage) {
          context.pushWarning(pagexporter::AlertInfoType::RangeSelectorUnitsIndex);
        }
        if (rangeSelector->basedOn != pag::TextSelectorBasedOn::Characters) {
          auto name = NameOfBasedOn[rangeSelector->basedOn];
          context.pushWarning(pagexporter::AlertInfoType::RangeSelectorBasedOn, name);
        }
        if (rangeSelector->shape == pag::TextRangeSelectorShape::Square &&
            (rangeSelector->smoothness->animatable() || rangeSelector->smoothness->value != 1.0f)) {
          context.pushWarning(pagexporter::AlertInfoType::RangeSelectorSmoothness);
        }
      } else if (selector->type() == pag::TextSelectorType::Wiggly) {
        auto wigglySelector = static_cast<pag::TextWigglySelector*>(selector);
        if (wigglySelector->basedOn != pag::TextSelectorBasedOn::Characters) {
          auto name = NameOfBasedOn[wigglySelector->basedOn];
          context.pushWarning(pagexporter::AlertInfoType::WigglySelectorBasedOn, name);
        }
      }
    }
  }
}

// 检查图层中一些开关/属性的有效性
static void CheckLayerFlags(pagexporter::Context& context, pag::Layer* layer) {
  auto layerH = context.getLayerHById(layer->id);
  if (layerH == nullptr) {
    return;
  }
  auto& suites = context.suites;
  AEGP_LayerFlags layerFlags;
  suites.LayerSuite6()->AEGP_GetLayerFlags(layerH, &layerFlags);
  if (layerFlags & AEGP_LayerFlag_ADJUSTMENT_LAYER) {
    context.pushWarning(pagexporter::AlertInfoType::AdjustmentLayer);
  }
  if ((layerFlags & AEGP_LayerFlag_LAYER_IS_3D) &&
      context.exportTagLevel < static_cast<uint16_t>(pag::TagCode::Transform3D)) {
    context.pushWarning(pagexporter::AlertInfoType::Layer3D);
  }
  if (layerFlags & AEGP_LayerFlag_TIME_REMAPPING) {
    context.pushWarning(pagexporter::AlertInfoType::LayerTimeRemapping);
  }
  auto motionBlur = static_cast<bool>(layerFlags & AEGP_LayerFlag_MOTION_BLUR);
  if (motionBlur &&
      context.exportTagLevel < static_cast<uint16_t>(pag::TagCode::LayerAttributesExtra)) {
    context.pushWarning(pagexporter::AlertInfoType::TagLevelMotionBlur);
  }
}

// 检查图层各属性的有效性
static void ProcessLayerPropterty(pagexporter::Context& context, pag::Layer* layer, void* ctx) {
  CheckLayerMarkerCharacter(context, layer);
  CheckLayerDisplacementMap(context, layer);
  if (layer->type() == pag::LayerType::Text) {
    CheckLayerTextAnimator(context, layer);
  }
  CheckLayerFlags(context, layer);
}

// 检查所有合成中文本动画的有效性
static void CheckLayerPropterty(pagexporter::Context& context, std::vector<pag::Composition*>& compositions) {
  PAGFilterUtil::TraversalLayers(context, compositions, pag::LayerType::Unknown,
                                 &ProcessLayerPropterty, nullptr);
}

// 检查文本中是否有竖排文本
static bool HasVerticalText(pag::Property<pag::TextDocumentHandle>* sourceText) {
  if (sourceText != nullptr) {
    if (sourceText->animatable()) {
      for (auto keyframe :
           reinterpret_cast<pag::AnimatableProperty<pag::TextDocumentHandle>*>(sourceText)
               ->keyframes) {
        if (keyframe->startValue->direction == pag::TextDirection::Vertical ||
            keyframe->endValue->direction == pag::TextDirection::Vertical) {
          return true;
        }
      }
    } else {
      auto textDocument = sourceText->getValueAt(0);
      if (textDocument->direction == pag::TextDirection::Vertical) {
        return true;
      }
    }
  }
  return false;
}

static void ProcessTextLayer(pagexporter::Context& context, pag::Layer* layer, void* ctx) {
  if (layer->type() != pag::LayerType::Text) {
    return;
  }

  auto textLayer = static_cast<pag::TextLayer*>(layer);

  if (textLayer->transform != nullptr && textLayer->transform->scale->animatable() == 0) {
    auto& scale = textLayer->transform->scale->value;
    auto minScale = std::min(scale.x, scale.y);
    if (minScale >= 5.0f) {  // 缩放比例过大
      if (textLayer->sourceText->animatable() == 0) {
        auto fontSize = textLayer->getTextDocument()->fontSize;
        if (fontSize <= 5.0f) {  // 且字体太小
          context.pushWarning(pagexporter::AlertInfoType::FontSmallAndScaleLarge,
                              std::to_string(static_cast<int>(minScale)));
        }
      }
    }
  }

  // 检查文本路径参数的正确性
  if (textLayer->pathOption != nullptr && textLayer->pathOption->path != nullptr) {
    auto perpendicularToPath = textLayer->pathOption->perpendicularToPath;
    auto forceAlignment = textLayer->pathOption->forceAlignment;
    if (perpendicularToPath->animatable() || perpendicularToPath->value != true) {
      context.pushWarning(pagexporter::AlertInfoType::TextPathParamPerpendicularToPath);
    }
    if (forceAlignment->animatable() || forceAlignment->value != false) {
      context.pushWarning(pagexporter::AlertInfoType::TextPathParamForceAlignment);
    }

    if (HasVerticalText(textLayer->sourceText)) {
      context.pushWarning(pagexporter::AlertInfoType::TextPathVertial);
    }

    if (textLayer->animators.size() > 0) {
      context.pushWarning(pagexporter::AlertInfoType::TextPathAnimator);
    }
  }
}

static void CheckTextLayerVerify(pagexporter::Context& context, std::vector<pag::Composition*>& compositions) {
  PAGFilterUtil::TraversalLayers(context, compositions, pag::LayerType::Text, &ProcessTextLayer,
                                 nullptr);
}

void ExportVerifyBefore(pagexporter::Context& context, std::vector<pag::Composition*>& compositions,
                        std::vector<pag::ImageBytes*>& images) {
  CheckBitmapAndVideoNum(context, images, compositions);
  EffectVerify(context, compositions.back());
  VideoTrackVerify(context, compositions.back());
  CheckContinuousSequenceComposition(context, compositions.back());
  BmpSuffixVerify(context, compositions.back());
  NumberOfLayersVerify(context, compositions);
  ImageFillRuleVerify(context, compositions.back());
  VideoCompositionInUIScenesVerfiy(context, compositions);
  VideoSequenceRepeatVerify(context, compositions);
  CheckLayerPropterty(context, compositions);
  CheckTextLayerVerify(context, compositions);
}

void ExportVerifyAfter(pagexporter::Context& context, std::vector<pag::Composition*>& compositions,
                       std::vector<pag::ImageBytes*>& images) {
  CheckStaticVideoSequence(context, compositions);

  //WriteH264Data(compositions);
}

void CheckGraphicsMemery(pagexporter::Context& context, const std::shared_ptr<pag::File> pagFile) {
  auto graphicsMemorySize = CalculateGraphicsMemory(pagFile);
  if (context.scenes == ExportScenes::UIScene && graphicsMemorySize > 30 * 1024 * 1024) {
    context.pushWarning(pagexporter::AlertInfoType::GraphicsMemoryUI,
                        std::to_string(graphicsMemorySize / 1024 / 1024) + "MB");
  } else if (graphicsMemorySize > 80 * 1024 * 1024) {
    context.pushWarning(pagexporter::AlertInfoType::GraphicsMemory,
                        std::to_string(graphicsMemorySize / 1024 / 1024) + "MB");
  }
}

static void WriteH264DataFromSequence(pag::ID id, pag::VideoSequence* sequence) {
  int width = sequence->width;
  int height = sequence->height;
  float frameRate = sequence->frameRate;
  const char* filePath = "/Users/chenxinxing/data/";
  char fileName[1024];
  snprintf(fileName, sizeof(fileName), "%s%d_%dx%dx%.0f.264", filePath, (int)(id), width, height,
           frameRate);
  FILE* fp = fopen(fileName, "wb");
  for (auto header : sequence->headers) {
    fwrite(header->data(), 1, header->length(), fp);
  }
  for (auto frame : sequence->frames) {
    fwrite(frame->fileBytes->data(), 1, frame->fileBytes->length(), fp);
  }
  fclose(fp);
}

void WriteH264Data(std::vector<pag::Composition*>& compositions) {
  for (auto composition : compositions) {
    if (composition->type() == pag::CompositionType::Video) {
      for (auto sequence : static_cast<pag::VideoComposition*>(composition)->sequences) {
        WriteH264DataFromSequence(composition->id, sequence);
      }
    }
  }
}
