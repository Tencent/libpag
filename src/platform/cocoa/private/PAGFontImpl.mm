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

//
// Created by dom on 26/12/2017.
//

#import "PAGFontImpl.h"
#import "PAGFont.h"
#import "pag/pag.h"

@implementation PAGFontImpl

+ (PAGFont*)RegisterFont:(NSString*)fontPath
                  family:(NSString*)fontFamily
                   style:(NSString*)fontStyle {
  if (fontPath == nil) {
    return nullptr;
  }
  auto font = pag::PAGFont::RegisterFont(std::string(fontPath.UTF8String), 0,
                                         std::string(fontFamily.UTF8String),
                                         std::string(fontStyle.UTF8String));
  if (font.fontFamily.empty()) {
    return nullptr;
  }
  PAGFont* pagFont = [[[PAGFont alloc] init] autorelease];
  pagFont.fontFamily = [NSString stringWithUTF8String:font.fontFamily.c_str()];
  pagFont.fontStyle = [NSString stringWithUTF8String:font.fontStyle.c_str()];
  return pagFont;
}

+ (PAGFont*)RegisterFont:(void*)data
                    size:(size_t)length
                  family:(NSString*)fontFamily
                   style:(NSString*)fontStyle {
  auto font = pag::PAGFont::RegisterFont(data, length, 0, std::string(fontFamily.UTF8String),
                                         std::string(fontStyle.UTF8String));
  if (font.fontFamily.empty()) {
    return nullptr;
  }
  PAGFont* pagFont = [[[PAGFont alloc] init] autorelease];
  pagFont.fontFamily = [NSString stringWithUTF8String:font.fontFamily.c_str()];
  pagFont.fontStyle = [NSString stringWithUTF8String:font.fontStyle.c_str()];
  return pagFont;
}

+ (void)UnregisterFont:(PAGFont*)font {
  if (font.fontFamily.length == 0) {
    return;
  }
  pag::PAGFont::UnregisterFont(
      {std::string(font.fontFamily.UTF8String), std::string(font.fontStyle.UTF8String)});
}

@end
