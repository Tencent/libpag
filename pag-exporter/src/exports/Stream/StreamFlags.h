/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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
#ifndef STREAMFLAGS_H
#define STREAMFLAGS_H
#include "src/exports/PAGDataTypes.h"

inline bool IsStreamHidden(const AEGP_SuiteHandler& suites, AEGP_StreamRefH stream) {
  AEGP_DynStreamFlags flags;
  suites.DynamicStreamSuite4()->AEGP_GetDynamicStreamFlags(stream, &flags);
  return static_cast<bool>(flags & AEGP_DynStreamFlag_HIDDEN);
}

inline bool IsStreamActive(const AEGP_SuiteHandler& suites, AEGP_StreamRefH stream) {
  AEGP_DynStreamFlags streamFlags;
  suites.DynamicStreamSuite4()->AEGP_GetDynamicStreamFlags(stream, &streamFlags);
  return (streamFlags & AEGP_DynStreamFlag_ACTIVE_EYEBALL) > 0;
}
#endif //STREAMFLAGS_H
