//
//  PAGTestUtils.h
//  PAGViewer
//
//  Created by kevingpqi on 2019/1/21.
//  Copyright © 2019年 libpag.org. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@interface PAGTestUtils : NSObject
+ (NSString *)md5ForCVPixelBuffer:(CVPixelBufferRef)cvPixelBuffer;

+ (BOOL)isTestTarget;

+ (NSString *)getFloderName;

@end
