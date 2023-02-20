/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <jni.h>

namespace tgfx {
/**
 * JNIEnvironment is a stack-allocated class that provides convenience method to access the JNIEnv
 * pointer of the calling thread. All the JNI local references created with the JNIEnv will be freed
 * automatically when the JNIEnvironment instance goes out of scope.
 */
class JNIEnvironment {
 public:
  /*
   * Sets the global Java virtual machine used by the tgfx runtime to retrieve the JNI environment.
   * The application should call SetJavaVM() in the JNI_OnLoad() function to initialize the Java VM
   * and set it to nullptr in the JNI_OnUnload() function. Once a Java VM is set, it cannot be
   * changed to another non-nullptr value afterward. Returns true on success, false otherwise.
   */
  static bool SetJavaVM(JavaVM* javaVM);

  /**
   * Allocates a JNIEnvironment instance. If the JNIEnv pointer of the calling thread does not
   * exist, permanently attach a JNIEnv to the current thread and retrieve it. If successfully
   * attached, the JNIEnv will automatically be detached at thread destruction.
   */
  JNIEnvironment();

  ~JNIEnvironment();

  /**
   * Returns the JNIEnv pointer of the calling thread.
   */
  JNIEnv* current() const {
    return env;
  }

  void* operator new(size_t) = delete;

 private:
  JNIEnv* env = nullptr;

  static JNIEnv* Current();

  template <typename T>
  friend class Global;
};
}  // namespace tgfx
