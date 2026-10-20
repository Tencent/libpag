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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>
#import "PAGImageLayerImpl.h"
#import "PAGLayerImpl.h"

// Backend factory declarations live in the opengl/ and metal/ category headers. The selected
// category implementation shares PAGSurfaceImpl.mm with the class for ordinary static linking.
@interface PAGSurfaceImpl : NSObject

+ (PAGSurfaceImpl*)FromCVPixelBuffer:(CVPixelBufferRef)pixelBuffer;

+ (PAGSurfaceImpl*)MakeOffscreen:(CGSize)size;

- (void)updateSize;

- (int)width;

- (int)height;

- (BOOL)clearAll;

- (void)freeCache;

- (CVPixelBufferRef)getCVPixelBuffer;

- (CVPixelBufferRef)makeSnapshot;

- (BOOL)copyPixelsTo:(void*)pixels rowBytes:(size_t)rowBytes;

@end
