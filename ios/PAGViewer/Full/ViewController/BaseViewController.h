#import <UIKit/UIKit.h>
#include <libpag/PAGView.h>

/**
 BaseViewController是Demo中的基类，该类实现了从路径加载素材并播放的基本功能。
 */
@interface BaseViewController : UIViewController

/**
 PAG显示视图
 */
@property (strong, nonatomic) PAGView* pagView;

/**
 PAG资源文件
 */
@property (strong, nonatomic) PAGFile* pagFile;

/**
 素材文件路径

 @return 素材文件路径
 */
- (NSString*)resourcePath;


@end

