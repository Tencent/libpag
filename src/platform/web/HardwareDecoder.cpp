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

#include "HardwareDecoder.h"
#include "base/utils/TimeUtil.h"
#include "codec/mp4/MP4BoxHelper.h"
#include "rendering/sequences/VideoSequenceDemuxer.h"
#include "tgfx/gpu/opengl/GLFunctions.h"

using namespace emscripten;

namespace pag {
static constexpr int ANDROID_ALIGNMENT = 16;

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

  auto texture = tgfx::Resource::Wrap(
      context, new WebVideoTexture(sampler, width, height, tgfx::SurfaceOrigin::TopLeft));
  if (isAndroidMiniprogram) {
    // https://stackoverflow.com/questions/28291204/something-about-stagefright-codec-input-format-in-android
    // Video decoder will align to multiples of 16 on the Android WeChat mini-program.
    texture->textureWidth += ANDROID_ALIGNMENT - width % ANDROID_ALIGNMENT;
    texture->textureHeight += ANDROID_ALIGNMENT - height % ANDROID_ALIGNMENT;
  }
  return texture;
}

WebVideoTexture::WebVideoTexture(const tgfx::GLSampler& glSampler, int width, int height,
                                 tgfx::SurfaceOrigin origin)
    : GLTexture(width, height, origin), textureWidth(width), textureHeight(height) {
  sampler = glSampler;
}

tgfx::Point WebVideoTexture::getTextureCoord(float x, float y) const {
  return {x / static_cast<float>(textureWidth), y / static_cast<float>(textureHeight)};
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

WebVideoBuffer::WebVideoBuffer(int width, int height, emscripten::val videoReader)
    : _width(width), _height(height), videoReader(videoReader) {
}

WebVideoBuffer::~WebVideoBuffer() {
  if (videoReader.as<bool>()) {
    videoReader.call<void>("onDestroy");
  }
}

std::shared_ptr<tgfx::Texture> WebVideoBuffer::onMakeTexture(tgfx::Context* context, bool) const {
  std::lock_guard<std::mutex> autoLock(locker);
  if (webVideoTexture == nullptr) {
    auto isAndroidMiniprogram =
        val::module_property("VideoReader").call<bool>("isAndroidMiniprogram");
    webVideoTexture = WebVideoTexture::Make(context, _width, _height, isAndroidMiniprogram);
  }
  auto& sampler = webVideoTexture->glSampler();
  videoReader.call<void>("renderToTexture", val::module_property("GL"), sampler.id);
  return webVideoTexture;
}

HardwareDecoder::HardwareDecoder(const VideoFormat& format) {
  auto demuxer = static_cast<VideoSequenceDemuxer*>(format.demuxer);
  file = demuxer->file;
  rootFile = demuxer->pagFile;
  auto sequence = demuxer->sequence;
  _width = sequence->getVideoWidth();
  _height = sequence->getVideoHeight();
  frameRate = sequence->frameRate;
  auto videoReaderClass = val::module_property("VideoReader");
  auto staticTimeRanges = val::array();
  for (const auto& timeRange : sequence->staticTimeRanges) {
    auto object = val::object();
    object.set("start", static_cast<int>(timeRange.start));
    object.set("end", static_cast<int>(timeRange.end));
    staticTimeRanges.call<void>("push", object);
  }
  if (videoReaderClass.call<bool>("isIOS")) {
    auto videoSequence = *sequence;
    videoSequence.MP4Header = nullptr;
    std::vector<VideoFrame> videoFrames;
    for (auto& frame : sequence->frames) {
      if (!videoFrames.empty() && frame->isKeyframe) {
        break;
      }
      videoFrames.push_back(*frame);
      auto& videoFrame = videoFrames.back();
      videoFrame.frame += static_cast<Frame>(sequence->frames.size());
      videoSequence.frames.emplace_back(&videoFrame);
    }
    mp4Data = MP4BoxHelper::CovertToMP4(&videoSequence);
    videoSequence.frames.clear();
    videoSequence.headers.clear();
    for (auto& frame : videoFrames) {
      frame.fileBytes = nullptr;
    }
  } else {
    mp4Data = MP4BoxHelper::CovertToMP4(sequence);
  }
  videoReader = videoReaderClass
                    .call<val>("create", val(typed_memory_view(mp4Data->length(), mp4Data->data())),
                               _width, _height, sequence->frameRate, staticTimeRanges)
                    .await();
  auto video = videoReader.call<val>("getVideo");
  if (!video.isNull()) {
    videoImageReader = tgfx::VideoImageReader::MakeFrom(video, _width, _height);
  }
}

DecodingResult HardwareDecoder::onSendBytes(void*, size_t, int64_t time) {
  pendingTimeStamp = time;
  return DecodingResult::Success;
}

DecodingResult HardwareDecoder::onEndOfStream() {
  endOfStream = true;
  return DecodingResult::Success;
}

DecodingResult HardwareDecoder::onDecodeFrame() {
  if (endOfStream) {
    return DecodingResult::EndOfStream;
  }
  currentTimeStamp = pendingTimeStamp;
  return DecodingResult::Success;
}

void HardwareDecoder::onFlush() {
  endOfStream = false;
}

int64_t HardwareDecoder::presentationTime() {
  return currentTimeStamp;
}

std::shared_ptr<tgfx::ImageBuffer> HardwareDecoder::onRenderFrame() {
  float playbackRate = 1;
  if (rootFile != nullptr && rootFile->timeStretchMode() == PAGTimeStretchMode::Scale) {
    playbackRate = file->duration() / ((rootFile->duration() / 1000000) * rootFile->frameRate());
  }
  auto targetFrame = TimeToFrame(currentTimeStamp, frameRate);
  auto imageBuffer = videoImageReader->acquireNextBuffer(
      videoReader.call<val>("prepare", static_cast<int>(targetFrame), playbackRate));
  return imageBuffer;
}
}  // namespace pag
