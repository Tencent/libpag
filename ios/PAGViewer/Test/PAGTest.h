//
//  PAGTest.h
//  PAGViewer
//
//  Created by kevingpqi on 2019/1/22.
//  Copyright © 2019年 libpag.org. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@protocol PAGTestDelegate <NSObject>

@optional
- (void)testSuccess;
- (void)testFailed:(UIImage *)image withPagName:(NSString *)pagName currentFrame:(NSInteger)currentFrame optional:(NSDictionary *)options;
- (void)testPAGFileNotFound;

@end

@interface PAGTest : NSObject
@property (nonatomic, weak) id<PAGTestDelegate> delegate;

- (void)doTest;

@end
