#import "ViewController.h"
#import "DragFilePAGView.h"
#import <PAGSurface.h>
#import <PAGPlayer.h>

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    DragFilePAGView *view = [[DragFilePAGView alloc] initWithFrame:self.view.bounds];
    NSString *path = [[NSBundle mainBundle] pathForResource:@"test2" ofType:@"pag"];
    [view setAutoresizingMask:kCALayerWidthSizable|kCALayerHeightSizable];
    [view setPath:path];
    [view setRepeatCount:-1];
    [view play];
    PAGFile* pagFile = (PAGFile*)[view getComposition];
    if ([pagFile numTexts] > 0) {
        PAGText* textData = [pagFile getTextData:0];
        textData.fontSize = textData.fontSize*0.5;
        textData.text = @"运动文字遮罩替换😀文本";
        [pagFile replaceText:0 data:textData];
    }
    
    [self.view addSubview:view];
    //[self testPixelBufferMode];
}


- (void)setRepresentedObject:(id)representedObject {
    [super setRepresentedObject:representedObject];

    // Update the view, if already loaded.
}


- (void)testPixelBufferMode {
    NSString* pagPath = [[NSBundle mainBundle] pathForResource:@"particle_video" ofType:@"pag"];
    PAGFile* pagFile = [PAGFile Load:pagPath];

    PAGSurface *pagSurface = [PAGSurface MakeOffscreen:CGSizeMake(pagFile.width, pagFile.height)];
    PAGPlayer *pagPlayer = [[PAGPlayer alloc] init];
    [pagPlayer setComposition:pagFile];
    [pagPlayer setSurface:pagSurface];
    
    //replacement
    NSString* filePath = [[NSBundle mainBundle] pathForResource:@"test" ofType:@"png"];
    NSImage* image = [[NSImage alloc] initWithContentsOfFile:filePath];
    CGRect rect = CGRectMake(0, 0, image.size.width, image.size.height);
    CVPixelBufferRef imageRef = [self pixelBufferFromCGImage:[image CGImageForProposedRect:&rect context:nil hints:nil]];
    
    PAGImage* pagImage;
    if (imageRef != 0) {
        pagImage = [PAGImage FromPixelBuffer:imageRef];
        [pagFile replaceImage:0 data:pagImage];
    }
    
    [pagPlayer setProgress:0.5f];
    [pagPlayer flush];
    
    CVPixelBufferRef result = [pagSurface makeSnapshot];
    NSLog(@"%@", result);
}

- (CVPixelBufferRef)pixelBufferFromCGImage:(CGImageRef)image
{
    CFDictionaryRef empty; // empty value for attr value.
    CFMutableDictionaryRef attrs;
    CVPixelBufferRef pixelBuffer;
    empty = CFDictionaryCreate(kCFAllocatorDefault, // our empty IOSurface properties dictionary
                               NULL,
                               NULL,
                               0,
                               &kCFTypeDictionaryKeyCallBacks,
                               &kCFTypeDictionaryValueCallBacks);
    attrs = CFDictionaryCreateMutable(kCFAllocatorDefault, 1,
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
                                                 CGImageGetHeight(image), 8, CVPixelBufferGetBytesPerRow(pixelBuffer), rgbColorSpace,
                                                 kCGImageAlphaNoneSkipLast);
    NSParameterAssert(context);
    
    CGContextDrawImage(context, CGRectMake(0, 0, CGImageGetWidth(image),
                                           CGImageGetHeight(image)), image);
    CGColorSpaceRelease(rgbColorSpace);
    CGContextRelease(context);
    
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
    return pixelBuffer;
}


@end
