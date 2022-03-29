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

#include "VideoSequenceReader.h"
#include "gpu/opengl/GLTexture.h"

using namespace emscripten;

namespace pag {
VideoSequenceReader::VideoSequenceReader(std::shared_ptr<File> file, VideoSequence* sequence)
    : SequenceReader(sequence->duration(), sequence->composition->staticContent()),
      file(std::move(file)) {
  width = sequence->alphaStartX + sequence->width;
  if (width % 2 == 1) {
    width++;
  }
  height = sequence->alphaStartY + sequence->height;
  if (height % 2 == 1) {
    height++;
  }
  auto videoReaderClass = val::module_property("VideoReader");
  if (videoReaderClass.as<bool>()) {
    auto headers = val::array();
    for (auto* header : sequence->headers) {
      headers.call<void>("push", val(typed_memory_view(header->length(), header->data())));
    }
    auto frames = val::array();
    auto ptsList = val::array();
    for (auto* frame : sequence->frames) {
      frames.call<void>(
          "push", val(typed_memory_view(frame->fileBytes->length(), frame->fileBytes->data())));
      ptsList.call<void>("push", static_cast<int>(frame->frame));
    }
    videoReader =
        videoReaderClass.new_(width, height, sequence->frameRate, headers, frames, ptsList);
  }
}

VideoSequenceReader::~VideoSequenceReader() {
  lastTask = nullptr;
  if (videoReader.as<bool>()) {
    videoReader.call<void>("onDestroy");
  }
}

void VideoSequenceReader::prepare(Frame targetFrame) {
  // Web 端渲染过程不能 await，否则会把渲染一半的 Canvas 上屏。
  if (videoReader.as<bool>()) {
    videoReader.call<val>("prepare", static_cast<int>(targetFrame)).await();
  }
}

std::shared_ptr<tgfx::Texture> VideoSequenceReader::makeTexture(tgfx::Context* context) {
  if (!videoReader.as<bool>()) {
    return nullptr;
  }
  if (texture == nullptr) {
    texture = tgfx::GLTexture::MakeRGBA(context, width, height);
  }
  auto& glInfo = std::static_pointer_cast<tgfx::GLTexture>(texture)->glSampler();
  videoReader.call<void>("renderToTexture", val::module_property("GL"), glInfo.id);
  return texture;
}

void VideoSequenceReader::recordPerformance(Performance*, int64_t) {
}
}  // namespace pag
