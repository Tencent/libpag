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

#include "CachePolicy.h"

namespace pag {
void ReadCachePolicy(DecodeStream* stream, Layer* layer) {
  layer->cachePolicy = static_cast<CachePolicy>(ReadUint8(stream));
}

TagCode WriteCachePolicy(EncodeStream* stream, Layer* layer) {
  WriteUint8(stream, static_cast<uint8_t>(layer->cachePolicy));
  return TagCode::CachePolicy;
}
};  // namespace pag
