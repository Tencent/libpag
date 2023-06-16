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

#include "DiskSequenceReader.h"
#include "base/utils/TGFXCast.h"
#include "platform/Platform.h"

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

int DiskSequenceReader::width() const {
  return sequence->composition->width;
}

int DiskSequenceReader::height() const {
  return sequence->composition->height;
}

std::shared_ptr<tgfx::ImageBuffer> DiskSequenceReader::onMakeBuffer(Frame targetFrame) {
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
  if (bitmap == nullptr) {
    bitmap = std::make_shared<tgfx::Bitmap>(pagDecoder->width(), pagDecoder->height(), false, true);
  }
  if (!pagDecoder->checkFrameChanged(targetFrame)) {
    return bitmap->makeBuffer();
  }
  bool success = false;
  if (bitmap->isHardwareBacked()) {
    success = pagDecoder->readFrame(targetFrame, bitmap->getHardwareBuffer());
  } else {
    auto pixels = bitmap->lockPixels();
    if (pixels) {
      success = pagDecoder->readFrame(targetFrame, pixels, bitmap->info().rowBytes(),
                                      ToPAG(bitmap->info().colorType()),
                                      ToPAG(bitmap->info().alphaType()));
      bitmap->unlockPixels();
    }
  }
  if (!success) {
    LOGE("DiskSequenceReader: Error on readFrame.\n");
    return nullptr;
  }
  return bitmap->makeBuffer();
}

void DiskSequenceReader::onReportPerformance(Performance*, int64_t) {
}

}  // namespace pag
