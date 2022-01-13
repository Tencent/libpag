### Auto Test for libpag under iOS platform

#### 测试原理：
预置pag测试文件及其校验文件（校验文件为json文件，包含pag测试文件渲染时每一帧图像的md5值），在真机上运行，获取每一帧图片的md5值并与预置检验文件进行对比，如果全部相等，则测试通过。

pag的md5值与机型或系统有关

#### 使用说明：
1、选择PAGViewerTest Scheme；
2、使用真机测试；
3、测试文件的每一帧图片的md5值存在于json文件中
     pag测试文件路径：md5
     预置检验文件路径：Resource/Test/Verify/md5.json
4、测试结果反馈：
     测试通过：手机界面上会显示“Pass!”
     测试不通过： 手机界面上会显示”Failed!“，并显示相关错误信息

#### 错误信息汇总：
1、Failed! pag file not found !
     提示无pag文件
2、Failed! ***.pag** currentFrame:**
     提示pag文件校验失败，显示具体帧数
     显示多少个pag文件没有通过测试及具体pag文件名称
     如果需要知道更多信息，需要在log信息中查看

#### 新增或更新测试用例：
1、在Resource/Test/PAG目录下添加相关pag文件
2、在真机上运行，显示Failed
3、新生成的校验文件位于手机/Documents/md5.json
     文件获取方法：打开iTunes-手机-文件共享-PAGViewerTest，在"PAGViewerTest"的文稿中可以看到md5.json
     更新PAGViewer/Resources/Test/Verify/md5.json文件即可

