//
//  FlutterPagPlugin.m
//  FlutterPagPlugin
//
//  Created by 黎敬茂 on 2022/3/14.
//  Copyright © 2022 Tencent. All rights reserved.
//
#import "FlutterPagPlugin.h"
#import "TGFlutterPagRender.h"
#import "TGFlutterPagDownloadManager.h"

/**
 FlutterPagPlugin，处理flutter MethodChannel约定的方法
 */
@interface FlutterPagPlugin()

/// flutter引擎注册的textures对象
@property(nonatomic, weak) NSObject<FlutterTextureRegistry>* textures;

/// flutter引擎注册的registrar对象
@property(nonatomic, weak) NSObject<FlutterPluginRegistrar>* registrar;

/// 保存textureId跟render对象的对应关系
@property (nonatomic, strong) NSMutableDictionary *renderMap;

/// pag对象的缓存
@property (nonatomic, strong)NSCache<NSString*, NSData *> *cache;

@end

@implementation FlutterPagPlugin
+ (void)registerWithRegistrar:(NSObject<FlutterPluginRegistrar>*)registrar {
  FlutterMethodChannel* channel = [FlutterMethodChannel
      methodChannelWithName:@"flutter_pag_plugin"
            binaryMessenger:[registrar messenger]];
  FlutterPagPlugin* instance = [[FlutterPagPlugin alloc] init];
    instance.textures = registrar.textures;
    instance.registrar = registrar;
  [registrar addMethodCallDelegate:instance channel:channel];
}

- (void)getLayersUnderPoint:(id)arguments result:(FlutterResult _Nonnull)result {
    NSNumber* textureId = arguments[@"textureId"];
    NSNumber* x = arguments[@"x"];
    NSNumber* y = arguments[@"y"];
    TGFlutterPagRender *render = [_renderMap objectForKey:textureId];
    NSArray<NSString *> *names = [render getLayersUnderPoint:CGPointMake(x.doubleValue, y.doubleValue)];
    result(names);
}

- (void)release:(id)arguments result:(FlutterResult _Nonnull)result {
    NSNumber* textureId = arguments[@"textureId"];
    if(textureId == nil){
        result(@{});
        return;
    }
    TGFlutterPagRender *render = [_renderMap objectForKey:textureId];
    [render releaseRender];
    [_renderMap removeObjectForKey:textureId];
    result(@{});
}

- (void)setProgress:(id)arguments result:(FlutterResult _Nonnull)result {
    NSNumber* textureId = arguments[@"textureId"];
    if(textureId == nil){
        result(@{});
        return;
    }
    double progress = 0.0;
    if (arguments[@"progress"]) {
        progress = [arguments[@"progress"] doubleValue];
    }
    TGFlutterPagRender *render = [_renderMap objectForKey:textureId];
    [render setProgress:progress];
    result(@{});
}

- (void)pause:(id)arguments result:(FlutterResult _Nonnull)result {
    NSNumber* textureId = arguments[@"textureId"];
    if(textureId == nil){
        result(@{});
        return;
    }
    TGFlutterPagRender *render = [_renderMap objectForKey:textureId];
    [render pauseRender];
    result(@{});
}

- (void)stop:(id)arguments result:(FlutterResult _Nonnull)result {
    NSNumber* textureId = arguments[@"textureId"];
    if(textureId == nil){
        result(@{});
        return;
    }
    TGFlutterPagRender *render = [_renderMap objectForKey:textureId];
    [render stopRender];
    result(@{});
}

- (void)start:(id)arguments result:(FlutterResult _Nonnull)result {
    NSNumber* textureId = arguments[@"textureId"];
    if(textureId == nil){
        result(@{});
        return;
    }
    TGFlutterPagRender *render = [_renderMap objectForKey:textureId];
    [render startRender];
    result(@{});
}

- (void)initPag:(id)arguments result:(FlutterResult _Nonnull)result {
    if (arguments == nil || (arguments[@"assetName"] == nil && arguments[@"url"] == nil)) {
        result(@-1);
        NSLog(@"showPag arguments is nil");
        return;
    }
    double initProgress = 0.0;
    if (arguments[@"initProgress"]) {
        initProgress = [arguments[@"initProgress"] doubleValue];
    }
    int repeatCount = -1;
    if(arguments[@"repeatCount"]){
        repeatCount = [[arguments objectForKey:@"repeatCount"] intValue];
    }
    
    BOOL autoPlay = NO;
    if(arguments[@"autoPlay"]){
        autoPlay = [[arguments objectForKey:@"autoPlay"] boolValue];
    }
    
    NSString* assetName = arguments[@"assetName"];
    NSData *pagData = nil;
    if ([assetName isKindOfClass:NSString.class] && assetName.length > 0) {
        NSString *key = assetName;
        pagData = [self getCacheData:key];
        if (!pagData) {
            NSString* resourcePath = [self.registrar lookupKeyForAsset:assetName];
            resourcePath = [[NSBundle mainBundle] pathForResource:resourcePath ofType:nil];
            
            pagData = [NSData dataWithContentsOfFile:resourcePath];
            [self setCacheData:key data:pagData];
            
        }
        [self pagRenderWithPagData:pagData progress:initProgress repeatCount:repeatCount autoPlay:autoPlay result:result];
    }
    NSString* url = arguments[@"url"];
    if ([url isKindOfClass:NSString.class] && url.length > 0) {
        NSURLSessionDownloadTask *task;
        [task resume];
        NSString *key = url;
        pagData = [self getCacheData:key];
        if (!pagData) {
            __weak typeof(self) weak_self = self;
            [TGFlutterPagDownloadManager download:url completionHandler:^(NSData * _Nonnull data, NSError * _Nonnull error) {
                if (data) {
                    [weak_self setCacheData:key data:pagData];
                    [weak_self pagRenderWithPagData:data progress:initProgress repeatCount:repeatCount autoPlay:autoPlay result:result];
                }else{
                    result(@"");
                }
            }];
        }else{
            [self pagRenderWithPagData:pagData progress:initProgress repeatCount:repeatCount autoPlay:autoPlay result:result];
        }
    }
}

- (void)handleMethodCall:(FlutterMethodCall*)call result:(FlutterResult)result {
    
    id arguments = call.arguments;
    if ([@"getPlatformVersion" isEqualToString:call.method]) {
        result([@"iOS " stringByAppendingString:[[UIDevice currentDevice] systemVersion]]);
    } else if([@"initPag" isEqualToString:call.method]){
        [self initPag:arguments result:result];
    } else if([@"start" isEqualToString:call.method]){
        [self start:arguments result:result];
    } else if([@"stop" isEqualToString:call.method]){
        [self stop:arguments result:result];
    } else if([@"pause" isEqualToString:call.method]){
        [self pause:arguments result:result];
    } else if([@"setProgress" isEqual:call.method]){
        [self setProgress:arguments result:result];
    } else if([@"release" isEqualToString:call.method]){
        [self release:arguments result:result];
    } else if([@"getLayersUnderPoint" isEqualToString:call.method]){
        [self getLayersUnderPoint:arguments result:result];
    } else {
        result(FlutterMethodNotImplemented);
    }
}

-(void)pagRenderWithPagData:(NSData *)pagData progress:(double)progress repeatCount:(int)repeatCount autoPlay:(BOOL)autoPlay result:(FlutterResult)result{
    __block int64_t textureId = -1;
    
    TGFlutterPagRender *render = [[TGFlutterPagRender alloc] initWithPagData:pagData progress:progress autoPlay:autoPlay frameUpdateCallback:^{
         [self.textures textureFrameAvailable:textureId];
    }];
    [render setRepeatCount:repeatCount];
    textureId = [self.textures registerTexture:render];
    if(_renderMap == nil){
      _renderMap = [[NSMutableDictionary alloc] init];
    }
    [_renderMap setObject:render forKey:@(textureId)];
    result(@{@"textureId":@(textureId), @"width":@([render size].width), @"height":@([render size].height)});
}

-(NSData *)getCacheData:(NSString *)key{
    return [self.cache objectForKey:key];
}

-(void)setCacheData:(NSString *)key data:(NSData *)data{
    if (data == nil || key == nil) {
        return;
    }
    [self.cache setObject:data forKey:key cost:data.length];
}

-(NSCache *)cache{
    if (!_cache) {
        _cache = [[NSCache alloc] init];
        ///缓存64m
        _cache.totalCostLimit = 64*1024*1024;
        _cache.countLimit = 32;
    }
    return _cache;
}
@end
