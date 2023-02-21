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
#include <string>

namespace pag {
class TestConstants {
 public:
  static const std::string PAG_ROOT;
  static const std::string BASELINE_ROOT;
  static const std::string BASELINE_VERSION_PATH;
  static const std::string CACHE_MD5_PATH;
  static const std::string CACHE_VERSION_PATH;
  static const std::string OUT_ROOT;
  static const std::string OUT_VERSION_PATH;
  static const std::string OUT_MD5_PATH;
  static const std::string HEAD_PATH;
  static const std::string WEBP_FILE_EXT;
  static const std::string PAG_FILE_EXT;
  static const std::string DEFAULT_PAG_PATH;
};

}  // namespace pag
