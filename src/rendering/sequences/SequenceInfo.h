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

#pragma once

#include "pag/file.h"
#include "rendering/sequences/SequenceReader.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/ImageGenerator.h"
#include "tgfx/core/Orientation.h"

namespace pag {
class SequenceInfo {
 public:
  static std::shared_ptr<SequenceInfo> Make(Sequence* sequence);

  virtual ~SequenceInfo() = default;

  virtual std::shared_ptr<SequenceReader> makeReader(std::shared_ptr<File> file,
                                                     PAGFile* pagFile = nullptr);

  virtual std::shared_ptr<tgfx::Image> makeStatic(std::shared_ptr<File> file);
  virtual std::shared_ptr<tgfx::Image> makeImage(std::shared_ptr<SequenceReader> reader,
                                                 Frame targetFrame);

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
  virtual tgfx::Orientation orientation() const;

 private:
  Sequence* sequence;
};
}  // namespace pag
