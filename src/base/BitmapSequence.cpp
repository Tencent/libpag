/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "base/utils/Verify.h"
#include "codec/utils/WebpDecoder.h"
#include "pag/file.h"

namespace pag {
BitmapRect::~BitmapRect() {
  delete fileBytes;
}

BitmapFrame::~BitmapFrame() {
  for (auto image : bitmaps) {
    delete image;
  }
}

bool BitmapFrame::verify() const {
  auto bitmapNotNull = [](BitmapRect* bitmap) {
    return bitmap != nullptr && bitmap->fileBytes != nullptr;
  };
  VerifyAndReturn(std::all_of(bitmaps.begin(), bitmaps.end(), bitmapNotNull));
}

BitmapSequence::~BitmapSequence() {
  for (auto frame : frames) {
    delete frame;
  }
}

bool BitmapSequence::isEmptyBitmapFrame(size_t frameIndex) {
  // There was a bug in PAGExporter that causes an empty frame being exported as 1x1 frame, so we
  // need to identify this kind of empty frame here.
  if (frameIndex >= frames.size()) {
    return false;
  }
  auto frame = frames[frameIndex];
  for (auto bitmap : frame->bitmaps) {
    if (bitmap->x != 0 || bitmap->y != 0) {
      return false;
    }
    if (bitmap->fileBytes->length() > 150) {
      return false;
    }

    int width = 0;
    int height = 0;
    if (!WebPGetInfo(bitmap->fileBytes->data(), bitmap->fileBytes->length(), &width, &height)) {
      LOGE("Get webP size fail.");
    }
    if (width > 1 || height > 1) {
      return false;
    }
  }
  return true;
}

bool BitmapSequence::verify() const {
  if (!Sequence::verify() || frames.empty()) {
    VerifyFailed();
    return false;
  }
  auto frameNotNull = [](BitmapFrame* frame) { return frame != nullptr && frame->verify(); };
  if (!std::all_of(frames.begin(), frames.end(), frameNotNull)) {
    VerifyFailed();
    return false;
  }
  return true;
}
}  // namespace pag
