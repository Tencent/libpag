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

NS_ASSUME_NONNULL_BEGIN

@interface PAGDiskCache : NSObject

+ (instancetype __nullable)MakeWithName:(NSString*)name frameCount:(NSUInteger)frameCount;

- (BOOL)objectForKey:(NSInteger)index pixelBuffer:(CVPixelBufferRef)pixelBuffer;

- (BOOL)containsObjectForKey:(NSInteger)index;

- (void)setObject:(CVPixelBufferRef)pixelBuffer
           forKey:(NSInteger)index
        withBlock:(void (^_Nullable)(void))block;

- (void)setObject:(CVPixelBufferRef)pixelBuffer forKey:(NSInteger)index;

- (NSUInteger)count;

- (NSString*)path;

- (NSInteger)maxFrameSize;

- (void)removeCachesWithBlock:(void (^_Nullable)(void))block;

- (void)removeCaches;

@end

NS_ASSUME_NONNULL_END