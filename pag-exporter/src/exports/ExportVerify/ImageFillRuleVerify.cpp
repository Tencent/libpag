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
#include "ImageFillRuleVerify.h"
#include "PAGFilterUtil.h"
#include "src/utils/AEUtils.h"

static inline bool HasVideoTrack(pag::Layer* layer) {
  for (auto marker : layer->markers) {
    auto pos1 = marker->comment.find("{\"videoTrack\" : 1}");
    auto pos2 = marker->comment.find("{\"videoTrack\":1}");
    if (pos1 != std::string::npos || pos2 != std::string::npos) {
      return true;
    }
  }
  return false;
}

// 判断 keyframe endTime-startTime 是否足够长 > 2 帧
static bool IsKeyframeDurationLongEnough(pag::Keyframe<pag::Frame>* keyframe) {
  return (keyframe->endTime - keyframe->startTime) > 2;
}

//倒放验证
static void ReversePlayVerfiy(pagexporter::Context& context, pag::AnimatableProperty<pag::Frame>* timeRemap) {
  auto keyframes = timeRemap->keyframes;
  for (auto& keyframe : keyframes) {
    if (keyframe->startValue > keyframe->endValue
        && keyframe->startTime + 1 != keyframe->endTime
        && IsKeyframeDurationLongEnough(keyframe)) {
      context.pushWarning(pagexporter::AlertInfoType::VideoPlayBackward);
    }
  }
}

// 将TimeRemap重新设置为从0开始
static void TimeRemapOffset(pag::AnimatableProperty<pag::Frame>* timeRemap) {
  auto keyframes = timeRemap->keyframes;
  pag::Frame startOffset = keyframes[0]->startValue;
  for (auto& keyframe : keyframes) {
    if (keyframe->startValue < startOffset) {
      startOffset = keyframe->startValue;
    }
  }
  for (auto& keyframe : keyframes) {
    keyframe->startValue = keyframe->startValue - startOffset;
    keyframe->endValue = keyframe->endValue - startOffset;
  }
}

//验证 快放建议不超过8倍速
static void TimeRemapFastPlayVerfiy(pagexporter::Context& context, pag::AnimatableProperty<pag::Frame>* timeRemap) {
  auto keyframes = timeRemap->keyframes;
  for (auto& keyframe : keyframes) {
    auto speed = (keyframe->endValue - keyframe->startValue) / (keyframe->endTime - keyframe->startTime);
    if (speed > 8 && IsKeyframeDurationLongEnough(keyframe)) {
      context.pushWarning(pagexporter::AlertInfoType::VideoSpeedTooFast);
    }
  }
}

//用户素材总时长至少大于0.5s （段落中需要填充的资源至少大于0.5s）
static void TimeRemapTotalDurationVerfiy(pagexporter::Context& context, pag::AnimatableProperty<pag::Frame>* timeRemap,
                                         pag::Composition* composition) {
  auto keyframes = timeRemap->keyframes;
  double totalDuration = 0;
  for (auto& keyframe : keyframes) {
    auto duration = (keyframe->endValue - keyframe->startValue) / composition->frameRate;
    if (IsKeyframeDurationLongEnough(keyframe)) {
      totalDuration += duration;
    }
  }
  if (abs(totalDuration) < 0.5) {
    context.pushWarning(pagexporter::AlertInfoType::VideoTimeTooShort);
  }
}

//验证 如果timeRemap中两个区间之间的间隔小于2帧，去掉该间隔区间
static void KeyframeDurationVerfiy(pagexporter::Context& context, pag::Layer* layer, pag::AnimatableProperty<pag::Frame>* timeRemap,
                                   pag::Composition* composition) {
  auto keyframes = timeRemap->keyframes;

  for (auto index = timeRemap->keyframes.size() - 1; index >= 1; index--) {
    auto keyframe = timeRemap->keyframes[index];
    auto duration = keyframe->endTime - keyframe->startTime;
    if (duration < 2) {
      timeRemap->keyframes.erase(timeRemap->keyframes.begin() + index);
    }
  }
}

struct VideoTrackLayer {
  pag::Frame compositionStartTime;
  pag::Layer* layer;
};

static void ProcessVideoTrackLayerContainVerify(pagexporter::Context& context,
                                                pag::Layer* layer,
                                                pag::Frame compositionStartTime,
                                                void* ctx) {
  auto trackLayers = static_cast<std::vector<VideoTrackLayer>*>(ctx);
  if (HasVideoTrack(layer)) {
    VideoTrackLayer trackLayer;
    trackLayer.compositionStartTime = compositionStartTime;
    trackLayer.layer = layer;
    trackLayers->push_back(trackLayer);
  }
}

// 验证 某个videoTrack图层的时间完全包含另一个videoTrack图层
static void VideoTrackLayerContainVerify(pagexporter::Context& context, std::vector<VideoTrackLayer>& trackLayers) {
  for (int i = 0; i < trackLayers.size(); i++) {
    auto videoTrackLayer = trackLayers[i];
    auto startI = videoTrackLayer.compositionStartTime + videoTrackLayer.layer->startTime;
    auto endI = startI + videoTrackLayer.layer->duration;

    for (int j = i + 1; j < trackLayers.size(); j++) {
      auto layer = trackLayers[j];
      auto startJ = layer.compositionStartTime + layer.layer->startTime;
      auto endJ = startJ + layer.layer->duration;
      if (startI <= startJ && endI >= endJ) {
        context.pushWarning(pagexporter::AlertInfoType::VideoTrackTimeCover,
                            context.curCompId,
                            videoTrackLayer.layer->id,
                            AEUtils::GetLayerName(context.getLayerHById(layer.layer->id)));
      } else if (startI >= startJ && endI <= endJ) {
        context.pushWarning(pagexporter::AlertInfoType::VideoTrackTimeCover,
                            context.curCompId,
                            layer.layer->id,
                            AEUtils::GetLayerName(context.getLayerHById(videoTrackLayer.layer->id)));
      }
    }
  }
}

// 不同videoTrack图层建议不要引用同一个素材（单个素材只能被引用一次）
static void VideoTrackLayerImageIDVerify(pagexporter::Context& context, std::vector<VideoTrackLayer>& trackLayers) {
  for (int i = 0; i < trackLayers.size(); i++) {
    auto videoTrackLayer = trackLayers[i];
    pag::ID layerIdI = videoTrackLayer.layer->id;
    pag::ID imageIdI = static_cast<pag::ImageLayer*>(videoTrackLayer.layer)->imageBytes->id;

    for (int j = i + 1; j < trackLayers.size(); j++) {
      auto layer = trackLayers[j];
      pag::ID layerIdJ = layer.layer->id;
      pag::ID imageIdJ = static_cast<pag::ImageLayer*>(layer.layer)->imageBytes->id;
      if (layerIdI != layerIdJ && imageIdI == imageIdJ) {
        context.pushWarning(pagexporter::AlertInfoType::VideoTrackRefSameSource,
                            context.curCompId,
                            layerIdI,
                            AEUtils::GetLayerName(context.getLayerHById(layerIdJ)));
      }
    }
  }
}

// 验证 videoTrack图层相关
static void VideoTrackLayerVerify(pagexporter::Context& context, pag::Composition* composition) {
  std::vector<VideoTrackLayer> trackLayers;
  trackLayers.clear();
  PAGFilterUtil::TraversalLayers(context, composition, pag::LayerType::Image, pag::ZeroFrame,
                                 &ProcessVideoTrackLayerContainVerify, &trackLayers);

  VideoTrackLayerContainVerify(context, trackLayers);

  if (context.scenes == ExportScenes::VideoEditScene) {
//        if (trackLayers.size() == 0) { //微视大片场景 每个模版至少有一个含videoTrack的图层
//            context.pushWarning("视频编辑场景每个模版至少有一个含videoTrack的图层");
//            return;
//        }
    VideoTrackLayerImageIDVerify(context, trackLayers);
  }
}

static void ProcessLayerImageFillRuleVerify(pagexporter::Context& context, pag::Layer* layer, void* ctx) {
  auto composition = static_cast<pag::Composition*>(ctx);
  auto imageFillRule = static_cast<pag::ImageLayer*>(layer)->imageFillRule;
  if (imageFillRule == nullptr) {
    return;
  }
  if (!imageFillRule->timeRemap->animatable()) {
    return;
  }
  auto timeRemap = static_cast<pag::AnimatableProperty<pag::Frame>*>(imageFillRule->timeRemap);

  TimeRemapOffset(timeRemap);

  TimeRemapFastPlayVerfiy(context, timeRemap);

  //TimeRemapTotalDurationVerfiy(context, timeRemap, composition);

  ReversePlayVerfiy(context, timeRemap);
}


void ImageFillRuleVerify(pagexporter::Context& context, pag::Composition* composition) {
  PAGFilterUtil::TraversalLayers(context, composition, pag::LayerType::Image,
                                 &ProcessLayerImageFillRuleVerify, composition);
  //VideoTrackLayerVerify(context, composition);
}
