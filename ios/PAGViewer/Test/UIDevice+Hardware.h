//
//  UIDevice+Hardware.h
//  PAGViewer
//
//  Created by kevingpqi on 2019/5/14.
//  Copyright © 2019 libpag.org. All rights reserved.
//

#import <UIKit/UIKit.h>
// 设备型号的枚举值
typedef NS_ENUM(NSUInteger, DeviceType) {
    iPhone_1G = 0,
    iPhone_3G,
    iPhone_3GS,
    iPhone_4,
    iPhone_4_Verizon,
    iPhone_4S,
    iPhone_5_GSM,
    iPhone_5_CDMA,
    iPhone_5C_GSM,
    iPhone_5C_GSM_CDMA,
    iPhone_5S,
    iPhone_6,
    iPhone_6_Plus,
    iPhone_6S,
    iPhone_6S_Plus,
    iPhone_SE,
    iPhone_7,
    iPhone_7_Plus,
    iPhone_8,
    iPhone_8_Plus,
    iPhone_X,
    iPhone_XS,
    iPhone_XS_Max,
    iPhone_XR,
    
    iUnknown,
};

static NSString *iDeviceNameContainer[] = {
    [iPhone_1G]                 = @"iPhone_1G",
    [iPhone_3G]                 = @"iPhone_3G",
    [iPhone_3GS]                = @"iPhone_3GS",
    [iPhone_4]                  = @"iPhone_4",
    [iPhone_4_Verizon]          = @"iPhone_4",
    [iPhone_4S]                 = @"iPhone_4S",
    [iPhone_5_GSM]              = @"iPhone_5",
    [iPhone_5_CDMA]             = @"iPhone_5",
    [iPhone_5C_GSM]             = @"iPhone_5C",
    [iPhone_5C_GSM_CDMA]        = @"iPhone_5C",
    [iPhone_5S]                 = @"iPhone_5S",
    [iPhone_6]                  = @"iPhone_6",
    [iPhone_6_Plus]             = @"iPhone_6Plus",
    [iPhone_6S]                 = @"iPhone_6S",
    [iPhone_6S_Plus]            = @"iPhone_6SPlus",
    [iPhone_SE]                 = @"iPhone_SE",
    [iPhone_7]                  = @"iPhone_7",
    [iPhone_7_Plus]             = @"iPhone_7Plus",
    [iPhone_8]                  = @"iPhone_8",
    [iPhone_8_Plus]             = @"iPhone_8Plus",
    [iPhone_X]                  = @"iPhone_X",
    [iPhone_XS]                 = @"iPhone_XS",
    [iPhone_XS_Max]             = @"iPhone_XSMax",
    [iPhone_XR]                 = @"iPhone_XR",
    
    [iUnknown]                  = @"Unknown"
};

@interface UIDevice (Hardware)
/** 获取设备类型 */
+ (DeviceType)getDeviceType;
/** 获取设备名称 */
+ (NSString *)getDeviceName;

@end

