# Reads the XSD file and generates a C++ header with the content as a string constant.
# Input variables: XSD_INPUT, HEADER_OUTPUT

file(READ "${XSD_INPUT}" XSD_RAW)

# Use bracket arguments to avoid CMake escaping issues with semicolons and quotes.
set(HEADER_CONTENT [=[
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

// Auto-generated from spec/pagx.xsd. Do not edit manually.

#pragma once

#include <string>

namespace pagx::cli {

inline const std::string& PagxXsdContent() {
  static const std::string content = R"XSD(]=])

string(APPEND HEADER_CONTENT "${XSD_RAW}")

string(APPEND HEADER_CONTENT [=[)XSD";
  return content;
}

}  // namespace pagx::cli
]=])

file(WRITE "${HEADER_OUTPUT}" "${HEADER_CONTENT}")
