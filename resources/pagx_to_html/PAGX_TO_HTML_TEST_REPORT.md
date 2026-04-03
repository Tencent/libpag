# PAGX 转 HTML 测试报告

## 1. 概览

本报告涵盖 `/resources/pagx_to_html/` 目录下的 **68 个测试文件**，全面验证 PAGX 到 HTML 转换功能的各个方面。

### 1.1 测试统计

| 类别 | 文件数量 | 涉及降级的文件数 |
|------|---------|----------------|
| 几何元素 | 5 | 0 |
| 颜色源系统 | 8 | 3 |
| 画笔系统 | 7 | 3 |
| 修改器 | 6 | 1 |
| 文本系统 | 14 | 0 |
| 图层系统 | 14 | 3 |
| 样式/滤镜 | 6 | 5 |
| 综合展示 | 4 | 多项 |
| 其他 | 4 | 1 |
| **总计** | **68** | **16** |

---

## 2. 测试文件详细列表

### 2.1 几何元素 (Geometry)

| 文件名 | 测试特性 | 是否降级 | 降级说明 | HTML 差异描述 |
|--------|----------|---------|---------|---------------|
| `geometry_rectangle.pagx` | Rectangle 中心点定位、size、roundness、reversed | 否 | - | CSS div + border-radius 实现，视觉一致 |
| `geometry_ellipse.pagx` | Ellipse 圆形/椭圆/reversed | 否 | - | CSS border-radius: 50% 实现，视觉一致 |
| `geometry_polystar.pagx` | Polystar polygon/star、roundness 变体 | 否 | - | 始终 SVG path 渲染，视觉一致 |
| `geometry_path.pagx` | Path 内联 data 和 @id 资源引用 | 否 | - | SVG path 直接输出，视觉一致 |
| `polystar_advanced.pagx` | 小数 pointCount、inner/outerRoundness、reversed | 否 | - | 贝塞尔曲线精确计算，视觉一致 |

### 2.2 颜色源系统 (Color)

| 文件名 | 测试特性 | 是否降级 | 降级说明 | HTML 差异描述 |
|--------|----------|---------|---------|---------------|
| `color_formats.pagx` | #RGB/#RRGGBB/#RRGGBBAA/srgb()/p3()/@id | 部分 | DisplayP3 在 SVG 属性中降级为 sRGB hex | P3 颜色在 SVG fill/stroke 中近似为 sRGB |
| `color_linear_gradient.pagx` | LinearGradient 方向角、多色标、matrix 变换 | 部分 | 带 matrix 时降级为 SVG linearGradient | 精确渐变需 SVG gradientTransform |
| `color_radial_gradient.pagx` | RadialGradient 圆形/椭圆(matrix) | 部分 | 旋转椭圆降级为 SVG radialGradient | CSS 支持椭圆但不支持旋转椭圆 |
| `color_conic_gradient.pagx` | ConicGradient 全角度/部分角度范围 | 部分 | 带 matrix 时降级为 SVG pattern+foreignObject | PAGX 0°=右 vs CSS 0°=上 需 +90° 修正 |
| `color_diamond_gradient.pagx` | DiamondGradient 菱形渐变 | **是** | **CSS 无等效，使用 SVG radialGradient 圆形近似** | **对角线方向视觉有差异** |
| `color_image_pattern.pagx` | ImagePattern tile 模式、filterMode | 部分 | mirror 降级为 repeat，clamp 降级为 no-repeat | 镜像和 clamp 边缘拉伸效果无法精确复现 |
| `color_p3.pagx` | Display P3 宽色域颜色对比 | 部分 | SVG 属性中 P3 降级为 sRGB | 支持 CSS color() 的浏览器可正确显示 |
| `gradient_matrix.pagx` | ConicGradient.matrix、DiamondGradient.matrix | **是** | **DiamondGradient 始终降级，Conic+matrix 需 SVG fallback** | 视觉近似但非精确 |

### 2.3 画笔系统 (Painter)

| 文件名 | 测试特性 | 是否降级 | 降级说明 | HTML 差异描述 |
|--------|----------|---------|---------|---------------|
| `painter_fill.pagx` | Fill alpha/blendMode/fillRule/placement/子元素覆盖 | 部分 | fillRule=evenOdd 需 SVG | CSS background 不支持 fill-rule |
| `painter_stroke.pagx` | Stroke 12 个属性: cap/join/dashes/align/gradient | 部分 | inside/outside 需 SVG 双倍宽度+clip/mask | 对齐模式实现复杂度高 |
| `painter_accumulated_geometry.pagx` | 多几何共享一个画笔 | 否 | - | 强制 SVG 路径渲染，视觉一致 |
| `painter_multiple.pagx` | 多个 Fill/Stroke 叠加 | 否 | - | 多层叠加渲染，视觉一致 |
| `stroke_align.pagx` | StrokeAlign center/inside/outside | **是** | **inside/outside 需 SVG 双倍宽度+裁剪实现** | 需额外 clipPath/mask |
| `stroke_dash_adaptive.pagx` | dashAdaptive 自适应虚线 | 部分 | 预计算缩放 dasharray | 算法近似，边界情况可能略有差异 |
| `stroke_advanced.pagx` | Stroke blendMode/miterLimit/dashAdaptive | 部分 | dashAdaptive 需预计算 | 同上 |

### 2.4 修改器 (Modifier)

| 文件名 | 测试特性 | 是否降级 | 降级说明 | HTML 差异描述 |
|--------|----------|---------|---------|---------------|
| `modifier_trim_path.pagx` | TrimPath separate/continuous/offset/wrap-around | 否 | - | SVG stroke-dasharray/dashoffset 实现 |
| `modifier_trim_continuous.pagx` | TrimPath continuous 模式多形状 | 否 | - | 按弧长比例分配，需精确路径长度计算 |
| `modifier_round_corner.pagx` | RoundCorner 圆角化 | 否 | - | 路径预处理：尖角转贝塞尔曲线 |
| `modifier_merge_path.pagx` | MergePath append/union/intersect/xor/difference | 否 | - | 使用 fill-rule 或 clipPath 实现 |
| `fillrule_comparison.pagx` | fillRule winding vs evenOdd 对比 | 部分 | evenOdd 需 SVG fill-rule | CSS 无直接等效 |
| `repeater.pagx` | Repeater 复制、小数 copies、order | 否 | - | 生成独立元素，alpha 插值 |

### 2.5 文本系统 (Text)

| 文件名 | 测试特性 | 是否降级 | 降级说明 | HTML 差异描述 |
|--------|----------|---------|---------|---------------|
| `text_basic.pagx` | Text 基础: anchor/spacing/fauxBold/fauxItalic | 否 | - | HTML span + CSS 实现 |
| `text_box.pagx` | TextBox textAlign/paragraphAlign/writingMode/overflow | 否 | - | Flexbox 布局实现 |
| `text_features.pagx` | letterSpacing/fauxBold/fauxItalic/CDATA/overflow=visible | 否 | - | CSS text 属性映射 |
| `text_glyph_run.pagx` | GlyphRun 预排版字形: xOffsets/rotations/scales | 否 | - | SVG path 逐字形渲染 |
| `text_modifier.pagx` | TextModifier + RangeSelector 逐字形变换 | 否 | - | 逐字符 span 或 SVG g 元素 |
| `text_modifier_advanced.pagx` | anchor/skew/strokeColor/strokeWidth/selector modes | 否 | - | factor 计算后应用变换 |
| `text_path.pagx` | TextPath 沿路径排列: perpendicular/margins | 否 | - | 弧长参数化逐字形定位 |
| `text_path_advanced.pagx` | baselineOrigin/baselineAngle/reversed/forceAlignment | 否 | - | 完整 TextPath 语义实现 |
| `text_rich.pagx` | 富文本: 多 Text+独立 Fill+共享 TextBox | 否 | - | 多 span 在同一容器内 |
| `text_writing_mode.pagx` | writingMode=vertical/paragraphAlign | 否 | - | CSS writing-mode 实现 |
| `selector_advanced.pagx` | RangeSelector unit=index/easeIn/easeOut/randomOrder | 否 | - | 完整 selector 算法实现 |
| `glyph_run_advanced.pagx` | positions/anchors/skews/bitmap Glyph | 否 | - | 逐字形 SVG 变换 |
| `coverage_gaps.pagx` | mipmapMode/Repeater.offset/textAlign=end/RangeSelector.offset | 否 | - | 边缘属性覆盖测试 |
| `edge_defaults.pagx` | 默认值和边界情况测试 | 否 | - | 验证默认行为正确性 |

### 2.6 图层系统 (Layer)

| 文件名 | 测试特性 | 是否降级 | 降级说明 | HTML 差异描述 |
|--------|----------|---------|---------|---------------|
| `root_document.pagx` | pagx 根元素 → div.pagx-root | 否 | - | overflow:hidden 实现画布裁剪 |
| `layer_visibility.pagx` | visible=false → display:none | 否 | - | 隐藏层不渲染 |
| `layer_alpha.pagx` | alpha → opacity | 否 | - | 直接映射 |
| `layer_transform_xy.pagx` | x/y → transform: translate | 否 | - | 直接映射 |
| `layer_transform_matrix.pagx` | matrix → transform: matrix() | 否 | - | 直接映射 |
| `layer_transform_matrix3d.pagx` | matrix3D/preserve3D → matrix3d/preserve-3d | 否 | - | 直接映射 |
| `layer_transform_priority.pagx` | matrix3D > matrix > x/y 优先级 | 否 | - | 高优先级覆盖低优先级 |
| `layer_group_opacity.pagx` | groupOpacity=true/false | 否 | - | opacity 应用位置不同 |
| `layer_pass_through.pagx` | passThroughBackground → isolation:isolate | 否 | - | CSS isolation 实现 |
| `layer_nesting.pagx` | 图层嵌套和 z-order | 否 | - | DOM 顺序即渲染顺序 |
| `layer_scroll_rect.pagx` | scrollRect → 嵌套 div+overflow:hidden | 否 | - | 双层 div 实现视口裁剪 |
| `layer_blend_modes.pagx` | 18 种 blendMode → mix-blend-mode | 部分 | **plusDarker 降级为 darken** | S+D-1 公式无 CSS 原生支持 |
| `layer_antialias.pagx` | antiAlias=false → shape-rendering:crispEdges | 部分 | 仅 SVG 支持，CSS 效果有限 | 浏览器渲染差异可能存在 |
| `blend_modes_showcase.pagx` | 所有 18 种混合模式展示 | 部分 | plusDarker 降级 | 同上 |

### 2.7 样式/滤镜 (Style/Filter)

| 文件名 | 测试特性 | 是否降级 | 降级说明 | HTML 差异描述 |
|--------|----------|---------|---------|---------------|
| `style_shadows.pagx` | DropShadowStyle 均匀/各向异性/showBehindLayer、InnerShadowStyle、BackgroundBlurStyle | 部分 | 各向异性模糊需 SVG feGaussianBlur；showBehindLayer=false 需 SVG feComposite | CSS drop-shadow 仅支持均匀模糊 |
| `filter_all.pagx` | BlurFilter/DropShadowFilter/InnerShadowFilter/BlendFilter/ColorMatrixFilter | 部分 | InnerShadowFilter 无 CSS 等效；shadowOnly 需 SVG filter；ColorMatrixFilter 需 SVG | 多种滤镜需 SVG 实现 |
| `filter_chain.pagx` | 多滤镜链式组合、shadowOnly | 部分 | 同上 | CSS filter 支持链式但部分效果需 SVG |
| `style_filter_advanced.pagx` | LayerStyle.blendMode/excludeChildEffects/tileMode/Stroke.alpha/placement | 部分 | **excludeChildEffects 降级为忽略**；tileMode 浏览器固定行为 | 复杂情况需额外 DOM 结构 |
| `clip_and_mask.pagx` | scrollRect/mask (alpha/luminance/contour) | 否 | - | SVG mask/clipPath 实现 |
| `group.pagx` | Group 变换/作用域隔离/几何传播 | 否 | - | div.pagx-group 实现 |

### 2.8 综合展示 (Showcase)

| 文件名 | 测试特性 | 是否降级 | 降级说明 | HTML 差异描述 |
|--------|----------|---------|---------|---------------|
| `showcase_dashboard_card.pagx` | 玻璃拟态卡片: LinearGradient/BackgroundBlurStyle/DropShadowStyle/groupOpacity/RoundCorner/Polystar/dashed Stroke | 部分 | 背景模糊各向异性降级 | 综合效果展示 |
| `showcase_mandala.pagx` | 曼陀罗图案: Repeater/Ellipse/Path/TrimPath/ConicGradient/blend modes | 部分 | plusDarker 降级 | 复杂重复图案 |
| `showcase_text_art.pagx` | 文字艺术: TextModifier/TextPath/GlyphRun/fauxBold/fauxItalic/letterSpacing | 否 | - | 丰富文字效果组合 |
| `showcase_infographic.pagx` | 信息图: TextBox/TextPath/rich text/Composition/MergePath/ImagePattern/Mask/InnerShadowStyle/foreground | 部分 | ImagePattern mirror 降级 | 完整信息图设计 |

### 2.9 其他

| 文件名 | 测试特性 | 是否降级 | 降级说明 | HTML 差异描述 |
|--------|----------|---------|---------|---------------|
| `resource_forward_ref.pagx` | 资源前向引用 (两遍解析) | 否 | - | 解析器实现要求 |
| `resource_composition.pagx` | Composition 资源内联展开 | 否 | - | 组件实例化 |
| `composition_advanced.pagx` | 多实例/不同变换的 Composition | 否 | - | 矩阵变换应用 |
| `data_attributes.pagx` | data-*/id/name 属性保留 | 否 | - | 语义信息透传 |

---

## 3. 降级特性汇总

### 3.1 完全不可达特性 (需 SVG fallback)

| PAGX 特性 | CSS 可达性 | 降级方案 | 受影响测试文件 |
|-----------|-----------|---------|---------------|
| **DiamondGradient** | 无 | SVG radialGradient 圆形近似 | `color_diamond_gradient.pagx`, `gradient_matrix.pagx` |
| **各向异性模糊** (blurX≠blurY) | 无 | SVG feGaussianBlur | `style_shadows.pagx`, `filter_all.pagx`, `showcase_dashboard_card.pagx` |
| **showBehindLayer=false** | 无 | SVG feComposite out | `style_shadows.pagx` |
| **InnerShadowFilter** | 无 | SVG filter 链 | `filter_all.pagx`, `filter_chain.pagx` |
| **shadowOnly=true** | 无 | SVG filter | `filter_all.pagx`, `filter_chain.pagx` |
| **ColorMatrixFilter** | 无 | SVG feColorMatrix | `filter_all.pagx` |
| **fillRule=evenOdd** (CSS bg) | 无 | SVG fill-rule | `painter_fill.pagx`, `fillrule_comparison.pagx`, `edge_defaults.pagx` |
| **align=inside/outside** | 无 | SVG 双倍宽度+clip/mask | `stroke_align.pagx`, `painter_stroke.pagx` |
| **plusDarker blend mode** | 无 | 近似为 CSS darken | `layer_blend_modes.pagx`, `blend_modes_showcase.pagx`, `showcase_mandala.pagx` |
| **excludeChildEffects** | 无 | 降级为忽略 (按 false 处理) | `style_filter_advanced.pagx` |

### 3.2 部分可达特性

| PAGX 特性 | 限制条件 | 降级方案 | 受影响测试文件 |
|-----------|---------|---------|---------------|
| **dashAdaptive** | 需预计算路径长度 | 缩放 dasharray 值 | `stroke_dash_adaptive.pagx`, `stroke_advanced.pagx` |
| **ImagePattern mirror** | CSS 无原生支持 | 降级为 repeat | `color_image_pattern.pagx`, `showcase_infographic.pagx` |
| **ImagePattern clamp** | CSS 无原生支持 | 降级为 no-repeat | `color_image_pattern.pagx` |
| **antiAlias=false** | SVG 支持较好 | shape-rendering:crispEdges; image-rendering:pixelated | `layer_antialias.pagx` |
| **ConicGradient + matrix** | CSS conic-gradient 不支持变换 | SVG pattern+foreignObject | `gradient_matrix.pagx` |
| **TileMode (blur filters)** | 浏览器固定边界行为 | 无法精确映射 decal/mirror | `style_filter_advanced.pagx` |
| **DisplayP3 in SVG** | SVG 属性不支持 color() | P3 通道值近似为 sRGB hex | `color_formats.pagx`, `color_p3.pagx` |
| **RadialGradient + 旋转矩阵** | CSS 支持椭圆但不支持旋转 | SVG radialGradient+gradientTransform | `color_radial_gradient.pagx` |
| **LinearGradient + 复杂矩阵** | CSS 无法精确表达起止位置 | SVG linearGradient+gradientTransform | `color_linear_gradient.pagx` |

### 3.3 完全可达特性 (无降级)

以下特性在 HTML/CSS 中有直接或间接的等效实现：

- TextPath (完整语义) - 弧长参数化逐字形定位
- TextModifier - 逐字形 span/SVG g 独立变换
- RoundCorner - 路径预处理贝塞尔曲线
- MergePath (所有模式) - fill-rule / clipPath 组合
- TrimPath (continuous) - 弧长比例分配
- Repeater 小数 copies - 最后副本 opacity × 小数
- groupOpacity - opacity 应用位置控制
- LayerStyle.blendMode - 独立 div + mix-blend-mode
- Luminance mask 渐变 - 完整渲染 Fill 颜色源

---

## 4. 渲染验证指南

### 4.1 验证流程

1. **构建测试工具**
   ```bash
   cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
   cmake --build cmake-build-debug --target PAGFullTest
   ```

2. **运行 HTML 导出测试**
   ```bash
   ./cmake-build-debug/PAGFullTest --gtest_filter="PAGXHtmlTest.*"
   ```

3. **手动验证**
   - 使用 `pagx export` CLI 工具导出 HTML
   - 在浏览器中打开导出的 HTML 文件
   - 与 PAGX 渲染引擎输出进行视觉对比

### 4.2 各类别验证重点

| 类别 | 验证重点 |
|------|---------|
| 几何元素 | 定位精度、圆角限制、路径方向 |
| 颜色源 | 渐变方向、色标位置、矩阵变换效果 |
| 画笔 | 描边对齐、虚线自适应、混合模式 |
| 修改器 | 路径裁剪范围、圆角曲率、布尔运算 |
| 文本 | 基线位置、字间距、沿路径排列 |
| 图层 | 变换顺序、透明度合成、裁剪边界 |
| 样式/滤镜 | 阴影偏移、模糊半径、滤镜链顺序 |
| 综合展示 | 整体视觉效果、多特性组合 |

### 4.3 已知视觉差异

| 场景 | 预期差异 |
|------|---------|
| DiamondGradient | 对角线方向渐变过渡与圆形不同 |
| plusDarker blend | 颜色计算公式不同，深色效果略有差异 |
| ImagePattern mirror | 镜像效果降级为简单重复 |
| 文本基线 position.y | 近似计算 top = y - fontSize × 0.8，±3px 误差 |
| antiAlias=false | 浏览器渲染引擎差异导致边缘效果不同 |

---

## 5. 测试覆盖率分析

### 5.1 特性覆盖统计

| 规范章节 | 测试文件数 | 覆盖状态 |
|---------|-----------|---------|
| §3 根文档与画布 | 1 | 完整 |
| §4 资源系统 | 3 | 完整 |
| §5 图层系统 | 14 | 完整 |
| §6 几何元素 | 5 | 完整 |
| §7 颜色源系统 | 8 | 完整 |
| §8 画笔系统 | 7 | 完整 |
| §9 形状修改器 | 6 | 完整 |
| §10 文本系统 | 14 | 完整 |
| §11 Repeater | 1 | 完整 |
| §12 Group | 1 | 完整 |
| §13 图层样式 | 2 | 完整 |
| §14 图层滤镜 | 2 | 完整 |
| §15 混合模式 | 2 | 完整 |
| §16 裁剪与遮罩 | 2 | 完整 |

### 5.2 边界情况覆盖

| 边界情况 | 覆盖文件 |
|---------|---------|
| 默认值行为 | `edge_defaults.pagx` |
| 零尺寸形状 | `edge_defaults.pagx` |
| 小数 pointCount | `polystar_advanced.pagx` |
| 小数 copies | `repeater.pagx` |
| 前向资源引用 | `resource_forward_ref.pagx` |
| TrimPath wrap-around | `modifier_trim_path.pagx` |
| 变换优先级 | `layer_transform_priority.pagx` |

---

## 6. 附录

### 6.1 文件分类索引

**几何类 (5)**:
`geometry_rectangle.pagx`, `geometry_ellipse.pagx`, `geometry_polystar.pagx`, `geometry_path.pagx`, `polystar_advanced.pagx`

**颜色类 (8)**:
`color_formats.pagx`, `color_linear_gradient.pagx`, `color_radial_gradient.pagx`, `color_conic_gradient.pagx`, `color_diamond_gradient.pagx`, `color_image_pattern.pagx`, `color_p3.pagx`, `gradient_matrix.pagx`

**画笔类 (7)**:
`painter_fill.pagx`, `painter_stroke.pagx`, `painter_accumulated_geometry.pagx`, `painter_multiple.pagx`, `stroke_align.pagx`, `stroke_dash_adaptive.pagx`, `stroke_advanced.pagx`

**修改器类 (6)**:
`modifier_trim_path.pagx`, `modifier_trim_continuous.pagx`, `modifier_round_corner.pagx`, `modifier_merge_path.pagx`, `fillrule_comparison.pagx`, `repeater.pagx`

**文本类 (14)**:
`text_basic.pagx`, `text_box.pagx`, `text_features.pagx`, `text_glyph_run.pagx`, `text_modifier.pagx`, `text_modifier_advanced.pagx`, `text_path.pagx`, `text_path_advanced.pagx`, `text_rich.pagx`, `text_writing_mode.pagx`, `selector_advanced.pagx`, `glyph_run_advanced.pagx`, `coverage_gaps.pagx`, `edge_defaults.pagx`

**图层类 (14)**:
`root_document.pagx`, `layer_visibility.pagx`, `layer_alpha.pagx`, `layer_transform_xy.pagx`, `layer_transform_matrix.pagx`, `layer_transform_matrix3d.pagx`, `layer_transform_priority.pagx`, `layer_group_opacity.pagx`, `layer_pass_through.pagx`, `layer_nesting.pagx`, `layer_scroll_rect.pagx`, `layer_blend_modes.pagx`, `layer_antialias.pagx`, `blend_modes_showcase.pagx`

**样式/滤镜类 (6)**:
`style_shadows.pagx`, `filter_all.pagx`, `filter_chain.pagx`, `style_filter_advanced.pagx`, `clip_and_mask.pagx`, `group.pagx`

**综合展示类 (4)**:
`showcase_dashboard_card.pagx`, `showcase_mandala.pagx`, `showcase_text_art.pagx`, `showcase_infographic.pagx`

**其他 (4)**:
`resource_forward_ref.pagx`, `resource_composition.pagx`, `composition_advanced.pagx`, `data_attributes.pagx`

---

*报告生成时间: 2026-04-03*
*测试文件总数: 68*
*规范版本: PAGX → HTML 转换技术方案 v1.0*
