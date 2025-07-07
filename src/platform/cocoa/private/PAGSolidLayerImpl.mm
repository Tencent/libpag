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

#import "PAGSolidLayerImpl.h"
#import "PAGColorUtility.h"
#import "PAGLayerImpl+Internal.h"

@implementation PAGSolidLayerImpl

- (CocoaColor*)solidColor {
  auto pagSolidLayer = std::static_pointer_cast<pag::PAGSolidLayer>([super pagLayer]);
  auto color = pagSolidLayer->solidColor();
  return PAGColorUtility::ToCocoaColor(color);
}

- (void)setSolidColor:(CocoaColor*)color {
  auto pagSolidLayer = std::static_pointer_cast<pag::PAGSolidLayer>([super pagLayer]);
  pagSolidLayer->setSolidColor(PAGColorUtility::ToColor(color));
}

@end
