//
//  SmallStickerVideoViewController.m
//  pag-ios
//
//  Created by kevingpqi on 2021/2/3.
//  Copyright © 2021 kevingpqi. All rights reserved.
//

#import "SmallStickerVideoViewController.h"

@interface SmallStickerVideoViewController ()

@end

@implementation SmallStickerVideoViewController

- (instancetype)initWithCoder:(NSCoder *)coder {
    if (self = [super initWithCoder:coder]) {
        self.stickerMode = Sticker_SMALL;
    }
    return self;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view.
}

@end
