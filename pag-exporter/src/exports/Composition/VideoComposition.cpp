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
#include "VideoComposition.h"
#include <src/utils/StringUtil.h>
#include <unordered_map>
#include "Composition.h"
#include "PAGFilterUtil.h"
#include "src/exports/Layer/ExporterLayer.h"
#include "src/exports/Sequence/VideoSequence.h"
#include "src/utils/UniqueID.h"

pag::VideoComposition* ExportVideoComposition(pagexporter::Context* context,
                                              const AEGP_CompH& compHandle) {
  auto composition = new pag::VideoComposition();

  GetCompositionAttributes(context, compHandle, composition);

  // 视频序列帧现在不导出，延后再真正导出。

  context->compositions.push_back(composition);
  return composition;
}

static void ExportVideoCompositionRect(pagexporter::Context* context,
                                       pag::VideoComposition* composition, float compositionFactor,
                                       int left, int top, int right, int bottom) {
  auto itemH = context->getCompItemHById(composition->id);
  if (itemH != nullptr) {
    context->alphaDetected = false;  // （视频序列帧）是否已经检测过是否含有alpha通道
    context->hasAlpha = false;       // （视频序列帧）是否含有alpha通道

    pagexporter::ScaleFactorAndFpsFactor reFactor = {1, 1};
    bool isFindFactor = false;
    for (int i = 0; i < context->videoCompositionFactorList.size(); i++) {
      auto videoComposition = context->videoCompositionFactorList[i];
      if (composition->id == videoComposition.first->id) {
        reFactor = videoComposition.second;
        context->videoCompositionFactorList[i].second = reFactor;
        isFindFactor = true;
        break;
      }
    }

    composition->sequences.clear();  // 重新导出前需清空 sequences

    for (int i = 0; i < context->scaleAndFpsList.size(); i++) {
      auto scaleAndFps = context->scaleAndFpsList[i];
      scaleAndFps.first = scaleAndFps.first * reFactor.first;
      scaleAndFps.second = scaleAndFps.second * reFactor.second;
      ExportVideoSequence(context, itemH, composition, scaleAndFps.first, scaleAndFps.second,
                          compositionFactor, left, top, right, bottom);
    }

    if (!isFindFactor) {
      pagexporter::ScaleAndFps scaleAndFps;
      scaleAndFps.height = composition->sequences[0]->height;
      scaleAndFps.width = composition->sequences[0]->width;
      scaleAndFps.frameRate = composition->sequences[0]->frameRate;
      context->originScaleAndFpsList.emplace_back(scaleAndFps);
      context->videoCompositionFactorList.emplace_back(std::make_pair(composition, reFactor));
    }
  }
}

// 重新更改预合成的引用关系
static void ProcessLayerReference(pagexporter::Context& context, pag::Layer* layer, void* ctx) {
  auto newComposition = static_cast<pag::Composition*>(ctx);
  auto preComposeLayer = static_cast<pag::PreComposeLayer*>(layer);
  if (preComposeLayer->composition->id == newComposition->id) {
    preComposeLayer->composition = newComposition;
  }
}

static void AdjustMainCompositionParam(pag::VectorComposition* newComposition,
                                       pag::VideoComposition* composition,
                                       std::vector<pag::Composition*>& compositions) {
  if (compositions.size() != 1) {
    return;  // 只有一个合成，说明总合成就是视频BMP合成
  }
  auto sequence = composition->sequences[0];
  double factorWidth = static_cast<double>(sequence->width) / composition->width;
  double factorHeight = static_cast<double>(sequence->height) / composition->height;
  double factorSize = std::max(factorWidth, factorHeight);
  newComposition->width = static_cast<int>(floor(newComposition->width * factorSize));
  newComposition->height = static_cast<int>(floor(newComposition->height * factorSize));

  auto& transform = newComposition->layers[0]->transform;
  transform->anchorPoint->value.x *= factorSize;
  transform->anchorPoint->value.y *= factorSize;
  transform->position->value.x *= factorSize;
  transform->position->value.y *= factorSize;

  composition->width = sequence->width;
  composition->height = sequence->height;
  composition->frameRate = sequence->frameRate;
  composition->duration = static_cast<pag::Frame>(sequence->frames.size());
  newComposition->frameRate = sequence->frameRate;
  newComposition->duration = static_cast<pag::Frame>(sequence->frames.size());
}

static void RebuildVideoComposition(pagexporter::Context* context,
                                    pag::VideoComposition* composition,
                                    std::vector<pag::Composition*>& compositions, int left, int top,
                                    int right, int bottom) {
  auto newComposition = new pag::VectorComposition();
  auto newLayer = new pag::PreComposeLayer();

  newLayer->id = GetLayerUniqueId(context, compositions);
  newLayer->transform = new pag::Transform2D();
  newLayer->transform->position = new pag::Property<pag::Point>();
  newLayer->transform->anchorPoint = new pag::Property<pag::Point>();
  newLayer->transform->scale = new pag::Property<pag::Point>();
  newLayer->transform->rotation = new pag::Property<float>();
  newLayer->transform->opacity = new pag::Property<pag::Opacity>();

  newLayer->transform->anchorPoint->value.x = (right - left) / 2;
  newLayer->transform->anchorPoint->value.y = (bottom - top) / 2;
  newLayer->transform->position->value.x = (left + right) / 2;
  newLayer->transform->position->value.y = (top + bottom) / 2;
  newLayer->transform->scale->value.x = 1.0f;
  newLayer->transform->scale->value.y = 1.0f;
  newLayer->transform->rotation->value = 0.0f;
  newLayer->transform->opacity->value = pag::Opaque;
  newLayer->composition = composition;
  newLayer->containingComposition = newComposition;
  newLayer->startTime = 0;
  newLayer->duration = composition->duration;

  newComposition->layers.push_back(newLayer);
  newComposition->width = composition->width;
  newComposition->height = composition->height;
  newComposition->duration = composition->duration;
  newComposition->frameRate = composition->frameRate;
  newComposition->id = composition->id;
  newComposition->backgroundColor = composition->backgroundColor;

  newComposition->audioBytes = composition->audioBytes;
  newComposition->audioMarkers = composition->audioMarkers;
  newComposition->audioStartTime = composition->audioStartTime;
  composition->audioBytes = nullptr;
  composition->audioMarkers.clear();
  composition->audioStartTime = pag::ZeroFrame;
  context->tmpCompositions.push_back(newComposition);

  // 重新更改预合成的引用关系
  PAGFilterUtil::TraversalLayers(*context, compositions, pag::LayerType::PreCompose,
                                 &ProcessLayerReference, newComposition);

  composition->id = GetCompositionUniqueId(context, compositions);
  composition->width = right - left;
  composition->height = bottom - top;

  AdjustMainCompositionParam(newComposition, composition, compositions);
}

void ReExportVideoComposition(pagexporter::Context* context,
                              std::vector<pag::Composition*>& compositions,
                              pag::VideoComposition* composition, float compositionFactor) {
  auto itemH = context->getCompItemHById(composition->id);
  if (itemH != nullptr) {
    int left = 0;
    int top = 0;
    int right = composition->width;
    int bottom = composition->height;

    // 视频裁剪功能与单独调节尺寸功能暂时冲突未解决，如果开启单独调节尺寸时，暂不进行视频裁剪
    if (!context->enableCompressionPanel) {
      ClipVideoComposition(context, itemH, composition, left, top, right, bottom);
    }
    if ((right - left) * (bottom - top) >= composition->width * composition->height * 0.9) {
      ExportVideoCompositionRect(context, composition, compositionFactor, 0, 0, composition->width,
                                 composition->height);
    } else {
      ExportVideoCompositionRect(context, composition, compositionFactor, left, top, right, bottom);

      RebuildVideoComposition(context, composition, compositions, left, top, right, bottom);
    }
  }
}
