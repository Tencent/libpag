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

#include "PAGTestEnvironment.h"
#include <fstream>
#include "ffavc.h"
#include "pag_test.h"

namespace pag {
PAGTestEnvironment::~PAGTestEnvironment() {
}

static void RegisterSoftwareDecoder() {
  auto factory = ffavc::DecoderFactory::GetHandle();
  pag::PAGVideoDecoder::RegisterSoftwareDecoderFactory(
      reinterpret_cast<pag::SoftwareDecoderFactory*>(factory));
}

void PAGTestEnvironment::SetUp() {
  std::vector<std::string> fontPaths = {"../resources/font/NotoSansSC-Regular.otf",
                                        "../resources/font/NotoColorEmoji.ttf"};
  std::vector<int> ttcIndices = {0, 0};
  pag::PAGFont::SetFallbackFontPaths(fontPaths, ttcIndices);
  RegisterSoftwareDecoder();
}

void PAGTestEnvironment::TearDown() {

}

}  // namespace pag
