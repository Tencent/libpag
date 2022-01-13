//
//  PAGTest.m
//  PAGViewer
//
//  Created by kevingpqi on 2019/1/22.
//  Copyright © 2019年 libpag.org. All rights reserved.
//

#import "PAGTest.h"
#import <libpag/PAGPlayer.h>
#import <libpag/PAGSurface.h>
#import <libpag/PAGFile.h>
#import "UIImage+CVPixelBufferRef.h"
#import "PAGTestUtils.h"
#import <CommonCrypto/CommonCryptor.h>
#import <CommonCrypto/CommonDigest.h>
#import "UIDevice+Hardware.h"

@interface PAGTest()
@property (nonatomic, strong) NSMutableDictionary *md5Dictionary;
@property (nonatomic, strong) NSMutableDictionary *resultDicitionary;
@property (nonatomic, strong) NSMutableDictionary *failedDicitionary;
@property (nonatomic, strong) UIImage *firstFailedImage;
@property (nonatomic, strong) NSString *firstFailedPagName;
@property (nonatomic, assign) NSUInteger firstFailedIndex;

@end

@implementation PAGTest

- (instancetype)init {
    self = [super init];
    if(self) {
        self.md5Dictionary = [NSMutableDictionary dictionary];
        self.resultDicitionary = [NSMutableDictionary dictionary];
        self.failedDicitionary = [NSMutableDictionary dictionary];
    }
    return self;
}

#pragma mark - Public Methods
- (void)doTest {
    NSArray<NSString *> *pagPathArray = [[NSBundle mainBundle] pathsForResourcesOfType:@"pag" inDirectory:@"md5"];
    if(pagPathArray.count == 0) {
        if([self.delegate respondsToSelector:@selector(testPAGFileNotFound)]) {
            [self.delegate testPAGFileNotFound];
        }
        return;
    }
    NSArray *pagNameArray = [self fileNameArrayFromPathArray:pagPathArray];
    NSDictionary *preMD5Dicitionary = [self imageMD5FromLocalJson];
    
    for(NSString *pagName in pagNameArray) {
        @autoreleasepool {
            [self testWithPAGName:pagName MD5Dictionary:preMD5Dicitionary];
        }
    }
    
    if (self.failedDicitionary.count == 0) {
        if([self.delegate respondsToSelector:@selector(testSuccess)]) {
            [self.delegate testSuccess];
            NSLog(@"---autoTest success!!! ----\n");
        }
    } else {
        NSString *filePath = [NSHomeDirectory() stringByAppendingString:@"/Documents/md5.json"];
        NSData *jsonData = [NSJSONSerialization dataWithJSONObject:self.resultDicitionary options:NSJSONWritingPrettyPrinted error:nil];
        [jsonData writeToFile:filePath atomically:YES];
        
        NSString *failedString = [self dictionaryToJson:self.failedDicitionary];
        NSLog(@"---autoTest failed information:%@ \n", failedString);
        
        if([self.delegate respondsToSelector:@selector(testFailed:withPagName:currentFrame:optional:)]) {
            [self.delegate testFailed:self.firstFailedImage withPagName:self.firstFailedPagName currentFrame:self.firstFailedIndex optional:self.failedDicitionary];
        }
    }
}

#pragma mark - Private Methods
- (void)testWithPAGName:(NSString *)pagName MD5Dictionary:(NSDictionary*)md5Dictionary {
    NSString *pagPath = [[NSBundle mainBundle] pathForResource:pagName ofType:@"pag" inDirectory:@"md5"];
    PAGFile *pagFile = [PAGFile Load:pagPath];
    
    NSArray<NSString *> *preImageMD5Array = [md5Dictionary objectForKey:pagName];
    NSMutableArray *imageMD5Array = [NSMutableArray new];
    NSMutableArray *failedIndexArray = [NSMutableArray new];
    
    NSInteger totalFrames = ceil(pagFile.duration * pagFile.frameRate / 1000000);
    
    NSUInteger index = 0;
    
    CGSize size = CGSizeMake(pagFile.width, pagFile.height);
    PAGSurface *pagSurface = [PAGSurface MakeFromGPU:size];
    PAGPlayer *pagPlayer = [[PAGPlayer alloc] init];
    [pagPlayer setComposition:pagFile];
    [pagPlayer setSurface:pagSurface];
    
    float frameRate = [pagFile frameRate];
    float maxFrameRate = [pagPlayer maxFrameRate];
    
    while([pagPlayer getProgress] < 1) {
        @autoreleasepool {
            if(index > 0) {
                if(maxFrameRate < frameRate && maxFrameRate > 0) {
                    totalFrames = ceilf(totalFrames * maxFrameRate / frameRate);
                }
                if(totalFrames <= 1) {
                    break;
                }
                int currentFrame = round([pagPlayer getProgress] * (totalFrames -1));
                currentFrame ++;
                if(currentFrame >= totalFrames) {
                    break;
                }
                double progress = currentFrame * 1.0 / (totalFrames - 1);
                [pagPlayer setProgress:progress];
            }
            
            [pagPlayer flush];
            
            CVPixelBufferRef pixelBuffer = [pagSurface getCVPixelBuffer];
            
            NSString *imageMD5 = [PAGTestUtils md5ForCVPixelBuffer:pixelBuffer];
            [imageMD5Array addObject:imageMD5];
            
            if (preImageMD5Array.count > index) {
                if (![imageMD5 isEqualToString:preImageMD5Array[index]]) {
                    [failedIndexArray addObject:[NSNumber numberWithInteger:index]];
                    if (!self.firstFailedImage) {
                        self.firstFailedImage = [UIImage uiImageFromPixelBuffer:pixelBuffer];
                        self.firstFailedIndex = index;
                        self.firstFailedPagName = pagName;
                    }
                }
            } else {
                [failedIndexArray addObject:[NSNumber numberWithInteger:index]];
                if (!self.firstFailedImage) {
                    self.firstFailedImage = [UIImage uiImageFromPixelBuffer:pixelBuffer];
                    self.firstFailedIndex = index;
                    self.firstFailedPagName = pagName;
                }
            }
            index++;
        }
    }
    
    if (failedIndexArray.count > 0) {
        [self.failedDicitionary setValue:failedIndexArray forKey:pagName];
    }
    
    [self.resultDicitionary setValue:imageMD5Array forKey:pagName];
}

/**
 Read data from json file

 @param json file name
 @return NSDictionary containing all MD5 datas for each frame
 */
- (NSDictionary *)imageMD5FromLocalJson {
    NSString *path = [[NSBundle mainBundle] pathForResource:@"md5" ofType:@"json"];
    if (!path) {
        return nil;
    }
    NSData *data = [NSData dataWithContentsOfFile:path options:NSDataReadingMappedIfSafe error:nil];
    NSDictionary *dataDictionary = [NSJSONSerialization JSONObjectWithData:data options:NSJSONReadingMutableContainers error:nil];
    return dataDictionary;
}

/**
 Get the file name through filePath

 @param pathArray
 @return fileNameArray
 */
- (NSArray<NSString *> *)fileNameArrayFromPathArray:(NSArray *)pathArray {
    NSMutableArray<NSString *> *fileNameArray = [NSMutableArray<NSString *> new];
    for(NSString *path in pathArray) {
        NSString *fileName = [[path lastPathComponent] stringByDeletingPathExtension];
        [fileNameArray addObject:fileName];
    }
    return [fileNameArray copy];
}

- (NSString*)dictionaryToJson:(NSDictionary *)dic{
    NSError *parseError = nil;
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:dic options:NSJSONWritingPrettyPrinted error:&parseError];
    return [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
}

@end
