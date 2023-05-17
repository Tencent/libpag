/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "tgfx/core/ImageGenerator.h"
#include "tgfx/utils/Task.h"

namespace tgfx {
class ImageResult;

/**
 * ImageGeneratorTask wraps an ImageGenerator and schedules an asynchronous decoding task
 * immediately.
 */
class ImageGeneratorTask {
 public:
  static std::shared_ptr<ImageGeneratorTask> MakeFrom(std::shared_ptr<ImageGenerator> generator,
                                                      bool tryHardware = true);

  ~ImageGeneratorTask();

  int imageWidth() const {
    return imageGenerator->width();
  }

  int imageHeight() const {
    return imageGenerator->height();
  }

  std::shared_ptr<ImageBuffer> getBuffer() const;

 private:
  std::shared_ptr<Task> task = nullptr;
  std::shared_ptr<ImageResult> imageResult = nullptr;
  std::shared_ptr<ImageGenerator> imageGenerator = nullptr;

  ImageGeneratorTask(std::shared_ptr<ImageGenerator> generator, bool tryHardware);
};
}  // namespace tgfx
