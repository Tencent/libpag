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

#import "DragFilePAGView.h"

@implementation DragFilePAGView

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self) {
        //注册文件拖动事件
        [self registerForDraggedTypes:[NSArray arrayWithObjects:NSPasteboardTypeFileURL, nil]];
    }
    return self;
}

- (void)dealloc {
    [self unregisterDraggedTypes];
}

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
    return NSDragOperationCopy;
}

//当文件在界面中放手
-(BOOL)prepareForDragOperation:(id<NSDraggingInfo>)sender{
    NSPasteboard *zPasteboard = [sender draggingPasteboard];
    NSArray *list = [zPasteboard readObjectsForClasses:@[[NSURL class]] options:nil];
    
    NSMutableArray<NSURL*> *urlList = [NSMutableArray array];
    for (NSObject *obj in list) {
        if ([obj isKindOfClass:[NSURL class]]) {
            [urlList addObject:(NSURL *)obj];
        } else if ([obj isKindOfClass:[NSString class]]) {
            NSURL *url = [NSURL fileURLWithPath:(NSString *)obj];
            [urlList addObject:url];
        }
    }
    if (urlList.count) {
        [self setPath:urlList.lastObject.path];
    }
    return YES;
}


@end
