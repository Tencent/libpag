#import "BaseViewController.h"
#import "BackgroundView.h"

@interface BaseViewController ()
@property (strong, nonatomic) BackgroundView* backgroundView;
@end

@implementation BaseViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    [self initPAGView];
    
    [self initBackgroundView];

}

- (void)touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event {
    if ([self.pagView isPlaying]) {
        [self.pagView stop];
    } else {
        [self.pagView play];
    }
    
}


/**
 初始化PAGView
 */
- (void)initPAGView {
    //读取PAG素材文件
    NSString* resourcePath = [self resourcePath];
    self.pagFile = [PAGFile Load:resourcePath];
    
    //创建PAG播放视图PAGView
    self.pagView = [[PAGView alloc] initWithFrame:self.view.bounds];
    [self.view addSubview:self.pagView];
    
    //关联PAGView和PAGFile
    [self.pagView setComposition:self.pagFile];
    //3.0中pagView中可以使用setPath直接使用file，setPath在pag中会根据path缓存pagFile，实现一次加载多个地方调用
    //[self.pagView setPath:resourcePath];
    //3.0中 [self.pagView setFile:self.pagFile]方法被废弃，如果使用setFile则无法通过file对替换数据进行控制;
    
    //设置循环播放，默认只播放一次
    [self.pagView setRepeatCount:-1];
    
    //播放
    [self.pagView play];
    
    
    [self.view sendSubviewToBack:self.pagView];
}



/**
 初始化背景视图，具体使用时无需添加
 */
- (void)initBackgroundView {
    self.backgroundView = [[BackgroundView alloc] initWithFrame:self.view.bounds];
    [self.view addSubview:self.backgroundView];
    [self.view sendSubviewToBack:self.backgroundView];
}

- (NSString*)resourcePath {
    return [[NSBundle mainBundle] pathForResource:@"replacement" ofType:@"pag"];
}

@end
