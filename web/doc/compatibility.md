# 兼容性情况

| [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/chrome/chrome_48x48.png" alt="Chrome" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Chrome | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/safari/safari_48x48.png" alt="Safari" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Safari | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/chrome/chrome_48x48.png" alt="Chrome" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Chrome for Android | [<img src="https://raw.githubusercontent.com/alrra/browser-logos/master/src/safari/safari_48x48.png" alt="Safari" width="24px" height="24px" />](http://godban.github.io/browsers-support-badges/)<br/>Safari on iOS |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Chrome >= 69                                                                                                                                                                                                  | Safari >= 11.3                                                                                                                                                                                                | Android >= 7.0                                                                                                                                                                                                            | iOS >= 11.3                                                                                                                                                                                                          |

libpag Web端是基于 WebAssembly + WebGL实现，并通过适配 Web 平台能力来支持 libpag 全能力，以上的兼容表仅代表可以运行的兼容性。

libpag 的渲染性能受以下条件影响：

- PAG 动效文件的复杂度
- libpag 调用方式
- Web 浏览器环境

本文主要讲解 libpag Web端在各个浏览器中的性能，以及一些兼容兜底方案。

> 注册软件解码器 ffavc 需要加载多一个 wasm 文件，会增加内存和 CPU 占用。

## 桌面端

### Chrome (Win/Mac)

性能表现良好

### Safari (Mac)

性能表现良好

### Firefox (Win/Mac)

除带 BMP 序列帧的 PAG 动效文件外，性能表现良好。

因为 Firefox 的 Video 标签无法解析 PAG 动效文件中 BMP 序列帧转化而成的视频，所以需要注册软件解码器 ffavc 来解析。[示例Demo](https://github.com/libpag/pag-web/blob/main/pages/software-decoder.html)

## 移动端

###  Safari (iOS)

除带 BMP 序列帧的 PAG 动效文件外，性能表现良好。

libpag 解析带 BMP 序列帧的 PAG 动效文件调用了 Video 标签的 blobURL 属性解码视频，而在 iOS Safari 浏览器上当 blobURL 为 src 的 Video 时，会存在“播放到视频末尾掉帧”、“修改currentTime后currentTime属性不变化，而video画面渲染成功”等BUG。

而且当 iOS 设备处于省电模式或者在微信浏览器中，存在“用户与页面交互之后才可以使用 Video 标签进行视频播放”的规则限制，所以，使用播放动效的场景需要经过用户交互之后播放。

以上两个环境兼容性的影响，可以使用注册软件解码器 ffavc 解析视频来规避，也是当前推荐的方案。[示例Demo](https://github.com/libpag/pag-web/blob/main/pages/software-decoder.html)

### Chrome (Android)

除带 BMP 序列帧的 PAG 动效文件外，性能表现良好。

部分 Android 设备和在微信浏览器中，存在“用户与页面交互之后才可以使用 Video 标签进行视频播放”的规则限制，所以，使用播放动效的场景需要经过用户交互之后播放。

这里也使用注册软件解码器 ffavc 解析视频来规避，但是 ffavc 软解码在 Android 设备中的性能表现较差，所以我们并不推荐使用这个方案，后续我们会努力寻求更优秀的方案来覆盖这个场景。暂时需要接入方从业务场景中引导用户产生交互来规避“用户与页面交互之后才可以使用 Video 标签进行视频播放”的规则限制。

### Android 原生浏览器

因为各个厂商都有自己的自带浏览器，而自带浏览器的环境除了会收到厂商浏览器实现差异影响还会受到GPU芯片差异影响。所以，我们暂时没有计划对这部分机器进行兼容，我们首要的精力会放在主流的浏览器中。
