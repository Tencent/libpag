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

#include "FTMask.h"
#include "FTPath.h"
#include "tgfx/core/Pixmap.h"

namespace tgfx {
static const FTLibrary& GetLibrary() {
  static const auto& library = *new FTLibrary;
  return library;
}

static void Iterator(PathVerb verb, const Point points[4], void* info) {
  auto path = reinterpret_cast<FTPath*>(info);
  switch (verb) {
    case PathVerb::Move:
      path->moveTo(points[0]);
      break;
    case PathVerb::Line:
      path->lineTo(points[1]);
      break;
    case PathVerb::Quad:
      path->quadTo(points[1], points[2]);
      break;
    case PathVerb::Cubic:
      path->cubicTo(points[1], points[2], points[3]);
      break;
    case PathVerb::Close:
      path->close();
      break;
  }
}

std::shared_ptr<Mask> Mask::Make(int width, int height, bool tryHardware) {
  auto pixelRef = PixelRef::Make(width, height, true, tryHardware);
  if (pixelRef == nullptr) {
    return nullptr;
  }
  pixelRef->clear();
  return std::make_shared<FTMask>(std::move(pixelRef));
}

void FTMask::onFillPath(const Path& path, const Matrix& matrix, bool needsGammaCorrection) {
  if (path.isEmpty()) {
    return;
  }
  auto pixels = pixelRef->lockWritablePixels();
  if (pixels == nullptr) {
    return;
  }
  const auto& info = pixelRef->info();
  auto finalPath = path;
  auto totalMatrix = matrix;
  totalMatrix.postScale(1, -1);
  totalMatrix.postTranslate(0, static_cast<float>(pixelRef->height()));
  finalPath.transform(totalMatrix);
  auto bounds = finalPath.getBounds();
  bounds.roundOut();
  markContentDirty(bounds, true);
  FTPath ftPath = {};
  finalPath.decompose(Iterator, &ftPath);
  ftPath.setFillType(path.getFillType());
  auto outlines = ftPath.getOutlines();
  FT_Bitmap bitmap;
  bitmap.width = info.width();
  bitmap.rows = info.height();
  bitmap.pitch = static_cast<int>(info.rowBytes());
  bitmap.buffer = static_cast<unsigned char*>(pixels);
  bitmap.pixel_mode = FT_PIXEL_MODE_GRAY;
  bitmap.num_grays = 256;
  auto ftLibrary = GetLibrary().library();
  for (auto& outline : outlines) {
    FT_Outline_Get_Bitmap(ftLibrary, &(outline->outline), &bitmap);
  }
  pixelRef->unlockPixels();
  if (needsGammaCorrection) {
    applyGamma(bounds, true);
  }
}
}  // namespace tgfx
