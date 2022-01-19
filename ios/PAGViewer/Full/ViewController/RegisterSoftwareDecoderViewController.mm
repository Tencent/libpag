#import "RegisterSoftwareDecoderViewController.h"
#import <libpag/PAGVideoDecoder.h>
#include <ffavc/ffavc.h>

static bool hasRegistered = false;
@implementation RegisterSoftwareDecoderViewController

- (void)viewDidLoad {
    if (!hasRegistered) {
        [self registerSoftwareDecoder];
    }
    [super viewDidLoad];
}

- (NSString *)resourcePath {
    return [[NSBundle mainBundle] pathForResource:@"particle_video" ofType:@"pag"];
}


/**
 SoftwareDecoder程序生命周期内注册一次即可，不需要多次注册。建议放在application启动时注册。
 软解注册功能是为了满足用户减少包体而设计，如果用户应用中本身有类似ffmpeg这种软解库，可以通过注册外部软解的方式，让pag提供不带软解的库，从而保证包体更小
 pag在使用过程中会优先使用硬件解码，之后是外部注册解码器，最后是自带软件解码（有些版本的pag库没有这个解码器），解码器要求支持x264解码。如果解码器不存在或者无法解码x264，则素材中的视频序列帧(videoComposition)会无法显示
 */
- (void)registerSoftwareDecoder {
    // 注册软件解码器工厂指针
    [PAGVideoDecoder RegisterSoftwareDecoderFactory:ffavc::DecoderFactory::GetHandle()];
    
    //注意，这里是为了测试需要把pag中同时使用的最大硬解数量设置为-1。正常使用中除非不希望使用硬解，否则不要设置为<=0的数
    [PAGVideoDecoder SetMaxHardwareDecoderCount:-1];
    hasRegistered = true;
}

@end
