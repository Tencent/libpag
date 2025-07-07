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

#include "TestEnvironment.h"
#include "ffavc.h"
#include "utils/Baseline.h"
#include "utils/ProjectPath.h"

namespace pag {
static void RegisterSoftwareDecoder() {
  auto factory = ffavc::DecoderFactory::GetHandle();
  PAGVideoDecoder::RegisterSoftwareDecoderFactory(
      reinterpret_cast<pag::SoftwareDecoderFactory*>(factory));
}

void TestEnvironment::SetUp() {
  std::vector<std::string> fontPaths = {
      ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"),
      ProjectPath::Absolute("resources/font/NotoColorEmoji.ttf")};
  std::vector<int> ttcIndices = {0, 0};
  PAGFont::SetFallbackFontPaths(fontPaths, ttcIndices);
  RegisterSoftwareDecoder();
  Baseline::SetUp();
}

void TestEnvironment::TearDown() {
  Baseline::TearDown();
}

}  // namespace pag
