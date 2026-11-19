/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "pag/c/pag_disk_cache.h"
#include "pag/pag.h"

void pag_disk_cache_set_cache_dir(const char* dir) {
  pag::PAGDiskCache::SetCacheDir(dir ? dir : "");
}

size_t pag_disk_cache_get_max_disk_size() {
  return pag::PAGDiskCache::MaxDiskSize();
}

void pag_disk_cache_set_max_disk_size(size_t maxDiskSize) {
  pag::PAGDiskCache::SetMaxDiskSize(maxDiskSize);
}

void pag_disk_cache_remove_all() {
  pag::PAGDiskCache::RemoveAll();
}
