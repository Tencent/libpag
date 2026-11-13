/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ostream>
#include <sstream>
#include <string>
#include "tgfx/core/Bitmap.h"

namespace pag {

/** Returns the CLI test output directory shared by native test translation units. */
std::string TempDir();

/** Resolves a fixture name under resources/cli. */
std::string TestResourcePath(const std::string& name);

/** Reads an entire UTF-8 or byte-oriented test fixture into a string. */
std::string ReadTextFile(const std::string& path);

/** Renders the shared animated-timeline fixture at a time in seconds. */
tgfx::Bitmap RenderAnimatedTimelinesAt(const std::string& timeSeconds,
                                       bool opaqueBackground = false);

/** Captures one output stream for the lifetime of the object and restores it on destruction. */
class StreamCapture {
 public:
  explicit StreamCapture(std::ostream& stream) : stream_(stream), oldBuf_(stream.rdbuf()) {
    stream_.rdbuf(captured_.rdbuf());
  }

  ~StreamCapture() {
    stream_.rdbuf(oldBuf_);
  }

  StreamCapture(const StreamCapture&) = delete;
  StreamCapture& operator=(const StreamCapture&) = delete;

  /** Returns all output captured so far. */
  std::string str() const {
    return captured_.str();
  }

 private:
  std::ostream& stream_;
  std::streambuf* oldBuf_;
  std::stringstream captured_;
};

}  // namespace pag
