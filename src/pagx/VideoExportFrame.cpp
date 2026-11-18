/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/PAGSurface.h"
#include "pagx/VideoExportFrameImpl.h"
#include "pagx/runtime/Drawable.h"
#include "rendering/utils/AsyncReadback.h"
#include "tgfx/core/Pixmap.h"

namespace pagx {

std::shared_ptr<Drawable> GetSurfaceDrawable(const PAGSurface& surface) {
  return surface.drawable;
}

std::shared_ptr<VideoExportFrameImpl> VideoExportFrameImpl::MakeGPU(
    std::shared_ptr<VideoExportState> state, int64_t index, std::shared_ptr<PAGSurface> surface,
    std::shared_ptr<tgfx::Pixmap> staticCache, VideoExportFailure* failure) {
  auto impl = std::make_shared<VideoExportFrameImpl>(std::move(state), index);
  impl->surface = std::move(surface);
  impl->staticCache = std::move(staticCache);
  if (impl->surface == nullptr || impl->surface->drawable == nullptr) {
    failure->set(VideoExportError::InvalidArgument, "the export surface is null");
    return nullptr;
  }
  impl->device = impl->surface->drawable->getDevice();
  if (impl->device == nullptr) {
    failure->set(VideoExportError::DeviceLost, "the export surface has no GPU device");
    return nullptr;
  }
  auto* context = impl->device->lockContext();
  if (context == nullptr) {
    failure->set(VideoExportError::DeviceLost, "failed to lock the export device");
    return nullptr;
  }
  auto tgfxSurface = impl->surface->drawable->getSurface(context);
  impl->readback = pag::StartAsyncReadback(context, tgfxSurface, &impl->flipY);
  impl->device->unlock();
  if (impl->readback == nullptr) {
    failure->set(VideoExportError::ReadbackFailed, "failed to start asynchronous surface readback");
    return nullptr;
  }
  return impl;
}

std::shared_ptr<VideoExportFrameImpl> VideoExportFrameImpl::MakeCPU(
    std::shared_ptr<VideoExportState> state, int64_t index, std::shared_ptr<tgfx::Pixmap> pixels) {
  auto impl = std::make_shared<VideoExportFrameImpl>(std::move(state), index);
  impl->cpuPixels = std::move(pixels);
  return impl;
}

VideoExportFrameImpl::VideoExportFrameImpl(std::shared_ptr<VideoExportState> state, int64_t index)
    : state(std::move(state)), frameIndex(index) {
}

void VideoExportFrameImpl::fail(VideoExportError code, const std::string& message) {
  if (consumed) {
    return;
  }
  failed = true;
  failureCode = code;
  errorMessage = message;
  if (!state->cancelled) {
    state->errorCode = code;
    state->error = message;
  }
}

void VideoExportFrameImpl::absorbSharedFailure() {
  auto code = state->errorCode;
  if (code == VideoExportError::None) {
    code = VideoExportError::Cancelled;
  }
  fail(code, state->error.empty() ? "the export sequence has failed" : state->error);
}

VideoExportFrameStatus VideoExportFrameImpl::pollLocked(tgfx::Context* context) {
  if (failed) {
    return VideoExportFrameStatus::Failed;
  }
  if (consumed) {
    return VideoExportFrameStatus::Ready;
  }
  if (state->failed || state->cancelled) {
    absorbSharedFailure();
    return VideoExportFrameStatus::Failed;
  }
  if (readback == nullptr) {
    fail(VideoExportError::ReadbackFailed, "the frame has no readback session");
    return VideoExportFrameStatus::Failed;
  }
  switch (pag::PollAsyncReadback(context, readback)) {
    case tgfx::ReadbackStatus::Pending:
      return VideoExportFrameStatus::Pending;
    case tgfx::ReadbackStatus::Ready:
      return VideoExportFrameStatus::Ready;
    case tgfx::ReadbackStatus::Failed:
      fail(VideoExportError::ReadbackFailed, "asynchronous surface readback failed");
      return VideoExportFrameStatus::Failed;
  }
  fail(VideoExportError::ReadbackFailed, "asynchronous surface readback returned an unknown state");
  return VideoExportFrameStatus::Failed;
}

VideoExportFrameStatus VideoExportFrameImpl::status() {
  if (failed) {
    return VideoExportFrameStatus::Failed;
  }
  if (consumed) {
    return VideoExportFrameStatus::Ready;
  }
  if (cpuPixels != nullptr) {
    if (state->failed || state->cancelled) {
      absorbSharedFailure();
      return VideoExportFrameStatus::Failed;
    }
    return VideoExportFrameStatus::Ready;
  }
  if (device == nullptr || readback == nullptr) {
    fail(VideoExportError::ReadbackFailed, "the frame has no readback session");
    return VideoExportFrameStatus::Failed;
  }
  auto* context = device->lockContext();
  if (context == nullptr) {
    fail(VideoExportError::DeviceLost, "failed to lock the export device");
    return VideoExportFrameStatus::Failed;
  }
  auto result = pollLocked(context);
  device->unlock();
  return result;
}

bool VideoExportFrameImpl::copyGPUPixelsLocked(tgfx::Context* context, void* dstPixels,
                                               size_t dstRowBytes) {
  if (!readback->isReady(context)) {
    return false;
  }
  auto pixels = readback->lockPixels(context, flipY);
  if (pixels == nullptr) {
    return false;
  }
  tgfx::Pixmap source(readback->info(), pixels);
  if (staticCache != nullptr && !staticCache->info().isEmpty()) {
    // The static frame cache is an optimization: fill it before the caller's buffer so that a cache
    // failure can never leave the destination partially written, and ignore the failure instead of
    // ending the sequence.
    source.readPixels(staticCache->info(), staticCache->writablePixels());
  }
  auto destination =
      tgfx::ImageInfo::Make(source.width(), source.height(), tgfx::ColorType::RGBA_8888,
                            tgfx::AlphaType::Premultiplied, dstRowBytes);
  auto success = !destination.isEmpty() && source.readPixels(destination, dstPixels);
  readback->unlockPixels(context);
  return success;
}

void VideoExportFrameImpl::commit() {
  consumed = true;
  state->currentFrame = frameIndex + 1;
}

bool VideoExportFrameImpl::readPixels(void* dstPixels, size_t dstRowBytes) {
  if (failed || consumed || dstPixels == nullptr) {
    return false;
  }
  if (cpuPixels != nullptr) {
    if (state->failed || state->cancelled) {
      absorbSharedFailure();
      return false;
    }
    auto width = cpuPixels->width();
    if (width <= 0 || dstRowBytes < static_cast<size_t>(width) * 4) {
      return false;
    }
    auto destination =
        tgfx::ImageInfo::Make(cpuPixels->width(), cpuPixels->height(), tgfx::ColorType::RGBA_8888,
                              tgfx::AlphaType::Premultiplied, dstRowBytes);
    if (destination.isEmpty() || !cpuPixels->readPixels(destination, dstPixels)) {
      fail(VideoExportError::ReadbackFailed, "failed to copy the export frame pixels");
      return false;
    }
    commit();
    return true;
  }
  if (device == nullptr || readback == nullptr) {
    fail(VideoExportError::ReadbackFailed, "the frame has no readback session");
    return false;
  }
  auto* context = device->lockContext();
  if (context == nullptr) {
    fail(VideoExportError::DeviceLost, "failed to lock the export device");
    return false;
  }
  auto success = false;
  if (pollLocked(context) == VideoExportFrameStatus::Ready) {
    auto width = readback->info().width();
    if (width <= 0 || dstRowBytes < static_cast<size_t>(width) * 4) {
      device->unlock();
      return false;
    }
    if (copyGPUPixelsLocked(context, dstPixels, dstRowBytes)) {
      success = true;
    } else {
      fail(VideoExportError::ReadbackFailed, "failed to copy the export frame pixels");
    }
  }
  device->unlock();
  if (!success) {
    return false;
  }
  commit();
  return true;
}

VideoExportFrame::VideoExportFrame(std::shared_ptr<VideoExportFrameImpl> impl)
    : impl(std::move(impl)) {
}

VideoExportFrame::~VideoExportFrame() = default;

int64_t VideoExportFrame::index() const {
  return impl != nullptr ? impl->index() : -1;
}

VideoExportFrameStatus VideoExportFrame::status() {
  return impl != nullptr ? impl->status() : VideoExportFrameStatus::Failed;
}

std::string VideoExportFrame::error() const {
  return impl != nullptr ? impl->error() : "the export frame is null";
}

VideoExportError VideoExportFrame::errorCode() const {
  return impl != nullptr ? impl->errorCode() : VideoExportError::InvalidArgument;
}

bool VideoExportFrame::readPixels(void* dstPixels, size_t dstRowBytes) {
  return impl != nullptr && impl->readPixels(dstPixels, dstRowBytes);
}

}  // namespace pagx
