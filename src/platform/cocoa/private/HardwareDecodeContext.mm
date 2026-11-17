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

#include "HardwareDecodeContext.h"

namespace pag {

namespace {
// Process-wide holder for contexts still pending when their decoder tears down its session. A late
// VideoToolbox callback can land on such a context safely because it is never deleted while
// pending; any decoder's reclaim sweep deletes the ones that have since resolved.
std::mutex& GraveyardMutex() {
  static std::mutex mutex;
  return mutex;
}

std::vector<VTDecodeOutputContext*>& Graveyard() {
  static std::vector<VTDecodeOutputContext*> graveyard;
  return graveyard;
}

void SweepGraveyardLocked() {
  auto& graveyard = Graveyard();
  for (auto item = graveyard.begin(); item != graveyard.end();) {
    if ((*item)->resolved()) {
      delete *item;
      item = graveyard.erase(item);
    } else {
      ++item;
    }
  }
}
}  // namespace

VTDecodeOutputContext::~VTDecodeOutputContext() {
  if (outputPixelBuffer != nullptr) {
    CVPixelBufferRelease(outputPixelBuffer);
  }
}

void VTDecodeOutputContext::deliverBuffer(CVPixelBufferRef buffer) {
  std::lock_guard<std::mutex> lock(locker);
  outputPixelBuffer = buffer;
  callbackDone = true;
}

CVPixelBufferRef VTDecodeOutputContext::takeBuffer() {
  std::lock_guard<std::mutex> lock(locker);
  consumerDone = true;
  auto result = outputPixelBuffer;
  outputPixelBuffer = nullptr;
  return result;
}

void VTDecodeOutputContext::abandon() {
  std::lock_guard<std::mutex> lock(locker);
  callbackNotExpected = true;
}

bool VTDecodeOutputContext::resolved() {
  std::lock_guard<std::mutex> lock(locker);
  return consumerDone && (callbackDone || callbackNotExpected);
}

VTDecodeContextPool::~VTDecodeContextPool() {
  drain();
}

VTDecodeOutputContext* VTDecodeContextPool::create() {
  auto* context = new VTDecodeOutputContext();
  contexts.push_back(context);
  return context;
}

void VTDecodeContextPool::reclaimResolved() {
  for (auto item = contexts.begin(); item != contexts.end();) {
    if ((*item)->resolved()) {
      delete *item;
      item = contexts.erase(item);
    } else {
      ++item;
    }
  }
  std::lock_guard<std::mutex> lock(GraveyardMutex());
  SweepGraveyardLocked();
}

void VTDecodeContextPool::drain() {
  std::lock_guard<std::mutex> lock(GraveyardMutex());
  SweepGraveyardLocked();
  for (auto* context : contexts) {
    if (context->resolved()) {
      delete context;
    } else {
      Graveyard().push_back(context);
    }
  }
  contexts.clear();
}

size_t VTDecodeGraveyardSize() {
  std::lock_guard<std::mutex> lock(GraveyardMutex());
  return Graveyard().size();
}

void DidDecompress(void*, void* sourceFrameRefCon, OSStatus status, VTDecodeInfoFlags,
                   CVImageBufferRef pixelBuffer, CMTime, CMTime) {
  auto* context = static_cast<VTDecodeOutputContext*>(sourceFrameRefCon);
  if (context == nullptr) {
    return;
  }
  CVPixelBufferRef outputPixelBuffer = nullptr;
  if (status == noErr && pixelBuffer) {
    outputPixelBuffer = CVPixelBufferRetain(pixelBuffer);
  }
  context->deliverBuffer(outputPixelBuffer);
}
}  // namespace pag
