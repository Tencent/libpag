//
//  VideoViewController.m
//  pag-ios
//
//  Created by kevingpqi on 2021/2/3.
//  Copyright © 2021 kevingpqi. All rights reserved.
//

#import "VideoViewController.h"
#import <AVFoundation/AVFoundation.h>
#import <Photos/Photos.h>
#import "Masonry.h"
#import "Toast.h"
#import "CustomVideoCompositing.h"

@interface VideoViewController ()
@property (nonatomic, strong) AVPlayer *player;
@property (nonatomic, strong) AVURLAsset *asset;
@property (nonatomic, strong) AVPlayerItem *playerItem;
@property (nonatomic, strong) AVPlayerLayer *playerLayer;
@property (nonatomic, strong) AVAssetExportSession *exportSession;
@property (nonatomic, strong) AVMutableVideoComposition *videoComposition;

@property (nonatomic, strong) NSString *exportPath;

@property (nonatomic, strong) UIButton *playButton;
@property (nonatomic, strong) UIButton *exportButton;

@property (nonatomic, assign) BOOL isExporting;
@end

@implementation VideoViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    [self commonInit];
}



#pragma mark - Private

- (void)commonInit {
    [self setupUI];
    [self setupPlayer];
}

- (void)setupUI {
    self.view.backgroundColor = [UIColor whiteColor];
    [self setupPlayButton];
    [self setupExportButton];
}

- (void)setupPlayButton {
    self.playButton = [[UIButton alloc] init];
    [self.view addSubview:self.playButton];
    [self.playButton mas_makeConstraints:^(MASConstraintMaker *make) {
        make.size.mas_equalTo(CGSizeMake(50, 50));
        make.centerX.equalTo(self.view).multipliedBy(0.5);
        make.top.equalTo(self.view).offset(self.view.frame.size.width + 120);
    }];
    [self configButton:self.playButton];
    [self.playButton setTitle:@"播放" forState:UIControlStateNormal];
    [self.playButton setTitle:@"暂停" forState:UIControlStateSelected];
    [self.playButton addTarget:self
                        action:@selector(playAction:)
              forControlEvents:UIControlEventTouchUpInside];
}

- (void)setupExportButton {
    self.exportButton = [[UIButton alloc] init];
    [self.view addSubview:self.exportButton];
    [self.exportButton mas_makeConstraints:^(MASConstraintMaker *make) {
        make.size.mas_equalTo(CGSizeMake(50, 50));
        make.centerX.equalTo(self.view).multipliedBy(1.5);
        make.top.equalTo(self.view).offset(self.view.frame.size.width + 120);
    }];
    [self configButton:self.exportButton];
    [self.exportButton setTitle:@"导出" forState:UIControlStateNormal];
    [self.exportButton addTarget:self
                          action:@selector(exportAction:)
                forControlEvents:UIControlEventTouchUpInside];
}

- (void)setupPlayer {
    // asset
    NSURL *url = [[NSBundle mainBundle] URLForResource:@"game" withExtension:@"mp4"];
    self.asset = [AVURLAsset assetWithURL:url];
    
    // videoComposition
    self.videoComposition = [self createVideoCompositionWithAsset:self.asset];
    self.videoComposition.customVideoCompositorClass = [CustomVideoCompositing class];
    
    
    // playerItem
    self.playerItem = [[AVPlayerItem alloc] initWithAsset:self.asset];
    self.playerItem.videoComposition = self.videoComposition;
    
    // player
    self.player = [[AVPlayer alloc] initWithPlayerItem:self.playerItem];
    
    // playerLayer
    self.playerLayer = [AVPlayerLayer playerLayerWithPlayer:self.player];
    self.playerLayer.frame = CGRectMake(0,
                                        80,
                                        self.view.frame.size.width,
                                        self.view.frame.size.width);
    [self.view.layer addSublayer:self.playerLayer];
}

- (AVMutableVideoComposition *)createVideoCompositionWithAsset:(AVAsset *)asset {
    AVMutableVideoComposition *videoComposition = [AVMutableVideoComposition videoCompositionWithPropertiesOfAsset:asset];
    NSArray *instructions = videoComposition.instructions;
    NSMutableArray *newInstructions = [NSMutableArray array];
    for (AVVideoCompositionInstruction *instruction in instructions) {
        NSArray *layerInstructions = instruction.layerInstructions;
        // TrackIDs
        NSMutableArray *trackIDs = [NSMutableArray array];
        for (AVVideoCompositionLayerInstruction *layerInstruction in layerInstructions) {
            [trackIDs addObject:@(layerInstruction.trackID)];
        }
        CustomVideoCompositionInstruction *newInstruction = [[CustomVideoCompositionInstruction alloc] initWithSourceTrackIDs:trackIDs timeRange:instruction.timeRange];
        newInstruction.stickerMode = self.stickerMode;
        newInstruction.layerInstructions = instruction.layerInstructions;
        [newInstructions addObject:newInstruction];
    }
    videoComposition.instructions = newInstructions;
    return videoComposition;
}

- (void)configButton:(UIButton *)button {
    [button setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    button.tintColor = [UIColor clearColor];
    [button.titleLabel setFont:[UIFont systemFontOfSize:14]];
    [button setBackgroundColor:[UIColor blackColor]];
    button.layer.cornerRadius = 5;
    button.layer.masksToBounds = YES;
}

#pragma mark - Action

- (void)playAction:(UIButton *)button {
    if (self.isExporting) {
        return;
    }
    
    button.selected = !button.selected;
    if (button.selected) {
        [self.player play];
    } else {
        [self.player pause];
    }
}

- (void)exportAction:(UIButton *)button {
    if (self.isExporting) {
        return;
    }
    self.isExporting = YES;
    
    // 先暂停播放
    [self.player pause];
    self.playButton.selected = NO;
    
    [self.view makeToastActivity:CSToastPositionCenter];
 
    // 创建导出任务
    self.exportSession = [[AVAssetExportSession alloc] initWithAsset:self.asset presetName:AVAssetExportPresetHighestQuality];
    self.exportSession.videoComposition = self.videoComposition;
    self.exportSession.outputFileType = AVFileTypeMPEG4;
    
    NSString *fileName = [NSString stringWithFormat:@"%f.m4v", [[NSDate date] timeIntervalSince1970] * 1000];
    self.exportPath = [NSTemporaryDirectory() stringByAppendingPathComponent:fileName];
    self.exportSession.outputURL = [NSURL fileURLWithPath:self.exportPath];
    
    __weak typeof(self) weakself = self;
    [self.exportSession exportAsynchronouslyWithCompletionHandler:^{
        [weakself saveVideo:weakself.exportPath completion:^(BOOL success) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [weakself.view hideToastActivity];
                if (success) {
                    [weakself.view.window makeToast:@"保存成功"];
                } else {
                    [weakself.view.window makeToast:@"保存失败"];
                }
                weakself.isExporting = NO;
            });
        }];
    }];
}

#pragma mark - Private

// 保存视频到相册
- (void)saveVideo:(NSString *)path completion:(void (^)(BOOL success))completion {
    void (^saveBlock)(void) = ^ {
        NSURL *url = [NSURL fileURLWithPath:path];
        [[PHPhotoLibrary sharedPhotoLibrary] performChanges:^{
            [PHAssetChangeRequest creationRequestForAssetFromVideoAtFileURL:url];
        } completionHandler:^(BOOL success, NSError * _Nullable error) {
            if (completion) {
                completion(success);
            }
        }];
    };
    
    PHAuthorizationStatus authStatus = [PHPhotoLibrary authorizationStatus];
    if (authStatus == PHAuthorizationStatusNotDetermined) {
        [PHPhotoLibrary requestAuthorization:^(PHAuthorizationStatus status) {
            if (status == PHAuthorizationStatusAuthorized) {
                saveBlock();
            } else {
                if (completion) {
                    completion(NO);
                }
            }
        }];
    } else if (authStatus != PHAuthorizationStatusAuthorized) {
        if (completion) {
            completion(NO);
        }
    } else {
        saveBlock();
    }
}

@end
