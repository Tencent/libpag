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

#include "Transform3D.h"
#include "export/stream/StreamProperty.h"

namespace exporter {

static float ClipTo360(float a) {
  a = std::fmod(a, 360.0f);
  if (a < 0.0f) {
    a += 360.0f;
  }
  return a;
}

static pag::Point3D ClipTo360(pag::Point3D a) {
  a.x = ClipTo360(a.x);
  a.y = ClipTo360(a.y);
  a.z = ClipTo360(a.z);
  return a;
}

static float OrientationOffset(float start, float end) {
  auto diff = end - start;
  float offset = 0.0f;
  if (diff > 180.0f) {
    offset = -360.0f;
  } else if (diff < -180.0f) {
    offset = 360;
  }
  return offset;
}

static pag::Point3D OrientationOffset(pag::Point3D& start, pag::Point3D& end) {
  pag::Point3D offset = {};
  offset.x = OrientationOffset(start.x, end.x);
  offset.y = OrientationOffset(start.y, end.y);
  offset.z = OrientationOffset(start.z, end.z);
  return offset;
}

static float OrientationDiff(float start, float end) {
  auto a = ClipTo360(start);
  auto b = ClipTo360(end);
  auto diff = abs(a - b);
  if (diff <= 180.0f) {
    return diff;
  }

  return 360.0f - diff;
}

static void ModifyTransform3DOrientation(pag::Property<pag::Point3D>* orientation) {
  if (!orientation->animatable()) {
    return;
  }

  for (auto& keyFrame :
       static_cast<pag::AnimatableProperty<pag::Point3D>*>(orientation)->keyframes) {
    keyFrame->startValue = ClipTo360(keyFrame->startValue);
    keyFrame->endValue = ClipTo360(keyFrame->endValue);
  }

  auto totalOffset = pag::Point3D::Zero();
  for (auto& keyFrame :
       static_cast<pag::AnimatableProperty<pag::Point3D>*>(orientation)->keyframes) {
    auto offset = OrientationOffset(keyFrame->startValue, keyFrame->endValue);
    keyFrame->startValue = keyFrame->startValue + totalOffset;
    totalOffset = totalOffset + offset;
    keyFrame->endValue = keyFrame->endValue + totalOffset;
  }

  for (auto& keyFrame :
       static_cast<pag::AnimatableProperty<pag::Point3D>*>(orientation)->keyframes) {
    auto distance = keyFrame->endValue - keyFrame->startValue;
    keyFrame->spatialIn.x = distance.x / 4;
    keyFrame->spatialIn.y = distance.y / 4;
    keyFrame->spatialIn.z = distance.z / 4;
    keyFrame->spatialOut.x = -keyFrame->spatialIn.x;
    keyFrame->spatialOut.y = -keyFrame->spatialIn.y;
    keyFrame->spatialOut.z = -keyFrame->spatialIn.z;
  }
}

static pag::Frame GetFrameStep(pag::Keyframe<pag::Point3D>* keyframe) {
  const float orientationStep = 2.0f;
  auto diffX = OrientationDiff(keyframe->endValue.x, keyframe->startValue.x);
  auto diffY = OrientationDiff(keyframe->endValue.y, keyframe->startValue.y);
  auto diffZ = OrientationDiff(keyframe->endValue.z, keyframe->startValue.z);
  float diff = std::max(diffX, std::max(diffY, diffZ));
  int numStep = static_cast<int>(diff / orientationStep);
  if (numStep < 1) {
    numStep = 1;
  }
  pag::Frame frameStep = (keyframe->endTime - keyframe->startTime) / numStep;
  if (frameStep < 1) {
    frameStep = 1;
  }
  return frameStep;
}

static pag::Property<pag::Point3D>* GetOrientationKeyframe(const AEGP_LayerH& layerHandle,
                                                           float frameRate) {
  auto orientation =
      GetProperty(layerHandle, AEGP_LayerStream_ORIENTATION, AEStreamParser::Point3DParser);
  if (!orientation->animatable()) {
    return orientation;
  }

  const A_long PluginID = GetPluginID();
  const auto Suites = GetSuites();

  AEGP_StreamRefH streamHandle = nullptr;
  Suites->StreamSuite4()->AEGP_GetNewLayerStream(PluginID, layerHandle,
                                                 AEGP_LayerStream_ORIENTATION, &streamHandle);
  std::vector<AEGP_KeyframeIndex> list = {};
  for (auto keyframe :
       static_cast<pag::AnimatableProperty<pag::Point3D>*>(orientation)->keyframes) {
    auto step = GetFrameStep(keyframe);
    for (auto frame = keyframe->startTime + step; frame < keyframe->endTime; frame += step) {
      AEGP_KeyframeIndex index = 0;
      A_Time time = {static_cast<A_long>(frame * 10), static_cast<A_u_long>(frameRate * 10)};
      Suites->KeyframeSuite3()->AEGP_InsertKeyframe(streamHandle, AEGP_LTimeMode_CompTime, &time,
                                                    &index);
      if (index > 0) {
        list.push_back(index);
      }
    }
  }

  delete orientation;
  auto newOrientation = GetProperty(streamHandle, AEStreamParser::Point3DParser);

  for (int index = static_cast<int>(list.size()) - 1; index >= 0; index--) {
    Suites->KeyframeSuite3()->AEGP_DeleteKeyframe(streamHandle, list.at(index));
  }
  Suites->StreamSuite4()->AEGP_DisposeStream(streamHandle);

  ModifyTransform3DOrientation(newOrientation);
  return newOrientation;
}

pag::Transform3D* GetTransform3D(const AEGP_LayerH& layerHandle, float frameRate) {
  const A_long PluginID = GetPluginID();
  const auto Suites = GetSuites();
  auto transform = new pag::Transform3D();

  AEGP_StreamRefH streamHandle = nullptr;
  Suites->StreamSuite4()->AEGP_GetNewLayerStream(PluginID, layerHandle, AEGP_LayerStream_POSITION,
                                                 &streamHandle);
  A_Boolean dimensionSeparated = 0;
  Suites->DynamicStreamSuite4()->AEGP_AreDimensionsSeparated(streamHandle, &dimensionSeparated);
  if (dimensionSeparated != 0) {
    AEGP_StreamRefH xPosition = nullptr;
    Suites->DynamicStreamSuite4()->AEGP_GetSeparationFollower(streamHandle, 0, &xPosition);
    transform->xPosition = GetProperty(xPosition, AEStreamParser::FloatParser);
    Suites->StreamSuite4()->AEGP_DisposeStream(xPosition);

    AEGP_StreamRefH yPosition = nullptr;
    Suites->DynamicStreamSuite4()->AEGP_GetSeparationFollower(streamHandle, 1, &yPosition);
    transform->yPosition = GetProperty(yPosition, AEStreamParser::FloatParser);
    Suites->StreamSuite4()->AEGP_DisposeStream(yPosition);

    AEGP_StreamRefH zPosition = nullptr;
    Suites->DynamicStreamSuite4()->AEGP_GetSeparationFollower(streamHandle, 1, &zPosition);
    transform->zPosition = GetProperty(zPosition, AEStreamParser::FloatParser);
    Suites->StreamSuite4()->AEGP_DisposeStream(zPosition);
  } else {
    transform->position = GetProperty(streamHandle, AEStreamParser::Point3DParser);
  }
  Suites->StreamSuite4()->AEGP_DisposeStream(streamHandle);
  transform->anchorPoint =
      GetProperty(layerHandle, AEGP_LayerStream_ANCHORPOINT, AEStreamParser::Point3DParser);
  transform->scale =
      GetProperty(layerHandle, AEGP_LayerStream_SCALE, AEStreamParser::Scale3DParser, {}, 3);
  transform->orientation = GetOrientationKeyframe(layerHandle, frameRate);
  transform->xRotation =
      GetProperty(layerHandle, AEGP_LayerStream_ROTATE_X, AEStreamParser::FloatParser);
  transform->yRotation =
      GetProperty(layerHandle, AEGP_LayerStream_ROTATE_Y, AEStreamParser::FloatParser);
  transform->zRotation =
      GetProperty(layerHandle, AEGP_LayerStream_ROTATE_Z, AEStreamParser::FloatParser);
  transform->opacity =
      GetProperty(layerHandle, AEGP_LayerStream_OPACITY, AEStreamParser::Opacity0_100Parser);
  return transform;
}

}  // namespace exporter
