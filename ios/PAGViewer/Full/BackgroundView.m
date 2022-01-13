//
//  BackgroundView.m
//  PAGTestPod
//
//  Created by kevingpqi on 2019/1/15.
//  Copyright © 2019年 kevingpqi. All rights reserved.
//

#import "BackgroundView.h"

@implementation BackgroundView

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    //    self.backgroundColor = [UIColor grayColor];
    return self;
}

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
