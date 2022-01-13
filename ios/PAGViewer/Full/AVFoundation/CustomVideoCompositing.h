//
//  CustomVideoCompositing.h
//  pag-ios
//
//  Created by kevingpqi on 2021/2/3.
//  Copyright © 2021 kevingpqi. All rights reserved.
//

#import <AVFoundation/AVFoundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface CustomVideoCompositing : NSObject <AVVideoCompositing>

@property (nonatomic, strong, nullable) NSDictionary<NSString *, id> *sourcePixelBufferAttributes;
@property (nonatomic, strong) NSDictionary<NSString *, id> *requiredPixelBufferAttributesForRenderContext;

@end

NS_ASSUME_NONNULL_END
