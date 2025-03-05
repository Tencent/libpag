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
#include "Transform3D.h"
#include "src/exports/Stream/StreamProperty.h"

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
  pag::Point3D offset;
  offset.x = OrientationOffset(start.x, end.x);
  offset.y = OrientationOffset(start.y, end.y);
  offset.z = OrientationOffset(start.z, end.z);
  return offset;
}

static void clipTo360(float& a) {
  while (a < 0.0f) {
    a += 360.0f;
  }
  while (a >= 360.0f) {
    a -= 360.0f;
  }
}

static void clipTo360(pag::Point3D& p) {
  clipTo360(p.x);
  clipTo360(p.y);
  clipTo360(p.z);
}

static void ModifyTransform3DOrientation(pag::Property<pag::Point3D>* orientation) {
  if (!orientation->animatable()) {
    return;
  }

  for (auto& keyFrame : static_cast<pag::AnimatableProperty<pag::Point3D>*>(orientation)->keyframes) {
    clipTo360(keyFrame->startValue);
    clipTo360(keyFrame->endValue);
  }

  auto totalOffset = pag::Point3D::Zero();
  for (auto& keyFrame : static_cast<pag::AnimatableProperty<pag::Point3D>*>(orientation)->keyframes) {
    auto offset = OrientationOffset(keyFrame->startValue, keyFrame->endValue);
    keyFrame->startValue = keyFrame->startValue + totalOffset;
    totalOffset = totalOffset + offset;
    keyFrame->endValue = keyFrame->endValue + totalOffset;
  }

  for (auto& keyFrame : static_cast<pag::AnimatableProperty<pag::Point3D>*>(orientation)->keyframes) {
    auto distance = keyFrame->endValue - keyFrame->startValue;
    keyFrame->spatialIn.x = distance.x / 4;
    keyFrame->spatialIn.y = distance.y / 4;
    keyFrame->spatialIn.z = distance.z / 4;
    keyFrame->spatialOut.x = -keyFrame->spatialIn.x;
    keyFrame->spatialOut.y = -keyFrame->spatialIn.y;
    keyFrame->spatialOut.z = -keyFrame->spatialIn.z;
  }
}

static float OrientationDiff(float start, float end) {
  auto a = start;
  auto b = end;
  clipTo360(a);
  clipTo360(b);
  auto diff = abs(a - b);
  if (diff <= 180.0f) {
    return diff;
  } else {
    return 360.0f - diff;
  }
}

static pag::Frame FrameStep(pag::Keyframe<pag::Point3D>* keyframe) {
  auto diffX = OrientationDiff(keyframe->endValue.x, keyframe->startValue.x);
  auto diffY = OrientationDiff(keyframe->endValue.y, keyframe->startValue.y);
  auto diffZ = OrientationDiff(keyframe->endValue.z, keyframe->startValue.z);
  auto diff = diffX > diffY ? diffX : diffY;
  diff = diff > diffZ ? diff : diffZ;
  int num = static_cast<int>(diff / 2.0f);  // 2.0f = 2度, 表示每一步最少2度（可调节为其它度数，精度不同）
  if (num < 1) {
    num = 1;
  }
  pag::Frame step = (keyframe->endTime - keyframe->startTime) / num;
  if (step < 1) {
    step = 1;
  }
  return step;
}

static pag::Property<pag::Point3D>* ExportOrientationKeyframe(pagexporter::Context* context, const AEGP_LayerH& layerHandle) {
  auto orientation = ExportProperty(context, layerHandle, AEGP_LayerStream_ORIENTATION, Parsers::Point3D);
  if (!orientation->animatable()) {
    return orientation;
  }

  auto& suites = context->suites;
  AEGP_StreamRefH stream;
  suites.StreamSuite4()->AEGP_GetNewLayerStream(context->pluginID, layerHandle, AEGP_LayerStream_ORIENTATION, &stream);
  std::vector<AEGP_KeyframeIndex> list;
  for (auto keyframe : static_cast<pag::AnimatableProperty<pag::Point3D>*>(orientation)->keyframes) {
    auto step = FrameStep(keyframe);
    for (auto frame = keyframe->startTime + step; frame < keyframe->endTime; frame += step) {
      AEGP_KeyframeIndex index = 0;
      A_Time time = {static_cast<A_long>(frame*10), static_cast<A_u_long>(context->frameRate*10)};
      suites.KeyframeSuite3()->AEGP_InsertKeyframe(stream, AEGP_LTimeMode_CompTime, &time, &index);
      if (index > 0) {
        list.push_back(index);
      }
    }
  }

  delete orientation;
  auto newOrientation = ExportProperty(context, stream, Parsers::Point3D);

  for (int i = static_cast<int>(list.size()) - 1; i >= 0; i--) {
    suites.KeyframeSuite3()->AEGP_DeleteKeyframe(stream, list.at(i));
  }
  context->suites.StreamSuite4()->AEGP_DisposeStream(stream);

  ModifyTransform3DOrientation(newOrientation);
  return newOrientation;
}

pag::Transform3D* ExportTransform3D(pagexporter::Context* context, const AEGP_LayerH& layerHandle) {
  auto transform = new pag::Transform3D();
  auto& suites = context->suites;
  AEGP_StreamRefH position;
  RECORD_ERROR(suites.StreamSuite4()->AEGP_GetNewLayerStream(context->pluginID, layerHandle, AEGP_LayerStream_POSITION,
                                                             &position));
  A_Boolean dimensionSeparated = 0;
  RECORD_ERROR(suites.DynamicStreamSuite4()->AEGP_AreDimensionsSeparated(position, &dimensionSeparated));
  if (dimensionSeparated != 0) {
    AEGP_StreamRefH xPosition;
    RECORD_ERROR(suites.DynamicStreamSuite4()->AEGP_GetSeparationFollower(position, 0, &xPosition));
    transform->xPosition = ExportProperty(context, xPosition, Parsers::Float);
    RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(xPosition));

    AEGP_StreamRefH yPosition;
    RECORD_ERROR(suites.DynamicStreamSuite4()->AEGP_GetSeparationFollower(position, 1, &yPosition));
    transform->yPosition = ExportProperty(context, yPosition, Parsers::Float);
    RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(yPosition));

    AEGP_StreamRefH zPosition;
    RECORD_ERROR(suites.DynamicStreamSuite4()->AEGP_GetSeparationFollower(position, 1, &zPosition));
    transform->zPosition = ExportProperty(context, zPosition, Parsers::Float);
    RECORD_ERROR(suites.StreamSuite4()->AEGP_DisposeStream(zPosition));
  } else {
    transform->position = ExportProperty(context, position, Parsers::Point3D);
  }
  suites.StreamSuite4()->AEGP_DisposeStream(position);
  transform->anchorPoint = ExportProperty(context, layerHandle, AEGP_LayerStream_ANCHORPOINT, Parsers::Point3D);
  transform->scale = ExportProperty(context, layerHandle, AEGP_LayerStream_SCALE, Parsers::Scale3D, 3);
  transform->orientation = ExportOrientationKeyframe(context, layerHandle);
  transform->xRotation = ExportProperty(context, layerHandle, AEGP_LayerStream_ROTATE_X, Parsers::Float);
  transform->yRotation = ExportProperty(context, layerHandle, AEGP_LayerStream_ROTATE_Y, Parsers::Float);
  transform->zRotation = ExportProperty(context, layerHandle, AEGP_LayerStream_ROTATE_Z, Parsers::Float);
  transform->opacity = ExportProperty(context, layerHandle, AEGP_LayerStream_OPACITY, Parsers::Opacity0_100);
  return transform;
}
