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

#include "SequenceReader.h"
#include "rendering/sequences/SequenceReaderFactory.h"
#include "tgfx/core/Image.h"

namespace pag {
class SequenceImage {
 public:
  /**
   * Creates an Image from a static Sequence. Returns nullptr if the contents of sequence are not
   * static.
   */
  static std::shared_ptr<tgfx::Image> MakeStatic(std::shared_ptr<File> file,
                                                 std::shared_ptr<SequenceReaderFactory> sequence);

  /**
   * Creates an Image from the specified frame of a SequenceReader.
   */
  static std::shared_ptr<tgfx::Image> MakeFrom(std::shared_ptr<SequenceReader> reader,
                                               std::shared_ptr<SequenceReaderFactory> sequence,
                                               Frame targetFrame);
};
}  // namespace pag
