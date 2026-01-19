[English](./README.md) | 简体中文

# Linux Demo

## 编译环境要求

请参考根目录下的 [README.md](../README.md)

## 安装 NodeJS

1) 下载

```
wget https://nodejs.org/dist/v17.3.1/node-v17.3.1-linux-x64.tar.xz
sudo mkdir -p /usr/local/lib/nodejs
sudo tar -xJvf node-v17.3.1-linux-x64.tar.xz -C /usr/local/lib/nodejs
```

2) 设置环境变量

进入 ~/.profile，添加以下行到末尾：

```
export PATH=/usr/local/lib/nodejs/node-v17.3.1-linux-x64/bin:$PATH
```

3) 测试安装

```
$ node -v
$ npm version
$ npx -v
```

## 安装 Ninja

```
yum install re2c
git clone git://github.com/ninja-build/ninja.git
cd ninja
./configure.py --bootstrap
cp ninja /usr/bin/
```

## 安装 X11

swiftshader 依赖一些头文件。

```
yum install libX11-devel --nogpg
```

## 编译 PAG

在 linux/ 目录下执行以下脚本：

```
./build_pag.sh 
```

库文件将会编译到 vendor/libpag 目录中。

## 编译 Demo

在 linux/ 目录下执行以下脚本：

```
cmake -B ./build -DCMAKE_BUILD_TYPE=Release
cmake --build ./build -- -j 12
```

然后可以在 build 目录中找到可执行文件。
