//
//  DragFilePAGView.m
//  PAGViewer
//
//  Created by partyhuang(黄庆然) on 2020/10/9.
//  Copyright © 2020 im.pag. All rights reserved.
//

#import "DragFilePAGView.h"

@implementation DragFilePAGView

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self) {
        //注册文件拖动事件
        if (@available(macOS 10.13, *)) {
            [self registerForDraggedTypes:[NSArray arrayWithObjects:NSPasteboardTypeFileURL, nil]];
        } else if (@available(macOS 10.0, *)){
            // Fallback on earlier versions
            [self registerForDraggedTypes:[NSArray arrayWithObjects:NSFilenamesPboardType, nil]];
        }
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
    NSArray *list;
    if (@available(macOS 10.13, *)) {
        //获取的路径看不懂，需要通过bookmark才能转成能看懂的磁盘路径
        list = [zPasteboard readObjectsForClasses:@[[NSURL class]] options:nil];
    } else {
        list = [zPasteboard propertyListForType:NSFilenamesPboardType];
    }
    
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
