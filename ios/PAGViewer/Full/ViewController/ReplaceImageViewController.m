#import "ReplaceImageViewController.h"

@interface ReplaceImageViewController ()<UINavigationControllerDelegate,UIImagePickerControllerDelegate>

@property (strong, nonatomic) UIImagePickerController* picker;

@end

@implementation ReplaceImageViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.picker = [[UIImagePickerController alloc]init];
    self.picker.delegate = self;
    self.picker.sourceType = UIImagePickerControllerSourceTypePhotoLibrary;
}


- (NSString*)resourcePath {
    return [[NSBundle mainBundle] pathForResource:@"replacement" ofType:@"pag"];
}

- (IBAction)pickPic:(id)sender {
    
    //获取可编辑图片图层个数
    int numImages = [self.pagFile numImages];
    if (numImages <= 0) {
        return;
    }
    
    if (![UIImagePickerController isSourceTypeAvailable:UIImagePickerControllerSourceTypePhotoLibrary]) {
        UIImage* image = [UIImage imageNamed:@"test"];
        [self replaceImage:image atIndex:0];
        return;
    }
    [self presentViewController:self.picker animated:YES completion:nil];
}

- (IBAction)reset:(id)sender {
    //获取可编辑图片图层个数
    int numImages = [self.pagFile numImages];
    if (numImages <= 0) {
        return;
    }
    
    //重置第0个图层的图像，这里可以设置的index为0到numImages-1
    [self.pagFile replaceImage:0 data:nil];
    
    //注意如果当时pagView不是播放状态，需要调用pagView的flush函数刷新一下界面，否则pagView会一直保持上一帧的状态
    if (![self.pagView isPlaying]) {
        [self.pagView flush];
    }
}


/**
 用image替换第index的imageLayer中的图像

 @param image 等待替换的图片
 @param index 待替换的imageLayer的index
 */
- (void)replaceImage:(UIImage*)image atIndex:(int)index {
    
    //通过image获取pagImage，pagImage还可以通过path/pixelBuffer等等途径获取，具体请看PAGImage类
    PAGImage* pagImage = [PAGImage FromCGImage:image.CGImage];
    
    //替换第0个图层的图像，这里可以设置的index为0到numImages-1
    [self.pagFile replaceImage:0 data:pagImage];
    
    //注意如果当时pagView不是播放状态，需要调用pagView的flush函数刷新一下界面，否则pagView会一直保持上一帧的状态
    if (![self.pagView isPlaying]) {
        [self.pagView flush];
    }
}

#pragma mark -- imagePicker

- (void)imagePickerController:(UIImagePickerController *)picker didFinishPickingMediaWithInfo:(NSDictionary<NSString *,id> *)info
{
    UIImage* image = info[UIImagePickerControllerOriginalImage];
    [picker dismissViewControllerAnimated:YES completion:nil];
    [self replaceImage:image atIndex:0];
}

- (void)imagePickerControllerDidCancel:(UIImagePickerController *)picker
{
    [picker dismissViewControllerAnimated:YES completion:nil];
}

@end
