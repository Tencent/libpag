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
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "PAGXView.h"
#include <tgfx/gpu/opengl/GLDevice.h>

#include <memory>
#include <utility>
#include "GridBackground.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Typeface.h"

using namespace emscripten;

namespace pagx {

std::shared_ptr<PAGXView> PAGXView::MakeFrom(int width, int height) {
  if (width <= 0 || height <= 0) {
    return nullptr;
  }

  auto device = tgfx::GLDevice::Current();
  if (device == nullptr) {
    return nullptr;
  }

  return std::make_shared<PAGXView>(device, width, height);
}

static std::vector<std::shared_ptr<tgfx::Typeface>> fallbackTypefaces;

static std::shared_ptr<tgfx::Data> GetDataFromEmscripten(const val& emscriptenData) {
  if (emscriptenData.isUndefined()) {
    return nullptr;
  }
  unsigned int length = emscriptenData["length"].as<unsigned int>();
  if (length == 0) {
    return nullptr;
  }
  auto buffer = new (std::nothrow) uint8_t[length];
  if (buffer) {
    auto memory = val::module_property("HEAPU8")["buffer"];
    auto memoryView = emscriptenData["constructor"].new_(
        memory, static_cast<unsigned int>(reinterpret_cast<uintptr_t>(buffer)), length);
    memoryView.call<void>("set", emscriptenData);
    return tgfx::Data::MakeAdopted(buffer, length, tgfx::Data::DeleteProc);
  }
  return nullptr;
}

PAGXView::PAGXView(std::shared_ptr<tgfx::Device> device, int width, int height)
: device(std::move(device)), _width(width), _height(height) {
  displayList.setRenderMode(tgfx::RenderMode::Tiled);
  displayList.setAllowZoomBlur(true);
  displayList.setMaxTileCount(512);
}


void PAGXView::loadPAGX(const val& pagxData) {
  auto data = GetDataFromEmscripten(pagxData);
  if (!data) {
    return;
  }
  LayerBuilder::Options options;
  options.fallbackTypefaces = fallbackTypefaces;
  auto content = pagx::LayerBuilder::FromData(data->bytes(), data->size(), options);
  if (!content.root) {
    return;
  }
  contentLayer = content.root;
  pagxWidth = content.width;
  pagxHeight = content.height;
  displayList.root()->removeChildren();
  displayList.root()->addChild(contentLayer);
  applyCenteringTransform();
}

void PAGXView::updateSize(int width, int height) {
  if (width <= 0 || height <= 0) {
    return;
  }
  if (_width == width && _height == height) {
    return;
  }
  _width = width;
  _height = height;
  surface = nullptr;
  lastSurfaceWidth = 0;
  lastSurfaceHeight = 0;
}

void PAGXView::applyCenteringTransform() {
  if (lastSurfaceWidth <= 0 || lastSurfaceHeight <= 0 || !contentLayer) {
    return;
  }
  if (pagxWidth <= 0 || pagxHeight <= 0) {
    return;
  }
  float scaleX = static_cast<float>(lastSurfaceWidth) / pagxWidth;
  float scaleY = static_cast<float>(lastSurfaceHeight) / pagxHeight;
  float scale = std::min(scaleX, scaleY);
  float offsetX = (static_cast<float>(lastSurfaceWidth) - pagxWidth * scale) * 0.5f;
  float offsetY = (static_cast<float>(lastSurfaceHeight) - pagxHeight * scale) * 0.5f;
  auto matrix = tgfx::Matrix::MakeTrans(offsetX, offsetY);
  matrix.preScale(scale, scale);
  contentLayer->setMatrix(matrix);
}

void PAGXView::updateZoomScaleAndOffset(float zoom, float offsetX, float offsetY) {
  displayList.setZoomScale(zoom);
  displayList.setContentOffset(offsetX, offsetY);
}

// void PAGXView::draw() {
//   if (device == nullptr) {
//     return;
//   }
//   bool hasContentChanged = displayList.hasContentChanged();
//   bool hasLastRecording = (lastRecording != nullptr);
//   if (!hasContentChanged && !hasLastRecording) {
//     return;
//   }
//   auto context = device->lockContext();
//   if (context == nullptr) {
//     return;
//   }
//   if (surface == nullptr) {
//     surface = tgfx::Surface::Make(context, _width, _height);
//     if (surface != nullptr) {
//       lastSurfaceWidth = surface->width();
//       lastSurfaceHeight = surface->height();
//       applyCenteringTransform();
//       presentImmediately = true;
//     }
//   }
//   if (surface == nullptr) {
//     device->unlock();
//     return;
//   }
//   auto canvas = surface->getCanvas();
//   canvas->clear();
//   auto density = 1.0f;
//   pagx::DrawBackground(canvas, surface->width(), surface->height(), density);
//   displayList.render(surface.get(), false);
//   auto recording = context->flush();
//   if (presentImmediately) {
//     presentImmediately = false;
//     if (recording) {
//       context->submit(std::move(recording));
//     }
//   } else {
//     std::swap(lastRecording, recording);
//     if (recording) {
//       context->submit(std::move(recording));
//     }
//   }
//   device->unlock();
// }

bool PAGXView::draw() {
  if (device == nullptr) {
    return false;
  }

  auto context = device->lockContext();
  if (context == nullptr) {
    return false;
  }

  if (surface == nullptr || surface->width() != _width || surface->height() != _height) {
    tgfx::GLFrameBufferInfo glInfo = {};
    glInfo.id = 0;
    glInfo.format = 0x8058;
    tgfx::BackendRenderTarget renderTarget(glInfo, _width, _height);
    surface = tgfx::Surface::MakeFrom(context, renderTarget, tgfx::ImageOrigin::BottomLeft);
    if (surface == nullptr) {
      device->unlock();
      return false;
    }
  }

  auto canvas = surface->getCanvas();
  canvas->clear();

  DrawBackground(canvas, _width, _height, 1.0f);

  displayList.render(surface.get(), false);

  auto recording = context->flush();
  if (recording) {
    context->submit(std::move(recording));
  }
  device->unlock();

  return true;
}

}  // namespace pagx
