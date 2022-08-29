# libpag-lite-miniprogram

## 介绍

libpag-lite-miniprogram 是 libpag 在微信小程序平台的简化版 SDK

## 特性

- 基于 Javascript + WebGL，不使用 WebAssembly
- 仅支持播放包含单独一个 BMP 视频序列帧的 PAG 动效文件
- 包体minify只有 57KB，GZip之后只有 15KB，比完整版 libpag 小很多
- 具有更好的兼容性

## 关于 BMP 序列帧

### 导出单一 BMP 序列帧的 PAG 文件

> 可参考 https://pag.io/docs/pag-export.html

点击菜单“文件” -> “导出” -> “PAG File(Panel)...”，选择需要导出的合成，点击设置按钮，在根节点勾选BMP，导出全BMP预合成的PAG文件。

### 查看 PAG 文件是否为单一 BMP 序列帧

可以下载 [PAGViewer](https://pag.io/docs/install.html) 打开 PAG 文件，点击"视图"->"显示 编辑面板"，在编辑面板中我们能看到 Video 的数量，当 Video数量为 1 时，即为单一 BMP 序列帧。

## 快速开始

1. npm 依赖

```bash
$ npm install libpag-lite-miniprogram
```

点击「微信开发者工具」- 「工具」- 「构建npm」，进行小程序 npm 依赖构建

2. 绘制

```xml
<!-- index.wxml -->
<canvas type="webgl" id="pag" style="width: 300px; height:300px;"></canvas>
```

```js
// index.js
import { PAGView } from 'libpag-lite';

Page({
  onReady() {
    wx.createSelectorQuery().select('#pag').node().exec(async(res) => {
      const canvas = res.node
      wx.request({
      	url,
      	method: 'GET',
      	responseType: 'arraybuffer',
      	success(res) {
          const pagView = PAGView.init(res.data, canvas);
          pagView.play();
      	},
    	});
    })
  }
})
```

## 使用指南

### 缓存

因为 PAG 解码的时候用到了 VideoDecoder，而 VideoDecoder 的解码并不支持 blobURL，所以会把 BMP 预合成的视频数据缓存在本地。

为避免PAG的缓存把用户缓存目录内存占用过高，建议在不需要使用PAG的时候，调用 `clearCache()` 方法进行缓存清理。

## API

### PAGView

#### static create(mp4Data, canvas, options)

创建 View 实例

| 名称       | 类型              | 说明                   | 默认值 | 必传 |
| ---------- | ----------------- | ---------------------- | ------ | ---- |
| **data**   | ArrayBuffer       | PAG文件                |        | Y    |
| **canvas** | HTMLCanvasElement | 用于绘图的 canvas 标签 |        | Y    |

#### play(): Promise\<void\>

播放

---

#### pause(): void

暂停

---

#### stop(): void

停止

---

#### destroy(): void

销毁

---

#### isPlaying(): boolean

是否播放中

---

#### isDestroyed(): boolean

是否已经销毁

---

#### duration(): nunber

动画持续时间，单位：秒

---

#### setRepeatCount(repeatCount): void

设置动画重复的次数

| 名称            | 类型   | 说明                                        | 默认值 | 必传 |
| --------------- | ------ | ------------------------------------------- | ------ | ---- |
| **repeatCount** | number | 设置动画重复的次数，如为 0 动画则无限播放。 | 1      | N    |

#### getProgress(): number

返回当前播放进度位置，取值范围为 0.0 到 1.0

---

#### setProgress(value): Promise\<boolean\>

设置播放进度位置

| 名称         | 类型   | 说明                                    | 默认值 | 必传 |
| ------------ | ------ | --------------------------------------- | ------ | ---- |
| **progress** | number | 设置播放进度位置，取值范围为 0.0 到 1.0 |        | Y    |

#### scaleMode(): [ScaleMode](#ScaleMode)

返回当前填充模式

---

#### setScaleMode(scaleMode: [ScaleMode](#ScaleMode))

指定内容的填充模式

| 名称          | 类型      | 说明                                                          | 默认值      | 必传 |
| ------------- | --------- | ------------------------------------------------------------- | ----------- | ---- |
| **scaleMode** | ScaleMode | 缩放模式，可选值：`None` 、`Stretch` 、 `LetterBox` 和 `Zoom` | `LetterBox` | N    |

#### addListener(eventName, listener):void

增加事件监听

| 名称          | 类型    | 说明     | 默认值 | 必传 |
| ------------- | ------- | -------- | ------ | ---- |
| **eventName** | string  | 事件名称 |        | Y    |
| **listener**  | funtion | 监听器   |        | Y    |

事件列表：

| 名称              | 说明         |
| ----------------- | ------------ |
| onAnimationStart  | 动画开始播放 |
| onAnimationCancel | 动画取消播放 |
| onAnimationEnd    | 动画结束播放 |
| onAnimationRepeat | 动画循环播放 |

#### removeListener(eventName, listener?):void

增加事件监听

| 名称          | 类型    | 说明                                                 | 默认值    | 必传 |
| ------------- | ------- | ---------------------------------------------------- | --------- | ---- |
| **eventName** | string  | 事件名称                                             |           | Y    |
| **listener**  | funtion | 监听器，如不传入 `listener` 则移除该事件下所有监听器 | undefined | N    |

#### getDebugData():[DebugData](#DebugData)

---

### clearCache():void

清理 PAG 产生的缓存

---

### Interface

#### <span id="DebugData">DebugData</span>

| 键                | 类型   | 说明                      | 默认值 |
| ----------------- | ------ | ------------------------- | ------ |
| **FPS**           | number | 过去一秒内渲染帧数量      | 0      |
| **getFrame**      | number | 当前帧获取耗时，单位 ms   | 0      |
| **draw**          | number | 当前帧渲染耗时，单位 ms   | 0      |
| **decodePAGFile** | number | PAG 文件解码耗时，单位 ms | 0      |
| **coverMP4**      | number | 合成 MP4 耗时，单位 ms    | 0      |
| **writeFile**     | number | 写入文件耗时，单位 ms     | 0      |
| **createDecoder** | number | 创建解码器耗时，单位 ms   |        |

### Enum

#### <span id="ScaleMode">ScaleMode</span>

填充模式

| 键            | 类型   | 值        | 说明                                   |
| ------------- | ------ | --------- | -------------------------------------- |
| **None**      | String | None      | 不缩放                                 |
| **Stretch**   | String | Stretch   | 拉伸内容到适应画布                     |
| **LetterBox** | String | LetterBox | 根据原始比例缩放内容                   |
| **Zoom**      | String | Zoom      | 根据原始比例被缩放适应，一个轴会被裁剪 |
