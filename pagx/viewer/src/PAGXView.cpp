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
#include <emscripten/html5.h>
#include "GridBackground.h"
#include "pagx/PAGXImporter.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Typeface.h"

using namespace emscripten;

namespace pagx {

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

PAGXView::PAGXView(const std::string& canvasID) : canvasID(canvasID) {
  displayList.setRenderMode(tgfx::RenderMode::Tiled);
  displayList.setAllowZoomBlur(true);
  displayList.setMaxTileCount(512);
}

void PAGXView::registerFonts(const val& fontVal, const val& emojiFontVal) {
  std::vector<std::shared_ptr<tgfx::Typeface>> fallbackTypefaces;
  auto fontData = GetDataFromEmscripten(fontVal);
  if (fontData) {
    auto typeface = tgfx::Typeface::MakeFromData(fontData, 0);
    if (typeface) {
      fallbackTypefaces.push_back(std::move(typeface));
    }
  }
  auto emojiFontData = GetDataFromEmscripten(emojiFontVal);
  if (emojiFontData) {
    auto typeface = tgfx::Typeface::MakeFromData(emojiFontData, 0);
    if (typeface) {
      fallbackTypefaces.push_back(std::move(typeface));
    }
  }
  typesetter.setFallbackTypefaces(std::move(fallbackTypefaces));
}

void PAGXView::loadPAGX(const val& pagxData) {
  auto data = GetDataFromEmscripten(pagxData);
  if (!data) {
    return;
  }
  auto document = PAGXImporter::FromXML(data->bytes(), data->size());
  if (!document) {
    return;
  }
  contentLayer = LayerBuilder::Build(document.get(), &typesetter);
  if (!contentLayer) {
    return;
  }
  pagxWidth = document->width;
  pagxHeight = document->height;
  displayList.root()->removeChildren();
  displayList.root()->addChild(contentLayer);
  applyCenteringTransform();
}

void PAGXView::updateSize() {
  if (window == nullptr) {
    window = tgfx::WebGLWindow::MakeFrom(canvasID);
  }
  if (window == nullptr) {
    return;
  }
  window->invalidSize();
  auto device = window->getDevice();
  auto context = device->lockContext();
  if (context == nullptr) {
    return;
  }
  auto surface = window->getSurface(context);
  if (surface == nullptr) {
    device->unlock();
    return;
  }
  if (surface->width() != lastSurfaceWidth || surface->height() != lastSurfaceHeight) {
    lastSurfaceWidth = surface->width();
    lastSurfaceHeight = surface->height();
    applyCenteringTransform();
    presentImmediately = true;
  }
  device->unlock();
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

void PAGXView::draw() {
  if (window == nullptr) {
    window = tgfx::WebGLWindow::MakeFrom(canvasID);
  }
  if (window == nullptr) {
    return;
  }
  bool hasContentChanged = displayList.hasContentChanged();
  bool hasLastRecording = (lastRecording != nullptr);
  if (!hasContentChanged && !hasLastRecording) {
    return;
  }
  auto device = window->getDevice();
  auto context = device->lockContext();
  if (context == nullptr) {
    return;
  }
  auto surface = window->getSurface(context);
  if (surface == nullptr) {
    device->unlock();
    return;
  }
  auto canvas = surface->getCanvas();
  canvas->clear();
  int width = 0;
  int height = 0;
  emscripten_get_canvas_element_size(canvasID.c_str(), &width, &height);
  auto density = static_cast<float>(surface->width()) / static_cast<float>(width);
  pagx::DrawBackground(canvas, surface->width(), surface->height(), density);
  displayList.render(surface.get(), false);
  auto recording = context->flush();
  if (presentImmediately) {
    presentImmediately = false;
    if (recording) {
      context->submit(std::move(recording));
      window->present(context);
    }
  } else {
    std::swap(lastRecording, recording);
    if (recording) {
      context->submit(std::move(recording));
      window->present(context);
    }
  }
  device->unlock();
}

}  // namespace pagx
