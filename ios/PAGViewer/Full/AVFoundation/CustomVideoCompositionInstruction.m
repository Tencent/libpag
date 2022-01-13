//
//  CustomVideoCompositionInstruction.m
//  pag-ios
//
//  Created by kevingpqi on 2021/2/3.
//  Copyright © 2021 kevingpqi. All rights reserved.
//

#import "CustomVideoCompositionInstruction.h"
#import <libpag/PAGPlayer.h>
#import <libpag/PAGSurface.h>
#import <libpag/PAGFile.h>

@interface CustomVideoCompositionInstruction()
@property (nonatomic, strong) PAGPlayer* player;
@property (nonatomic, strong) PAGSurface* pagSurface;
@property (nonatomic, strong) PAGFile* pagFile;
@property (nonatomic, strong) CIContext *context;

@end

@implementation CustomVideoCompositionInstruction
- (instancetype)initWithPassthroughTrackID:(CMPersistentTrackID)passthroughTrackID timeRange:(CMTimeRange)timeRange {
    self = [super init];
    if (self) {
        _passthroughTrackID = passthroughTrackID;
        _timeRange = timeRange;
        _requiredSourceTrackIDs = @[];
        _containsTweening = NO;
        _enablePostProcessing = NO;
    }
    return self;
}

- (instancetype)initWithSourceTrackIDs:(NSArray<NSValue *> *)sourceTrackIDs timeRange:(CMTimeRange)timeRange {
    self = [super init];
    if (self) {
        _requiredSourceTrackIDs = sourceTrackIDs;
        _timeRange = timeRange;
        _passthroughTrackID = kCMPersistentTrackID_Invalid;
        _containsTweening = YES;
        _enablePostProcessing = NO;
    }
    return self;
}

#pragma mark - Public
- (CVPixelBufferRef)applyPixelBuffer:(CVPixelBufferRef)pixelBuffer currentTime:(NSInteger)time {
    double progress = time % [self.player duration] * 1.0 / [self.player duration];
    CIImage *sourceImage = [[CIImage alloc] initWithCVPixelBuffer:pixelBuffer];
    CVPixelBufferRef outputPixelBuffer;
    CIImage *outputImage;
    if (self.stickerMode == Sticker_SMALL) {
        [self.player setProgress:progress];
        [self.player flush];
        CVPixelBufferRef cvPixelBuffer = [self.pagSurface getCVPixelBuffer];
        CIImage *stickerImage = [CIImage imageWithCVPixelBuffer:cvPixelBuffer];
        outputImage = [stickerImage imageByCompositingOverImage:sourceImage];
        
        CGSize size = CGSizeMake(CVPixelBufferGetWidth(pixelBuffer),
                                 CVPixelBufferGetHeight(pixelBuffer));
        
        outputPixelBuffer = [self createPixelBufferWithSize:size];
    } else {
        CGSize size = CGSizeMake(CVPixelBufferGetWidth(pixelBuffer),
                                 CVPixelBufferGetHeight(pixelBuffer));
        CVPixelBufferRef inputPixelBuffer = [self createPixelBufferWithSize:size];
        if (inputPixelBuffer) {
            [self.context render:sourceImage toCVPixelBuffer:inputPixelBuffer];
            PAGImage* pagImage = [PAGImage FromPixelBuffer:inputPixelBuffer];
            [self.pagFile replaceImage:0 data:pagImage];
        }
        
        [self.player setProgress:progress];
        [self.player flush];
        
        CVPixelBufferRef cvPixelBuffer = [self.pagSurface getCVPixelBuffer];
        
        outputImage = [CIImage imageWithCVPixelBuffer:cvPixelBuffer];
        CGSize outSize = CGSizeMake(CVPixelBufferGetWidth(cvPixelBuffer),
                                 CVPixelBufferGetHeight(cvPixelBuffer));
        
        outputPixelBuffer = [self createPixelBufferWithSize:outSize];
    }
    [self.context render:outputImage toCVPixelBuffer:outputPixelBuffer];
    return outputPixelBuffer;
}

#pragma mark - Private
- (PAGPlayer *)player {
    if (!_player) {
        NSString* pagPath;
        if (self.stickerMode == Sticker_SMALL) {
            pagPath = [[NSBundle mainBundle] pathForResource:@"small" ofType:@"pag"];
        } else {
            pagPath = [[NSBundle mainBundle] pathForResource:@"jisha" ofType:@"pag"];
        }
        _pagFile = [PAGFile Load:pagPath];
        _pagSurface = [PAGSurface MakeFromGPU:CGSizeMake([_pagFile width], [_pagFile height])];
        _player = [[PAGPlayer alloc] init];
        [_player setSurface:_pagSurface];
        [_player setComposition:_pagFile];
    }
    return _player;
}

- (CVPixelBufferRef)createPixelBufferWithSize:(CGSize)size {
    CVPixelBufferRef pixelBuffer;
    NSDictionary *pixelBufferAttributes = @{(id)kCVPixelBufferIOSurfacePropertiesKey: @{}};
    CVReturn status = CVPixelBufferCreate(nil,
                                          size.width,
                                          size.height,
                                          kCVPixelFormatType_32BGRA,
                                          (__bridge CFDictionaryRef _Nullable)(pixelBufferAttributes),
                                          &pixelBuffer);
    if (status != kCVReturnSuccess) {
        NSLog(@"Can't create pixelbuffer");
    }
    return pixelBuffer;
}

- (CIContext *)context {
    if (!_context) {
        _context = [[CIContext alloc] init];
    }
    return _context;
}

@end
