//
//  TGFlutterPagDownloadManager.h
//  flutter_pag_plugin
//
//  Created by 黎敬茂 on 2022/3/14.
//  Copyright © 2022 Tencent. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/**
 Pag文件下载工具
 */
@interface TGFlutterPagDownloadManager : NSObject

///下载pag文件
+(void)download:(NSString *)urlStr completionHandler:(void (^)(NSData * data, NSError * error))handler;

@end

NS_ASSUME_NONNULL_END
