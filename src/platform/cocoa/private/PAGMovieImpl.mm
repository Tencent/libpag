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

#import "PAGMovieImpl.h"
#import "PAGLayer+Internal.h"
#import "PAGLayerImpl+Internal.h"

@interface PAGImageImpl ()

@property(nonatomic) std::shared_ptr<pag::PAGImage> pagImage;

- (instancetype)initWidthPAGImage:(std::shared_ptr<pag::PAGImage>)value;

@end

@implementation PAGMovieImpl

+ (PAGMovieImpl*)FromComposition:(PAGComposition*)composition {
  if (composition == nil) {
    return nil;
  }
  auto layer = [[composition impl] pagLayer];
  auto pagComposition = std::static_pointer_cast<pag::PAGComposition>(layer);
  auto movie = pag::PAGMovie::FromComposition(pagComposition);
  if (movie == nullptr) {
    return nil;
  }
  return [[[PAGMovieImpl alloc] initWidthPAGImage:movie] autorelease];
}

+ (PAGMovieImpl*)FromVideoPath:(NSString*)filePath {
  if (filePath == nil) {
    return nil;
  }
  std::string path = std::string([filePath UTF8String]);
  auto movie = pag::PAGMovie::FromVideoPath(path);
  if (movie == nullptr) {
    return nil;
  }
  return [[[PAGMovieImpl alloc] initWidthPAGImage:movie] autorelease];
}

+ (PAGMovieImpl*)FromVideoPath:(NSString*)filePath
                     startTime:(int64_t)startTime
                      duration:(int64_t)duration {
  if (filePath == nil) {
    return nil;
  }
  std::string path = std::string([filePath UTF8String]);
  auto movie = pag::PAGMovie::FromVideoPath(path, startTime, duration);
  if (movie == nullptr) {
    return nil;
  }
  return [[[PAGMovieImpl alloc] initWidthPAGImage:movie] autorelease];
}

- (int64_t)duration {
  return std::static_pointer_cast<pag::PAGMovie>(self.pagImage)->duration();
}

@end
