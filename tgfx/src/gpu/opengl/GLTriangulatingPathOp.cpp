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
#include "core/TriangularPathMesh.h"
#include "gpu/DefaultGeometryProcessor.h"
#include "tgfx/core/Mesh.h"

namespace tgfx {
// https://chromium-review.googlesource.com/c/chromium/src/+/1099564/
static constexpr int AA_TESSELLATOR_MAX_VERB_COUNT = 100;

std::unique_ptr<GLTriangulatingPathOp> GLTriangulatingPathOp::Make(const Path& path,
                                                                   Rect clipBounds) {
  const auto& skPath = PathRef::ReadAccess(path);
  if (skPath.countVerbs() > AA_TESSELLATOR_MAX_VERB_COUNT) {
    return nullptr;
  }
  std::vector<float> vertices;
  auto skRect =
      pk::SkRect::MakeLTRB(clipBounds.left, clipBounds.top, clipBounds.right, clipBounds.bottom);
  int count = skPath.toAATriangles(DefaultTolerance, skRect, &vertices);
  if (count == 0) {
    return nullptr;
  }
  return std::make_unique<GLTriangulatingPathOp>(std::move(vertices), count, path.getBounds());
}

GLTriangulatingPathOp::GLTriangulatingPathOp(std::vector<float> vertex, int vertexCount,
                                             Rect bounds)
    : vertex(std::move(vertex)), vertexCount(vertexCount) {
  setBounds(bounds);
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
