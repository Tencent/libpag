#import "PAGPlayerView.h"
#import <libpag/PAGView.h>
#import <libpag/PAGFile.h>
#import <libpag/PAGImage.h>
#import <libpag/PAGSurface.h>
#import <libpag/PAGPlayer.h>
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>
#import <libpag/PAGImageLayer.h>
#import <libpag/PAGDecoder.h>

@implementation PAGPlayerView {
    PAGView* pagView;
    NSString* pagPath;
    EAGLContext *context;
    PAGSurface * surface;
    PAGPlayer * player;
}

- (void)loadPAGAndPlay: (NSString*)pagPath{
    if (pagView != nil) {
        [pagView stop];
        [pagView removeFromSuperview];
        pagView = nil;
    }
    pagView = [[PAGView alloc] init];

    [self testFile: pagPath];

    [self addSubview:pagView];
    pagView.frame = self.frame;
    [pagView setRepeatCount:-1];
    [pagView play];

//    [self testMutiThread];
}

- (void)testFile: (NSString*)path {
    NSString* fileName = [path substringToIndex:path.length-4];
    NSString* extension = [path substringFromIndex:path.length-3];
    pagPath = [[NSBundle mainBundle] pathForResource:fileName ofType:extension];
    PAGFile* pagFile = [PAGFile Load:pagPath];

    if ([pagFile numTexts] > 0) {
        PAGText* textData = [pagFile getTextData:0];
        textData.text = @"hah哈 哈哈哈哈👌하";
        [pagFile replaceText:0 data:textData];
    }

    if ([pagFile numImages] > 0) {
        NSString* filePath = [[NSBundle mainBundle] pathForResource:@"rotation" ofType:@"jpg"];
//        CVPixelBufferRef pixelBuffer = [self pixelBufferFromPath:filePath];
//        PAGImage* pagImage = [PAGImage FromPixelBuffer:pixelBuffer];
        PAGImage* pagImage = [PAGImage FromPath:filePath];
        if (pagImage) {
            [pagFile replaceImage:0 data:pagImage];
        }
    }

    [pagView setComposition:pagFile];
}

- (void)testMutiThread {
    pagPath = [[NSBundle mainBundle] pathForResource:@"test2" ofType:@"pag"];
    PAGFile* pagFile = [PAGFile Load:pagPath];
    PAGSurface *pagSurface = [PAGSurface MakeOffscreen:CGSizeMake(pagFile.width, pagFile.height)];
    PAGSurface *pagSurface1 = [PAGSurface MakeOffscreen:CGSizeMake(pagFile.width, pagFile.height)];
    PAGPlayer *player = [[PAGPlayer alloc] init];
    PAGPlayer *player2 = [[PAGPlayer alloc] init];
    [player setSurface:pagSurface];
    [player setComposition:pagFile];
    [player2 setSurface:pagSurface1];
    [player2 setComposition:[pagFile copyOriginal]];
    dispatch_queue_t testQueue = dispatch_queue_create("test", DISPATCH_QUEUE_SERIAL);
    dispatch_async(testQueue, ^{
        int i = 0;
        while (i < 100) {
            [player setProgress:i * 1.0/100];
            [player flush];
            [player2 setProgress:i * 1.0/100];
            [player2 flush];
            i++;
        }
    });
    int i = 0;
    while (i < 100) {
        [pagSurface freeCache];
        [pagSurface1 freeCache];
        i++;
    }
}

- (CVPixelBufferRef)pixelBufferFromPath:(NSString*)imagePath {
    UIImage* uiImage = [[UIImage alloc] initWithContentsOfFile:imagePath];
    if(!uiImage){
        return nil;
    }
    CGImageRef image = [uiImage CGImage];
    CVPixelBufferRef pixelBuffer = nil;
    CFDictionaryRef empty = CFDictionaryCreate(kCFAllocatorDefault, NULL, NULL,0,
                               &kCFTypeDictionaryKeyCallBacks,
                               &kCFTypeDictionaryValueCallBacks);
    CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(kCFAllocatorDefault, 1,
                                      &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks);
    size_t width = CGImageGetWidth(image);
    size_t height = CGImageGetHeight(image);
    CFDictionarySetValue(attrs, kCVPixelBufferIOSurfacePropertiesKey, empty);
    CVPixelBufferCreate(kCFAllocatorDefault, width, height,
                        kCVPixelFormatType_32BGRA, attrs, &pixelBuffer);
    CFRelease(attrs);
    CFRelease(empty);

    CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    void *pxdata = CVPixelBufferGetBaseAddress(pixelBuffer);

    CGColorSpaceRef rgbColorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(pxdata, CGImageGetWidth(image),
                                                 CGImageGetHeight(image), 8,
                                                 CVPixelBufferGetBytesPerRow(pixelBuffer), rgbColorSpace,
                                                 kCGImageAlphaPremultipliedFirst|kCGImageByteOrder32Little);

    CGContextDrawImage(context, CGRectMake(0, 0, CGImageGetWidth(image),
                                           CGImageGetHeight(image)), image);
    CGColorSpaceRelease(rgbColorSpace);
    CGContextRelease(context);
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
    CFAutorelease(pixelBuffer);
    return pixelBuffer;
}

- (void)stop {
    if (pagView != nil) {
        [pagView stop];
        [pagView removeFromSuperview];
        pagView = nil;
    }
}

- (void)touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event {
    if ([pagView isPlaying]) {
        [pagView stop];
        PAGDecoder* pagDecoder = [PAGDecoder Make:[pagView getComposition]];
        UIImage* image = [pagDecoder frameAtIndex:50];
        UIImage* image1 = [pagDecoder frameAtIndex:50];
        image = nil;
        UIImage* image2 = [pagDecoder frameAtIndex:50];
        UIImage* image3 = [pagDecoder frameAtIndex:51];
        UIImage* image4 = [pagDecoder frameAtIndex:52];
        UIImage* image5 = [pagDecoder frameAtIndex:0];
        pagDecoder = nil;
        
    } else {
//        [pagView play];
        pagPath = [[NSBundle mainBundle] pathForResource:@"test2" ofType:@"pag"];
        PAGFile* pagFile = [PAGFile Load:pagPath];
        PAGDecoder* pagDecoder = [PAGDecoder Make:pagFile];
        UIImage* image = [pagDecoder frameAtIndex:50];
        UIImage* image1 = [pagDecoder frameAtIndex:50];
        image = nil;
        UIImage* image2 = [pagDecoder frameAtIndex:50];
        UIImage* image3 = [pagDecoder frameAtIndex:51];
        UIImage* image4 = [pagDecoder frameAtIndex:52];
        UIImage* image5 = [pagDecoder frameAtIndex:0];
        pagDecoder = nil;
    }

}

@end
