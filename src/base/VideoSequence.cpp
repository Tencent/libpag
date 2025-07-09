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
VideoFrame::~VideoFrame() {
  delete fileBytes;
}

VideoSequence::~VideoSequence() {
  for (auto frame : frames) {
    delete frame;
  }

  for (auto header : headers) {
    delete header;
  }

  delete MP4Header;
}

bool VideoSequence::verify() const {
  if (!Sequence::verify() || frames.empty()) {
    VerifyFailed();
    return false;
  }
  auto frameNotNull = [](VideoFrame* frame) {
    return frame != nullptr && frame->fileBytes != nullptr;
  };
  if (!std::all_of(frames.begin(), frames.end(), frameNotNull)) {
    VerifyFailed();
    return false;
  }
  auto headerNotNull = [](ByteData* header) { return header != nullptr; };
  if (!std::all_of(headers.begin(), headers.end(), headerNotNull)) {
    VerifyFailed();
    return false;
  }
  return true;
}

// The exact total width and height of the picture were not recorded when the video sequence frame
// was exported，You need to do the calculation yourself with width and alphaStartX，
// If an odd size is encountered, the exporter plugin automatically increments by one，
// This matches the rules for exporter plugin。
int32_t VideoSequence::getVideoWidth() const {
  auto videoWidth = alphaStartX + width;
  if (videoWidth % 2 == 1) {
    videoWidth++;
  }
  return videoWidth;
}

int32_t VideoSequence::getVideoHeight() const {
  auto videoHeight = alphaStartY + height;
  if (videoHeight % 2 == 1) {
    videoHeight++;
  }
  return videoHeight;
}
}  // namespace pag
