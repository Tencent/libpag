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

#import <Foundation/Foundation.h>

#import "PAGDiskCache.h"

NS_ASSUME_NONNULL_BEGIN

@interface PAGDiskCacheItem : NSObject
@property(nonatomic, retain) PAGDiskCache* diskCache;
@property(nonatomic, assign) NSInteger count;
@property(nonatomic, assign) BOOL deleteAfterUse;
@end

@interface PAGDiskCacheManager : NSObject

+ (instancetype)shareInstance;

- (PAGDiskCacheItem* __nullable)objectDiskCacheFor:(NSString* __nonnull)cacheName
                                        frameCount:(NSInteger)frameCount;

- (void)removeDiskCacheFrom:(NSString* __nonnull)cacheName;

@end

NS_ASSUME_NONNULL_END
