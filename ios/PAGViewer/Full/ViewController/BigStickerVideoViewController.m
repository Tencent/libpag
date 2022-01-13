//
//  BigStickerVideoViewController.m
//  pag-ios
//
//  Created by kevingpqi on 2021/2/3.
//  Copyright © 2021 kevingpqi. All rights reserved.
//

#import "BigStickerVideoViewController.h"

@interface BigStickerVideoViewController ()

@end

@implementation BigStickerVideoViewController

- (instancetype)initWithCoder:(NSCoder *)coder {
    if (self = [super initWithCoder:coder]) {
        self.stickerMode = Sticker_BIG;
    }
    return self;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view.
}

@end
