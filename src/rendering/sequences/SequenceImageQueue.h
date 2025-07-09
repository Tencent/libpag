/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "SequenceInfo.h"
#include "SequenceReader.h"
#include "pag/file.h"
#include "pag/pag.h"

namespace pag {
class SequenceImageQueue {
 public:
  static std::unique_ptr<SequenceImageQueue> MakeFrom(std::shared_ptr<SequenceInfo> sequence,
                                                      PAGLayer* pagLayer, bool useDiskCache);

  /**
   * Prepares the image of the next frame.
   */
  void prepareNextImage();

  /**
   * Prepares the image of the specified frame.
   */
  void prepare(Frame targetFrame);

  /**
   * Returns the Image of the specified frame.
   */
  std::shared_ptr<tgfx::Image> getImage(Frame targetFrame);

  /**
   * Reports the decoding performance data.
   */
  void reportPerformance(Performance* performance);

 private:
  std::shared_ptr<SequenceInfo> sequence = nullptr;
  std::shared_ptr<SequenceReader> reader = nullptr;
  Frame firstFrame = -1;
  Frame totalFrames = 0;
  Frame currentFrame = -1;
  Frame preparedFrame = -1;
  std::shared_ptr<tgfx::Image> currentImage = nullptr;
  std::shared_ptr<tgfx::Image> preparedImage = nullptr;
  bool useDiskCache = false;

  SequenceImageQueue(std::shared_ptr<SequenceInfo> sequence, std::shared_ptr<SequenceReader> reader,
                     Frame firstFrame, bool useDiskCache);

  friend class RenderCache;
};
}  // namespace pag
