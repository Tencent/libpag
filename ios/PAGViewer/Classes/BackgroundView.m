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

#import "BackgroundView.h"

@implementation BackgroundView

- (void)drawRect:(CGRect)rect {
    [super drawRect:rect];
    CGContextRef context = UIGraphicsGetCurrentContext();
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGColorRef whiteColor = CGColorCreate(colorSpace, (CGFloat[]) {1.0f, 1.0f, 1.0f, 1.0f});
    CGContextSetFillColorWithColor(context, whiteColor);
    CGContextAddRect(context, self.frame);
    CGContextFillPath(context);
    CGContextBeginPath(context);
    CGColorRef greyColor = CGColorCreate(colorSpace, (CGFloat[]) {0.8f, 0.8f, 0.8f, 1.0f});
    CGContextSetFillColorWithColor(context, greyColor);
    CGSize size = self.frame.size;
    int tileSize = 8;
    for (int y = 0; y < size.height; y += tileSize) {
        BOOL draw = (y / tileSize) % 2 == 1;
        for (int x = 0; x < size.width; x += tileSize) {
            if (draw) {
                CGContextAddRect(context, CGRectMake(x, y, tileSize, tileSize));
            }
            draw = !draw;
        }
    }
    CGColorRelease(whiteColor);
    CGColorRelease(greyColor);
    CGContextFillPath(context);
    CGColorSpaceRelease(colorSpace);
}


@end
