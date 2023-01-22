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

#include "pathkit.h"

namespace tgfx {
// https://chromium-review.googlesource.com/c/chromium/src/+/1099564/
static constexpr int AA_TESSELLATOR_MAX_VERB_COUNT = 100;
// A factor used to estimate the memory size of a tessellated path, based on the average value of
// Buffer.size() / Path.countPoints() from 4300+ tessellated path data.
static constexpr int AA_TESSELLATOR_BUFFER_SIZE_FACTOR = 170;

class Path;

struct Rect;

class PathRef {
 public:
  static const pk::SkPath& ReadAccess(const Path& path);

  static pk::SkPath& WriteAccess(Path& path);

  static int ToAATriangles(const Path& path, const Rect& clipBounds, std::vector<float>* vertices);

  PathRef() = default;

  explicit PathRef(const pk::SkPath& path) : path(path) {
  }

 private:
  pk::SkPath path = {};

  friend class Path;
  friend bool operator==(const Path& a, const Path& b);
  friend bool operator!=(const Path& a, const Path& b);
};
}  // namespace tgfx
