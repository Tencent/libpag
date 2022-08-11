# libpag-lite

## 介绍

libpag-lite 是 libpag 在 Web 平台的简化版 SDK

## 特性

- 基于 Javascript + WebGL，不使用 WebAssembly
- 仅支持播放包含单独一个 BMP 视频序列帧的 PAG 动效文件
- 包体只有 57KB，GZip之后只有 15KB，比完整版 libpag 小很多
- 具有更好的兼容性

## 关于兼容性

简化版使用纯 Javascript 开发，所以它只依赖于 WebGL 与 Video BlobURL 这两个 Web 技术。

所以，可以配合 [babel](https://babeljs.io/) 将代码兼容到较老的浏览器版本。

**值得注意的是因为 FireFox 对 H264 视频的支持不兼容带 Bframe 的视频，所以简化版不支持 FireFox 浏览器。如果需要在 FireFox 上使用，可以参考 libpag完整版 + ffavc 软件解码的方案**

## 关于 BMP 序列帧

### 导出单一 BMP 序列帧的 PAG 文件

> 可参考 https://pag.io/docs/pag-export.html

点击菜单“文件” -> “导出” -> “PAG File(Panel)...”，选择需要导出的合成，点击设置按钮，在根节点勾选BMP，导出全BMP预合成的PAG文件。

### 查看 PAG 文件是否为单一 BMP 序列帧

可以下载 [PAGViewer](https://pag.io/docs/install.html) 打开 PAG 文件，点击"视图"->"显示 编辑面板"，在编辑面板中我们能看到 Video 的数量，当 Video数量为 1 时，即为单一 BMP 序列帧。

## 快速开始

### browser

```html
<canvas id="pag"></canvas>
<script src="https://cdn.jsdelivr.net/npm/libpag-lite@latest/lib/pag.min.js"></script>
<script>
  window.onload = async () => {
    const { PAGView, types } = window.libpag;
    const arrayBuffer = await fetch('./assets/particle_video.pag').then((res) => res.arrayBuffer());
    const canvas = document.getElementById('pag');
    canvas.width = 720;
    canvas.height = 720;
    const pagView = PAGView.init(arrayBuffer, canvas, {
      renderingMode: types.RenderingMode.WebGL
    });
    pagView.play();
  };
</script>
```

### npm

```bash
$ npm install libpag-lite
```

```js
import { PAGView, types } from 'libpag-lite';
```

## API

### <span id="PAGRender">PAG.PAGView</span>

#### static create(mp4Data, canvas, options)

创建 View 实例

| 名称        | 类型                            | 说明                   | 默认值 | 必传 |
| ----------- | ------------------------------- | ---------------------- | ------ | ---- |
| **data**    | ArrayBuffer                     | PAG文件                |        | Y    |
| **canvas**  | HTMLCanvasElement               | 用于绘图的 canvas 标签 |        | Y    |
| **options** | [RenderOptions](#RenderOptions) | 渲染选项               |        | Y    |

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

返回当前缩放模式

---

#### setScaleMode(scaleMode: [ScaleMode](#ScaleMode))

指定缩放内容的模式

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

### Interface

#### <span id="RenderOptions">RenderOptions</span>

| 键                | 类型                                 | 说明                                  | 默认值  | 必传 |
| ----------------- | ------------------------------------ | ------------------------------------- | ------- | ---- |
| **renderingMode** | enum [RenderingMode](#RenderingMode) | 渲染模式，可选值： `WebGL` 、`Canvas` | `WebGL` | N    |
| **useScale** | boolean | 是否按照设备dpi进行缩放 | true | N    |

### Enum

#### <span id="RenderingMode">RenderingMode</span>

| 键         | 类型   | 值     | 说明       |
| ---------- | ------ | ------ | ---------- |
| **Canvas** | String | Canvas | 2D 渲染    |
| **WebGL**  | String | WebGL  | WebGL 渲染 |

#### <span id="ScaleMode">ScaleMode</span>

| 键            | 类型   | 值        | 说明                                   |
| ------------- | ------ | --------- | -------------------------------------- |
| **None**      | String | None      | 不缩放                                 |
| **Stretch**   | String | Stretch   | 拉伸内容到适应画布                     |
| **LetterBox** | String | LetterBox | 根据原始比例缩放内容                   |
| **Zoom**      | String | Zoom      | 根据原始比例被缩放适应，一个轴会被裁剪 |
