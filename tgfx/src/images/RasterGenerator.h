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

#include "tgfx/core/Data.h"
#include "tgfx/core/ImageGenerator.h"
#include "tgfx/core/ImageInfo.h"

namespace tgfx {
class RasterGenerator : public ImageGenerator {
 public:
  static std::shared_ptr<ImageGenerator> MakeFrom(const ImageInfo& info,
                                                  std::shared_ptr<Data> pixels);

  bool isAlphaOnly() const override {
    return info.isAlphaOnly();
  }

 protected:
  std::shared_ptr<ImageBuffer> onMakeBuffer(bool tryHardware) const override;

 private:
  ImageInfo info = {};
  std::shared_ptr<Data> pixels = nullptr;

  RasterGenerator(const ImageInfo& info, std::shared_ptr<Data> pixels);
};
}  // namespace tgfx
