/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "GLTriangulatingPathOp.h"
#include "core/PathRef.h"
#include "gpu/DefaultGeometryProcessor.h"

namespace tgfx {
// https://chromium-review.googlesource.com/c/chromium/src/+/1099564/
static constexpr int AA_TESSELLATOR_MAX_VERB_COUNT = 100;

// When tessellating curved paths into linear segments, this defines the maximum distance in
// screen space which a segment may deviate from the mathematically correct value. Above this
// value, the segment will be subdivided. This value was chosen to approximate the super sampling
// accuracy of the raster path (16 samples, or one quarter pixel).
static constexpr float DefaultTolerance = 0.25f;

std::unique_ptr<GLTriangulatingPathOp> GLTriangulatingPathOp::Make(const Path& path,
                                                                   const Rect& clipBounds) {
  const auto& skPath = PathRef::ReadAccess(path);
  if (skPath.countVerbs() > AA_TESSELLATOR_MAX_VERB_COUNT) {
    return nullptr;
  }
  std::vector<float> vertex;
  auto skRect =
      pk::SkRect::MakeLTRB(clipBounds.left, clipBounds.top, clipBounds.right, clipBounds.bottom);
  int count = skPath.toAATriangles(DefaultTolerance, skRect, &vertex);
  if (count == 0) {
    return nullptr;
  }
  return std::unique_ptr<GLTriangulatingPathOp>(new GLTriangulatingPathOp(vertex, count));
}

std::unique_ptr<GeometryProcessor> GLTriangulatingPathOp::getGeometryProcessor(
    const DrawArgs& args) {
  return DefaultGeometryProcessor::Make(args.renderTarget->width(), args.renderTarget->height());
}

std::vector<float> GLTriangulatingPathOp::vertices(const DrawArgs&) {
  return vertex;
}

void GLTriangulatingPathOp::draw(const DrawArgs& args) {
  GLDrawer::DrawArrays(args.context, GL_TRIANGLES, 0, vertexCount);
}
}  // namespace tgfx
