# pagx_to_pag 样本集

`pagx` → PAG 二进制格式（PAG v4）的专用测试样本集，与 `resources/pagx_to_html/` 命名一致。
每个文件聚焦一个特性，供导出/解码往返（T1/T3）、健壮性（T5）、引用与作用域（T11）与渲染对比页使用。

- 一期对比范围：不含动效内容的样本（layout_flatten、resource、static_animated 的纯静态部分、
  text_font、style_filter、sub_document）。
- 二期起覆盖：animation、state_machine、viewmodel 与含动效的样本（设计文档 §12.3）。
- 样本按当前 PAGX 规范书写；`spec_extension/`（规范扩展与 §7.4 能力清单）随三期功能补充。

## 目录与内容

| 目录 | 内容 |
|---|---|
| `animation/` | 每种通道值类型（float/int/bool/string/color/matrix）、四种插值、三种循环模式、Path 逐点通道、Composition 作用域自动播放、Timelines 挂载（playing/evaluationOffset）、多个根动画 |
| `state_machine/` | 三种输入与全部条件运算符、crossfade 参数（duration/interpolation/earlyExit/pauseOnExit）、多区域并行、from="any"、空状态与 animation 引用 |
| `viewmodel/` | 全部属性类型（number/string/boolean/color/image/enum/trigger）、嵌套 ViewModel、rangeMapper 转换器、四种绑定方向、Composition 级 ViewModel |
| `style_filter/` | NoiseStyle/NoiseFilter 各模式、Layer 与绘制器混合模式、Blur/BackgroundBlur 的 tileMode、shadowOnly 与 excludeChildEffects |
| `layout_flatten/` | §4.2 扁平化边界：三种 textAnchor、被布局缩放的 Text 与 Path、TextBox 自动高度、clipToBounds、可动画布局通道（size.width）与布局专用通道（flex）绑定 |
| `text_font/` | 嵌入矢量字形、嵌入位图字形（PNG data URI）、嵌入/未嵌入混合、Font `file`、竖排、富文本（粗体独立填色 + 双向文本 + emoji 回退） |
| `resource/` | data URI 与外部共享图片、缺失图片、P3 广色域颜色、matrix3D |
| `sub_document/` | 外部子 PAGX：单次/多次引用、多级嵌套、缺失文件、环形引用（a↔b）、带约束与图片的子文档 |
| `static_animated/` | 纯静态、静态 + ViewModel、静态与动画混合、无对象的 Animation、时长为 0 |

## 故意的边界样本（不要"修复"它们）

以下样本的行为是刻意的，被健壮性测试（T5/T11）消费：

- `resource/image_missing.pagx`：引用不存在的图片。导出应产生 MissingImage 告警、空 Image。
- `sub_document/missing_reference.pagx`：引用不存在的子文档。导出告警、图层输出为空。
- `sub_document/circular/`：a.pagx ↔ b.pagx 环形引用链。加载器必须报错而不是无限递归。
- `static_animated/zero_duration_animation.pagx`：时长 0 的动画。导出保留 + 告警，运行时时长为 0。
- `static_animated/empty_animation.pagx`：无对象的 Animation。二进制保留其时长（C20）。
- `layout_flatten/layout_channel_targets.pagx`：对 `flex`（布局专用通道）的 DataBind。
  XML 下同样不生效；导出时删除并告警（§4.2）。

## 校验

```bash
pagx verify <file>
```

全部样本应零诊断通过（2026-10-22 全量 57/57）。构建本样本集时同步修正了三处工具/规范不一致：

1. `spec/pagx.xsd`：`composition` 属性原先只接受 `@id`，新增 `CompositionRefType` 允许外部
   PAGX 文件路径（规范 §8.3 早已支持）；`Timelines/Animation` 驱动器补 `evaluationOffset` 属性。
2. 规范 §8.3.1 示例（中英文）原先把 `<Animations>` 写在 `<Resources>` 内，与 xsd 冲突，已改为文档级。
3. `pagx verify` 的未引用资源检测：带 `file` 的 Font 是字体源声明（经 fontFamily 匹配消费，
   非 @id 引用），已豁免（`src/cli/CommandVerify.cpp`）。
