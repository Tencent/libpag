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

#pragma once

#include "gpu/GpuPaint.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/Stroke.h"
#include "tgfx/core/TextBlob.h"

namespace tgfx {
class PathProxy {
 public:
  static std::unique_ptr<PathProxy> MakeFromFill(const Path& path);

  static std::unique_ptr<PathProxy> MakeFromFill(std::shared_ptr<TextBlob> textBlob);

  static std::unique_ptr<PathProxy> MakeFromStroke(std::shared_ptr<TextBlob> textBlob,
                                                   const Stroke& stroke);

  virtual ~PathProxy() = default;

  virtual Rect getBounds(float scale) const = 0;

  virtual Path getPath(float scale) const = 0;
};
}  // namespace tgfx
