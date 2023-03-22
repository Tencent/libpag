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

#import "PAGDiskCacheManager.h"

@implementation PAGDiskCacheItem
- (void)dealloc {
  [_diskCache release];
  [super dealloc];
}
@end

@implementation PAGDiskCacheManager {
  NSMutableDictionary<NSString*, PAGDiskCacheItem*>* diskCacheDict;
}

+ (instancetype)shareInstance {
  static PAGDiskCacheManager* queueManager = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    queueManager = [PAGDiskCacheManager new];
  });
  return queueManager;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    diskCacheDict = [NSMutableDictionary new];
  }
  return self;
}

- (void)dealloc {
  [diskCacheDict release];
  [super dealloc];
}

- (PAGDiskCacheItem*)objectDiskCacheFor:(NSString* __nonnull)cacheName
                             frameCount:(NSInteger)frameCount {
  @synchronized(diskCacheDict) {
    PAGDiskCacheItem* diskCacheItem = [diskCacheDict objectForKey:cacheName];
    if (!diskCacheItem) {
      PAGDiskCache* diskCache = [PAGDiskCache MakeWithName:cacheName frameCount:frameCount];
      if (diskCache == nil) {
        return nil;
      }
      diskCacheItem = [[[PAGDiskCacheItem alloc] init] autorelease];
      diskCacheItem.diskCache = diskCache;
      diskCacheItem.count = 1;
      [diskCacheDict setObject:diskCacheItem forKey:cacheName];
      return diskCacheItem;
    }
    diskCacheItem.count++;
    return diskCacheItem;
  }
}

- (void)removeDiskCacheFrom:(NSString* __nonnull)cacheName {
  @synchronized(diskCacheDict) {
    PAGDiskCacheItem* diskCacheItem = [diskCacheDict objectForKey:cacheName];
    if (diskCacheItem) {
      diskCacheItem.count--;
      if (diskCacheItem.count == 0) {
        [diskCacheDict removeObjectForKey:cacheName];
      }
    }
  }
}

@end
