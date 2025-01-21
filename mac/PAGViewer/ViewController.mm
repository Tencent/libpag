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

#import "ViewController.h"
#import <libpag/PAGSurface.h>
#import "pag/pag.h"

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    NSTextField* textField = [[NSTextField alloc] initWithFrame:NSMakeRect(100, 100, 200, 50)];
    textField.stringValue = @"Memory Test";
    textField.editable = NO;
    textField.bordered = NO;
    textField.bezeled = NO;
    [self.view addSubview:textField];
    NSClickGestureRecognizer *clickGesture = [[NSClickGestureRecognizer alloc] initWithTarget:self action:@selector(handleClick:)];
    [textField addGestureRecognizer:clickGesture];
}

- (void)handleClick:(NSClickGestureRecognizer *)gesture {
    for (int i = 0; i < 10000; i++) {
        @autoreleasepool {
            auto surface = pag::PAGSurface::MakeOffscreen(1280, 720);
            int width = surface->width();
            surface->clearAll();
            printf("--width:%d ,index:%d\n", width, i);
        }
    }
    NSLog(@"handleClick\n");
}


@end
