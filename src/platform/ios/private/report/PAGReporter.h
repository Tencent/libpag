/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#import <Foundation/Foundation.h>

/**
 * PAG statistical report data
 * @param event The Report event
 * @param reportKey PAG beacon reporting unique identification
 * @param params PAG report data
 */
typedef void (^OnReportCallBack)(NSString* event, NSString* reportKey, NSDictionary* params);

@interface PAGReporter : NSObject
/**
 * Get statistical report data
 * Once transferred to other application scenarios, you need to reset when returning
 */
+ (void)SetReportCallBack:(OnReportCallBack)reportCallBack;

@end
