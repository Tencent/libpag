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

#include "CGMask.h"
#include "platform/apple/BitmapContextUtil.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Mask.h"

namespace tgfx {
static void Iterator(PathVerb verb, const Point points[4], void* info) {
  auto cgPath = reinterpret_cast<CGMutablePathRef>(info);
  switch (verb) {
    case PathVerb::Move:
      CGPathMoveToPoint(cgPath, nullptr, points[0].x, points[0].y);
      break;
    case PathVerb::Line:
      CGPathAddLineToPoint(cgPath, nullptr, points[1].x, points[1].y);
      break;
    case PathVerb::Quad:
      CGPathAddQuadCurveToPoint(cgPath, nullptr, points[1].x, points[1].y, points[2].x,
                                points[2].y);
      break;
    case PathVerb::Cubic:
      CGPathAddCurveToPoint(cgPath, nullptr, points[1].x, points[1].y, points[2].x, points[2].y,
                            points[3].x, points[3].y);
      break;
    case PathVerb::Close:
      CGPathCloseSubpath(cgPath);
      break;
  }
}

std::shared_ptr<Mask> Mask::Make(int width, int height) {
  auto buffer = PixelBuffer::Make(width, height, true);
  if (buffer == nullptr) {
    return nullptr;
  }
  Bitmap(buffer).eraseAll();
  return std::make_shared<CGMask>(std::move(buffer));
}

CGMask::CGMask(std::shared_ptr<PixelBuffer> buffer)
    : Mask(buffer->width(), buffer->height()), buffer(std::move(buffer)) {
}

void CGMask::fillPath(const Path& path) {
  if (path.isEmpty()) {
    return;
  }
  const auto& info = buffer->info();
  Bitmap bm(buffer);
  auto cgContext = CreateBitmapContext(info, bm.writablePixels());
  if (cgContext == nullptr) {
    return;
  }

  auto finalPath = path;
  auto totalMatrix = matrix;
  totalMatrix.postScale(1, -1);
  totalMatrix.postTranslate(0, static_cast<float>(info.height()));
  finalPath.transform(totalMatrix);
  auto cgPath = CGPathCreateMutable();
  finalPath.decompose(Iterator, cgPath);

  CGContextSetShouldAntialias(cgContext, true);
  static const CGFloat white[] = {1.f, 1.f, 1.f, 1.f};
  if (finalPath.isInverseFillType()) {
    auto rect = CGRectMake(0.f, 0.f, info.width(), info.height());
    CGContextAddRect(cgContext, rect);
    CGContextSetFillColor(cgContext, white);
    CGContextFillPath(cgContext);
    CGContextAddPath(cgContext, cgPath);
    if (finalPath.getFillType() == PathFillType::InverseWinding) {
      CGContextClip(cgContext);
    } else {
      CGContextEOClip(cgContext);
    }
    CGContextClearRect(cgContext, rect);
  } else {
    CGContextAddPath(cgContext, cgPath);
    CGContextSetFillColor(cgContext, white);
    if (finalPath.getFillType() == PathFillType::Winding) {
      CGContextFillPath(cgContext);
    } else {
      CGContextEOFillPath(cgContext);
    }
  }

  CGContextRelease(cgContext);
  CGPathRelease(cgPath);
}

void CGMask::clear() {
  Bitmap(buffer).eraseAll();
}
}  // namespace tgfx
