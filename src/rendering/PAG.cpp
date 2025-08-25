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

#include "pag/pag.h"

namespace pag {

static const char sdkVersion[] = "4.0.0";

std::string PAG::SDKVersion() {
  return sdkVersion;
}
}  // namespace pag

#ifdef PAG_BUILD_FOR_WEB
// Error: Undefined symbol: main
// Note: A `main` function must be implemented as the entry point for the application.
// Without it, the WebAssembly build will fail. Ensure that you have defined a `main` function
// or correctly specified an alternative entry point during the build process.

int main() {
  return 0;
}

#endif
