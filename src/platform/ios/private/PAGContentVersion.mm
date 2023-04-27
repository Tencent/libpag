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

#import "PAGContentVersion.h"

#import "platform/cocoa/private/PAGLayer+Internal.h"
#import "platform/cocoa/private/PAGLayerImpl+Internal.h"
#import "platform/ios/private/PAGDecoder+Internal.h"
#include "rendering/layers/ContentVersion.h"

@implementation PAGContentVersion
+ (NSInteger)Get:(PAGComposition*)pagComposition {
  if (pagComposition == nil) {
    return 0;
  }
  auto composition = [[pagComposition impl] pagLayer];
  return pag::ContentVersion::Get(composition);
}

+ (BOOL)CheckFrameChanged:(PAGDecoder*)decoder index:(NSInteger)index {
  if (decoder) {
    return pag::ContentVersion::CheckFrameChanged([[decoder impl] decoder], (int)index);
  }
  return NO;
}

@end
