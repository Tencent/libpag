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

#include "ImageGeneratorTask.h"

namespace tgfx {
class ImageResult {
 public:
  std::shared_ptr<ImageBuffer> imageBuffer = nullptr;
};

std::shared_ptr<ImageGeneratorTask> ImageGeneratorTask::MakeFrom(
    std::shared_ptr<ImageGenerator> generator, bool tryHardware) {
  if (generator == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<ImageGeneratorTask>(
      new ImageGeneratorTask(std::move(generator), tryHardware));
}

ImageGeneratorTask::ImageGeneratorTask(std::shared_ptr<ImageGenerator> generator, bool tryHardware)
    : imageGenerator(std::move(generator)) {
  imageResult = std::make_shared<ImageResult>();
  task = Task::Run([result = imageResult, generator = imageGenerator, tryHardware]() {
    result->imageBuffer = generator->makeBuffer(tryHardware);
  });
}

ImageGeneratorTask::~ImageGeneratorTask() {
  task->cancel();
}

std::shared_ptr<ImageBuffer> ImageGeneratorTask::getBuffer() const {
  task->wait();
  return imageResult->imageBuffer;
}
}  // namespace tgfx
