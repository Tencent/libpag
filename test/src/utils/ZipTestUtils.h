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
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <unordered_map>

namespace pagx {
class Data;
}

namespace pag {

// Extracts every entry from an in-memory ZIP. Reading through minizip validates
// the central directory and each entry's CRC in addition to returning its data.
bool ExtractZipEntries(const pagx::Data* data,
                       std::unordered_map<std::string, std::string>* entries,
                       std::string* errorMsg = nullptr);

}  // namespace pag
