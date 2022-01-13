//
//  PAGTestUtils.m
//  PAGViewer
//
//  Created by kevingpqi on 2019/1/21.
//  Copyright © 2019年 libpag.org. All rights reserved.
//

#import "PAGTestUtils.h"
#import <CommonCrypto/CommonCryptor.h>
#import <CommonCrypto/CommonDigest.h>
#import "UIDevice+Hardware.h"

@implementation PAGTestUtils

#pragma mark - Public Methods
+ (NSString *)md5ForCVPixelBuffer:(CVPixelBufferRef)cvPixelBuffer {
    size_t length = CVPixelBufferGetDataSize(cvPixelBuffer);
    CVPixelBufferLockBaseAddress(cvPixelBuffer, 0);
    void *baseAddress = CVPixelBufferGetBaseAddress(cvPixelBuffer);
    unsigned char digist[CC_MD5_DIGEST_LENGTH]; //CC_MD5_DIGEST_LENGTH = 16
    CC_MD5(baseAddress, (uint)(length), digist);
    NSMutableString* outPutStr = [NSMutableString stringWithCapacity:10];
    for(int  i =0; i<CC_MD5_DIGEST_LENGTH;i++){
        [outPutStr appendFormat:@"%02x", digist[i]];//小写x表示输出的是小写MD5，大写X表示输出的是大写MD5
    }
    CVPixelBufferUnlockBaseAddress(cvPixelBuffer, 0);
    return [outPutStr lowercaseString];
}

+ (BOOL)isTestTarget {
    if([[self getBundleID] isEqualToString:@"im.pag.viewer.test"]) {
        return YES;
    }
    return false;
}

+ (NSString *)getFloderName {
    NSString *deviceName = [UIDevice getDeviceName];
    NSString *configPath = [[NSBundle mainBundle] pathForResource:@"config" ofType:@"plist"];
    NSDictionary *configDict = [[NSDictionary alloc] initWithContentsOfFile:configPath];
    
    return [configDict objectForKey:deviceName];
}

#pragma mark - Private Methods
+ (NSString *)getBundleID {
    static NSString *bundleID = nil;
    static dispatch_once_t onceToken;
    
    dispatch_once(&onceToken, ^(void) {
        bundleID = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleIdentifier"];
    });
    
    return bundleID;
}


@end
