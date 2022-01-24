//
//  PixelBufferDemoViewController.m
//  pag-ios
//
//  Created by partyhuang(黄庆然) on 2020/1/8.
//  Copyright © 2020 kevingpqi. All rights reserved.
//

#import <objc/runtime.h>

#import <libpag/PAGPlayer.h>
#import <libpag/PAGSurface.h>
#import <libpag/PAGFile.h>
#import <libpag/PAGTextLayer.h>
#import "PixelBufferDemoViewController.h"


@interface PixelBufferDemoViewController ()

@property (nonatomic, strong) PAGFile* file;

@property (nonatomic, strong) PAGSurface* surface;

@property (nonatomic, strong) PAGPlayer* player;

@property (nonatomic, strong) UIImageView* imageView;

@property (weak, nonatomic) IBOutlet UISlider *progressSlider;

@end


//工具方法，非libpag使用必须函数
@interface PixelBufferDemoViewController (Tool)
+ (UIImage*)imageFromCVPixelBufferRef:(CVPixelBufferRef)pixelBuffer;
@end

@implementation PixelBufferDemoViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    //初始化PAG组件
    [self initPAG];
    //替换数据demo方法
//    [self replaceImageAndText];
    //进度控制和渲染，result为渲染结果
    CVPixelBufferRef result = [self presentOnceWithProgress:0.5f];
    UIImage* image = [self.class imageFromCVPixelBufferRef:result];
    self.imageView = [[UIImageView alloc] initWithImage:image];
    [self.view addSubview:self.imageView];
    [self.view bringSubviewToFront:self.progressSlider];
}

- (IBAction)sliderValueChanged:(id)sender {
    float value = ((UISlider *)sender).value;
    __weak typeof(self) weakself = self;
    dispatch_async(dispatch_get_main_queue(), ^{
        CVPixelBufferRef result = [weakself presentOnceWithProgress:value];
        weakself.imageView.image = [weakself.class imageFromCVPixelBufferRef:result];
    });
}

- (void)initPAG {
    //文件加载
    NSString* path = [[NSBundle mainBundle] pathForResource:@"replacement" ofType:@"pag"];
    self.file = [PAGFile Load:path];
    //player是libpag3.0中的控制器，用于播放进度控制，与surface为一一对应的关系。
    //render之中的功能转移到了PAGPlayer和PAGFile中。如果需要绘制多个File的内容，可以使用PAGComposition组装PAGFile。具体请看{@link CombinePAGViewController}
    self.player = [[PAGPlayer alloc] init];
    //surface是libpag中的工作台，用于提供渲染环境
    self.surface = [PAGSurface MakeFromGPU:self.view.bounds.size];
    //绑定数据
    [self.player setSurface:self.surface];
    [self.player setComposition:self.file];
}

- (CVPixelBufferRef)presentOnceWithProgress:(float)value {
    [self.player setProgress:value];
    [self.player flush];
    return [self.surface getCVPixelBuffer];
}


- (void)replaceImageAndText {
    ///两种方式替换数据
    ///1.使用file通过editableIndex进行替换，如果editableIndex是同一个，那么对应的editableIndex的Layer的显示内容都会改变
    NSString* imagePath = [[NSBundle mainBundle] pathForResource:@"test" ofType:@"png"];
    PAGImage* image = [PAGImage FromPath:imagePath];
    [self.file replaceImage:0 data:image];
    
    PAGText* text = [self.file getTextData:0];
    text.text = @"测试数据";
    [self.file replaceText:0 data:text];
    
    ///2.使用Layer上的接口直接进行替换，只会修改对应Layer的数据，不会影响同一个editableIndex的其他Layer
    PAGImageLayer* imageLayer = (PAGImageLayer*)[self.file getLayersByEditableIndex:1 layerType:PAGLayerTypeImage].firstObject;
    [imageLayer replaceImage:image];
    
    PAGTextLayer* textLayer = (PAGTextLayer*)[self.file getLayersByEditableIndex:1 layerType:PAGLayerTypeText].firstObject;
    [textLayer setText:@"test"];
}
@end

@implementation PixelBufferDemoViewController (Tool)

+ (UIImage *)imageFromCVPixelBufferRef:(CVPixelBufferRef)pixelBuffer {
    CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    CIImage *ciImage = [CIImage imageWithCVPixelBuffer:pixelBuffer];
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
    CIContext *temporaryContext = [CIContext contextWithOptions:nil];
    CGImageRef videoImage = [temporaryContext
                             createCGImage:ciImage
                             fromRect:CGRectMake(0, 0,
                                                 CVPixelBufferGetWidth(pixelBuffer),
                                                 CVPixelBufferGetHeight(pixelBuffer))];
    
    UIImage *uiImage = [UIImage imageWithCGImage:videoImage];
    CGImageRelease(videoImage);
    return uiImage;
}

@end
