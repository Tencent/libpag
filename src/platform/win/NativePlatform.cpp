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
#include <Windows.h>
#include <shlobj_core.h>
#include <codecvt>
#include <locale>
#include <vector>
#include "pag/pag.h"

#ifdef PAG_USE_WIN_HARDWARE_DECODER
#include "HardwareDecoder.h"
#endif

namespace pag {

#ifdef PAG_USE_WIN_HARDWARE_DECODER
class HardwareDecoderFactory : public VideoDecoderFactory {
 public:
  bool isHardwareBacked() const override {
    return true;
  }

 protected:
  std::unique_ptr<VideoDecoder> onCreateDecoder(const VideoFormat& format) const override {
    auto decoder = new HardwareDecoder(format);
    if (!decoder->isValid) {
      delete decoder;
      return nullptr;
    }
    return std::unique_ptr<VideoDecoder>(decoder);
  }
};

static HardwareDecoderFactory hardwareDecoderFactory = {};
#endif

const Platform* Platform::Current() {
  static const NativePlatform platform = {};
  return &platform;
}

std::vector<const VideoDecoderFactory*> NativePlatform::getVideoDecoderFactories() const {
#ifdef PAG_USE_WIN_HARDWARE_DECODER
  return {&hardwareDecoderFactory, VideoDecoderFactory::ExternalDecoderFactory(),
          VideoDecoderFactory::SoftwareAVCDecoderFactory()};
#else
  return {VideoDecoderFactory::ExternalDecoderFactory(),
          VideoDecoderFactory::SoftwareAVCDecoderFactory()};
#endif
}

bool NativePlatform::registerFallbackFonts() const {
  std::vector<std::string> fallbackList = {
      "Microsoft YaHei", "Times New Roman", "Microsoft Sans Serif", "Microsoft JhengHei",
      "Leelawadee UI",   "MS Gothic",       "Malgun Gothic",        "STSong"};
  PAGFont::SetFallbackFontNames(fallbackList);
  return true;
}

std::string GetAppName() {
  char appPath[MAX_PATH];
  GetModuleFileName(NULL, appPath, MAX_PATH);
  std::string fullPath(appPath);
  std::string appName;
  size_t pos = fullPath.find_last_of("\\/");
  if (pos != std::wstring::npos) {
    appName = fullPath.substr(pos + 1);
  } else {
    appName = fullPath;
  }
  pos = appName.find_last_of(".");
  if (pos != std::string::npos) {
    appName = appName.substr(0, pos);
  }
  return appName;
}

std::string GetAppDataDir() {
  LPWSTR appDir;
  // Copy the folder path from the Qt source code.
  if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DONT_VERIFY, 0, &appDir))) {
    return "";
  }
  std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
  auto result = converter.to_bytes(appDir);
  CoTaskMemFree(appDir);
  return result;
}

std::string NativePlatform::getCacheDir() const {
  auto result = GetAppDataDir();
  if (result.empty()) {
    return "";
  }
  std::string appName = GetAppName();
  if (appName.empty()) {
    return "";
  }
  result.append("\\" + appName + "\\cache");
  return result;
}

}  // namespace pag
