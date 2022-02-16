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

#include <list>
#include <unordered_map>
#include "Program.h"

namespace tgfx {
/**
 * Manages the lifetime of all Program instances.
 */
class ProgramCache {
 public:
  explicit ProgramCache(Context* context);

  /**
   * Returns true if there is no cache at all.
   */
  bool empty() const;

  /**
   * Returns a program cache of specified ProgramMaker. If there is no associated cache available,
   * a new program will be created by programMaker. Returns null if the programMaker fails to make a
   * new program.
   */
  Program* getProgram(const ProgramCreator* programMaker);

 private:
  Context* context = nullptr;
  std::list<Program*> programLRU = {};
  std::unordered_map<BytesKey, Program*, BytesHasher> programMap = {};

  void removeOldestProgram(bool releaseGPU = true);
  void releaseAll(bool releaseGPU);

  friend class Context;
};
}  // namespace tgfx
