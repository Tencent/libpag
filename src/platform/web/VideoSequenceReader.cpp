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
#include "rendering/caches/RenderCache.h"

using namespace emscripten;

namespace pag {
std::shared_ptr<SequenceReader> SequenceReader::Make(std::shared_ptr<File> file,
                                                     VideoSequence* sequence,
                                                     DecodingPolicy policy) {
  return std::make_shared<VideoSequenceReader>(std::move(file), sequence, policy);
}

VideoSequenceReader::VideoSequenceReader(std::shared_ptr<File> file, VideoSequence* sequence,
                                         DecodingPolicy)
    : SequenceReader(std::move(file), sequence) {
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
  if (videoReader.as<bool>()) {
    videoReader.call<void>("onDestroy");
  }
}

void VideoSequenceReader::prepareAsync(Frame targetFrame) {
  // Web 端没有异步初始化解码器，也没有预测。
  // 这个方法是 Graphic->prepare() 每次调用的。
  // Web 端渲染过程不能 await，否则会把渲染一半的 Canvas 上屏。
  if (videoReader.as<bool>()) {
    videoReader.call<val>("prepareAsync", static_cast<int>(targetFrame)).await();
  }
}

std::shared_ptr<tgfx::Texture> VideoSequenceReader::readTexture(Frame targetFrame,
                                                                RenderCache* cache) {
  if (!videoReader.as<bool>()) {
    return nullptr;
  }
  if (targetFrame == lastFrame) {
    return texture;
  }
  tgfx::GLStateGuard stateGuard(cache->getContext());
  if (texture == nullptr) {
    texture = tgfx::GLTexture::MakeRGBA(cache->getContext(), width, height);
  }
  auto& glInfo = std::static_pointer_cast<tgfx::GLTexture>(texture)->glSampler();
  videoReader.call<void>("renderToTexture", val::module_property("GL"), glInfo.id);
  lastFrame = targetFrame;
  return texture;
}
}  // namespace pag
