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

#include "EAGLProcGetter.h"
#include <dlfcn.h>

namespace tgfx {
EAGLProcGetter::EAGLProcGetter() {
  fLibrary = dlopen("/System/Library/Frameworks/OpenGLES.framework/OpenGLES", RTLD_LAZY);
}

EAGLProcGetter::~EAGLProcGetter() {
  if (fLibrary) {
    dlclose(fLibrary);
  }
}

void* EAGLProcGetter::getProcAddress(const char* name) const {
  auto handle = fLibrary ? fLibrary : RTLD_DEFAULT;
  return dlsym(handle, name);
}

std::unique_ptr<GLProcGetter> GLProcGetter::Make() {
  return std::make_unique<EAGLProcGetter>();
}
}  // namespace tgfx
