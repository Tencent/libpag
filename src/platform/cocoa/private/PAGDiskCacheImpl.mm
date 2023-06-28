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

#import "PAGDiskCacheImpl.h"
#import "pag/pag.h"
#include "rendering/caches/DiskCache.h"

@implementation PAGDiskCacheImpl

+ (size_t)MaxDiskSize {
  return pag::PAGDiskCache::MaxDiskSize();
}

+ (void)SetMaxDiskSize:(size_t)size {
  pag::PAGDiskCache::SetMaxDiskSize(size);
}

+ (void)RemoveAll {
  pag::PAGDiskCache::RemoveAll();
}

+ (NSString*)GetFilePath:(NSString*)key {
  if (key == nil) {
    return nil;
  }
  std::string path = pag::DiskCache::GetFilePath([key UTF8String]);
  if (path.length() == 0) {
    return nil;
  }
  return [NSString stringWithUTF8String:path.c_str()];
}

+ (BOOL)WritFile:(NSString*)key data:(NSData*)data {
  if (key == nil || data == nil) {
    return false;
  }
  std::string cacheKey = [key UTF8String];
  auto cacheDatas = tgfx::Data::MakeWithoutCopy(data.bytes, data.length);
  return pag::DiskCache::WriteFile(cacheKey, cacheDatas);
}

@end
