/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 THL A29 Limited, a Tencent company. All rights reserved.
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
#include <napi/native_api.h>
#include <string>
#include "pag/pag.h"
#include "pag/types.h"

#define PAG_DEFAULT_METHOD_ENTRY(name, func) \
  { #name, nullptr, func, nullptr, nullptr, nullptr, napi_default, nullptr }
#define PAG_STATIC_METHOD_ENTRY(name, func) \
  { #name, nullptr, func, nullptr, nullptr, nullptr, napi_static, nullptr }

namespace pag {

/**
 * Defines the rules on how to scale the content to fit the specified area.
 */
class PAG_API PAGAnimatorState {
 public:
  static const Enum Start = 0;
  static const Enum Cancel = 1;
  static const Enum End = 2;
  static const Enum Repeat = 3;
};

napi_status DefineClass(napi_env env, napi_value exports, const std::string& utf8name,
                        size_t propertyCount, const napi_property_descriptor* properties,
                        napi_callback constructor, const std::string& parentName);

napi_value GetConstructor(napi_env env, const std::string& name);

napi_value NewInstance(napi_env env, const std::string& name, void* handler);

napi_value CreateMarkers(napi_env env, const std::vector<const Marker*>& markers);

napi_value CreateRect(napi_env env, const Rect& rect);

Rect GetRect(napi_env env, napi_value value);

napi_value CreateMatrix(napi_env env, const Matrix& matrix);

Matrix GetMatrix(napi_env env, napi_value value);

napi_value MakeSnapshot(napi_env env, PAGSurface* surface);

int MakeColorInt(uint32_t red, uint32_t green, uint32_t blue);

Color ToColor(int value);

napi_value CreateVideoRanges(napi_env env, const std::vector<PAGVideoRange>& videoRanges);

}  // namespace pag
