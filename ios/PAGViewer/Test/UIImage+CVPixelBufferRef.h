//
//  UIImage+CVPixelBufferRef.h
//  PAGViewer
//
//  Created by kevingpqi on 2019/1/21.
//  Copyright © 2019年 libpag.org. All rights reserved.
//

#import <UIKit/UIKit.h>


@interface UIImage (CVPixelBufferRef)

+ (UIImage*)uiImageFromPixelBuffer:(CVPixelBufferRef)p;
@end

