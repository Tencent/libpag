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

#include "pag/file.h"
#include "rendering/Performance.h"
#include "rendering/graphics/TextureProxy.h"
#include "video/DecodingPolicy.h"

namespace pag {
class RenderCache;

class SequenceReader {
 public:
  static std::shared_ptr<SequenceReader> Make(std::shared_ptr<File> file, VideoSequence* sequence,
                                              DecodingPolicy policy);

  SequenceReader(std::shared_ptr<File> file, Sequence* sequence);

  virtual ~SequenceReader() = default;

  Sequence* getSequence() const {
    return sequence;
  }

  virtual void prepareAsync(Frame targetFrame) = 0;

  virtual std::shared_ptr<tgfx::Texture> readTexture(Frame targetFrame, RenderCache* cache) = 0;

 protected:
  // 持有 File 引用，防止在异步解码时 Sequence 被析构。
  std::shared_ptr<File> file = nullptr;
  Sequence* sequence = nullptr;
  bool staticContent = false;
};

}  // namespace pag
