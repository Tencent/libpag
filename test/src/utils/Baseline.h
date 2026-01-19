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

#include "nlohmann/json.hpp"
#include "pag/pag.h"
#include "tgfx/core/Pixmap.h"

namespace pag {
class Baseline {
 public:
  static bool Compare(std::shared_ptr<tgfx::Surface> surface, const std::string& key);

  static bool Compare(const tgfx::Bitmap& bitmap, const std::string& key);

  static bool Compare(const tgfx::Pixmap& pixmap, const std::string& key);

  static bool Compare(const std::shared_ptr<PAGSurface>& surface, const std::string& key);

  static bool Compare(const std::shared_ptr<ByteData>& byteData, const std::string& key);

  Baseline(const std::string& baselinePath, const std::string& cachePath,
           const std::string& outputPath, const std::string& prefix = "");

  bool compare(std::shared_ptr<tgfx::Surface> surface, const std::string& key);

  bool compare(const tgfx::Bitmap& bitmap, const std::string& key);

  bool compare(const tgfx::Pixmap& pixmap, const std::string& key);

  bool compare(const std::shared_ptr<PAGSurface>& surface, const std::string& key);

  bool compare(const std::shared_ptr<ByteData>& byteData, const std::string& key);

  void saveData();

 private:
  std::mutex locker = {};
  std::string currentVersion;
  nlohmann::json baselineVersions = {};
  nlohmann::json cacheVersions = {};
  nlohmann::json outputVersions = {};
  nlohmann::json cacheMD5 = {};
  nlohmann::json outputMD5 = {};
  std::string baselineVersionPath;
  std::string cacheVersionPath;
  std::string outVersionPath;
  std::string cacheMD5Path;
  std::string outMD5Path;

  bool compareMd5(const std::string& key, const std::string& md5);
  std::string getJSONValue(nlohmann::json& target, const std::string& key);
  void setJSONValue(nlohmann::json& target, const std::string& key, const std::string& value);
#ifdef GENERATE_BASELINE_IMAGES
  bool tryCopyExistingBaseline(const std::string& key, const std::string& md5);
#endif

  static void SetUp();

  static void TearDown();

  friend class TestEnvironment;
};
}  // namespace pag
