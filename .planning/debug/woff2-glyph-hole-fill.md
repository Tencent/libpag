---
status: diagnosed-pending-fix
trigger: "童鞋电商首页 HTML 转 PAGX 后，部分字体孔洞被填实；用户要求先记录，暂不修复。"
created: 2026-10-12
updated: 2026-10-12
---

# WOFF2 字形轮廓孔洞丢失

## 状态与范围

已复现并定位到 WOFF2 导出轮廓的填充语义转换。按用户要求暂不修改实现。
本问题独立于 ZIP 导入、字体文件引用、字体合并和原始文本保留。

## 复现输入

- `/Users/codywwang/Downloads/导出png/童鞋电商首页.zip`
- `/Users/codywwang/Downloads/童鞋电商首页 3/`：包含 `index.pagx`、`index.html`、图片和 8 个 WOFF2。
- 第二份目录的 8 个 WOFF2 与第一份 ZIP 的字体逐字节相同。
- 页面为 375 × 1444；明显样例为标题“萌宝鞋屋”和“为宝贝推荐”中的“宝”。

## 已验证的阶段对照

| 阶段 | 孔洞结果 |
| --- | --- |
| 第二份目录中的 `index.pagx` 直接渲染 | 正常 |
| 同目录 `index.html` 在 Chromium 中绘制 | 正常 |
| HTML 导入 PAGX，直接引用 WOFF2，不嵌入字体 | 部分孔洞被填实 |
| 对引用版执行 `pagx embed` 后再渲染 | 同样被填实 |

引用版包含 8 个文件字体、0 个内嵌 Glyph；再次嵌入后包含 146 个向量 Glyph。
因此，错误在再次嵌入之前已经存在；原 PAGX 的内嵌字形本身可正常显示。

![四阶段标题对照](woff2-glyph-hole-fill/four-stages.png)

## 定位与证据

涉及 `src/pagx/utils/Woff2FontGenerator.cpp`：

1. `ResolveWoff2GlyphPath()` 使用 `PathOp::Union` 对原轮廓做布尔合并。
2. 合并结果中的部分孔洞依赖奇偶填充语义；`BuildCharString()` 随后通过
   `WriteTGFXPathToCharString()` 直接将轮廓写入 CFF，没有将这种语义转换为适合
   非零绕数填充的内外轮廓方向。
3. 浏览器字体绘制与 libpag 提取轮廓后的绘制对此产生不同结果。

对原 `font_f1.woff2` 的 U+E001（标题中的“宝”）使用 fontTools 读取 CFF 轮廓，
同一轮廓分别按奇偶和非零绕数规则判定：49 × 49 个内部网格采样点中有 170 个结果不同。
例如字体坐标 `(584.813, 66.797)` 按奇偶规则为空、按非零绕数规则为实心。
这证明原 WOFF2 中已存在两种填充规则下不一致的轮廓。

此前 CoreText 对照中，直接绘制该字形与对提取路径使用奇偶填充可以保留孔洞，
对同一路径使用非零绕数填充则丢失部分孔洞。

结论：修复重点是原字形轮廓到 CFF 的转换，不能用“浏览器显示正常”认定字体轮廓
在不同绘制方式下均正确。完整修复尚未实施或验证。

## 复现命令

在仓库根目录执行；输出到独立目录，保留原始输入。

```sh
mkdir -p /private/tmp/pagx-woff2-holes-repro
./cmake-build-debug/pagx render '/Users/codywwang/Downloads/童鞋电商首页 3/index.pagx' -o /private/tmp/pagx-woff2-holes-repro/original.png
./cmake-build-debug/pagx import --input '/Users/codywwang/Downloads/童鞋电商首页 3/index.html' --output /private/tmp/pagx-woff2-holes-repro/reference.pagx --images embed
./cmake-build-debug/pagx render /private/tmp/pagx-woff2-holes-repro/reference.pagx -o /private/tmp/pagx-woff2-holes-repro/reference.png
./cmake-build-debug/pagx embed /private/tmp/pagx-woff2-holes-repro/reference.pagx -o /private/tmp/pagx-woff2-holes-repro/embedded.pagx
./cmake-build-debug/pagx render /private/tmp/pagx-woff2-holes-repro/embedded.pagx -o /private/tmp/pagx-woff2-holes-repro/embedded.png
```

HTML 导入需要 Chromium；本机渲染需要可用的图形设备。

## 后续修复方向与验收

- 在 WOFF2 导出阶段解决布尔合并后轮廓方向与填充语义的一致性，覆盖共享导出链路。
- 优先检查并复用底层路径库能力；此前较大的手写轮廓算法方案未被用户接受。
- 回归覆盖相接触的孔洞轮廓、曲线嵌套孔洞以及孔洞内的实心岛。
- 同一源字形在原 PAGX、HTML 浏览器、引用字体 PAGX、再次嵌入 PAGX 中应保留相同孔洞。
- 不能通过将所有字体统一改成奇偶填充来掩盖问题；需验证重叠笔画的显示结果。
- 保留原始文本、GlyphRun 字形与位置、字体引用及字体合并现有行为。

## 本次诊断产物

四阶段对照图已随记录保存。详细临时产物在 `/private/tmp/pagx-font-holes/`：
`shoes3-original.png`、`shoes3-browser.png`、`shoes3-reference.pagx/png`、
`shoes3-reembedded.pagx/png`。
此前暂停的回归草稿为 `paused-regression-tests.cpp` 和 `regression-tests.patch`；
恢复修复时需结合当前代码审查，临时目录可能被系统清理。
