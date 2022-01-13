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

#pragma once

#include "SequenceReader.h"
#include "base/utils/Task.h"
#include "image/Bitmap.h"
#include "pag/file.h"

namespace pag {
class BitmapSequenceReader : public SequenceReader {
 public:
  BitmapSequenceReader(std::shared_ptr<File> file, BitmapSequence* sequence);

  void decodeFrame(Frame targetFrame);

  void prepareAsync(Frame targetFrame) override;

  std::shared_ptr<Texture> readTexture(Frame frame, RenderCache* cache) override;

 private:
  std::mutex locker = {};
  Frame lastDecodeFrame = -1;
  Frame lastTextureFrame = -1;
  Frame pendingFrame = -1;
  Bitmap bitmap = {};
  std::shared_ptr<Texture> lastTexture = nullptr;
  std::shared_ptr<Task> lastTask = nullptr;

  Frame findStartFrame(Frame targetFrame);
};
}  // namespace pag
