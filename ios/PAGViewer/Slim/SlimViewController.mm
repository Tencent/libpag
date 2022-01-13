//
//  SlimViewController.m
//  PAGViewer
//
//  Created by dom on 25/09/2017.
//  Copyright © 2017 idom.me. All rights reserved.
//

#import "SlimViewController.h"
#import "PAGPlayerView.h"
#import "BackgroundView.h"
#import "PAGTestUtils.h"
#import "PAGTest.h"
#import <libpag/PAGVideoDecoder.h>

@interface SlimViewController () <UITabBarControllerDelegate, PAGTestDelegate>
@property (nonatomic, strong) PAGTest *pagTest;
@property (nonatomic, strong) UIImageView *imageView;
@property (nonatomic, strong) UILabel *label;
@property (weak, nonatomic) IBOutlet BackgroundView *bgView;
@property (nonatomic, strong) UITabBarController *tabBarController;

@end

@implementation SlimViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.tintColor = [UIColor colorWithRed:0.00 green:0.35 blue:0.85 alpha:1.00];
    
    // 测试软解
//    [PAGVideoDecoder SetMaxHardwareDecoderCount:0];
    if ([PAGTestUtils isTestTarget]) {
        self.view.backgroundColor = [UIColor grayColor];
        self.bgView.hidden = true;
        PAGTest *pagTest = [PAGTest new];
        pagTest.delegate = self;
        self.pagTest = pagTest;
        [self initView];
    } else {
        [self loadTabBar];
    }
}

- (void)loadTabBar {
    _tabBarController = [[UITabBarController alloc] init];
    _tabBarController.delegate = self;
    
    CGRect screenBounds = [[UIScreen mainScreen] bounds];
    
    UIViewController *vc1 = [[UIViewController alloc] init];
    UITabBarItem *item1 = [[UITabBarItem alloc] init];
    item1.title = @"test2.pag";
    [item1 setTitleTextAttributes:@{NSFontAttributeName:[UIFont systemFontOfSize:30]} forState:UIControlStateNormal];
    vc1.tabBarItem = item1;
    
    PAGPlayerView *uiView1 = [[PAGPlayerView alloc] initWithFrame:screenBounds];
    uiView1.userInteractionEnabled = TRUE;
    
    [vc1.view addSubview:uiView1];
    
    UIViewController *vc2 = [[UIViewController alloc] init];
    UITabBarItem *item2 = [[UITabBarItem alloc] init];
    item2.title = @"particle_video.pag";
    item2.image = [UIImage imageNamed:@"ffmpeg"];
    [item2 setTitleTextAttributes:@{NSFontAttributeName:[UIFont systemFontOfSize:30]} forState:UIControlStateNormal];
    vc2.tabBarItem = item2;
    
    PAGPlayerView *uiView2 = [[PAGPlayerView alloc] initWithFrame:screenBounds];
    uiView2.userInteractionEnabled = TRUE;
    
    [vc2.view addSubview:uiView2];
    
    _tabBarController.viewControllers = [NSArray arrayWithObjects:vc1, vc2, nil];
    
    [self.view addSubview:_tabBarController.view];
    NSString* pagPath = _tabBarController.selectedViewController.tabBarItem.title;
    [uiView1 loadPAGAndPlay: pagPath];
}

- (BOOL)tabBarController:(UITabBarController *)tabBarController shouldSelectViewController:(UIViewController *)viewController {
    PAGPlayerView *oldView = (PAGPlayerView *)tabBarController.selectedViewController.view.subviews.firstObject;
    [oldView stop];

    NSUInteger selectedIndex = [_tabBarController selectedIndex]==0?1:0;
    [PAGVideoDecoder SetMaxHardwareDecoderCount:(selectedIndex == 0 ? 15 : 0)];
    UIViewController *controller = [_tabBarController.viewControllers objectAtIndex:selectedIndex];
    NSString* pagPath = controller.tabBarItem.title;
    PAGPlayerView *newView = (PAGPlayerView *)viewController.view.subviews.firstObject;
    [newView loadPAGAndPlay: pagPath];
    return YES;
}

- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    if (self.pagTest) {
        [self.pagTest doTest];
    }
}

#pragma mark - Private Methods
- (void)initView {
    UIImageView *imageView = [[UIImageView alloc] initWithFrame:CGRectMake((self.view.frame.size.width - 300)/2.0, (self.view.frame.size.height - 300 - 96 - 20)/2.0, 300, 300)];
    [self.view addSubview:imageView];
    self.imageView = imageView;
    [self.imageView setImage:[UIImage imageNamed:@"loading.jpeg"]];
    imageView.layer.borderWidth = 0.5;
    imageView.layer.borderColor = [[UIColor whiteColor] CGColor];
    imageView.contentMode = UIViewContentModeScaleAspectFit;
    
    UILabel *lable = [[UILabel alloc] initWithFrame:CGRectMake(10, imageView.frame.origin.y + imageView.frame.size.height + 20, self.view.frame.size.width - 20, 144)];
    [lable setFont:[UIFont fontWithName:@"PingFangSC-Semibold" size:14]];
    [lable setTextColor:[UIColor whiteColor]];
    [self.view addSubview:lable];
    self.label = lable;
    self.label.lineBreakMode = NSLineBreakByWordWrapping;
    self.label.numberOfLines = 0;
    lable.textAlignment = NSTextAlignmentCenter;
    lable.text = @"测试中...";
}

- (NSString *)arrayToJsonString:(NSArray *)array {
    NSError *error = nil;
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:array options:NSJSONWritingPrettyPrinted error:&error];
    
    return [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];;
}

#pragma mark - PAGTestDelegate
- (void)testSuccess {
    self.label.text = @"Pass!";
    [self.label setTextColor:[UIColor greenColor]];
    [self.imageView setImage:[UIImage imageNamed:@"success.jpeg"]];
}

- (void)testFailed:(UIImage *)image withPagName:(NSString *)pagName currentFrame:(NSInteger)currentFrame optional:(NSDictionary *)options {
    [self.imageView setImage:image];
    NSString *failedString = [NSString stringWithFormat:@"Failed!\nFileName:%@.pag currentFrame:%ld \n", pagName, (long)currentFrame];
    NSArray *allKeys = [options allKeys];
    failedString = [failedString stringByAppendingFormat:@"%ld pag files test failed! \n Failed pag files:%@", options.count, [self arrayToJsonString:allKeys]];
    self.label.text = failedString;
}

- (void)testPAGFileNotFound{
    self.label.text = [NSString stringWithFormat:@"Failed!\n pag file not found!"];
    [self.label setTextColor:[UIColor whiteColor]];
    [self.imageView setImage:[UIImage imageNamed:@"fail.jpeg"]];
}

@end
