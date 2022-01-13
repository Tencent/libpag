//
//  UIImage+CVPixelBufferRef.m
//  PAGViewer
//
//  Created by kevingpqi on 2019/1/21.
//  Copyright © 2019年 libpag.org. All rights reserved.
//

#import "UIImage+CVPixelBufferRef.h"

@implementation UIImage (CVPixelBufferRef)

+ (UIImage*)uiImageFromPixelBuffer:(CVPixelBufferRef)p {
    CIImage* ciImage = [CIImage imageWithCVPixelBuffer:p];
    
    CIContext* context = [CIContext contextWithOptions:@{kCIContextUseSoftwareRenderer : @(YES)}];
    
    CGRect rect = CGRectMake(0, 0, CVPixelBufferGetWidth(p), CVPixelBufferGetHeight(p));
    CGImageRef videoImage = [context createCGImage:ciImage fromRect:rect];
    
    UIImage* image = [UIImage imageWithCGImage:videoImage];
    CGImageRelease(videoImage);
    
    return image;
}
@end
