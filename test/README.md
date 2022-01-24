# PAG测试框架使用指南

[TOC]

## 框架

PAG C++  测试框架使用了Google的开源框架[GTest](<https://github.com/google/googletest>)，用来测试相关的API，场景，压力测试等相关模块。

PAG 客户端侧，使用各端默认测试框架

## C++框架使用

> 测试用例位于根目录下test目录，test目录下平铺相应的测试代码，子目录framework为框架相关代码，r子目录es为测试使用的
>
> 测试用例会自动被注册到测试框架

测试文件的代码结构如下

```c++
#include "nlohmann/json.hpp"
#include "framework/pag_test.h"
#include "framework/utils/PAGTestUtils.h"
namespace pag {
    //声明一个测试的SUIT，所有PAG_TEST_F中传入PAGTestDemo的测试用例都会共享一个PAG环境
    PAG_TEST_SUIT(PAGTestDemo)
    //声明一个测试的CASE，所有PAG_TEST_F中传入PAGTestDemo的测试用例都会重新初始化一个PAG环境
    //PAG_TEST_CASE(PAGTESTDemo)
    PAG_TEST_F(PAGTestDemo, case) {
        // 使用DumpMD5()来生成MD5，合并为一个json，赋值给 PAGTestEnvironment::DumpJson["PAGTestDemo"]
        //如果定义了COMPARE_JSON_PATH，则需要读取PAGTestEnvironment::CompareJson中相应key来比较
    }
    // 声明一个测试的CASE，不会做其他初始化操作，但是PAGTestEnvironment相关的还是可以使用
    PAG_TEST(SimpleTestDemo, case) {
    
    }
}
```



框架预设有 ASSERT和EXPECT相关宏来做校验

- ASSERT 如果当前case 没有满足，则中止执行本case
- EXPECT  如果当前case没有满足，会继续往下执行本case

### MD5校验

> 对于跟渲染相关的测试case，需要渲染一个图片，然后对图片做MD5，把MD5跟基准比较来确认是否渲染正确，json库的使用可以[查看手册](<https://github.com/nlohmann/json>)

- 跑测试用例的时候，会在 test/out/ 目录生成compare_dump.json文件，每次重新跑都会覆盖，只生成跟本次跑的case相关的json。

- 对于新写的接口，是没有历史的基准MD5的，所以需要人工来确认是否正确，在执行代码中间插入截图的命令，生成一个png，然后人工来确认是否正确绘制。

```
// 会在test/out目录生成一个png图片
void SnapImageToFile(PAGSurface* pagSurface, std::string fileName)
```

- 当本次测试编写完毕，并确认所有用例通过时候，跑一次全量测试，把生成的compare_dump.json文件放到 test/res/compare_dump.json，作为后续的基准比较文件



**所以在编写代码时候，要注意把MD5值写入到对应的节点，同时要编写和基准json对比的代码**

### Clion

在clion中选择pag-test，然后点击运行，即可看到Clion下面的Run窗口的输出接口，输出窗口又可以选择把接口以xml形式导出。

debug模式也可以正常生效，断点后，debug即可按正常流程来调试

默认运行的是所有case，如果只需要执行某个Suit， 只需要点击运行按钮旁边的配置按钮，然后在Suite中输入相关名字，输入的时候Clion有自动提示，方便选择。

## 平台渲染框架
> Android测使用Android标准测试框架，测试代码位于androidTest/java/libpag/pagviewer，和java包平级。
>
> 测试的文件位于根目录 resources/md5/ 此目录被映射为androidTest的assets目录

### Android 框架使用

> 系统通过注解来识别测试的类

- @Test 此注解代表本函数是一个测试用例，此注解会在代码旁边生成一个小三角按钮，点击即可直接执行用例
- @Before 此注解代表，本函数是测试用例执行前的初始化函数
- @After 此注解代码，本函数是测试用例执行后的销毁函数

### PAGFile的MD5渲染校验
> 此校验会在真机上跑resources/md5/目录下的pag测试文件，渲染每帧并对图像做MD5，用MD5对比来确认平台渲染的正常

#### 校验具体流程

在稳定版本的代码工程中

- 在AndroidStudio中，打开 app/src/androidTest/java/libpag/pagviewer/PAGFullMd5Test.kt文件。
- 点击上面文件中，"class" 字母左边的小箭头按钮，点击后，默认会执行testAllPAGFileCase用例。
- 当执行成功以后会在手机的SDCard中生成一个md5.json的文件，此文件存放本次的渲染结果。

在要测试的代码工程中

- 把上步骤中的md5.json重命名为compare_md5.json, 并放入源码中的 resources/md5/ 目录下。
- 在AndroidStudio中，打开 app/src/androidTest/java/libpag/pagviewer/PAGFullMd5Test.kt。
- 点击上面文件中，"class" 字母左边的小箭头按钮，点击后，默认会执行testAllPAGFileCase用例。
- 如果渲染正确，测试用例会正常通过。渲染异常，会在相应面板输出相关信息：渲染异常的文件，每个文件中相应的帧渲染异常的帧号。
- 自动校验的逻辑可以简单理解为，本次生成的md5.json和用户手动放入的compare_md5.jon文件对比

#### md5.json文件格式
```json
{
    "fingerprint": "google\/hammerhead\/hammerhead:6.0.1\/M4B30Z\/3437181:user\/release-keys",
    "data_vector.pag": "d41d8cd98f00b204e9800998ecf8427e;d41d8cd98f00b204e9800998ecf8427e;ca8f210b5af2ad80a2fbb618a1aa12b0;...",
    "particle_video.pag": "7355d3a6e1af6be9c692e634a9d8577e;2197d3241540990536d6a75446b9fa2d;ecd5a0cec1c88ea3e115d68fcbb184c8;...",
    "replacement_vector.pag": "e783e4a1aa5fe320fe1e7974fa72b766;2d1d074451447aa9a83081427f9fd6c9;3e0e280528246978f112325af8d20e97;..."
}
```
###PAG测试素材更新MD5
####更新测试素材MD5
当有新增的PAG素材时，我们需要更新对应的md5校对文件。具体步骤如下：

1. 运行对应的测试用例
2. 检查用例结果是否正确（对比ae源文件效果和测试用例结果的逐帧截图）
3. 获得对应的md5文件，替换原有的用于校对的md5文件（注意命名）
4. 提交代码

####注意事项：
-推荐使用蓝盾服务器运行测试用例。md5值的生成结果与系统版本有关，如果本地机器系统版本与蓝盾服务器版本不一致，用例将无法在蓝盾上通过md5校验。
-如何取得蓝盾服务器的md5结果文件？登录蓝盾机器，在/test/out目录下找到对应的md5文件

####新增一个素材MD5用例
目前MD5相关测试，已覆盖到部分接口的校验、全量MD5测试，全特效MD5测试。
如后续有新增需求，可仿照PAGSmokeTest。主要关注如下步骤：
1. 使用GetAllPAGFiles("../resources/smoke", files)，指定MD5测试素材路径，建议统一放在assets目录下。
2. 遍历文件，使用MakeSnapshot()逐帧截图
3. 遍历截图，使用DumpMD5()获取MD5值
4. 使用dumpJson[fileName]，归档MD5值
5. 输出归档的MD5文件
6. 检查用例结果截图，同更新步骤2
7. 移动MD5文件至校对目录/test/res
8. 提交代码
