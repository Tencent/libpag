<img src="https://pag.io/img/readme/logo.png" alt="PAG Logo" width="474"/>

[官网](https://pag.io) | [English](../README.md) | [Web版本](../README.zh_CN.md) | [Weblite版本](../lite/README.md) | 小程序版本 | [小程序lite版本](../lite/wechat/README.md)

## 介绍

libpag 是 PAG (Portable Animated Graphics) 动效文件的渲染 SDK，目前已覆盖几乎所有的主流平台，包括：iOS, Android, macOS,
Windows, Linux, 以及 Web 端。

## 特性

- Web 平台能力适配，支持 libpag 全能力
- 基于 WebAssembly + WebGL

## 快速开始

PAG Web 端，由 libpag.js + libpag.wasm.br 文件组成。

1. NPM 依赖

``` bash
$ npm install libpag-miniprogram
```

点击「微信开发者工具」- 「工具」- 「构建npm」，进行小程序 npm 依赖构建

2. 将 node_modules/libpag-miniprogram/lib/libpag.wasm.br 文件复制到utils目录下
3. 初始化 PAG

``` javascript
// index.js
import { PAGInit } from 'libpag-miniprogram';

Page({
  async onReady() {
    this.PAG = await PAGInit({locateFile: (file) => '/utils/' + file});
  }
})
```

4. 播放 PAG 动效
```xml
<!-- index.wxml -->
<canvas type="webgl" id="pag" style="width: 300px; height:300px;"></canvas>
```

``` javascript
// index.js
wx.createSelectorQuery()
  .select('#pag')
  .node()
  .exec(async (res) => {
    const canvas = res[0].node;
    const buffer = await loadFileByRequest('https://pag.io/file/test.pag');
    const pagFile = await this.PAG.PAGFile.load(buffer);
    const pagView = await this.PAG.PAGView.init(pagFile, canvas);
    pagView.play();
  });

const loadFileByRequest = async (url) => {
  return new Promise((resolve) => {
    wx.request({
      url,
      method: 'GET',
      responseType: 'arraybuffer',
      success(res) {
        if (res.statusCode !== 200) {
          resolve(null);
        }
        resolve(res.data);
      },
      fail() {
        resolve(null);
      },
    });
  });
};
```
