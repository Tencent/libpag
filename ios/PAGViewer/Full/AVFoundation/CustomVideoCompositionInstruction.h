//
//  CustomVideoCompositionInstruction.h
//  pag-ios
//
//  Created by kevingpqi on 2021/2/3.
//  Copyright © 2021 kevingpqi. All rights reserved.
//

#import <AVFoundation/AVFoundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, StickerMode) {
    Sticker_SMALL = 0,//小贴纸方案,pag贴纸叠加到视频中
    Sticker_BIG//大贴纸方案,视频填充到pag贴纸占位图中
};

@interface CustomVideoCompositionInstruction : NSObject <AVVideoCompositionInstruction>
@property (nonatomic, assign) CMTimeRange timeRange;
@property (nonatomic, assign) BOOL enablePostProcessing;
@property (nonatomic, assign) BOOL containsTweening;

@property (nonatomic, readonly, nullable) NSArray<NSValue *> *requiredSourceTrackIDs;
@property (nonatomic, readonly) CMPersistentTrackID passthroughTrackID;

@property (nonatomic, copy) NSArray<AVVideoCompositionLayerInstruction *> *layerInstructions;
@property (nonatomic, assign) StickerMode stickerMode;

- (instancetype)initWithPassthroughTrackID:(CMPersistentTrackID)passthroughTrackID timeRange:(CMTimeRange)timeRange;
- (instancetype)initWithSourceTrackIDs:(NSArray<NSValue *> *)sourceTrackIDs timeRange:(CMTimeRange)timeRange;

/// 处理 pixelBuffer，并返回结果
- (CVPixelBufferRef)applyPixelBuffer:(CVPixelBufferRef)pixelBuffer currentTime:(NSInteger)time;

@end

NS_ASSUME_NONNULL_END
