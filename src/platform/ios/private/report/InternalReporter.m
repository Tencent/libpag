///////////////////////////////////////////////////////////////////////////////////////////////////
//
//  The MIT License (MIT)
//
//  Copyright (c) 2016-present, Tencent. All rights reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy of this software
//  and associated documentation files (the "Software"), to deal in the Software without
//  restriction, including without limitation the rights to use, copy, modify, merge, publish,
//  distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//      The above copyright notice and this permission notice shall be included in all copies or
//      substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
//  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
//  DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#import "InternalReporter.h"

#import <objc/runtime.h>
#import <objc/message.h>

@interface InternalReporter()
@property(nonatomic, strong) id beaconReport;
@property(nonatomic, assign) Class reportClass;
@end

@implementation InternalReporter

static InternalReporter* _instance = nil;

+ (InternalReporter*)shareInstance {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        _instance = [[self alloc] init];
    });
    return _instance;
}

- (Class)reportClass {
  if (!_reportClass) {
    _reportClass = NSClassFromString(@"BeaconReport");
  }
  return _reportClass;
}

- (id)beaconReport {
  if (!_beaconReport) {
    _beaconReport = [[self.reportClass alloc] init];
  }
  return _beaconReport;
}

- (void)reportData:(NSString*)event reportKey:(NSString*)reportKey params:(NSDictionary*)params {
  if (_reportCallBack) {
    _reportCallBack(event, reportKey, params);
    return;
  }
  static bool reflectStatus = true;
  if (!reflectStatus) {
    return;
  }
  Class eventClass = NSClassFromString(@"BeaconEvent");
  id beaconEvent = [[[eventClass alloc] init] autorelease];
  SEL sel = NSSelectorFromString(@"initWithAppKey:code:type:success:params:");
  Method method = class_getInstanceMethod(eventClass, sel);
  if (!method) {
    reflectStatus = false;
    return;
  }
  IMP imp = method_getImplementation(method);
  void* (*func)(id, SEL, NSString*, NSString*, int, BOOL, NSDictionary*) =
      (void* (*)(id, SEL, NSString*, NSString*, int, BOOL, NSDictionary*))imp;
  func(beaconEvent, sel, reportKey, event, 0, true, params);

  SEL selReport = NSSelectorFromString(@"reportEvent:");
  Method methodReport = class_getInstanceMethod(self.reportClass, selReport);
  if (!methodReport) {
    reflectStatus = false;
    return;
  }
  IMP impReport = method_getImplementation(methodReport);
  void* (*funcReport)(id, SEL, id) = (void* (*)(id, SEL, id))impReport;
  funcReport(self.beaconReport, selReport, beaconEvent);
}

- (void)dealloc {
  if (_reportCallBack) {
    [_reportCallBack release];
  }
  [_beaconReport release];
  [super dealloc];
}

@end
