//
//  CustomVideoCompositing.m
//  pag-ios
//
//  Created by kevingpqi on 2021/2/3.
//  Copyright © 2021 kevingpqi. All rights reserved.
//

#import <CoreImage/CoreImage.h>
#import "CustomVideoCompositing.h"
#import "CustomVideoCompositionInstruction.h"

@interface CustomVideoCompositing ()

@property (nonatomic, strong) dispatch_queue_t renderContextQueue;
@property (nonatomic, strong) dispatch_queue_t renderingQueue;
@property (nonatomic, assign) BOOL shouldCancelAllRequests;

@property (nonatomic, strong) AVVideoCompositionRenderContext *renderContext;
@property (nonatomic, strong) CIContext *ciContext;

@end

@implementation CustomVideoCompositing
- (instancetype)init {
    self = [super init];
    if (self) {
        _sourcePixelBufferAttributes = @{(id)kCVPixelBufferOpenGLCompatibilityKey: @YES,
                                         (id)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)};
        _requiredPixelBufferAttributesForRenderContext = @{(id)kCVPixelBufferOpenGLCompatibilityKey: @YES,
                                                           (id)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)};

        _renderContextQueue = dispatch_queue_create("com.tencent.pag.test", 0);
        _renderingQueue = dispatch_queue_create("com.tencent.pag.test", 0);
    }
    return self;
}

- (void)renderContextChanged:(AVVideoCompositionRenderContext *)newRenderContext {
    dispatch_sync(self.renderContextQueue, ^{
        self.renderContext = newRenderContext;
    });
}

- (void)startVideoCompositionRequest:(AVAsynchronousVideoCompositionRequest *)asyncVideoCompositionRequest {
    dispatch_async(self.renderingQueue, ^{
        @autoreleasepool {
            if (self.shouldCancelAllRequests) {
                [asyncVideoCompositionRequest finishCancelledRequest];
            } else {
                CVPixelBufferRef resultPixels = [self newRenderdPixelBufferForRequest:asyncVideoCompositionRequest];
                if (resultPixels) {
                    [asyncVideoCompositionRequest finishWithComposedVideoFrame:resultPixels];
                    CVPixelBufferRelease(resultPixels);
                } else {
                    NSError *error = [NSError errorWithDomain:@"com.tencent.pag.test" code:0 userInfo:@{NSLocalizedDescriptionKey: NSLocalizedString(@"Composition request new pixel buffer failed.", nil)}];
                    [asyncVideoCompositionRequest finishWithError:error];
                    NSLog(@"%@", error);
                }
            }
        }
    });
}

- (void)cancelAllPendingVideoCompositionRequests {
    self.shouldCancelAllRequests = YES;
    dispatch_barrier_async(self.renderingQueue, ^{
        self.shouldCancelAllRequests = NO;
    });
}

#pragma mark - Private

- (CVPixelBufferRef)newRenderdPixelBufferForRequest:(AVAsynchronousVideoCompositionRequest *)request {
    CustomVideoCompositionInstruction *videoCompositionInstruction = (CustomVideoCompositionInstruction *)request.videoCompositionInstruction;
    NSArray<AVVideoCompositionLayerInstruction *> *layerInstructions = videoCompositionInstruction.layerInstructions;
    CMPersistentTrackID trackID = layerInstructions.firstObject.trackID;
    
    CVPixelBufferRef sourcePixelBuffer = [request sourceFrameByTrackID:trackID];
    NSInteger currentTime = CMTimeGetSeconds(request.compositionTime) * 1000000;
    CVPixelBufferRef resultPixelBuffer = [videoCompositionInstruction applyPixelBuffer:sourcePixelBuffer currentTime:currentTime];
    return resultPixelBuffer;
}

@end
