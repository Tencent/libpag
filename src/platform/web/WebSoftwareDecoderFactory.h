/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include <emscripten/val.h>
#include "pag/decoder.h"

namespace pag {
class WebSoftwareDecoderFactory : public SoftwareDecoderFactory {
 public:
  static std::unique_ptr<WebSoftwareDecoderFactory> Make(emscripten::val factory);

  std::unique_ptr<SoftwareDecoder> createSoftwareDecoder() override;

 private:
  explicit WebSoftwareDecoderFactory(emscripten::val factory) : factory(std::move(factory)) {
  }

  emscripten::val factory = emscripten::val::null();
};
}  // namespace pag
