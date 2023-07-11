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

struct RasterTarget {
  unsigned char* origin;
  int pitch;
  const uint8_t* gammaTable;
};

static void SpanFunc(int y, int, const FT_Span* spans, void* user) {
  auto* target = reinterpret_cast<RasterTarget*>(user);
  auto* q = target->origin - target->pitch * y + spans->x;
  auto c = spans->coverage;
  if (target->gammaTable) {
    c = target->gammaTable[c];
  }
  auto aCount = spans->len;
  /**
   * For small-spans it is faster to do it by ourselves than calling memset.
   * This is mainly due to the cost of the function call.
   */
  switch (aCount) {
    case 7:
      *q++ = c;
    case 6:
      *q++ = c;
    case 5:
      *q++ = c;
    case 4:
      *q++ = c;
    case 3:
      *q++ = c;
    case 2:
      *q++ = c;
    case 1:
      *q = c;
    case 0:
      break;
    default:
      memset(q, c, aCount);
  }
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
  auto ftLibrary = GetLibrary().library();
  auto buffer = static_cast<unsigned char*>(pixels);
  int rows = info.height();
  int pitch = static_cast<int>(info.rowBytes());
  RasterTarget target{};
  target.origin = buffer + (rows - 1) * pitch;
  target.pitch = pitch;
  if (needsGammaCorrection) {
    target.gammaTable = PixelRefMask::GammaTable().data();
  } else {
    target.gammaTable = nullptr;
  }
  FT_Raster_Params params;
  params.flags = FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT;
  params.gray_spans = SpanFunc;
  params.user = &target;
  for (auto& outline : outlines) {
    FT_Outline_Render(ftLibrary, &(outline->outline), &params);
  }
  pixelRef->unlockPixels();
}
}  // namespace tgfx
