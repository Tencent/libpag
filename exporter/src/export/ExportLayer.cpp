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

#include "ExportLayer.h"
#include <base/utils/Log.h>
#include "ExportComposition.h"
#include "Marker.h"
#include "data/CameraOption.h"
#include "data/Effect.h"
#include "data/LayerStyle.h"
#include "data/Mask.h"
#include "data/Shape.h"
#include "data/TextProperty.h"
#include "data/Transform2D.h"
#include "data/Transform3D.h"
#include "stream/StreamProperty.h"
#include "utils/AEDataTypeConverter.h"
#include "utils/ScopedHelper.h"

namespace exporter {

static bool IsLayerReferencedByEffect(pag::ID id, const std::vector<pag::Layer*>& layers) {
  for (auto& layer : layers) {
    for (auto effect : layer->effects) {
      if (effect->type() == pag::EffectType::DisplacementMap) {
        auto displacementMap = static_cast<pag::DisplacementMapEffect*>(effect);
        if (displacementMap->displacementMapLayer &&
            id == displacementMap->displacementMapLayer->id) {
          return true;
        }
      }
    }
  }
  return false;
}

static bool IsLayerReferenced(pag::ID id, const std::vector<pag::Layer*>& layers,
                              bool lastLayerHasTrackMatte) {
  if (IsLayerReferencedByEffect(id, layers)) {
    return true;
  }

  auto iter = std::find_if(layers.begin(), layers.end(), [id](pag::Layer* layer) -> bool {
    return layer->parent != nullptr && id == layer->parent->id;
  });
  if (iter != layers.end()) {
    return true;
  }

  return lastLayerHasTrackMatte;
}

static pag::Layer* ExportLayer(const AEGP_LayerH& layerHandle,
                               std::shared_ptr<PAGExportSession> session);

static void ModifyTransform3DForCameraLayer(pag::Layer* layer, AEGP_LayerFlags layerFlags) {
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
      for (auto& srcKeyFrame :
           static_cast<pag::AnimatableProperty<pag::Point3D>*>(position)->keyframes) {
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

static void InitLayer(std::shared_ptr<PAGExportSession> session, const AEGP_LayerH& layerHandle,
                      pag::Layer* layer, ExportLayerType layerType) {
  layer->id = GetLayerID(layerHandle);
  layer->name = GetLayerName(layerHandle);
  AEGP_LayerH parentLayerHandle = GetLayerParentLayerH(layerHandle);
  if (parentLayerHandle != nullptr) {
    layer->parent = new pag::Layer();
    layer->parent->id = GetLayerID(parentLayerHandle);
  }
  layer->stretch = GetLayerStretch(layerHandle);
  layer->startTime = GetLayerStartTime(layerHandle, session->frameRate);
  layer->duration = GetLayerDuration(layerHandle, session->frameRate);
  AEGP_LayerFlags layerFlags = GetLayerFlags(layerHandle);
  layer->autoOrientation = layerFlags & AEGP_LayerFlag_AUTO_ORIENT_ROTATION;

  if ((layerFlags & AEGP_LayerFlag_LAYER_IS_3D) &&
      session->configParam.isTagCodeSupport(pag::TagCode::Transform3D)) {
    layer->transform3D = GetTransform3D(layerHandle, session->frameRate);
  } else {
    layer->transform = GetTransform2D(layerHandle, session->frameRate);
  }
  if (layerFlags & AEGP_LayerFlag_TIME_REMAPPING) {
    layer->timeRemap =
        GetProperty(layerHandle, AEGP_LayerStream_TIME_REMAP, AEStreamParser::FloatParser);
  }
  if (layerType == ExportLayerType::Camera) {
    ModifyTransform3DForCameraLayer(layer, layerFlags);
  } else {
    layer->blendMode = GetLayerBlendMode(layerHandle);
    layer->trackMatteType = GetLayerTrackMatteType(layerHandle);
    if (session->configParam.isTagCodeSupport(pag::TagCode::LayerAttributesExtra)) {
      layer->motionBlur = static_cast<bool>(layerFlags & AEGP_LayerFlag_MOTION_BLUR);
    }
    if (layer->trackMatteType != pag::TrackMatteType::None) {
      AEGP_LayerH trackMatteLayerHandle = GetLayerTrackMatteLayerH(layerHandle);
      if (trackMatteLayerHandle == nullptr) {
        layer->trackMatteType = pag::TrackMatteType::None;
      } else {
        int trackMatteIndex = -1;
        AEGP_CompH compHandle = nullptr;
        if (GetSuites()->LayerSuite6()->AEGP_GetLayerParentComp(trackMatteLayerHandle,
                                                                &compHandle) == A_Err_NONE) {
          A_long numLayers = 0;
          if (GetSuites()->LayerSuite6()->AEGP_GetCompNumLayers(compHandle, &numLayers) ==
              A_Err_NONE) {
            for (int i = 0; i < numLayers; i++) {
              AEGP_LayerH tempLayerHandle = nullptr;
              if (GetSuites()->LayerSuite6()->AEGP_GetCompLayerByIndex(
                      compHandle, i, &tempLayerHandle) == A_Err_NONE) {
                if (tempLayerHandle == trackMatteLayerHandle) {
                  trackMatteIndex = i;
                  break;
                }
              }
            }
          }
        }

        DEBUG_ASSERT(trackMatteIndex >= 0)
        ScopedAssign<int> tempLayerIndex(session->layerIndex, trackMatteIndex);
        layer->trackMatteLayer = ExportLayer(trackMatteLayerHandle, session);
        layer->trackMatteLayer->isActive = false;
        layer->trackMatteLayer->trackMatteType = pag::TrackMatteType::None;
      }
    }
    layer->masks = GetMasks(layerHandle);
    layer->effects = GetEffects(layerHandle);
    layer->layerStyles = GetLayerStyles(layerHandle);
    GetAttachments(layerHandle, session->frameRate, layer, session->configParam.exportTagLevel);
  }

  if (layerType == ExportLayerType::Video) {
    auto marker = new pag::Marker();
    marker->comment = "{\"videoTrack\":1}";
    marker->startTime = layer->startTime;
    marker->duration = layer->duration;
    layer->markers.emplace_back(marker);
  }

  if (layerType == ExportLayerType::Audio || layerType == ExportLayerType::Unknown) {
    layer->isActive = false;
  } else {
    layer->isActive = layerFlags & AEGP_LayerFlag_VIDEO_ACTIVE;
  }

  if (session->configParam.isTagCodeSupport(pag::TagCode::MarkerList)) {
    auto markers = ExportMarkers(session, layerHandle);
    layer->markers.insert(layer->markers.end(), markers.begin(), markers.end());
    ParseMarkers(layer);
  }
}

static pag::SolidLayer* CreateSolidLayer(const AEGP_LayerH& layerHandle) {
  const auto& Suites = GetSuites();
  auto layer = new pag::SolidLayer();

  AEGP_ItemH itemHandle = GetLayerItemH(layerHandle);
  AEGP_ColorVal solidColor = {};
  Suites->FootageSuite5()->AEGP_GetSolidFootageColor(itemHandle, FALSE, &solidColor);
  Suites->ItemSuite6()->AEGP_GetItemDimensions(itemHandle, &layer->width, &layer->height);
  layer->solidColor = AEColorToColor(solidColor);
  return layer;
}

static pag::ImageLayer* CreateImageOrVideoLayer(const AEGP_LayerH& layerHandle,
                                                std::shared_ptr<PAGExportSession> session,
                                                bool isVideoLayer) {
  auto layer = new pag::ImageLayer();
  AEGP_ItemH itemHandle = GetLayerItemH(layerHandle);
  pag::ID imageID = GetItemID(itemHandle);
  auto res = std::find_if(
      session->imageBytesList.begin(), session->imageBytesList.end(),
      [imageID](const pag::ImageBytes* image) -> bool { return image->id == imageID; });
  if (res == session->imageBytesList.end()) {
    auto imageBytes = new pag::ImageBytes();
    imageBytes->id = imageID;
    layer->imageBytes = imageBytes;
    session->imageBytesList.emplace_back(imageBytes);
    session->imageLayerHandleList.emplace_back(isVideoLayer, layerHandle);
  } else {
    layer->imageBytes = *res;
  }

  return layer;
}

static pag::PreComposeLayer* CreatePreComposeLayer(const AEGP_LayerH& layerHandle,
                                                   std::shared_ptr<PAGExportSession> session) {
  const auto& Suites = GetSuites();
  auto layer = new pag::PreComposeLayer();

  AEGP_ItemH itemHandle = GetLayerItemH(layerHandle);
  layer->composition = new pag::Composition();
  layer->composition->id = GetItemID(itemHandle);

  A_Time layerOffset = {};
  Suites->LayerSuite6()->AEGP_GetLayerOffset(layerHandle, &layerOffset);
  layer->compositionStartTime = AEDurationToFrame(layerOffset, session->frameRate);

  for (const auto& composition : session->compositions) {
    if (composition->id == layer->composition->id) {
      return layer;
    }
  }

  ExportComposition(session, itemHandle);

  return layer;
}

static pag::TextLayer* CreateTextLayer(const AEGP_LayerH& layerHandle,
                                       std::shared_ptr<PAGExportSession> session) {
  auto layer = new pag::TextLayer();
  GetTextProperties(session, layerHandle, layer);
  return layer;
}

static pag::CameraLayer* CreateCameraLayer(const AEGP_LayerH& layerHandle) {
  auto layer = new pag::CameraLayer();
  layer->cameraOption = GetCameraOption(layerHandle);
  return layer;
}

static pag::ShapeLayer* CreateShapeLayer(const AEGP_LayerH& layerHandle) {
  auto layer = new pag::ShapeLayer();
  layer->contents = GetShapes(layerHandle);
  return layer;
}

static pag::Layer* ExportLayer(const AEGP_LayerH& layerHandle,
                               std::shared_ptr<PAGExportSession> session) {
  ExportLayerType layerType = GetLayerType(layerHandle);
  pag::Layer* layer = nullptr;
  ScopedAssign<pag::ID> layerID(session->layerID, GetLayerID(layerHandle));
  switch (layerType) {
    case ExportLayerType::Solid:
      layer = CreateSolidLayer(layerHandle);
      break;
    case ExportLayerType::Image:
      layer = CreateImageOrVideoLayer(layerHandle, session, false);
      break;
    case ExportLayerType::Video:
      layer = CreateImageOrVideoLayer(layerHandle, session, true);
      break;
    case ExportLayerType::PreCompose:
      layer = CreatePreComposeLayer(layerHandle, session);
      break;
    case ExportLayerType::Text:
      layer = CreateTextLayer(layerHandle, session);
      break;
    case ExportLayerType::Camera:
      layer = CreateCameraLayer(layerHandle);
      break;
    case ExportLayerType::Shape:
      layer = CreateShapeLayer(layerHandle);
      break;
    case ExportLayerType::Null:
      layer = new pag::NullLayer();
      break;
    default:
      layer = new pag::Layer();
      break;
  }
  InitLayer(session, layerHandle, layer, layerType);

  return layer;
}

ExportLayerType GetLayerType(const AEGP_LayerH& layerHandle) {
  AEGP_LayerFlags layerFlags = GetLayerFlags(layerHandle);
  if (layerFlags &
      (AEGP_LayerFlag_NULL_LAYER | AEGP_LayerFlag_GUIDE_LAYER | AEGP_LayerFlag_ADJUSTMENT_LAYER)) {
    return ExportLayerType::Null;
  }
  AEGP_ObjectType objectType;
  GetSuites()->LayerSuite6()->AEGP_GetLayerObjectType(layerHandle, &objectType);
  if (objectType == AEGP_ObjectType_VECTOR) {
    return ExportLayerType::Shape;
  }
  if (objectType == AEGP_ObjectType_TEXT) {
    return ExportLayerType::Text;
  }
  if (objectType == AEGP_ObjectType_CAMERA) {
    return ExportLayerType::Camera;
  }
  if (objectType == AEGP_ObjectType_AV) {
    AEGP_ItemH itemHandle;
    GetSuites()->LayerSuite6()->AEGP_GetLayerSourceItem(layerHandle, &itemHandle);
    AEGP_ItemType itemType;
    GetSuites()->ItemSuite6()->AEGP_GetItemType(itemHandle, &itemType);
    if (itemType == AEGP_ItemType_COMP) {
      return ExportLayerType::PreCompose;
    }
    if (itemType == AEGP_ItemType_FOOTAGE) {
      AEGP_ItemFlags itemFlags;
      GetSuites()->ItemSuite6()->AEGP_GetItemFlags(itemHandle, &itemFlags);
      if (itemFlags & AEGP_ItemFlag_STILL) {
        AEGP_FootageH footageHandle = nullptr;
        GetSuites()->FootageSuite5()->AEGP_GetMainFootageFromItem(itemHandle, &footageHandle);
        AEGP_FootageSignature signature;
        GetSuites()->FootageSuite5()->AEGP_GetFootageSignature(footageHandle, &signature);
        if (signature == AEGP_FootageSignature_SOLID) {
          return ExportLayerType::Solid;
        }
        if (signature != AEGP_FootageSignature_MISSING && signature != AEGP_FootageSignature_NONE) {
          return ExportLayerType::Image;
        }
      }
      if (itemFlags & AEGP_ItemFlag_HAS_VIDEO) {
        return ExportLayerType::Video;
      }
      if (itemFlags & AEGP_ItemFlag_HAS_AUDIO && !(itemFlags & AEGP_ItemFlag_HAS_VIDEO)) {
        return ExportLayerType::Audio;
      }
      if (itemFlags & AEGP_ItemFlag_MISSING) {
        return ExportLayerType::Unknown;
      }
    }
    return ExportLayerType::Null;
  }
  return ExportLayerType::Unknown;
}

std::vector<pag::Layer*> ExportLayers(std::shared_ptr<PAGExportSession> session,
                                      const AEGP_CompH& compHandle) {
  bool hasSoloLayer = false;
  std::vector<bool> soloFlags = {};
  std::vector<pag::Layer*> layers = {};

  A_long numLayers = 0;
  if (GetSuites()->LayerSuite6()->AEGP_GetCompNumLayers(compHandle, &numLayers) != A_Err_NONE) {
    session->pushWarning(AlertInfoType::ExportAEError);
    return layers;
  }

  for (int index = 0; index < numLayers; index++) {
    AEGP_LayerH layerHandle = nullptr;
    if (GetSuites()->LayerSuite6()->AEGP_GetCompLayerByIndex(compHandle, index, &layerHandle) !=
        A_Err_NONE) {
      session->pushWarning(AlertInfoType::ExportAEError);
      continue;
    }

    if (layerHandle == nullptr) {
      continue;
    }

    uint32_t layerID = GetLayerID(layerHandle);
    session->layerHandleMap[layerID] = layerHandle;
    ExportLayerType layerType = GetLayerType(layerHandle);
    if (layerType == ExportLayerType::Audio && session->audioMarkers != nullptr &&
        session->configParam.isTagCodeSupport(pag::TagCode::MarkerList)) {
      auto markers = ExportMarkers(session, layerHandle);
      session->audioMarkers->insert(session->audioMarkers->end(), markers.begin(), markers.end());
    }
    ScopedAssign<int> layerIndex(session->layerIndex, index);
    auto layer = ExportLayer(layerHandle, session);
    if (layer == nullptr) {
      continue;
    }
    if (layer->trackMatteLayer != nullptr) {
      soloFlags.push_back(false);
      layers.push_back(layer->trackMatteLayer);
    }
    layers.push_back(layer);

    AEGP_LayerFlags layerFlags = GetLayerFlags(layerHandle);
    if (layerType == ExportLayerType::Audio || layerType == ExportLayerType::Unknown) {
      layer->isActive = false;
    } else {
      layer->isActive = layerFlags & AEGP_LayerFlag_VIDEO_ACTIVE;
    }
    bool soloFlag = layer->isActive && (layerFlags & AEGP_LayerFlag_SOLO) &&
                    (layer->type() != pag::LayerType::Camera);
    if (soloFlag) {
      hasSoloLayer = true;
    }
    soloFlags.push_back(soloFlag);
  }
  if (session->stopExport) {
    return layers;
  }

  pag::Codec::InstallReferences(layers);
  AEGP_ItemH itemHandle = GetCompItemH(compHandle);
  if (itemHandle == nullptr) {
    return layers;
  }
  A_Time duration = {};
  GetSuites()->ItemSuite6()->AEGP_GetItemDuration(itemHandle, &duration);
  pag::Frame totalDuration = AEDurationToFrame(duration, session->frameRate);

  bool nextLayerHasTrackMatte = false;
  numLayers = static_cast<A_long>(layers.size());
  for (int index = numLayers - 1; index >= 0; index--) {
    pag::Layer* layer = layers[index];
    if (layer->startTime >= totalDuration) {
      layer->isActive = false;
    }
    if (hasSoloLayer && !soloFlags[index]) {
      layer->isActive = false;
    }

    bool isErrorType =
        layer->type() == pag::LayerType::Null || layer->type() == pag::LayerType::Unknown;
    bool isReferenced = IsLayerReferenced(layer->id, layers, nextLayerHasTrackMatte);
    if (isErrorType && (isReferenced || layer->isActive)) {
      ScopedAssign<pag::ID> layerID(session->layerID, layer->id);
      AEGP_LayerH layerHandle = session->layerHandleMap[layer->id];
      auto layerFlags = GetLayerFlags(layerHandle);
      if (layerFlags & AEGP_LayerFlag_ADJUSTMENT_LAYER) {
        session->pushWarning(AlertInfoType::AdjustmentLayer);
      }
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

    if (!layer->isActive && !isReferenced) {
      if (layer->trackMatteLayer != nullptr) {
        layer->trackMatteLayer->isActive = false;
      }
      session->layerHandleMap.erase(layer->id);
      layers.erase(layers.begin() + index);
      delete layer;
    } else {
      nextLayerHasTrackMatte = layer->trackMatteType != pag::TrackMatteType::None;
    }
  }

  return layers;
}

}  // namespace exporter