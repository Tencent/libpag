//
//  UIDevice+Hardware.m
//  PAGViewer
//
//  Created by kevingpqi on 2019/5/14.
//  Copyright © 2019 libpag.org. All rights reserved.
//

#import "UIDevice+Hardware.h"
#import "sys/utsname.h"

@implementation UIDevice (Hardware)

#pragma mark Public Methods
+ (DeviceType)getDeviceType{
    // 需要#import "sys/utsname.h"
    struct utsname systemInfo;
    uname(&systemInfo);
    NSString *machineString = [NSString stringWithUTF8String:systemInfo.machine];
    
    if ([machineString isEqualToString:@"iPhone1,1"])   return iPhone_1G;
    if ([machineString isEqualToString:@"iPhone1,2"])   return iPhone_3G;
    if ([machineString isEqualToString:@"iPhone2,1"])   return iPhone_3GS;
    if ([machineString isEqualToString:@"iPhone3,1"])   return iPhone_4;
    if ([machineString isEqualToString:@"iPhone3,3"])   return iPhone_4_Verizon;
    if ([machineString isEqualToString:@"iPhone4,1"])   return iPhone_4S;
    if ([machineString isEqualToString:@"iPhone5,1"])   return iPhone_5_GSM;
    if ([machineString isEqualToString:@"iPhone5,2"])   return iPhone_5_CDMA;
    if ([machineString isEqualToString:@"iPhone5,3"])   return iPhone_5C_GSM;
    if ([machineString isEqualToString:@"iPhone5,4"])   return iPhone_5C_GSM_CDMA;
    if ([machineString isEqualToString:@"iPhone6,1"])   return iPhone_5S;
    if ([machineString isEqualToString:@"iPhone6,2"])   return iPhone_5S;
    if ([machineString isEqualToString:@"iPhone7,2"])   return iPhone_6;
    if ([machineString isEqualToString:@"iPhone7,1"])   return iPhone_6_Plus;
    if ([machineString isEqualToString:@"iPhone8,1"])   return iPhone_6S;
    if ([machineString isEqualToString:@"iPhone8,2"])   return iPhone_6S_Plus;
    if ([machineString isEqualToString:@"iPhone8,4"])   return iPhone_SE;
    
    // 日行两款手机型号均为日本独占，可能使用索尼FeliCa支付方案而不是苹果支付
    if ([machineString isEqualToString:@"iPhone9,1"])   return iPhone_7;
    if ([machineString isEqualToString:@"iPhone9,2"])   return iPhone_7_Plus;
    if ([machineString isEqualToString:@"iPhone9,3"])   return iPhone_7;
    if ([machineString isEqualToString:@"iPhone9,4"])   return iPhone_7_Plus;
    if ([machineString isEqualToString:@"iPhone10,1"])  return iPhone_8;
    if ([machineString isEqualToString:@"iPhone10,4"])  return iPhone_8;
    if ([machineString isEqualToString:@"iPhone10,2"])  return iPhone_8_Plus;
    if ([machineString isEqualToString:@"iPhone10,5"])  return iPhone_8_Plus;
    if ([machineString isEqualToString:@"iPhone10,3"])  return iPhone_X;
    if ([machineString isEqualToString:@"iPhone10,6"])  return iPhone_X;
    if ([machineString isEqualToString:@"iPhone11,2"])  return iPhone_XS;
    if ([machineString isEqualToString:@"iPhone11,4"] || [machineString isEqualToString:@"iPhone11,6"])  return iPhone_XS_Max;
    if ([machineString isEqualToString:@"iPhone11,8"])  return iPhone_XR;
    
    return iUnknown;
}

+ (NSString *)getDeviceName {
    return [iDeviceNameContainer[[UIDevice getDeviceType]] copy];
}


@end
