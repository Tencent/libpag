# PAGX HTML 测试用例总结

## 一、视觉效果对比

| # | 文件名 | 状态 | 说明 |
|---|--------|------|------|
| 1 | `root_document` | ✅ 一致 | 蓝色圆角矩形，位置大小正确 |
| 2 | `edge_defaults` | ✅ 一致 | 默认黑色方块左上角，零大小/空层不可见 |
| 3 | `geometry_rectangle` | ✅ 一致 | 5 个不同圆角/颜色/大小的矩形 |
| 4 | `geometry_ellipse` | ✅ 一致 | 3 个不同大小的椭圆/圆 |
| 5 | `geometry_path` | ✅ 一致 | 贝塞尔曲线 + 心形 path 引用 |
| 6 | `geometry_polystar` | ✅ 一致 | 6 种多角星/多边形 |
| 7 | `color_formats` | ✅ 一致 | 6 种颜色格式均正确显示 |
| 8 | `color_linear_gradient` | ✅ 一致 | 4 种方向线性渐变 |
| 9 | `color_radial_gradient` | ⚠️ 偏差 | 第 1 个正确；第 2 个白→红渐变几乎纯红，可能是渐变范围较小+matrix 导致白色区域不可见 |
| 10 | `color_conic_gradient` | ✅ 一致 | 全角度和部分角度锥形渐变 |
| 11 | `color_diamond_gradient` | ✅ 一致 | 钻石渐变效果正确 |
| 12 | `color_image_pattern` | ⚠️ 偏差 | 前 3 个绿色方块可见（但纯色小图无法区分平铺模式差异）；第 4 个 decal 模式不可见 |
| 13 | `painter_fill` | ✅ 一致 | alpha/blendMode/evenOdd/placement/SolidColor |
| 14 | `painter_stroke` | ✅ 一致 | 12 种描边属性全部正确 |
| 15 | `painter_multiple` | ✅ 一致 | 多重填充+描边叠加 |
| 16 | `painter_accumulated_geometry` | ✅ 一致 | 多几何体共享画笔 |
| 17 | `clip_and_mask` | ⚠️ 偏差 | scrollRect 正确；alpha/luminance/contour mask 不可见（疑似引擎未实现 mask 引用方式） |
| 18 | `modifier_merge_path` | ✅ 一致 | 5 种布尔路径运算 |
| 19 | `modifier_round_corner` | ✅ 一致 | 圆角三角形 |
| 20 | `modifier_trim_path` | ✅ 一致 | 4 种裁剪路径效果 |
| 21 | `stroke_dash_adaptive` | ✅ 一致 | 普通虚线 vs 自适应虚线 |
| 22 | `layer_alpha` | ✅ 一致 | 不同透明度叠加 |
| 23 | `layer_antialias` | ✅ 一致 | 抗锯齿开/关对比 |
| 24 | `layer_visibility` | ✅ 一致 | 可见性控制 |
| 25 | `layer_blend_modes` | ✅ 一致 | 17 种混合模式 |
| 26 | `layer_group_opacity` | ✅ 一致 | 分组透明度 vs 独立透明度 |
| 27 | `layer_nesting` | ✅ 一致 | 嵌套层级和 z-order |
| 28 | `layer_pass_through` | ✅ 一致 | pass-through 背景穿透 |
| 29 | `layer_transform_xy` | ✅ 一致 | x/y 偏移 |
| 30 | `layer_transform_matrix` | ✅ 一致 | 2D matrix 旋转+平移 |
| 31 | `layer_transform_matrix3d` | ✅ 一致 | 3D matrix 透视变换 |
| 32 | `layer_transform_priority` | ⚠️ 偏差 | 蓝色和红色矩形正确；**绿色矩形（matrix 覆盖 x/y）不可见** |
| 33 | `repeater` | ✅ 一致 | 5 种重复器配置 |
| 34 | `resource_composition` | ✅ 一致 | Composition 组件实例化 + 缩放 |
| 35 | `resource_forward_ref` | ✅ 一致 | 前向引用颜色资源 |
| 36 | `filter_all` | ✅ 一致 | 8 种滤镜效果 |
| 37 | `style_shadows` | ✅ 一致 | 5 种阴影和背景模糊样式 |
| 38 | `group` | ✅ 一致 | Group 变换/作用域隔离/几何传播 |
| 39 | `text_basic` | ✅ 一致 | 7 种文本属性 |
| 40 | `text_box` | ✅ 一致 | 6 种 TextBox 布局 |
| 41 | `text_glyph_run` | ⚠️ 偏差 | 字形可见但**偏小偏移**，Layer 2 红色字形几乎不可见 |
| 42 | `text_modifier` | ✅ 一致 | 4 种文本修饰器效果 |
| 43 | `text_path` | ✅ 一致 | 文本沿路径排列 |
| 44 | `text_rich` | ⚠️ 偏差 | 预期 3 色富文本，实际只显示**单一颜色**（最后一个 Fill 覆盖了所有文字） |

**总计**：✅ 37 个完全符合预期 / ⚠️ 7 个有部分偏差

---

## 二、PAGX 特性/属性覆盖表

| 类别 | 特性/属性 | 测试文件 | 说明 |
|------|-----------|----------|------|
| **文档根** | `<pagx>` width/height/version | `root_document` | 画布尺寸和版本号 |
| **几何** | `<Rectangle>` position/size/roundness/reversed | `geometry_rectangle` | 中心定位、大小、圆角、路径反转 |
| | `<Ellipse>` position/size/reversed | `geometry_ellipse` | 椭圆/圆 |
| | `<Path>` data（内联/资源引用） | `geometry_path` | SVG 路径数据 + @id 引用 |
| | `<Polystar>` type/pointCount/outerRadius/innerRadius/outerRoundness/innerRoundness/rotation | `geometry_polystar` | polygon/star 类型、小数 pointCount |
| **颜色** | `<SolidColor>` #RGB/#RRGGBB/#RRGGBBAA/srgb()/p3()/@id | `color_formats` | 6 种颜色格式 |
| | `<LinearGradient>` startPoint/endPoint/ColorStop/matrix | `color_linear_gradient` | 多方向渐变 + matrix 变换 |
| | `<RadialGradient>` center/radius/matrix/ColorStop | `color_radial_gradient` | 径向渐变 + matrix 椭圆化 |
| | `<ConicGradient>` center/startAngle/endAngle/ColorStop | `color_conic_gradient` | 全角度/部分角度锥形渐变 |
| | `<DiamondGradient>` center/radius/ColorStop | `color_diamond_gradient` | 钻石形渐变 |
| | `<ImagePattern>` image/tileModeX/tileModeY/filterMode | `color_image_pattern` | repeat/clamp/mirror/decal 平铺模式 |
| **填充** | `<Fill>` color/alpha/blendMode/fillRule/placement | `painter_fill` | evenOdd/winding、foreground placement |
| | 子元素 `<SolidColor>` 覆盖 color 属性 | `painter_fill` | 颜色源优先级 |
| **描边** | `<Stroke>` color/width/alpha/cap/join/miterLimit | `painter_stroke` | butt/round/square cap, miter/round/bevel join |
| | `<Stroke>` dashes/dashOffset/dashAdaptive | `painter_stroke`, `stroke_dash_adaptive` | 虚线模式 + 自适应虚线 |
| | `<Stroke>` align (center/inside/outside) | `painter_stroke` | 描边对齐方式 |
| | `<Stroke>` 渐变色源 `<LinearGradient>` | `painter_stroke` | 描边渐变色 |
| | 多重 Fill + Stroke 叠加 | `painter_multiple` | 同一形状多画笔层叠 |
| | 多几何体累积共享画笔 | `painter_accumulated_geometry` | Rectangle + Ellipse 共享 Fill/Stroke |
| **修饰器** | `<MergePath>` mode (append/union/intersect/xor/difference) | `modifier_merge_path` | 5 种布尔路径运算 |
| | `<RoundCorner>` radius | `modifier_round_corner` | 圆角化尖角 |
| | `<TrimPath>` start/end/offset/type (separate/continuous) | `modifier_trim_path` | 路径裁剪 + 环绕 |
| **重复器** | `<Repeater>` copies/position/rotation/scale/anchor/offset | `repeater` | 多种重复配置 |
| | startAlpha/endAlpha/order (belowOriginal/aboveOriginal) | `repeater` | 透明度渐变 + 层序 |
| | 分数 copies (3.5) / copies=0 | `repeater` | 边缘情况 |
| **Layer 基础** | name/visible/alpha/antiAlias | `layer_visibility`, `layer_alpha`, `layer_antialias`, `edge_defaults` | 可见性、透明度、抗锯齿 |
| | blendMode（17 种） | `layer_blend_modes` | 完整混合模式列表 |
| | groupOpacity (true/false) | `layer_group_opacity` | 分组透明度 |
| | passThroughBackground (true/false) | `layer_pass_through` | 背景穿透 |
| | 嵌套子 Layer / z-order | `layer_nesting` | 父子层级关系 |
| **Layer 变换** | x/y 偏移 | `layer_transform_xy` | translate |
| | matrix (2D 6 值) | `layer_transform_matrix` | 旋转/缩放/倾斜/平移 |
| | matrix3D (16 值) + preserve3D | `layer_transform_matrix3d` | 3D 变换 |
| | 变换优先级: matrix3D > matrix > x/y | `layer_transform_priority` | 高优先级覆盖低优先级 |
| **裁剪/遮罩** | scrollRect | `clip_and_mask` | 矩形裁剪区域 |
| | mask + maskType (alpha/luminance/contour) | `clip_and_mask` | 遮罩引用（⚠️ 渲染待确认） |
| **Group** | anchor/position/rotation/scale/skew/skewAxis/alpha | `group` | Group 完整变换链 |
| | 作用域隔离 / 几何传播 | `group` | 子 Group 几何不泄露 / 传播给父 Fill |
| **资源** | `<Resources>` + `<SolidColor>` / `<PathData>` / `<Image>` / `<Font>` / `<Composition>` | 多个文件 | 各类资源定义 |
| | @id 引用 + 前向引用 | `resource_forward_ref` | 先使用后定义 |
| | `<Composition>` 实例化 + 变换 | `resource_composition` | 组件复用 |
| **滤镜** | `<BlurFilter>` blurX/blurY（均匀/非对称） | `filter_all` | 高斯模糊 |
| | `<DropShadowFilter>` offsetX/Y/blurX/Y/color/shadowOnly | `filter_all` | 投影滤镜 |
| | `<InnerShadowFilter>` offsetX/Y/blurX/Y/color/shadowOnly | `filter_all` | 内阴影滤镜 |
| | `<BlendFilter>` color/blendMode | `filter_all` | 颜色混合滤镜 |
| | `<ColorMatrixFilter>` matrix (20 值) | `filter_all` | 色彩矩阵变换 |
| | 链式滤镜（Blur + DropShadow） | `filter_all` | 多滤镜串联 |
| **样式** | `<DropShadowStyle>` offsetX/Y/blurX/Y/color/showBehindLayer | `style_shadows` | 投影样式 |
| | `<InnerShadowStyle>` offsetX/Y/blurX/Y/color | `style_shadows` | 内阴影样式 |
| | `<BackgroundBlurStyle>` blurX/blurY | `style_shadows` | 背景模糊样式 |
| **文本** | `<Text>` text/fontFamily/fontStyle/fontSize/position/textAnchor | `text_basic` | 基础文本属性 |
| | letterSpacing/fauxBold/fauxItalic | `text_basic` | 文本装饰 |
| | `<TextBox>` position/size/textAlign/paragraphAlign/lineHeight | `text_box` | 文本框布局 |
| | writingMode (vertical)/overflow (hidden)/wordWrap | `text_box` | 竖排/溢出裁剪/换行控制 |
| | `<GlyphRun>` font/fontSize/glyphs/xOffsets/rotations/scales | `text_glyph_run` | 自定义字体字形定位 |
| | `<Font>` + `<Glyph>` id/advance/path | `text_glyph_run` | 内嵌字体资源 |
| | `<TextModifier>` position/rotation/scale/alpha/fillColor | `text_modifier` | 逐字符变换 |
| | `<RangeSelector>` start/end/shape/easeIn/easeOut | `text_modifier` | triangle/rampUp/smooth/rampDown 选择器 |
| | `<TextPath>` path/perpendicular/firstMargin | `text_path` | 文本沿路径排列 |
| | 富文本（多 Text + 各自 Fill + 共享 TextBox） | `text_rich` | 多色文本（⚠️ 渲染单色） |
| **默认值** | 所有属性的默认值行为 | `edge_defaults` | 无属性时的默认渲染 |

---

## 三、需要人工确认的 7 个偏差项

| # | 文件 | 偏差描述 | 可能原因 |
|---|------|----------|----------|
| 1 | `color_radial_gradient` | 第 2 个矩形几乎纯红，预期应看到白→红渐变过渡 | 渐变 center+radius 相对局部坐标，加 matrix 后白色区域可能太小 |
| 2 | `color_image_pattern` | 仅 3 个方块可见，decal 模式不可见；3 个可见方块均为纯绿无法区分平铺差异 | 源图 10×10 纯绿太小，不能有效展示 tile 差异 |
| 3 | `clip_and_mask` | 仅 scrollRect 区域可见，3 种 mask 类型(alpha/luminance/contour)不可见 | 可能是引擎尚未实现 `mask="@id"` 引用方式 |
| 4 | `layer_transform_priority` | 绿色矩形不可见（matrix 覆盖 x/y 场景） | 可能是 matrix+position 组合的坐标计算差异 |
| 5 | `text_glyph_run` | 字形可见但偏小，Layer 2 红色字形几乎不可见 | unitsPerEm=1000 + fontSize=36/48 缩放后字形较小；rotations/scales 变换导致位置偏移 |
| 6 | `text_rich` | 3 色富文本只显示单一颜色（绿色） | 引擎中多 Text+Fill 交替排列时最后一个 Fill 覆盖所有文字 |
| 7 | `repeater` | 左上角有一个小红色三角形残留，不属于任何预期图形 | Repeater 的 rotation 旋转 8 个 copies 时，部分 copy 超出画布外但起始位置残留 |
