//
//  CombinePAGViewController.m
//  pag-ios
//
//  Created by partyhuang(黄庆然) on 2019/12/30.
//  Copyright © 2019 kevingpqi. All rights reserved.
//

#import "CombinePAGViewController.h"
#import <libpag/PAGPlayer.h>

@interface CombinePAGViewController ()

@end

@implementation CombinePAGViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    PAGComposition* composition = [self makeComposition];
    [self.pagView setComposition:composition];
}


- (NSString *)resourcePath {
    return nil;
}


- (PAGComposition *)makeComposition {
    PAGComposition* compostion = [PAGComposition Make:self.view.bounds.size];
    ///data-TimeStretch是一个特殊pag，pag中标记了mark，使得在duration变化后可以填充满整个时长
    ///mark具体的数据可以看文档 https://pag.io/docs/time-stretch.html 如何使用时间伸缩插件
    PAGFile* file = [PAGFile Load:[[NSBundle mainBundle] pathForResource:@"data-TimeStretch" ofType:@"pag"]];
    [file replaceImage:0 data:[PAGImage FromPath:[[NSBundle mainBundle] pathForResource:@"test" ofType:@"png"]]];
    CGRect bounds = CGRectMake(0, 0, file.width, file.height);
    //pag目前只支持通过matrix控制Layer的位置
    [file setMatrix:[self.class transformFromRect:bounds toRect:CGRectMake(100, 0, 200, 400)]];
    //3.0接口设置时长，将时长扩大
    [file setDuration:10000000];
    //3.0接口设置开始时间
    [file setStartTime:3000000];
    //组装layer
    [compostion addLayer:file];
    
    file = [PAGFile Load:[[NSBundle mainBundle] pathForResource:@"data_video" ofType:@"pag"]];
    bounds = CGRectMake(0, 0, file.width, file.height);
    //普通pag设置时长，在超过最后一帧后会定格在最后一帧
    [file setDuration:10000000];
    [file setMatrix:[self.class transformFromRect:bounds toRect:self.view.bounds]];
    //组装layer
    [compostion addLayer:file atIndex:0];
    return compostion;
}


//transform 计算工具函数
+ (CGAffineTransform) transformFromRect:(CGRect)sourceRect toRect:(CGRect)finalRect {
    CGAffineTransform transform = CGAffineTransformIdentity;
    transform = CGAffineTransformTranslate(transform, -(CGRectGetMinX(sourceRect)-CGRectGetMinX(finalRect)), -(CGRectGetMinY(sourceRect)-CGRectGetMinY(finalRect)));
    transform = CGAffineTransformScale(transform, finalRect.size.width/sourceRect.size.width, finalRect.size.height/sourceRect.size.height);
    return transform;
}

/*
#pragma mark - Navigation

// In a storyboard-based application, you will often want to do a little preparation before navigation
- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
    // Get the new view controller using [segue destinationViewController].
    // Pass the selected object to the new view controller.
}
*/

@end
