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

namespace tgfx {
/*
 * Manually set a Java virtual machine which will be used by tgfx to retrieve the JNI environment.
 * Once a Java VM is set it cannot be changed afterwards, meaning you can call multiple times
 * SetJavaVM() with the same Java VM pointer however it will error out if you try to set a different
 * Java VM. Returns true on success, false otherwise.
 */
bool SetJavaVM(void* javaVM);
}  // namespace tgfx
