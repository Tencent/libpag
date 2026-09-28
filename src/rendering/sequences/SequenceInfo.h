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

#pragma once

#include "pag/file.h"
#include "rendering/sequences/SequenceReader.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/ImageGenerator.h"

namespace pag {
class SequenceInfo {
 public:
  static std::shared_ptr<SequenceInfo> Make(Sequence* sequence);

  virtual ~SequenceInfo() = default;

  virtual std::shared_ptr<SequenceReader> makeReader(std::shared_ptr<File> file,
                                                     PAGFile* pagFile = nullptr,
                                                     bool useDiskCache = false);

  virtual std::shared_ptr<tgfx::Image> makeStaticImage(
      std::shared_ptr<File> file, bool useDiskCache,
      std::shared_ptr<SequenceReadResult> result = nullptr);
  virtual std::shared_ptr<tgfx::Image> makeFrameImage(std::shared_ptr<SequenceReader> reader,
                                                      Frame targetFrame, bool useDiskCache,
                                                      std::shared_ptr<SequenceReadResult> result);

  virtual bool staticContent() const;
  virtual ID uniqueID() const;
  virtual int width() const;
  virtual int height() const;
  virtual Frame duration() const;
  virtual bool isVideo() const;
  virtual Frame firstVisibleFrame(const Layer* layer) const;

 public:
  std::weak_ptr<SequenceInfo> weakThis;

 protected:
  explicit SequenceInfo(Sequence* sequence);

 private:
  Sequence* sequence;
};

class StaticSequenceGenerator : public tgfx::ImageGenerator {
 public:
  StaticSequenceGenerator(std::shared_ptr<File> file, std::shared_ptr<SequenceInfo> info, int width,
                          int height, bool useDiskCache, std::shared_ptr<SequenceReadResult> result)
      : tgfx::ImageGenerator(width, height), file(std::move(file)), info(info),
        useDiskCache(useDiskCache), result(std::move(result)) {
  }

  bool isAlphaOnly() const override {
    return false;
  }

#ifdef PAG_BUILD_FOR_WEB
  bool asyncSupport() const override {
    return false;
  }
#endif

 protected:
  std::shared_ptr<tgfx::ImageBuffer> onMakeBuffer(bool) const override {
    auto reader = info->makeReader(file, nullptr, useDiskCache);
    if (reader == nullptr) {
      if (result != nullptr) {
        result->status.store(SequenceReadStatus::Failed, std::memory_order_release);
      }
      return nullptr;
    }
    return reader->readBuffer(0, result);
  }

 private:
  std::shared_ptr<File> file = nullptr;
  std::shared_ptr<SequenceInfo> info = nullptr;
  bool useDiskCache = false;
  std::shared_ptr<SequenceReadResult> result = nullptr;
};

class SequenceFrameGenerator : public tgfx::ImageGenerator {
 public:
  SequenceFrameGenerator(std::shared_ptr<SequenceReader> reader, Frame targetFrame,
                         std::shared_ptr<SequenceReadResult> result)
      : tgfx::ImageGenerator(reader->width(), reader->height()), reader(std::move(reader)),
        targetFrame(targetFrame), result(std::move(result)) {
  }

  bool isAlphaOnly() const override {
    return false;
  }

#ifdef PAG_BUILD_FOR_WEB
  bool asyncSupport() const override {
    return false;
  }
#endif

 protected:
  std::shared_ptr<tgfx::ImageBuffer> onMakeBuffer(bool) const override {
    return reader->readBuffer(targetFrame, result);
  }

 private:
  std::shared_ptr<SequenceReader> reader = nullptr;
  Frame targetFrame = 0;
  std::shared_ptr<SequenceReadResult> result = nullptr;
};
}  // namespace pag
