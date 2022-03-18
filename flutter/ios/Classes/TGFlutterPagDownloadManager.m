//
//  TGFlutterPagDownloadManager.m
//  flutter_pag_plugin
//
//  Created by 黎敬茂 on 2022/3/14.
//  Copyright © 2022 Tencent. All rights reserved.
//

#import "TGFlutterPagDownloadManager.h"
#import <CommonCrypto/CommonDigest.h>

#define CACHE_DIR @"pagCache"

@implementation TGFlutterPagDownloadManager

+(void)download:(NSString *)urlStr completionHandler:(void (^)(NSData * data, NSError * error))handler{
    NSString *cachePath = [TGFlutterPagDownloadManager cachePath:urlStr];
    if(cachePath == nil){
        handler(nil, [[NSError alloc] initWithDomain:NSURLErrorDomain code:-1 userInfo:nil]);
        return;
    }
    if ([[NSFileManager defaultManager] fileExistsAtPath:cachePath]) {
        NSData *data = [NSData dataWithContentsOfFile:cachePath];
        if (data) {
            handler(data, nil);
            return;
        }
        
    }
    NSURL *url = [NSURL URLWithString:urlStr];
    NSURLRequest *request = [NSURLRequest requestWithURL:url];
    NSURLSessionDataTask *dataTask = [[NSURLSession sharedSession]
                                      dataTaskWithRequest:request
                                      completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
        handler(data, error);
        if (data) {
            [TGFlutterPagDownloadManager saveData:data path:cachePath];
        }
    }];
    [dataTask resume];
}

+(NSString *)cacheDir{
    NSArray *cachePaths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
    static NSString *cacheDir;
    if (!cacheDir) {
        cacheDir= [NSString stringWithFormat:@"%@/%@", cachePaths.firstObject, CACHE_DIR];
    }
    return cacheDir;
}

+(NSString *)cachePath:(NSString *)url{
    if (![url isKindOfClass:NSString.class] || url.length <= 0) {
        return nil;
    }
    
    NSString *key = [TGFlutterPagDownloadManager md5:url];
    return [NSString stringWithFormat:@"%@/%@", [self cacheDir], key];
}

+(NSString *)md5:(NSString *)str{
    const char* input = [str UTF8String];
    unsigned char result[CC_MD5_DIGEST_LENGTH];
    CC_MD5(input, (CC_LONG)strlen(input), result);
    
    NSMutableString *digest = [NSMutableString stringWithCapacity:CC_MD5_DIGEST_LENGTH * 2];
    for (NSInteger i = 0; i < CC_MD5_DIGEST_LENGTH; i++) {
        [digest appendFormat:@"%02x", result[i]];
    }
    return digest;
}

+(void)saveData:(NSData *)data path:(NSString *)cachePath{
    NSString *cahceDir = [TGFlutterPagDownloadManager cacheDir];
    if (![[NSFileManager defaultManager] fileExistsAtPath:cachePath]) {
        [[NSFileManager defaultManager] createDirectoryAtPath:cahceDir  withIntermediateDirectories:YES attributes:nil error:nil];
    }
    [data writeToFile:cachePath atomically:YES];
}

@end
