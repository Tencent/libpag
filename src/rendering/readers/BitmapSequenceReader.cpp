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

#include "BitmapSequenceReader.h"
#include "BitmapDecodingTask.h"
#include "rendering/caches/RenderCache.h"
#include "rendering/graphics/Picture.h"

namespace pag {
BitmapSequenceReader::BitmapSequenceReader(std::shared_ptr<File> file, BitmapSequence* sequence)
    : SequenceReader(std::move(file), sequence) {
  // 若内容非静态，强制使用非 hardware 的 Bitmap，否则纹理内容跟 bitmap
  // 内存是共享的，无法进行解码预测。
  if (bitmap.allocPixels(sequence->width, sequence->height, false, staticContent)) {
    // 必须清零，否则首帧是空帧的情况会绘制错误。
    bitmap.eraseAll();
  }
}
void BitmapSequenceReader::decodeFrame(Frame targetFrame) {
  // decodeBitmap 这里需要立即加锁，防止异步解码时线程冲突。
  std::lock_guard<std::mutex> autoLock(locker);
  if (lastDecodeFrame == targetFrame || bitmap.isEmpty()) {
    return;
  }
  auto startFrame = findStartFrame(targetFrame);
  auto& bitmapFrames = static_cast<BitmapSequence*>(sequence)->frames;
  BitmapLock bitmapLock(bitmap);
  auto pixels = bitmapLock.pixels();
  for (Frame frame = startFrame; frame <= targetFrame; frame++) {
    auto bitmapFrame = bitmapFrames[frame];
    auto firstRead = true;
    for (auto bitmapRect : bitmapFrame->bitmaps) {
      auto imageBytes =
          Data::MakeWithoutCopy(bitmapRect->fileBytes->data(), bitmapRect->fileBytes->length());
      auto image = Image::MakeFrom(imageBytes);
      if (image != nullptr) {
        // 关键帧不是全屏的时候要清屏
        if (firstRead && bitmapFrame->isKeyframe &&
            !(image->width() == bitmap.width() && image->height() == bitmap.height())) {
          memset(pixels, 0, bitmap.byteSize());
        }
        auto offset = bitmap.rowBytes() * bitmapRect->y + bitmapRect->x * 4;
        image->readPixels(bitmap.info(), reinterpret_cast<uint8_t*>(pixels) + offset);
        firstRead = false;
      }
    }
    lastDecodeFrame = targetFrame;
  }
}

Frame BitmapSequenceReader::findStartFrame(Frame targetFrame) {
  Frame startFrame = 0;
  auto& bitmapFrames = static_cast<BitmapSequence*>(sequence)->frames;
  for (Frame frame = targetFrame; frame >= 0; frame--) {
    if (frame == lastDecodeFrame + 1 || bitmapFrames[static_cast<size_t>(frame)]->isKeyframe) {
      startFrame = frame;
      break;
    }
  }
  return startFrame;
}

void BitmapSequenceReader::prepareAsync(Frame targetFrame) {
  if (lastTask != nullptr) {
    if (lastTextureFrame != -1) {
      pendingFrame = targetFrame;
    }
  } else {
    lastTask = BitmapDecodingTask::MakeAndRun(this, targetFrame);
  }
}

std::shared_ptr<Texture> BitmapSequenceReader::readTexture(Frame targetFrame, RenderCache* cache) {
  if (lastTextureFrame == targetFrame || bitmap.isEmpty()) {
    return lastTexture;
  }
  auto startTime = GetTimer();
  // 必须提前置空，否则会出现对 Bitmap 写入的竞争导致内容不一致，或者死锁，析构会触发 cancel(),
  // 可能会阻塞当前线程。
  lastTask = nullptr;
  decodeFrame(targetFrame);
  cache->imageDecodingTime += GetTimer() - startTime;
  lastTexture = nullptr;  // 先释放上一次的 Texture，允许在 Context 里复用。
  startTime = GetTimer();
  lastTexture = bitmap.makeTexture(cache->getContext());
  lastTextureFrame = targetFrame;
  cache->textureUploadingTime += GetTimer() - startTime;
  if (!staticContent) {
    auto nextFrame = targetFrame + 1;
    if (nextFrame >= sequence->duration() && pendingFrame >= 0) {
      nextFrame = pendingFrame;
      pendingFrame = -1;
    }
    if (nextFrame < sequence->duration()) {
      lastTask = BitmapDecodingTask::MakeAndRun(this, nextFrame);
    }
  }
  return lastTexture;
}
}  // namespace pag
