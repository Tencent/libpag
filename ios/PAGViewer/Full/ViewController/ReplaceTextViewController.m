#import "ReplaceTextViewController.h"

@interface ReplaceTextViewController () {
    BOOL isEndingInput;
}

@end

@implementation ReplaceTextViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    isEndingInput = NO;
}

- (NSString*)resourcePath {
    return [[NSBundle mainBundle] pathForResource:@"test2" ofType:@"pag"];
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self.view endEditing:YES];
    if (isEndingInput) {
        isEndingInput = NO;
    } else {
        [super touchesBegan:touches withEvent:event];
    }
}

- (IBAction)doneClick:(id)sender {
    
}

- (IBAction)endInput:(UITextField*)sender {
    isEndingInput = YES;
    
    //获取可编辑文字图层个数
    int numTexts = [self.pagFile numTexts];
    if (numTexts <= 0) {
        return;
    }
    
    //获取第0个图层的文字信息
    PAGText* text = [self.pagFile getTextData:0];
    text.text = sender.text;
    
    //这里除了设置text以外，还可以设置字体，斜体，加粗，字号等等信息，具体请看PAGText类
    
    //设置第0个图层的文字信息，这里可以设置的index为0到numText-1
    [self.pagFile replaceText:0 data:text];
    
    //注意如果当时pagView不是播放状态，需要调用pagView的flush函数刷新一下界面，否则pagView会一直保持上一帧的状态
    if (![self.pagView isPlaying]) {
        [self.pagView flush];
    }
}

- (IBAction)reset:(id)sender {
    //获取可编辑文字图层个数
    int numTexts = [self.pagFile numTexts];
    if (numTexts <= 0) {
        return;
    }
    
    //重置第0个图层的文字信息，这里可以设置的index为0到numText-1
    [self.pagFile replaceText:0 data:nil];
    
    //注意如果当时pagView不是播放状态，需要调用pagView的flush函数刷新一下界面，否则pagView会一直保持上一帧的状态
    if (![self.pagView isPlaying]) {
        [self.pagView flush];
    }
}

@end
