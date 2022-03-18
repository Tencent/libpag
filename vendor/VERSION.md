# 第三方库

[TOC]

## FFmpeg

### MAC

> 官方正式Release版本

#### 版本

 [FFmpeg 4.1.4 "al-Khwarizmi"](https://ffmpeg.org/releases/ffmpeg-4.1.4.tar.gz) 发布日期 2019/07/08

#### 编译命令

```makefile
./configure --disable-all --enable-avcodec --disable-everything --enable-decoder=h264 --enable-small --prefix=path-to-ffmpeg --extra-cflags="-mmacosx-version-min=10.9"

make
```

## GoogleTest

### 源码引入

>  代码比较少，源码引入

#### 版本

代码比较少，更新也比较慢，鉴于是测试框架，直接取[master分支](https://github.com/google/googletest.git)代码，最后提交id是：b4676595c03a26bb84f68542c8b74d3d89b38b68

## Mesa

### MAC

#### 版本

[19.0.8]([Mesa 19.0.8](https://www.mesa3d.org/relnotes/19.0.8.html))  发布日期 2019/0

#### 编译依赖

```
brew install meson
brew install flex
brew install byacc
pip3 install Mako
\\\\\\\\\\\\\\\\\\\\\\\\\\\
编译问题
 ERROR: Can not do gettext because xgettext is not installed.
问题解决
brew install gettext
brew link gettext --force
\\\\\\\\\\\\\\\\\\\\\\\\\\\
brew install cmake
```

#### 编译命令

```
// 生成build
meson build/ -Dosmesa=gallium -Dgallium-drivers=swrast -Ddri-drivers= -Dvulkan-drivers= -Dprefix=$PWD/build/install -Ddefault_library=both -Dc_args="-Wno-missing-prototypes" -Dplatforms=surfaceless -Dglx=disabled
// 编译
ninja -C build install
```

#### 文件位置

> 因为Mac的特殊性，需要拷贝指定的文件，把如下文件拷入工程

```
build/builder/src/mapi/shared-glapi/libglapi.0.dylib
build/builder/src/gallium/targets/osmesa/libOSMesa.8.dylib
```

## freetype
#### 版本
skia->third_party->externals->freetype
skia版本：chrome/m62
#### 编译
通过CMake编译
skia->third_party->externals->freetype->CMakeList.txt
去除不必要系统依赖：ZLIB_FOUND、BZIP2_FOUND、PNG_FOUND、HARFBUZZ_FOUND
然后执行cmake编译静态库即可

编译平台：mac、linux

## swiftshader
#### 版本
https://github.com/google/swiftshader.git，
master分支，commit：860369fc43

#### 编译
修改源码：libGLESv2.cpp
 	
 	case GL_VERSION:
 	   return (GLubyte*)"OpenGL ES 3.0 SwiftShader " VERSION_STRING;
将3.0修改为2.0

执行cmake编译，生成的为动态库


编译平台：mac、linux