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

#include "pagx/VideoExportFrame.h"
#include "pagx/PAGSurface.h"
#include "pagx/runtime/Drawable.h"
#include "rendering/utils/AsyncReadback.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Pixmap.h"
#include "tgfx/gpu/GPUBuffer.h"

namespace pagx {

std::shared_ptr<VideoExportFrameImpl> VideoExportFrameImpl::MakeGPU(
    std::shared_ptr<VideoExportState> state, int64_t index, std::shared_ptr<PAGSurface> surface,
    std::shared_ptr<tgfx::Bitmap> staticCache, std::string* errorMsg) {
  auto impl = std::make_shared<VideoExportFrameImpl>(std::move(state), index);
  impl->surface = std::move(surface);
  impl->staticCache = std::move(staticCache);
  if (impl->surface == nullptr || impl->surface->drawable == nullptr) {
    if (errorMsg != nullptr) {
      *errorMsg = "the export surface is null";
    }
    return nullptr;
  }
  impl->device = impl->surface->drawable->getDevice();
  if (impl->device == nullptr) {
    if (errorMsg != nullptr) {
      *errorMsg = "the export surface has no GPU device";
    }
    return nullptr;
  }
  auto* context = impl->device->lockContext();
  if (context == nullptr) {
    if (errorMsg != nullptr) {
      *errorMsg = "failed to lock the export device";
    }
    return nullptr;
  }
  auto tgfxSurface = impl->surface->drawable->getSurface(context);
  impl->readback = pag::StartAsyncReadback(context, tgfxSurface, &impl->flipY);
  impl->device->unlock();
  if (impl->readback == nullptr) {
    if (errorMsg != nullptr) {
      *errorMsg = "failed to start asynchronous surface readback";
    }
    return nullptr;
  }
  return impl;
}

std::shared_ptr<VideoExportFrameImpl> VideoExportFrameImpl::MakeCPU(
    std::shared_ptr<VideoExportState> state, int64_t index, std::shared_ptr<tgfx::Bitmap> pixels) {
  auto impl = std::make_shared<VideoExportFrameImpl>(std::move(state), index);
  impl->cpuPixels = std::move(pixels);
  return impl;
}

VideoExportFrameImpl::VideoExportFrameImpl(std::shared_ptr<VideoExportState> state, int64_t index)
    : state(std::move(state)), frameIndex(index) {
}

void VideoExportFrameImpl::fail(const std::string& message) {
  if (consumed) {
    return;
  }
  failed = true;
  errorMessage = message;
  if (!state->cancelled) {
    state->failed = true;
    state->error = message;
  }
}

VideoExportFrameStatus VideoExportFrameImpl::status() {
  if (failed) {
    return VideoExportFrameStatus::Failed;
  }
  if (consumed) {
    return VideoExportFrameStatus::Ready;
  }
  if (state->failed || state->cancelled) {
    fail(state->error.empty() ? "the export sequence has failed" : state->error);
    return VideoExportFrameStatus::Failed;
  }
  if (cpuPixels != nullptr) {
    return VideoExportFrameStatus::Ready;
  }
  if (device == nullptr || readback == nullptr) {
    fail("the frame has no readback session");
    return VideoExportFrameStatus::Failed;
  }
  auto* context = device->lockContext();
  if (context == nullptr) {
    fail("failed to lock the export device");
    return VideoExportFrameStatus::Failed;
  }
  auto result = pag::PollAsyncReadback(context, readback);
  device->unlock();
  switch (result) {
    case tgfx::ReadbackStatus::Pending:
      return VideoExportFrameStatus::Pending;
    case tgfx::ReadbackStatus::Ready:
      return VideoExportFrameStatus::Ready;
    case tgfx::ReadbackStatus::Failed:
      fail("asynchronous surface readback failed");
      return VideoExportFrameStatus::Failed;
  }
  fail("asynchronous surface readback returned an unknown state");
  return VideoExportFrameStatus::Failed;
}

bool VideoExportFrameImpl::readGPUPixels(void* dstPixels, size_t dstRowBytes) {
  auto* context = device->lockContext();
  if (context == nullptr) {
    return false;
  }
  auto buffer = readback->getGPUBuffer(context);
  if (buffer == nullptr || !buffer->isReady()) {
    device->unlock();
    return false;
  }
  // Ready means the transfer and mapping have completed. lockPixels() would unnecessarily wait for
  // the whole GPU queue again, so map the ready buffer directly.
  auto pixels = buffer->map();
  if (pixels == nullptr) {
    device->unlock();
    return false;
  }
  tgfx::Pixmap source(readback->info(), pixels);
  auto copyTo = [&source, this](const tgfx::ImageInfo& destination, void* destinationPixels) {
    if (destination.isEmpty()) {
      return false;
    }
    if (!flipY) {
      return source.readPixels(destination, destinationPixels);
    }
    for (int row = 0; row < source.height(); ++row) {
      auto rowInfo =
          tgfx::ImageInfo::Make(source.width(), 1, destination.colorType(), destination.alphaType(),
                                destination.rowBytes(), destination.colorSpace());
      auto* rowPixels = static_cast<uint8_t*>(destinationPixels) + row * destination.rowBytes();
      if (!source.readPixels(rowInfo, rowPixels, 0, source.height() - row - 1)) {
        return false;
      }
    }
    return true;
  };
  auto destination =
      tgfx::ImageInfo::Make(source.width(), source.height(), tgfx::ColorType::RGBA_8888,
                            tgfx::AlphaType::Premultiplied, dstRowBytes);
  auto success = copyTo(destination, dstPixels);
  if (success && staticCache != nullptr) {
    tgfx::Pixmap cache(*staticCache);
    success = copyTo(cache.info(), cache.writablePixels());
  }
  buffer->unmap();
  device->unlock();
  return success;
}

bool VideoExportFrameImpl::readPixels(void* dstPixels, size_t dstRowBytes) {
  if (failed || consumed || dstPixels == nullptr) {
    return false;
  }
  if (status() != VideoExportFrameStatus::Ready) {
    return false;
  }
  auto success = false;
  if (cpuPixels != nullptr) {
    tgfx::Pixmap source(*cpuPixels);
    auto destination =
        tgfx::ImageInfo::Make(source.width(), source.height(), tgfx::ColorType::RGBA_8888,
                              tgfx::AlphaType::Premultiplied, dstRowBytes);
    success = !destination.isEmpty() && source.readPixels(destination, dstPixels);
  } else {
    success = readGPUPixels(dstPixels, dstRowBytes);
  }
  if (!success) {
    fail("failed to copy the export frame pixels");
    return false;
  }
  consumed = true;
  state->currentFrame = frameIndex + 1;
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

bool VideoExportFrame::readPixels(void* dstPixels, size_t dstRowBytes) {
  return impl != nullptr && impl->readPixels(dstPixels, dstRowBytes);
}

}  // namespace pagx
