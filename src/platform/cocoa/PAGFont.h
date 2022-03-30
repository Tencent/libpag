/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
#import "PAG.h"

PAG_API @interface PAGFont : NSObject

/**
 * A string with the name of the font family.
 **/
@property(nonatomic, copy) NSString* fontFamily;

/**
 * A string with the style information — e.g., “bold”, “italic”.
 **/
@property(nonatomic, copy) NSString* fontStyle;

+ (PAGFont*)RegisterFont:(NSString*)fontPath;

+ (PAGFont*)RegisterFont:(NSString*)fontPath
                  family:(NSString*)fontFamily
                   style:(NSString*)fontStyle;

+ (PAGFont*)RegisterFont:(void*)data size:(size_t)length;

+ (PAGFont*)RegisterFont:(void*)data
                    size:(size_t)length
                  family:(NSString*)fontFamily
                   style:(NSString*)fontStyle;

+ (void)UnregisterFont:(PAGFont*)font;

@end
