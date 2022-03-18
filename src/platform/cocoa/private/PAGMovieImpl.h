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

#import "PAGImageImpl.h"
#import "platform/cocoa/PAGComposition.h"

@interface PAGMovieImpl : PAGImageImpl

+ (PAGMovieImpl*)FromComposition:(PAGComposition*)composition;

+ (PAGMovieImpl*)FromVideoPath:(NSString*)filePath;

+ (PAGMovieImpl*)FromVideoPath:(NSString*)filePath
                     startTime:(int64_t)startTime
                      duration:(int64_t)duration;

- (int64_t)duration;

@end
