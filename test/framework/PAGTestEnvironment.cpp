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
#include <dirent.h>
#include <fstream>
#include "ffavc.h"
#include "pag_test.h"

namespace pag {
nlohmann::json PAGTestEnvironment::CompareJson;
nlohmann::json PAGTestEnvironment::DumpJson;

PAGTestEnvironment::~PAGTestEnvironment() {
}

static void RegisterDefaultFonts(const std::string& defaultFontPath) {
  static bool status = true;
  if (status) {
    std::vector<std::string> fallbackList = {};
    DIR* dir = opendir(defaultFontPath.c_str());
    if (dir != nullptr) {
      struct dirent* ent;
      while ((ent = readdir(dir)) != nullptr) {
        if (std::strstr(ent->d_name, ".ttf") || std::strstr(ent->d_name, ".ttc")) {
          std::string fontName = ent->d_name;
          std::string fontFullPath = defaultFontPath;
          fontFullPath += "/" + fontName;
          fallbackList.push_back(fontFullPath);
        }
      }
      closedir(dir);
    }

    if (fallbackList.size() > 0) {
      std::vector<int> ttcIndices(fallbackList.size());
      pag::PAGFont::SetFallbackFontPaths(fallbackList, ttcIndices);
    }

    status = false;
  }
}

static void RegisterSoftwareDecoder() {
  auto factory = ffavc::DecoderFactory::GetHandle();
  pag::PAGVideoDecoder::RegisterSoftwareDecoderFactory(
      reinterpret_cast<pag::SoftwareDecoderFactory*>(factory));
}

void PAGTestEnvironment::SetUp() {
  //#ifdef  __APPLE__
  //  std::vector<std::string> fallbackList = {"PingFang SC", "Apple Color Emoji"};
  //  FontManager::SetFallbackFontNames(fallbackList);
  //#else
  RegisterSoftwareDecoder();
  RegisterDefaultFonts("../test/res/font");
  //#endif

#ifdef COMPARE_JSON_PATH
  std::ifstream inputFile(COMPARE_JSON_PATH);
  if (!inputFile) {
    std::cout << "open " << COMPARE_JSON_PATH << " fail";
    CompareJson = {};
  } else {
    inputFile >> CompareJson;
  }
#endif
}

void PAGTestEnvironment::TearDown() {
  std::ofstream outFile(DUMP_JSON_PATH);
  outFile << std::setw(4) << DumpJson << std::endl;
  outFile.close();
}

}  // namespace pag
