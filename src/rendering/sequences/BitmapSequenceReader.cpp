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
#include "core/Image.h"

namespace pag {
BitmapSequenceReader::BitmapSequenceReader(std::shared_ptr<File> file, BitmapSequence* sequence)
    : file(std::move(file)), sequence(sequence) {
  // Force allocating raster PixelBuffer when staticContent is false, otherwise the asynchronous
  // decoding will fail due to the GPU memory sharing.
  pixelBuffer = tgfx::PixelBuffer::Make(sequence->width, sequence->height, false, staticContent());
  tgfx::Bitmap(pixelBuffer).eraseAll();
}

BitmapSequenceReader::~BitmapSequenceReader() {
  lastTask = nullptr;
}

int64_t BitmapSequenceReader::getNextFrameAt(int64_t targetFrame) {
  auto nextFrame = targetFrame + 1;
  return nextFrame >= sequence->duration() ? -1 : nextFrame;
}

bool BitmapSequenceReader::decodeFrame(int64_t targetFrame) {
  // a locker is required here because decodeFrame() could be called from multiple threads.
  std::lock_guard<std::mutex> autoLock(locker);
  if (lastDecodeFrame == targetFrame) {
    return true;
  }
  if (pixelBuffer == nullptr) {
    return false;
  }
  lastDecodeFrame = -1;
  auto startFrame = findStartFrame(targetFrame);
  auto& bitmapFrames = static_cast<BitmapSequence*>(sequence)->frames;
  tgfx::Bitmap bitmap(pixelBuffer);
  for (Frame frame = startFrame; frame <= targetFrame; frame++) {
    auto bitmapFrame = bitmapFrames[frame];
    auto firstRead = true;
    for (auto bitmapRect : bitmapFrame->bitmaps) {
      auto imageBytes = tgfx::Data::MakeWithoutCopy(bitmapRect->fileBytes->data(),
                                                    bitmapRect->fileBytes->length());
      auto image = tgfx::Image::MakeFrom(imageBytes);
      if (image == nullptr) {
        return false;
      }
      if (firstRead && bitmapFrame->isKeyframe &&
          !(image->width() == bitmap.width() && image->height() == bitmap.height())) {
        // clear the whole screen if the size of the key frame is smaller than the screen.
        bitmap.eraseAll();
      }
      auto offset = bitmap.rowBytes() * bitmapRect->y + bitmapRect->x * 4;
      auto result = image->readPixels(bitmap.info(),
                                      reinterpret_cast<uint8_t*>(bitmap.writablePixels()) + offset);
      if (!result) {
        return false;
      }
      firstRead = false;
    }
  }
  lastDecodeFrame = targetFrame;
  return true;
}

std::shared_ptr<tgfx::Texture> BitmapSequenceReader::makeTexture(tgfx::Context* context) {
  if (lastDecodeFrame == -1 || pixelBuffer == nullptr) {
    return nullptr;
  }
  return pixelBuffer->makeTexture(context);
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

void BitmapSequenceReader::recordPerformance(Performance* performance, int64_t decodingTime) const {
  performance->imageDecodingTime += decodingTime;
}
}  // namespace pag
