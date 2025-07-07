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

#import "PAGFont.h"
#import "platform/cocoa/private/PAGFontImpl.h"

@implementation PAGFont

@synthesize fontFamily;
@synthesize fontStyle;

+ (PAGFont*)RegisterFont:(NSString*)fontPath {
  return [PAGFont RegisterFont:fontPath family:@"" style:@""];
}

+ (PAGFont*)RegisterFont:(NSString*)fontPath
                  family:(NSString*)fontFamily
                   style:(NSString*)fontStyle {
  return [PAGFontImpl RegisterFont:fontPath family:fontFamily style:fontStyle];
}

+ (PAGFont*)RegisterFont:(void*)data size:(size_t)length {
  return [PAGFont RegisterFont:data size:length family:@"" style:@""];
}

+ (PAGFont*)RegisterFont:(void*)data
                    size:(size_t)length
                  family:(NSString*)fontFamily
                   style:(NSString*)fontStyle {
  return [PAGFontImpl RegisterFont:data size:length family:fontFamily style:fontStyle];
}

+ (void)UnregisterFont:(PAGFont*)font {
  [PAGFontImpl UnregisterFont:font];
}

- (void)dealloc {
  [fontFamily release];
  [fontStyle release];
  [super dealloc];
}

@end
