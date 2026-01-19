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
#include "pag/file.h"

namespace pag {
Sequence* Sequence::Get(Composition* composition) {
  // Multiple sequences in one composition is no longer supported. Right now we just use the last
  // one for best rendering quality, ignore all others.
  if (composition != nullptr) {
    switch (composition->type()) {
      case CompositionType::Video:
        return static_cast<VideoComposition*>(composition)->sequences.back();
      case CompositionType::Bitmap:
        return static_cast<BitmapComposition*>(composition)->sequences.back();
      default:
        break;
    }
  }
  return nullptr;
}

bool Sequence::verify() const {
  VerifyAndReturn(composition != nullptr && width > 0 && height > 0 && frameRate > 0);
}

Frame Sequence::toSequenceFrame(Frame compositionFrame) {
  auto sequenceFrame =
      ConvertFrameByStaticTimeRanges(composition->staticTimeRanges, compositionFrame);
  double timeScale = frameRate / composition->frameRate;
  sequenceFrame = static_cast<Frame>(round(sequenceFrame * timeScale));
  if (sequenceFrame >= duration()) {
    sequenceFrame = static_cast<Frame>(duration()) - 1;
  }
  return sequenceFrame;
}
}  // namespace pag
