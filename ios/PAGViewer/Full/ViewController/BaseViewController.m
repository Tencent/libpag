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
