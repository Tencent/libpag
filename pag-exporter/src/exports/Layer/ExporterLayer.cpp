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
#include "ExporterLayer.h"
#include <unordered_set>
#include <unordered_map>
#include "src/exports/CameraOption.h"
#include "src/exports/Transform/Transform2D.h"
#include "src/exports/Transform/Transform3D.h"
#include "src/exports/Stream/StreamProperty.h"
#include "src/exports/Mask/Mask.h"
#include "src/exports/Effect/Effect.h"
#include "src/exports/Layer/LayerStyles.h"
#include "src/exports/TextProperties/TextProperties.h"
#include "src/exports/Shape/Shape.h"
#include "src/exports/ImageBytes/ImageBytes.h"
#include "src/exports/Composition/Composition.h"
#include "src/exports/AEMarker/AEMarker.h"
#include "src/utils/StringUtil.h"
#include "src/utils/AEUtils.h"
#include "src/utils/CommonMethod.h"
#include "AE_GeneralPlug.h"

static bool LayerHasTrackMatte(pag::Enum type) {
  switch (type) {
    case pag::TrackMatteType::Alpha:
    case pag::TrackMatteType::AlphaInverted:
    case pag::TrackMatteType::Luma:
    case pag::TrackMatteType::LumaInverted:
      return true;
    default:
      return false;
  }
}

static ExportLayerType GetLayerType(pagexporter::Context* context, const AEGP_LayerH& layerHandle) {
  auto& suites = context->suites;
  AEGP_LayerFlags layerFlags;
  RECORD_ERROR(suites.LayerSuite6()->AEGP_GetLayerFlags(layerHandle, &layerFlags));
  if (layerFlags & AEGP_LayerFlag_NULL_LAYER ||
      layerFlags & AEGP_LayerFlag_GUIDE_LAYER) {
    return ExportLayerType::Null;
  }
  if (layerFlags & AEGP_LayerFlag_ADJUSTMENT_LAYER) {
    return ExportLayerType::Null;
  }
  AEGP_ObjectType layerType;
  RECORD_ERROR(suites.LayerSuite6()->AEGP_GetLayerObjectType(layerHandle, &layerType));
  if (layerType == AEGP_ObjectType_VECTOR) {
    return ExportLayerType::Shape;
  } else if (layerType == AEGP_ObjectType_TEXT) {
    return ExportLayerType::Text;
  } else if (layerType == AEGP_ObjectType_CAMERA) {
    return ExportLayerType::Camera;
  } else if (layerType == AEGP_ObjectType_AV) {
    auto itemHandle = AEUtils::GetItemFromLayer(layerHandle);
    AEGP_ItemType itemType;
    RECORD_ERROR(suites.ItemSuite6()->AEGP_GetItemType(itemHandle, &itemType));
    if (itemType == AEGP_ItemType_COMP) {
      return ExportLayerType::PreCompose;
    }
    if (itemType == AEGP_ItemType_FOOTAGE) {
      AEGP_ItemFlags itemFlags;
      RECORD_ERROR(suites.ItemSuite6()->AEGP_GetItemFlags(itemHandle, &itemFlags));
      // solid, psd 都会带 video 属性，所以挪到前面判断。这样可以过滤掉
      // solid 和 psd。后面就是真正的 video 了
      if (itemFlags & AEGP_ItemFlag_STILL) {
        AEGP_FootageH footageHandle;
        RECORD_ERROR(suites.FootageSuite5()->AEGP_GetMainFootageFromItem(
            itemHandle, &footageHandle));
        AEGP_FootageSignature signature;
        RECORD_ERROR(suites.FootageSuite5()->AEGP_GetFootageSignature(
            footageHandle, &signature));
        if (signature == AEGP_FootageSignature_SOLID) {
          return ExportLayerType::Solid;
        }
        if (signature != AEGP_FootageSignature_MISSING &&
            signature != AEGP_FootageSignature_NONE) {
          return ExportLayerType::Image;
        }
      } else if (itemFlags & AEGP_ItemFlag_HAS_VIDEO) {
        return ExportLayerType::Video;
      } else if ((itemFlags & AEGP_ItemFlag_HAS_AUDIO) &&
                 !(itemFlags & AEGP_ItemFlag_HAS_VIDEO)) {
        // Audio Layer;

        // 这里导出AudioLayer的marker信息（用于记录音乐ID）
        // 这段代码本不应该出现在这里，但因为目前没有导出专门的AudioLayer，因此在别的地方不方便操作。。。所以暂时放在这里
        if (context->exportTagLevel >=
            static_cast<uint16_t>(pag::TagCode::MarkerList)) {
          AEMarker::ExportMarkers(context, layerHandle, *context->pAudioMarkers);
        }

        return ExportLayerType::Audio;
        // in this case, it is a movie.
      } else if (itemFlags & AEGP_ItemFlag_MISSING) {
        return ExportLayerType::Unknown;
      }
    }
    return ExportLayerType::Null;
  }
  return ExportLayerType::Unknown;
}

static void ExportBaseLayer(pagexporter::Context* context, const AEGP_LayerH& layerHandle, pag::Layer* layer) {
  auto& suites = context->suites;
  // 这里默认写入layer name 在导出验证完成后再决定是否置空
  layer->name = AEUtils::GetLayerName(layerHandle);
  layer->id = AEUtils::GetLayerId(layerHandle);
  AEGP_LayerH parentHandle;
  RECORD_ERROR(suites.LayerSuite6()->AEGP_GetLayerParent(layerHandle, &parentHandle));
  layer->parent = ExportLayerID(parentHandle, suites);
  A_Ratio stretch = {};
  RECORD_ERROR(suites.LayerSuite6()->AEGP_GetLayerStretch(layerHandle, &stretch));
  layer->stretch = ExportRatio(stretch);
  A_Time inPoint = {};
  RECORD_ERROR(suites.LayerSuite6()->AEGP_GetLayerInPoint(layerHandle, AEGP_LTimeMode_CompTime, &inPoint));
  layer->startTime = ExportTime(inPoint, context);
  A_Time duration = {};
  RECORD_ERROR(suites.LayerSuite6()->AEGP_GetLayerDuration(layerHandle, AEGP_LTimeMode_CompTime, &duration));
  layer->duration = ExportTime(duration, context);
  AEGP_LayerFlags layerFlags;
  RECORD_ERROR(suites.LayerSuite6()->AEGP_GetLayerFlags(layerHandle, &layerFlags));
  layer->autoOrientation = static_cast<bool>(layerFlags & AEGP_LayerFlag_AUTO_ORIENT_ROTATION);
  if (layer->type() != pag::LayerType::Camera) {
    layer->motionBlur = static_cast<bool>(layerFlags & AEGP_LayerFlag_MOTION_BLUR);
    if (layer->motionBlur &&
        context->exportTagLevel < static_cast<uint16_t>(pag::TagCode::LayerAttributesExtra)) {
      layer->motionBlur = false;
    }
  }
  if ((layerFlags & AEGP_LayerFlag_LAYER_IS_3D) &&
      context->exportTagLevel >= static_cast<uint16_t>(pag::TagCode::Transform3D)) {
    layer->transform3D = ExportTransform3D(context, layerHandle);
  } else {
    layer->transform = ExportTransform2D(context, layerHandle);
  }
  if (layerFlags & AEGP_LayerFlag_TIME_REMAPPING) {
    layer->timeRemap = ExportProperty(context, layerHandle, AEGP_LayerStream_TIME_REMAP, Parsers::Float);
  }
  if (layer->type() != pag::LayerType::Camera) {
    AEGP_LayerTransferMode transferMode = {};
    RECORD_ERROR(suites.LayerSuite6()->AEGP_GetLayerTransferMode(layerHandle, &transferMode));
    layer->blendMode = ExportLayerBlendMode(transferMode.mode);
    layer->trackMatteType = ExportTrackMatte(transferMode.track_matte);
    // AEGP_GetLayerTransferMode查询到的track_matte可能不准确，存在ae项目未设置轨道遮罩，但track_matte不为none的情况
    // 因此通过如下接口做二次检查，若不存在轨道遮罩则将trackMatteType改为none
    A_Boolean    has_track_mattePB;
    RECORD_ERROR(suites.LayerSuite9()->AEGP_DoesLayerHaveTrackMatte(layerHandle,&has_track_mattePB));
    if(!has_track_mattePB){
      layer->trackMatteType = pag::TrackMatteType::None;
    }
    {
      if(LayerHasTrackMatte(layer->trackMatteType)){
        // 获取当前图层的遮罩层
        AEGP_LayerH trackMatteLayer;
        RECORD_ERROR(suites.LayerSuite9()->AEGP_GetTrackMatteLayer(layerHandle,&trackMatteLayer));
        if(trackMatteLayer!=nullptr){
          if(!ifCopyMatlayer){
            pagToAELayer[layer]=trackMatteLayer;
          }
        }else{
          // trackMatteType不为none，但没有获取到轨道遮罩层的地址，则将trackMatteType改为none
          layer->trackMatteType = pag::TrackMatteType::None;
        }
      }
    }
    layer->masks = ExportMasks(context, layerHandle);
    layer->effects = ExportEffects(context, layerHandle);
    layer->layerStyles = ExportLayerStyles(context, layerHandle);
    ExportAttachments(context, layerHandle, layer);
  }

  if (context->exportTagLevel >= static_cast<uint16_t>(pag::TagCode::MarkerList)) {
    AEMarker::ExportMarkers(context, layerHandle, layer->markers);
    AEMarker::ParseMarkers(layer);
  }
}

static pag::SolidLayer* CreateSolidLayer(pagexporter::Context* context, const AEGP_LayerH& layerHandle) {
  auto layer = new pag::SolidLayer();
  ExportBaseLayer(context, layerHandle, layer);
  auto& suites = context->suites;
  auto itemHandle = AEUtils::GetItemFromLayer(layerHandle);
  AEGP_ColorVal solidColor = {};
  RECORD_ERROR(suites.FootageSuite5()->AEGP_GetSolidFootageColor(itemHandle, FALSE, &solidColor));
  RECORD_ERROR(suites.ItemSuite6()->AEGP_GetItemDimensions(itemHandle, &layer->width, &layer->height));
  layer->solidColor = ExportColor(solidColor);
  return layer;
}

static pag::TextLayer* CreateTextLayer(pagexporter::Context* context, const AEGP_LayerH& layerHandle) {
  auto layer = new pag::TextLayer();
  ExportTextProperties(context, layerHandle, layer);
  ExportBaseLayer(context, layerHandle, layer);
  return layer;
}

static pag::ShapeLayer* CreateShapeLayer(pagexporter::Context* context, const AEGP_LayerH& layerHandle) {
  auto layer = new pag::ShapeLayer();
  ExportBaseLayer(context, layerHandle, layer);
  layer->contents = ExportShapes(context, layerHandle);
  return layer;
}

static pag::ImageLayer* CreateImageLayer(pagexporter::Context* context, const AEGP_LayerH& layerHandle) {
  auto layer = new pag::ImageLayer();
  ExportBaseLayer(context, layerHandle, layer);
  layer->imageBytes = ExportImageBytes(context, layerHandle);
  return layer;
}

/**
 * 为视频生成黑色占位图
 */
static pag::ImageLayer* CreateVideoPlaceHolderLayer(pagexporter::Context* context, const AEGP_LayerH& layerHandle) {
  auto layer = new pag::ImageLayer();
  ExportBaseLayer(context, layerHandle, layer);
  layer->imageBytes = ExportImageBytes(context, layerHandle, true);

  // 自动打上占位图标签
  auto marker = new pag::Marker();
  marker->comment = "{\"videoTrack\":1}";
  marker->startTime = layer->startTime;
  marker->duration = layer->duration;
  layer->markers.emplace_back(marker);

  return layer;
}

static pag::PreComposeLayer* CreatePreComposeLayer(pagexporter::Context* context, const AEGP_LayerH& layerHandle) {
  auto layer = new pag::PreComposeLayer();
  ExportBaseLayer(context, layerHandle, layer);
  auto& suites = context->suites;
  auto itemHandle = AEUtils::GetItemFromLayer(layerHandle);
  layer->composition = ExportCompositionID(itemHandle, suites);
  A_Time layerOffset = {};
  RECORD_ERROR(suites.LayerSuite6()->AEGP_GetLayerOffset(layerHandle, &layerOffset));
  layer->compositionStartTime = ExportTime(layerOffset, context);

  for (auto& composition : context->compositions) {
    if (composition->id == layer->composition->id) {
      return layer;
    }
  }

  ExportComposition(context, itemHandle);
  return layer;
}

static pag::CameraLayer* CreateCameraLayer(pagexporter::Context* context, const AEGP_LayerH& layerHandle) {
  auto layer = new pag::CameraLayer();
  ExportBaseLayer(context, layerHandle, layer);
  layer->cameraOption = ExportCameraOption(context, layerHandle);
  return layer;
}

static pag::Layer* ExportLayer(pagexporter::Context* context, AEGP_LayerH layerHandle, ExportLayerType& layerType) {
  layerType = GetLayerType(context, layerHandle);
  pag::Layer* layer = nullptr;
  switch (layerType) {
    case ExportLayerType::Solid:
      layer = CreateSolidLayer(context, layerHandle);
      break;
    case ExportLayerType::Text:
      layer = CreateTextLayer(context, layerHandle);
      break;
    case ExportLayerType::Shape:
      layer = CreateShapeLayer(context, layerHandle);
      break;
    case ExportLayerType::Image:
      layer = CreateImageLayer(context, layerHandle);
      break;
    case ExportLayerType::Video:
      layer = CreateVideoPlaceHolderLayer(context, layerHandle);
      break;
    case ExportLayerType::PreCompose:
      layer = CreatePreComposeLayer(context, layerHandle);
      break;
    case ExportLayerType::Camera:
      layer = CreateCameraLayer(context, layerHandle);
      break;
    case ExportLayerType::Null:
      layer = new pag::NullLayer();
      ExportBaseLayer(context, layerHandle, layer);
      break;
    default:
      layer = new pag::Layer();
      break;
  }
  return layer;
}

// 检查该Layer是否被别的图层的效果（比如DisplacementMap）所引用
static bool IsLayerBeReferencedByEffect(const std::vector<pag::Layer*>& layers, pag::ID id) {
  for (auto layer : layers) {
    for (auto effect : layer->effects) {
      auto displacementMap = static_cast<pag::DisplacementMapEffect*>(effect);
      if (effect->type() == pag::EffectType::DisplacementMap && displacementMap->displacementMapLayer) {
        if (id == displacementMap->displacementMapLayer->id) {
          return true;
        }
      }
    }
  }
  return false;
}

static bool IsLayerBeReference(pag::Layer* layer,
                               const std::vector<pag::Layer*>& layers,
                               const std::unordered_set<pag::ID>& parentLayerIDs,
                               bool lastLayerHasTrackMatte) {
  if (IsLayerBeReferencedByEffect(layers, layer->id)) {
    return true;
  }
  if (parentLayerIDs.count(layer->id) > 0) {
    return true;
  }
  if (lastLayerHasTrackMatte) {
    return true;
  }
  return false;
}

// 检查图层中trackMatte的有效性
static void CheckLayerType(pagexporter::Context* context, AEGP_CompH compHandle, int layerIndex) {
  auto& suites = context->suites;
  AEGP_LayerH layerH;
  suites.LayerSuite6()->AEGP_GetCompLayerByIndex(compHandle, layerIndex, &layerH);
  AEGP_LayerFlags layerFlags;
  suites.LayerSuite6()->AEGP_GetLayerFlags(layerH, &layerFlags);
  if (layerFlags & AEGP_LayerFlag_ADJUSTMENT_LAYER) {
    context->pushWarning(pagexporter::AlertInfoType::AdjustmentLayer);
  }
}

static void ModifyCameraLayerTransform(pag::Layer* layer, AEGP_LayerFlags layerFlags) {
  if (layer->type() != pag::LayerType::Camera) {
    return;
  }
  auto transform3D = layer->transform3D;
  if (transform3D == nullptr) {
    return;
  }

  if (transform3D->scale->animatable()) {
    delete transform3D->scale;
    transform3D->scale = new pag::Property<pag::Point3D>();
  }
  transform3D->scale->value = pag::Point3D::Make(1, 1, 1);

  if (transform3D->opacity->animatable()) {
    delete transform3D->opacity;
    transform3D->opacity = new pag::Property<pag::Opacity>();
  }
  transform3D->opacity->value = pag::Opaque;

  if (!(layerFlags & AEGP_LayerFlag_LOOK_AT_POI)) {
    delete transform3D->anchorPoint;

    auto position = transform3D->position;
    if (!position->animatable()) {
      transform3D->anchorPoint = new pag::Property<pag::Point3D>();
      transform3D->anchorPoint->value = position->value;
    } else {
      std::vector<pag::Keyframe<pag::Point3D>*> anchorPointKeyframes;
      for (auto& srcKeyFrame: static_cast<pag::AnimatableProperty<pag::Point3D>*>(position)->keyframes) {
        auto dstKeyframe = new pag::Keyframe<pag::Point3D>();
        dstKeyframe->startValue = srcKeyFrame->startValue;
        dstKeyframe->endValue = srcKeyFrame->endValue;
        dstKeyframe->startTime = srcKeyFrame->startTime;
        dstKeyframe->endTime = srcKeyFrame->endTime;
        dstKeyframe->bezierOut = srcKeyFrame->bezierOut;
        dstKeyframe->bezierIn = srcKeyFrame->bezierIn;
        dstKeyframe->spatialOut = srcKeyFrame->spatialOut;
        dstKeyframe->spatialIn = srcKeyFrame->spatialIn;
        anchorPointKeyframes.push_back(dstKeyframe);
      }
      transform3D->anchorPoint = new pag::AnimatableProperty<pag::Point3D>(anchorPointKeyframes);
    }
  }
}

std::vector<pag::Layer*> ExportLayers(pagexporter::Context* context, AEGP_CompH compHandle) {
  std::vector<pag::Layer*> layers;
  auto& suites = context->suites;
  std::unordered_set<pag::ID> parentLayerIDs;
  std::vector<bool> soloFlags;
  bool hasSoloLayer = false;
  A_long numLayers = 0;
  RECORD_ERROR(suites.LayerSuite6()->AEGP_GetCompNumLayers(compHandle, &numLayers));
  pagToAELayer.clear();
  aeToPagLayer.clear();
  ifCopyMatlayer=false;
  for (int i = 0; i < numLayers && !context->bEarlyExit; i++) {
    AEGP_LayerH layerHandle;
    RECORD_ERROR(suites.LayerSuite6()->AEGP_GetCompLayerByIndex(compHandle, i, &layerHandle));
    auto id = AEUtils::GetLayerId(layerHandle);
    context->layerHList.insert(std::make_pair(id, layerHandle));
    AssignRecover<pag::ID> arLI(context->curLayerId, id);
    context->layerIndex = i;
    ExportLayerType layerType = ExportLayerType::Unknown;
    auto layer = ExportLayer(context, layerHandle, layerType);
    layers.push_back(layer);
    aeToPagLayer[layerHandle]=layer;
    if (layer->parent) {
      parentLayerIDs.insert(layer->parent->id);
    }
    AEGP_LayerFlags layerFlags;
    RECORD_ERROR(suites.LayerSuite6()->AEGP_GetLayerFlags(layerHandle, &layerFlags));
    if (layerType == ExportLayerType::Audio || layerType == ExportLayerType::Unknown) {
      //layer->isActive = (layerFlags & AEGP_LayerFlag_AUDIO_ACTIVE) > 0;
      layer->isActive = false;
    } else {
      layer->isActive = (layerFlags & AEGP_LayerFlag_VIDEO_ACTIVE) > 0;
    }

    ModifyCameraLayerTransform(layer, layerFlags);

    auto soloFlag = (layer->isActive &&
                     (layerFlags & AEGP_LayerFlag_SOLO) > 0 &&
                     layer->type() != pag::LayerType::Camera);
    soloFlags.push_back(soloFlag);
    if (soloFlag) {
      hasSoloLayer = true;
    }
  }
  // 重新排序layers，使其符合遮罩层在前，被遮罩层在后的顺序
  {
    std::vector<pag::Layer*> newLayers;
    ifCopyMatlayer=true;
    for(auto layer:layers){
      if(layer->isActive){
        if(LayerHasTrackMatte(layer->trackMatteType)){
          if(pagToAELayer.count(layer)){
            if(aeToPagLayer.count(pagToAELayer[layer])){
              auto matLayer=aeToPagLayer[pagToAELayer[layer]];
              if(matLayer->isActive){
                // 若遮罩层本身是可见的，则再导出一份遮罩层并将其设置为不可见，将其排在被遮罩层前面
                // 可见的遮罩层与被遮罩层的顺序不变
                ExportLayerType layerType = ExportLayerType::Unknown;
                pag::Layer* copyLayer = ExportLayer(context, pagToAELayer[layer], layerType);
                copyLayer->isActive=false;
                copyLayer->trackMatteType=pag::TrackMatteType::None;
                AEGP_LayerFlags layerFlags;
                RECORD_ERROR(suites.LayerSuite6()->AEGP_GetLayerFlags(pagToAELayer[layer], &layerFlags));
                ModifyCameraLayerTransform(copyLayer, layerFlags);
                newLayers.emplace_back(copyLayer);
              }else{
                // 若遮罩层本身是不可见的，将遮罩层排在被遮罩层前面即可
                newLayers.emplace_back(matLayer);
              }
            }
          }
        }
        newLayers.emplace_back(layer);
      }
    }
    layers=newLayers;
  }
  if (context->bEarlyExit) {
    return layers;
  }

  pag::Codec::InstallReferences(layers);

  auto itemHandle = AEUtils::GetItemFromComp(compHandle);
  A_Time duration = {};
  RECORD_ERROR(suites.ItemSuite6()->AEGP_GetItemDuration(itemHandle, &duration));
  auto totalDuration = ExportTime(duration, context);

  bool lastLayerHasTrackMatte = false;
  for (int i = numLayers - 1; i >= 0; i--) {
    auto layer = layers[i];
    if (layer->startTime >= totalDuration) {
      layer->isActive = false;
    }
    if (hasSoloLayer && !soloFlags[i]) {
      layer->isActive = false;
    }

    bool bErrorType = (layer->type() == pag::LayerType::Unknown ||
                       layer->type() == pag::LayerType::Null);
    bool bReference = IsLayerBeReference(layer, layers, parentLayerIDs, lastLayerHasTrackMatte);
    if (bErrorType && (layer->isActive || bReference)) {
      // 为什么选择在这个时候检测图层类型？
      // 因为在此之前（导出每个图层时）不知道别的图层是否有solo标志使得本图层是不需导出的；
      // 而在此之后因为不支持的图层类型已经被删除，所以无法参与检测。
      // 所以在这个时候比较合适。
      CheckLayerType(context, compHandle, i);
      layer->isActive = false;
    }
    if (layer->duration == 0) {
      layer->duration = 1;
      layer->isActive = false;
    }
    auto opacity2D = layer->transform == nullptr ? nullptr : layer->transform->opacity;
    auto opacity3D = layer->transform3D == nullptr ? nullptr : layer->transform3D->opacity;
    if ((opacity2D == nullptr || (!opacity2D->animatable() && opacity2D->value == 0)) &&
        (opacity3D == nullptr || (!opacity3D->animatable() && opacity3D->value == 0))) {
      layer->isActive = false;
    }
    if (!layer->isActive && !bReference) {
      layers.erase(layers.begin() + i);
      delete layer;
    } else {
      lastLayerHasTrackMatte = layer->trackMatteType != pag::TrackMatteType::None;
    }
  }
  return layers;
}
