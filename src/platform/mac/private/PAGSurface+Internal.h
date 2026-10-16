/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#import <Foundation/Foundation.h>
#import "platform/mac/PAGSurface.h"

@class PAGSurfaceImpl;

// The class extension is declared here (instead of inside PAGSurface.m) so the backend category
// implementations in PAGSurface+OpenGL.m / PAGSurface+Metal.m can assign the backing property.
// Keep this header free of C++ includes: it is imported from pure-Objective-C (.m) translation
// units. The pag::PAGSurface plumbing stays in .mm files through PAGSurfaceImpl+Internal.h or
// local extensions (the same pattern PAGPlayerImpl.mm uses).
@interface PAGSurface ()

@property(nonatomic, strong) PAGSurfaceImpl* surface;

@end
