/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "DiskSequenceReader.h"
#include <tgfx/core/ImageCodec.h>
#include "base/utils/TGFXCast.h"
#include "platform/Platform.h"
#include "tgfx/core/Buffer.h"
#include "tgfx/core/ImageCodec.h"

namespace pag {

std::shared_ptr<DiskSequenceReader> DiskSequenceReader::Make(std::shared_ptr<File> file,
                                                             Sequence* sequence) {
  if (sequence == nullptr || file == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<DiskSequenceReader>(new DiskSequenceReader(std::move(file), sequence));
}

DiskSequenceReader::DiskSequenceReader(std::shared_ptr<File> file, Sequence* sequence)
    : sequence(sequence), file(file) {
}

DiskSequenceReader::~DiskSequenceReader() {
  if (frontHardWareBuffer) {
    tgfx::HardwareBufferRelease(frontHardWareBuffer);
  }
  if (backHardwareBuffer) {
    tgfx::HardwareBufferRelease(backHardwareBuffer);
  }
}

int DiskSequenceReader::width() const {
  return sequence->composition->width;
}

int DiskSequenceReader::height() const {
  return sequence->composition->height;
}

std::shared_ptr<tgfx::ImageBuffer> DiskSequenceReader::onMakeBuffer(Frame targetFrame) {
  // Need a locker here in case there are other threads are decoding at the same time.
  std::lock_guard<std::mutex> autoLock(locker);
  if (pagDecoder == nullptr) {
    auto root = PAGComposition::Make(sequence->width, sequence->height);
    auto composition = std::make_shared<PAGComposition>(
        file, PreComposeLayer::Wrap(sequence->composition).release());
    composition->setMatrix(
        Matrix::MakeScale(sequence->width * 1.f / sequence->composition->width,
                          sequence->height * 1.f / sequence->composition->height));
    root->addLayer(composition);
    pagDecoder = PAGDecoder::MakeFrom(root, sequence->frameRate);
    pagDecoder->setCacheKeyGeneratorFun(
        [this](PAGDecoder*, std::shared_ptr<PAGComposition>) -> std::string {
          auto cachePath = Platform::Current()->getSandboxPath(file->path);
          if (cachePath.empty()) {
            return "";
          }
          return std::string(cachePath + ".bmp." + std::to_string(sequence->composition->id));
        });
  }
  if (pagDecoder == nullptr) {
    return nullptr;
  }
  if (frontHardWareBuffer == nullptr && pixels == nullptr) {
    if (tgfx::HardwareBufferAvailable()) {
      frontHardWareBuffer =
          tgfx::HardwareBufferAllocate(pagDecoder->width(), pagDecoder->height(), false);
      if (frontHardWareBuffer && !sequence->composition->staticContent()) {
        backHardwareBuffer =
            tgfx::HardwareBufferAllocate(pagDecoder->width(), pagDecoder->height(), false);
      }
    }
    if (frontHardWareBuffer == nullptr) {
      info = tgfx::ImageInfo::Make(pagDecoder->width(), pagDecoder->height(),
                                   tgfx::ColorType::RGBA_8888);
      tgfx::Buffer buffer(info.byteSize());
      buffer.clear();
      pixels = buffer.release();
    }
  }

  if (!pagDecoder->checkFrameChanged(static_cast<int>(targetFrame))) {
    return imageBuffer;
  }
  bool success = false;
  auto renderBuffer = useFrontBuffer ? frontHardWareBuffer : backHardwareBuffer;
  if (frontHardWareBuffer) {
    success = pagDecoder->readFrame(static_cast<int>(targetFrame), renderBuffer);
  } else {
    if (pixels) {
      success =
          pagDecoder->readFrame(static_cast<int>(targetFrame), const_cast<void*>(pixels->data()),
                                info.rowBytes(), ToPAG(info.colorType()), ToPAG(info.alphaType()));
    }
  }
  if (!success) {
    LOGE("DiskSequenceReader: Error on readFrame.\n");
    return nullptr;
  }
  if (frontHardWareBuffer) {
    if (backHardwareBuffer) {
      useFrontBuffer = !useFrontBuffer;
    }
    imageBuffer = tgfx::ImageBuffer::MakeFrom(renderBuffer);
  } else {
    auto codec = tgfx::ImageCodec::MakeFrom(info, pixels);
    imageBuffer = codec ? codec->makeBuffer() : nullptr;
  }
  return imageBuffer;
}

void DiskSequenceReader::onReportPerformance(Performance*, int64_t) {
}

}  // namespace pag
