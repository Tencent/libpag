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

#include "NativePlatform.h"
#include <QStandardPaths>
#include <vector>
#include "pag/pag.h"
#ifdef __APPLE__
#include "platform/mac/private/HardwareDecoder.h"
#endif

namespace pag {

const Platform* Platform::Current() {
  static const NativePlatform platform = {};
  return &platform;
}

bool NativePlatform::registerFallbackFonts() const {
  std::vector<std::string> fallbackList;
#ifdef __APPLE__
  fallbackList = {"PingFang SC",       "Apple SD Gothic Neo",
                  "Apple Color Emoji", "Helvetica",
                  "Myanmar Sangam MN", "Thonburi",
                  "Mishafi",           "Menlo",
                  "Kailasa",           "Kefa",
                  "Kohinoor Telugu",   "Hiragino Maru Gothic ProN"};
#else
  fallbackList = {"Microsoft YaHei",    "Times New Roman", "Microsoft Sans Serif",
                  "Microsoft JhengHei", "Leelawadee UI",   "MS Gothic",
                  "Malgun Gothic",      "STSong"};
#endif
  PAGFont::SetFallbackFontNames(fallbackList);
  return true;
}

std::vector<const VideoDecoderFactory*> NativePlatform::getVideoDecoderFactories() const {
#ifdef __APPLE__
  return {HardwareDecoder::Factory(), VideoDecoderFactory::ExternalDecoderFactory(),
          VideoDecoderFactory::SoftwareAVCDecoderFactory()};
#else
  return Platform::getVideoDecoderFactories();
#endif
}

NALUType NativePlatform::naluType() const {
#ifdef __APPLE__
  return NALUType::AVCC;
#else
  return NALUType::AnnexB;
#endif
}

std::string NativePlatform::getCacheDir() const {
  auto cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  return cacheDir.toStdString();
}

}  // namespace pag
