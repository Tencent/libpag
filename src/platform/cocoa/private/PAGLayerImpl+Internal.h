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

#import <Foundation/Foundation.h>
#import "PAGLayerImpl.h"
#import "pag/file.h"
#import "pag/pag.h"

@interface PAGLayerImpl (Internal)

- (instancetype)initWithPagLayer:(std::shared_ptr<pag::PAGLayer>)pagLayer;

- (std::shared_ptr<pag::PAGLayer>)pagLayer;

- (void)setPagLayer:(std::shared_ptr<pag::PAGLayer>)pagLayer;

/*
 * Convert pag::PAGLayer to a PAGLayer wrapper
 */
+ (PAGLayer*)ToPAGLayer:(std::shared_ptr<pag::PAGLayer>)layer;

/*
 * Batch Convert pag::PAGLayer to a PAGLayer wrapper
 */
+ (NSArray<PAGLayer*>*)BatchConvertToPAGLayers:
    (const std::vector<std::shared_ptr<pag::PAGLayer>>&)layerVector;

/*
 * Batch Convert pag::Marker to a PAGMarker wrapper
 */
+ (NSArray<PAGMarker*>*)BatchConvertToPAGMarkers:(std::vector<const pag::Marker*>&)markers;
@end
