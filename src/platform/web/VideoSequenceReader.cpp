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
#include "codec/mp4/MP4BoxHelper.h"
#include "tgfx/gpu/opengl/GLFunctions.h"
#include "tgfx/gpu/opengl/GLTexture.h"

using namespace emscripten;

namespace pag {
std::shared_ptr<WebVideoTexture> WebVideoTexture::Make(tgfx::Context* context, int width,
                                                       int height, bool isAndroidMiniprogram) {
  tgfx::GLSampler sampler = {};
  sampler.target = GL_TEXTURE_2D;
  sampler.format = tgfx::PixelFormat::RGBA_8888;
  auto gl = tgfx::GLFunctions::Get(context);
  gl->genTextures(1, &sampler.id);
  if (sampler.id == 0) {
    return nullptr;
  }
  gl->bindTexture(sampler.target, sampler.id);
  gl->texParameteri(sampler.target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->texParameteri(sampler.target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl->texParameteri(sampler.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->texParameteri(sampler.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl->texImage2D(sampler.target, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  return tgfx::Resource::Wrap(
      context, new WebVideoTexture(sampler, width, height, tgfx::ImageOrigin::TopLeft,
                                   isAndroidMiniprogram));
}

WebVideoTexture::WebVideoTexture(const tgfx::GLSampler& glSampler, int width, int height,
                                 tgfx::ImageOrigin origin, bool isAndroidMiniprogram)
    : GLTexture(width, height, origin), isAndroidMiniprogram(isAndroidMiniprogram) {
  sampler = glSampler;
}

tgfx::Point WebVideoTexture::getTextureCoord(float x, float y) const {
  // https://stackoverflow.com/questions/28291204/something-about-stagefright-codec-input-format-in-android
  // Video decoder will align to multiples of 16 on the Android WeChat mini-program.
  if (isAndroidMiniprogram && width() &&
      ((width() % androidAlignment != 0) || (height() % androidAlignment != 0))) {
    return {
        x / (ceil(static_cast<float>(width()) / static_cast<float>(androidAlignment)) *
             static_cast<float>(androidAlignment)),
        y / (ceil(static_cast<float>(height()) / static_cast<float>(androidAlignment)) *
             static_cast<float>(androidAlignment)),
    };
  }
  return {x / static_cast<float>(width()), y / static_cast<float>(height())};
}

size_t WebVideoTexture::memoryUsage() const {
  return width() * height() * 4;
}

void WebVideoTexture::onReleaseGPU() {
  if (sampler.id > 0) {
    auto gl = tgfx::GLFunctions::Get(context);
    gl->deleteTextures(1, &sampler.id);
  }
}

VideoSequenceReader::VideoSequenceReader(std::shared_ptr<File> file, VideoSequence* sequence,
                                         PAGFile* pagFile)
    : SequenceReader(sequence->duration(), sequence->composition->staticContent()), file(file),
      rootFile(pagFile) {
  _width = sequence->getVideoWidth();
  _height = sequence->getVideoHeight();
  auto videoReaderClass = val::module_property("VideoReader");
  if (videoReaderClass.as<bool>()) {
    auto staticTimeRanges = val::array();
    for (const auto& timeRange : sequence->staticTimeRanges) {
      auto object = val::object();
      object.set("start", static_cast<int>(timeRange.start));
      object.set("end", static_cast<int>(timeRange.end));
      staticTimeRanges.call<void>("push", object);
    }
    if (videoReaderClass.call<bool>("isIOS")) {
      auto* videoSequence = new VideoSequence(*sequence);
      videoSequence->MP4Header = nullptr;
      std::vector<VideoFrame*> videoFrames;
      for (auto* frame : sequence->frames) {
        if (!videoFrames.empty() && frame->isKeyframe) {
          break;
        }
        auto* videoFrame = new VideoFrame(*frame);
        videoFrame->frame += static_cast<Frame>(sequence->frames.size());
        videoSequence->frames.emplace_back(videoFrame);
        videoFrames.emplace_back(videoFrame);
      }
      mp4Data = MP4BoxHelper::CovertToMP4(videoSequence);
      videoSequence->frames.clear();
      videoSequence->headers.clear();
      delete videoSequence;
      for (auto* frame : videoFrames) {
        frame->fileBytes = nullptr;
        delete frame;
      }
    } else {
      mp4Data = MP4BoxHelper::CovertToMP4(sequence);
    }
    videoReader =
        videoReaderClass
            .call<val>("create", val(typed_memory_view(mp4Data->length(), mp4Data->data())), _width,
                       _height, sequence->frameRate, staticTimeRanges)
            .await();
  }
}

VideoSequenceReader::~VideoSequenceReader() {
  lastTask = nullptr;
  if (videoReader.as<bool>()) {
    videoReader.call<void>("onDestroy");
  }
}

bool VideoSequenceReader::decodeFrame(Frame targetFrame) {
  // Web 端渲染过程不能 await，否则会把渲染一半的 Canvas 上屏。
  if (videoReader.as<bool>()) {
    float playbackRate = 1;
    if (rootFile != nullptr && rootFile->timeStretchMode() == PAGTimeStretchMode::Scale) {
      playbackRate = file->duration() / ((rootFile->duration() / 1000000) * rootFile->frameRate());
    }
    videoReader.call<val>("prepare", static_cast<int>(targetFrame), playbackRate).await();
    return true;
  }
  return false;
}

std::shared_ptr<tgfx::Texture> VideoSequenceReader::onMakeTexture(tgfx::Context* context) {
  if (!videoReader.as<bool>()) {
    return nullptr;
  }
  if (webVideoTexture == nullptr) {
    auto isAndroidMiniprogram =
        val::module_property("VideoReader").call<bool>("isAndroidMiniprogram");
    webVideoTexture = WebVideoTexture::Make(context, _width, _height, isAndroidMiniprogram);
  }
  auto& sampler = webVideoTexture->glSampler();
  videoReader.call<void>("renderToTexture", val::module_property("GL"), sampler.id);
  return webVideoTexture;
}
}  // namespace pag
