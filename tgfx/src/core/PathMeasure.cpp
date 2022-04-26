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

#include "tgfx/core/PathMeasure.h"
#include "core/PathRef.h"

namespace tgfx {
using namespace pk;

class PkPathMeasure : public PathMeasure {
 public:
  explicit PkPathMeasure(const SkPath& path) {
    pathMeasure = new SkPathMeasure(path, false);
  }

  ~PkPathMeasure() override {
    delete pathMeasure;
  }

  float getLength() override {
    return pathMeasure->getLength();
  }

  bool getSegment(float startD, float stopD, Path* result) override {
    if (result == nullptr) {
      return false;
    }
    auto& path = PathRef::WriteAccess(*result);
    return pathMeasure->getSegment(startD, stopD, &path, true);
  }

 private:
  SkPathMeasure* pathMeasure = nullptr;
};

std::unique_ptr<PathMeasure> PathMeasure::MakeFrom(const Path& path) {
  return std::make_unique<PkPathMeasure>(PathRef::ReadAccess(path));
}
}  // namespace tgfx