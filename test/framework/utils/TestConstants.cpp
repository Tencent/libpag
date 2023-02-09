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

#include "TestConstants.h"

namespace pag {
const std::string TestConstants::ASSETS_ROOT = "../assets/";
const std::string TestConstants::RESOURCES_ROOT = "../resources/";
const std::string TestConstants::BASELINE_ROOT = "../test/baseline/";
const std::string TestConstants::BASELINE_VERSION_PATH =
    TestConstants::BASELINE_ROOT + "version.json";
const std::string TestConstants::CACHE_MD5_PATH = TestConstants::BASELINE_ROOT + ".cache/md5.json";
const std::string TestConstants::CACHE_VERSION_PATH =
    TestConstants::BASELINE_ROOT + ".cache/version.json";
const std::string TestConstants::OUT_ROOT = "../test/out/";
const std::string TestConstants::OUT_MD5_PATH = TestConstants::OUT_ROOT + "md5.json";
const std::string TestConstants::OUT_VERSION_PATH = TestConstants::OUT_ROOT + "version.json";
const std::string TestConstants::HEAD_PATH = "./HEAD";
const std::string TestConstants::WEBP_FILE_EXT = ".webp";
const std::string TestConstants::PAG_FILE_EXT = ".pag";
const std::string TestConstants::DEFAULT_PAG_PATH = RESOURCES_ROOT + "apitest/test.pag";
}  // namespace pag