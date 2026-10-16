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

#import "PAGSurfaceImpl.h"
#include "pag/pag.h"

// Constructors and the pag::PAGSurface accessor used by the backend factory categories and the
// cocoa PAGPlayer bridge. The declarations used to live in a class extension inside
// PAGSurfaceImpl.mm; they moved here when the factories were split into per-backend files.
@interface PAGSurfaceImpl (Internal)

- (instancetype)initWithSurface:(std::shared_ptr<pag::PAGSurface>)value;

- (std::shared_ptr<pag::PAGSurface>)pagSurface;

@end
