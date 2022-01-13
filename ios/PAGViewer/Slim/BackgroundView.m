//
//  BackgroundView.m
//  PAGViewer
//
//  Created by dom on 2018/9/23.
//  Copyright © 2018 libpag.org. All rights reserved.
//

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
