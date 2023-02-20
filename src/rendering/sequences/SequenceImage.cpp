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

#include "SequenceImage.h"
#include "SequenceReaderFactory.h"

namespace pag {
static std::shared_ptr<tgfx::Image> MakeSequenceImage(
    std::shared_ptr<tgfx::ImageGenerator> generator,
    std::shared_ptr<SequenceReaderFactory> sequence) {
  std::shared_ptr<tgfx::Image> image = nullptr;
  auto layout = sequence->layout();
  if (layout.alphaStartX >= 0 && layout.alphaStartY >= 0) {
    return tgfx::Image::MakeRGBAAA(std::move(generator), layout.width, layout.height,
                                   layout.alphaStartX, layout.alphaStartY);
  }
  return tgfx::Image::MakeFromGenerator(std::move(generator), sequence->orientation());
}

std::shared_ptr<tgfx::Image> SequenceImage::MakeStatic(
    std::shared_ptr<File> file, std::shared_ptr<SequenceReaderFactory> sequence) {
  auto generator = sequence->makeGenerator(file);
  return MakeSequenceImage(std::move(generator), sequence);
}

std::shared_ptr<tgfx::Image> SequenceImage::MakeFrom(
    std::shared_ptr<SequenceReader> reader, std::shared_ptr<SequenceReaderFactory> sequence,
    Frame targetFrame) {
  auto generator = sequence->makeGenerator(std::move(reader), targetFrame);
  return MakeSequenceImage(std::move(generator), sequence);
}
}  // namespace pag
