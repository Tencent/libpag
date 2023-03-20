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

#import "PAGCacheQueueManager.h"

@implementation PAGCacheQueueManager {
  NSMutableDictionary<NSString*, dispatch_queue_t>* cacheQueueDict;
}

+ (instancetype)shareInstance {
  static PAGCacheQueueManager* queueManager = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    queueManager = [PAGCacheQueueManager new];
  });
  return queueManager;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    cacheQueueDict = [NSMutableDictionary new];
  }
  return self;
}

- (void)dealloc {
  for (dispatch_queue_t queue in [cacheQueueDict allValues]) {
    dispatch_release(queue);
  }
  [cacheQueueDict release];
  [super dealloc];
}

- (dispatch_queue_t)queueForPath:(NSString* __nonnull)cachePath {
  @synchronized(cacheQueueDict) {
    dispatch_queue_t queue = [cacheQueueDict objectForKey:cachePath];
    if (!queue) {
      static int index = 0;
      NSString* queueName = [NSString stringWithFormat:@"ImageViewCache.art.pag.%d", index++];
      queue = dispatch_queue_create([queueName UTF8String], DISPATCH_QUEUE_SERIAL);
      [cacheQueueDict setObject:queue forKey:cachePath];
    }
    return queue;
  }
}

- (void)removeQueueForPath:(NSString* __nonnull)cachePath {
  @synchronized(cacheQueueDict) {
    dispatch_queue_t queue = [cacheQueueDict objectForKey:cachePath];
    if (queue) {
      [cacheQueueDict removeObjectForKey:cachePath];
    }
  }
}

@end
