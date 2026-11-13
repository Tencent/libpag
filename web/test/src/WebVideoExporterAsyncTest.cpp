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
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <emscripten/emscripten.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "pagx/PAGXImporter.h"
#include "pagx/VideoExporter.h"

namespace {

std::shared_ptr<pagx::VideoExporter> exporter = nullptr;
std::shared_ptr<pagx::VideoExportFrame> firstFrame = nullptr;
std::vector<uint8_t> firstPixels = {};
std::string testError = {};
bool sawPending = false;

int Fail(const std::string& message) {
  testError = message;
  return 1;
}

bool CheckRedPixels(const std::vector<uint8_t>& pixels, size_t rowBytes) {
  for (int row = 0; row < 2; ++row) {
    for (int column = 0; column < 2; ++column) {
      auto offset = static_cast<size_t>(row) * rowBytes + static_cast<size_t>(column) * 4;
      if (pixels[offset] != 255 || pixels[offset + 1] != 0 || pixels[offset + 2] != 0 ||
          pixels[offset + 3] != 255) {
        return false;
      }
    }
    for (size_t offset = 8; offset < rowBytes; ++offset) {
      if (pixels[static_cast<size_t>(row) * rowBytes + offset] != 0) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE
int StartAsyncVideoExportTest() {
#ifdef PAG_USE_ASYNCIFY
  // This driver intentionally uses short synchronous ccall polling and is run by test:webgpu:noasync.
  return Fail("the async video export driver requires PAG_TEST_ASYNCIFY=OFF");
#endif
  exporter = nullptr;
  firstFrame = nullptr;
  firstPixels.clear();
  testError.clear();
  sawPending = false;
  auto document = pagx::PAGXImporter::FromXML(
      "<pagx width=\"2\" height=\"2\">"
      "  <Layer>"
      "    <Rectangle position=\"1,1\" size=\"2,2\"/>"
      "    <Fill color=\"#FF0000\"/>"
      "  </Layer>"
      "</pagx>");
  if (document == nullptr) {
    return Fail("failed to create the async video export test document");
  }
  pagx::VideoExportOptions options = {};
  options.frameRate = {1, 1};
  options.duration = 2.0f;
  options.renderMode = pagx::PAGRenderMode::Direct;
  exporter = pagx::VideoExporter::Make(document, options);
  if (exporter == nullptr || exporter->frameCount() != 2) {
    return Fail("failed to create the two-frame exporter");
  }
  if (exporter->renderMode() != pagx::PAGRenderMode::Direct) {
    return Fail("the async video exporter did not apply Direct rendering");
  }
  auto started = exporter->startNextFrame();
  if (started.status != pagx::VideoExportStartStatus::FrameStarted || started.frame == nullptr ||
      started.frame->index() != 0 || exporter->currentFrame() != 0) {
    return Fail("the first frame did not start with index zero: " + started.error);
  }
  firstFrame = started.frame;
  sawPending = firstFrame->status() == pagx::VideoExportFrameStatus::Pending;
  if (sawPending) {
    std::vector<uint8_t> pendingPixels(24, 0xCD);
    auto beforeRead = pendingPixels;
    if (firstFrame->readPixels(pendingPixels.data(), 12) || exporter->currentFrame() != 0 ||
        pendingPixels != beforeRead) {
      return Fail("Pending readPixels modified pixels or committed the frame index");
    }
  }
  return 0;
}

EMSCRIPTEN_KEEPALIVE
int PollAsyncVideoExportTest() {
  if (firstFrame == nullptr) {
    Fail("poll called before the first frame was started");
    return static_cast<int>(pagx::VideoExportFrameStatus::Failed);
  }
  auto status = firstFrame->status();
  sawPending = sawPending || status == pagx::VideoExportFrameStatus::Pending;
  return static_cast<int>(status);
}

EMSCRIPTEN_KEEPALIVE
int ConsumeAsyncVideoExportTest() {
  if (!sawPending) {
    return Fail("WebGPU completed the first frame without an observable Pending state");
  }
  if (firstFrame == nullptr || firstFrame->status() != pagx::VideoExportFrameStatus::Ready) {
    return Fail("the first frame was consumed before it became Ready");
  }
  constexpr size_t rowBytes = 12;  // Two RGBA pixels plus four bytes of padding.
  firstPixels.assign(rowBytes * 2, 0);
  if (!firstFrame->readPixels(firstPixels.data(), rowBytes) || exporter->currentFrame() != 1) {
    return Fail("failed to consume the first GPU-backed frame: " + firstFrame->error());
  }
  if (!CheckRedPixels(firstPixels, rowBytes)) {
    return Fail("the first GPU-backed frame has incorrect RGBA pixels or row padding");
  }

  auto second = exporter->startNextFrame();
  if (second.status != pagx::VideoExportStartStatus::FrameStarted || second.frame == nullptr ||
      second.frame->index() != 1 || exporter->currentFrame() != 1) {
    return Fail("the second static frame has an incorrect start state: " + second.error);
  }
  if (second.frame->status() != pagx::VideoExportFrameStatus::Ready) {
    return Fail("the cached static frame was not immediately Ready");
  }
  std::vector<uint8_t> secondPixels(rowBytes * 2, 0);
  if (!second.frame->readPixels(secondPixels.data(), rowBytes) || exporter->currentFrame() != 2) {
    return Fail("failed to consume the cached CPU frame: " + second.frame->error());
  }
  if (secondPixels != firstPixels) {
    return Fail("the cached CPU frame differs from the first GPU frame");
  }
  auto end = exporter->startNextFrame();
  if (end.status != pagx::VideoExportStartStatus::End || end.frame != nullptr ||
      exporter->currentFrame() != 2) {
    return Fail("the exporter did not end after both frames were consumed");
  }
  return 0;
}

EMSCRIPTEN_KEEPALIVE
const char* GetAsyncVideoExportTestError() {
  return testError.c_str();
}

}  // extern "C"
