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
#import <libpag/PAGVideoDecoder.h>

@interface SlimViewController () <UITabBarControllerDelegate>
@property (nonatomic, strong) UIImageView *imageView;
@property (nonatomic, strong) UILabel *label;
@property (weak, nonatomic) IBOutlet BackgroundView *bgView;
@property (nonatomic, strong) UITabBarController *tabBarController;

@end

@implementation SlimViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.tintColor = [UIColor colorWithRed:0.00 green:0.35 blue:0.85 alpha:1.00];
    
    [self loadTabBar];
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
}

@end
