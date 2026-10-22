# PAGX 节点字段提取：矢量元素与文本

> M0-1 产出。来源：main 仓库 HEAD（分支 feature/henryjpxie_pagx_to_pag）include/pagx/nodes/*.h、include/pagx/types/*.h、src/pagx/PAGXNodeChannel.cpp:405-820。
> 注意：libpag_pagx_to_pag/ 原型目录是分叉的旧拷贝，其 NodeType 数值与 main 不一致；本规范一律以 main 仓库为准。

# PAGX 矢量元素与文本节点字段清单（PAGX→PAG 二进制规范用）

来源：main 仓库 HEAD ba9952640（分支 feature/henryjpxie_pagx_to_pag，= origin/main）。字段/默认值/语义全部取自 include/pagx/nodes/*.h、include/pagx/types/*.h 实际代码注释；通道取自 src/pagx/PAGXNodeChannel.cpp 通道表（PAGXNodeChannel.cpp:405-820）。

## 通用约定

**通道标记**（src/pagx/PAGXChannelTable.h:26 ChannelFlags）：
- `Anim` = Animatable：有运行时 writer，动画/DataBind 可原位驱动；文档编辑即时生效
- `AnimLayout` = Animatable|RequiresLayout：可动画；文档编辑后需重跑 layout 才生效
- `Layout` = RequiresLayout：不可动画；仅文档编辑，需 layout
- `NoFlags`：有通道可文档编辑，不可动画，无需 layout
- `无通道`：字段完全不走通道 API（容器、资源指针、向量值等）

**通道值类型**（ChannelValueType，include/pagx/nodes/KeyValue.h）：Float=0, Bool=1, Int=2, String=3, ImageRef=4, Color=5, Matrix=6, PAGImage=7。本范围只用 Float/Bool/Int/String/Color；枚举通道以 String（XML 属性名，经 StringParser 转换）承载。

**Node 基类公共字段**（所有节点，均无通道）：id: string = ""；customData: unordered_map<string,string> = {}（键不含 "data-" 前缀，仅元数据不参与运行时）；sourceLine: int = -1（导入器填的源 XML 行号，非序列化语义）。

**LayoutNode 公共字段**（Rectangle/Ellipse/Polystar/Path/Text/TextPath/Group/TextBox 继承；每节点追加 10 个通道，全部 Float + Layout）：

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| left | float | NAN | 距父容器左缘偏移 px | left: Float, Layout |
| right | float | NAN | 距父容器右缘偏移 px | right: Float, Layout |
| top | float | NAN | 距父容器顶缘偏移 px | top: Float, Layout |
| bottom | float | NAN | 距父容器底缘偏移 px | bottom: Float, Layout |
| centerX | float | NAN | 距父容器中心水平偏移，优先于 left/right | centerX: Float, Layout |
| centerY | float | NAN | 距父容器中心垂直偏移，优先于 top/bottom | centerY: Float, Layout |
| percentWidth | float | NAN | 父容器布局宽（padding 内区）百分比，预期 [0,100]，优先于 width，被双向边约束覆盖 | percentWidth: Float, Layout |
| percentHeight | float | NAN | 同上垂直方向 | percentHeight: Float, Layout |
| width | float | NAN | 显式布局宽；可缩放元素（Path/Polystar/Text/TextPath）以此为目标均匀缩放 | width: Float, Layout |
| height | float | NAN | 显式布局高 | height: Float, Layout |

LayoutNode 私有布局输出（preferredX/Y/Width/Height、layoutX/Y/Width/Height）为运行时缓存，非序列化字段。

---

### Rectangle (NodeType::Rectangle = 34)

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| position | Point | {NAN, NAN} | 矩形中心点；NaN=未设置，回退 (size.width/2, size.height/2)；可被布局约束覆盖 | position.x / position.y: Float, AnimLayout |
| size | Size | {0, 0} | 矩形尺寸 | size.width / size.height: Float, AnimLayout |
| roundness | float | 0 | 圆角半径 px | roundness: Float, Anim |
| reversed | bool | false | 路径方向反转 | reversed: Bool, NoFlags |

+ LayoutNode 公共字段 10 项。
子节点：无（叶子几何）。运行时派生方法（非字段）：renderPosition()、renderSize()。

### Ellipse (NodeType::Ellipse = 35)

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| position | Point | {NAN, NAN} | 椭圆中心点；NaN 回退同 Rectangle | position.x / position.y: Float, AnimLayout |
| size | Size | {0, 0} | 椭圆尺寸 | size.width / size.height: Float, AnimLayout |
| reversed | bool | false | 路径方向反转 | reversed: Bool, NoFlags |

+ LayoutNode ×10。子节点：无。

### Polystar (NodeType::Polystar = 36)

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| position | Point | {NAN, NAN} | 中心点；NaN=左上角对齐布局原点 | position.x / position.y: Float, AnimLayout |
| type | PolystarType | Star | 星形/多边形 | type: String, Layout |
| pointCount | float | 5 | 点数 | pointCount: Float, AnimLayout |
| outerRadius | float | 100 | 外半径 px | outerRadius: Float, AnimLayout |
| innerRadius | float | 50 | 内半径，仅 Star 生效 | innerRadius: Float, AnimLayout |
| rotation | float | 0 | 旋转角（度） | rotation: Float, AnimLayout |
| outerRoundness | float | 0 | 外角圆化程度 0..1 | outerRoundness: Float, Anim |
| innerRoundness | float | 0 | 内角圆化 0..1，仅 Star | innerRoundness: Float, Anim |
| reversed | bool | false | 路径方向反转 | reversed: Bool, NoFlags |

+ LayoutNode ×10。子节点：无。方法：getContentBounds()（按实际顶点算紧致包围盒）、renderPosition()、renderScale()、renderOuterRadius()、renderInnerRadius()。

### Path (NodeType::Path = 37)

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| data | PathData* | nullptr | 路径数据（顶点/出入控制点）资源引用（NodeType::PathData=1） | 无通道 |
| position | Point | {0, 0} | 路径坐标系原点偏移 | position.x / position.y: Float, **Layout**（注意与其他形状不同，非 AnimLayout） |
| reversed | bool | false | 路径方向反转 | reversed: Bool, NoFlags |

+ LayoutNode ×10。子节点：无（data 为资源引用）。方法：renderPosition()、renderScale()。

### Fill (NodeType::Fill = 39)

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| color | ColorSource* | nullptr | 颜色源（SolidColor/LinearGradient/RadialGradient/ConicGradient/DiamondGradient/ImagePattern 或 "@id" 引用） | 无通道（指针） |
| alpha | float | 1 | 不透明度 0..1 | alpha: Float, Anim |
| blendMode | BlendMode | Normal | 合成模式 | blendMode: String, NoFlags |
| fillRule | FillRule | Winding | 自交路径内部判定规则 | fillRule: String, NoFlags |
| placement | LayerPlacement | Background | 相对描边的前后层叠 | placement: String, NoFlags |

特殊通道 `color`（Color, Anim）：读写穿透到 color 指针指向的 **SolidColor** 子节点的 color 字段；color 源不是 SolidColor（渐变/图案）时读写返回失败（PAGXNodeChannel.cpp:182-211 AccessFillStrokeColor）。
子节点：color: ColorSource*（单引用，非容器）。无 LayoutNode 继承。

### Stroke (NodeType::Stroke = 40)

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| color | ColorSource* | nullptr | 颜色源，同 Fill | 无通道（指针；穿透通道 color: Color, Anim，同 Fill 规则） |
| width | float | 1 | 线宽 px | width: Float, Anim |
| alpha | float | 1 | 不透明度 0..1 | alpha: Float, Anim |
| blendMode | BlendMode | Normal | 合成模式 | blendMode: String, NoFlags |
| cap | LineCap | Butt | 开放路径端帽样式 | cap: String, NoFlags |
| join | LineJoin | Miter | 拐角结合样式 | join: String, NoFlags |
| miterLimit | float | 4 | Miter 被 Bevel 截断的阈值 | miterLimit: Float, Anim |
| dashes | vector<float> | {} | 虚线 [dash,gap,...] 序列，空=实线 | **无通道**（向量值不可经通道 API 编辑/动画） |
| dashOffset | float | 0 | 虚线相位偏移 | dashOffset: Float, Anim |
| dashAdaptive | bool | false | 缩放虚线间隔使各段等长 | dashAdaptive: Bool, NoFlags |
| align | StrokeAlign | Center | 描边相对路径的对齐（居中/内/外） | align: String, NoFlags |
| placement | LayerPlacement | Background | 相对填充的前后层叠 | placement: String, NoFlags |

子节点：color: ColorSource*。无 LayoutNode 继承。

### TrimPath (NodeType::TrimPath = 41)

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| start | float | 0 | 裁剪起点，路径长度百分比 0..1 | start: Float, Anim |
| end | float | 1 | 裁剪终点 0..1 | end: Float, Anim |
| offset | float | 0 | 相位偏移；单位为度，360°=一整圈路径长 | offset: Float, Anim |
| type | TrimType | Separate | Separate=逐路径裁剪；Continuous=所有路径连成一条裁剪 | type: String, NoFlags |

子节点：无。

### RoundCorner (NodeType::RoundCorner = 42)

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| radius | float | 10 | 圆角半径 px | radius: Float, Anim |

子节点：无。

### MergePath (NodeType::MergePath = 43)

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| mode | MergePathMode | Append | 布尔合并模式 | mode: String, NoFlags |

子节点：无。

### Repeater (NodeType::Repeater = 48)

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| copies | float | 3 | 拷贝数量（允许小数=分数拷贝） | copies: Float, Anim |
| offset | float | 0 | 拷贝索引偏移 | offset: Float, Anim |
| order | RepeaterOrder | BelowOriginal | 拷贝堆叠在原图下/上 | order: String, NoFlags |
| anchor | Point | {0, 0} | 变换锚点 | anchor.x / anchor.y: Float, Anim |
| position | Point | {100, 100} | 相邻拷贝间位移增量 | position.x / position.y: Float, Anim |
| rotation | float | 0 | 相邻拷贝间旋转增量（度） | rotation: Float, Anim |
| scale | Point | {1, 1} | 相邻拷贝间缩放增量 | scale.x / scale.y: Float, Anim |
| startAlpha | float | 1 | 首拷贝不透明度 0..1 | startAlpha: Float, Anim |
| endAlpha | float | 1 | 末拷贝不透明度 0..1 | endAlpha: Float, Anim |

子节点：无（修改器，作用于容器中前面的兄弟元素）。无 LayoutNode 继承。

### TextPath (NodeType::TextPath = 45)

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| path | PathData* | nullptr | 文本沿排的路径数据（内联或 PathData 资源引用） | 无通道 |
| baselineOrigin | Point | {0, 0} | 局部坐标系基线原点；字形按基线距离落到曲线，垂直偏移保留 | baselineOrigin.x / baselineOrigin.y: Float, Layout |
| baselineAngle | float | 0 | 基线角度（度）；0=水平，90=垂直 | baselineAngle: Float, Layout |
| firstMargin | float | 0 | 路径起点边距 px | firstMargin: Float, Layout |
| lastMargin | float | 0 | 路径终点边距 px | lastMargin: Float, Layout |
| perpendicular | bool | true | 字符旋转以垂直于路径 | perpendicular: Bool, Layout |
| reversed | bool | false | 反转路径方向 | reversed: Bool, NoFlags |
| forceAlignment | bool | false | 拉伸字形间距填满 firstMargin~lastMargin 区域 | forceAlignment: Bool, Layout |

+ LayoutNode ×10。子节点：无（path 为资源引用）。方法：renderPosition()、renderScale()、renderBaselineOrigin()。

### Text (NodeType::Text = 38)

渲染模式：预整形（有 glyphRuns 时优先，配嵌入字体保证跨端一致）/ 运行时整形（用 text+字体属性现场排版，跨端可能有差异）。

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| text | string | "" | 文本内容；\n 换行（行高=字体 ascent+descent+leading） | text: String, AnimLayout |
| position | Point | {0, 0} | 文本原点；y 语义由 baseline 字段决定 | x / y: Float, AnimLayout（注意通道名是 x、y 不是 position.x/y） |
| fontFamily | string | "" | 字体族名（如 "Arial"），运行时整形用 | fontFamily: String, AnimLayout |
| fontStyle | string | "" | 字体变体名（"Regular"/"Bold"/"Italic"/"Bold Italic"），空通常匹配到 Regular | fontStyle: String, AnimLayout |
| fontSize | float | 12 | 字号 px | fontSize: Float, AnimLayout |
| letterSpacing | float | 0 | 字距（tracking） | letterSpacing: Float, AnimLayout |
| fauxBold | bool | false | 算法加粗 | fauxBold: Bool, AnimLayout |
| fauxItalic | bool | false | 算法斜体 | fauxItalic: Bool, AnimLayout |
| textAnchor | TextAnchor | Start | 相对原点对齐（Start/Center/End）；被 TextBox 接管排版时忽略 | textAnchor: String, Layout |
| glyphRuns | vector<GlyphRun*> | {} | 预整形字形运行，存在时优先于运行时整形 | 无通道（容器） |
| baseline | TextBaseline | LineBox | position.y 语义：LineBox=行盒顶部（按字体行高）；Alphabetic=字母基线（SVG text 用） | baseline: String, Layout |

+ LayoutNode ×10。私有运行时：glyphData 指针、textBounds、textScale（非序列化）。
子节点：glyphRuns: GlyphRun*。
通道表注释（PAGXNodeChannel.cpp:477-484）：text 塑形类通道有运行时 writer（TextHolder），动画/VM 可原位驱动但也需 reshape；textAnchor/baseline 无 writer，是容器级参数，保持 Layout。

### GlyphRun (NodeType::GlyphRun = 50)（Text 的子节点元素）

| 字段 | 类型 | 默认值 | 语义 |
|---|---|---|---|
| font | Font* | nullptr | 字体资源引用（NodeType::Font=11） |
| fontSize | float | 12 | 渲染字号；实际缩放 = fontSize / font.unitsPerEm |
| glyphs | vector<uint16_t> | {} | GlyphID 序列；0=缺字形不渲染 |
| x | float | 0 | 整体 X 偏移 |
| y | float | 0 | 整体 Y 偏移 |
| xOffsets | vector<float> | {} | 每字形 X 偏移；最终 X = x + xOffsets[i] + positions[i].x |
| positions | vector<Point> | {} | 每字形 (x,y) 偏移；最终 Y = y + positions[i].y |
| anchors | vector<Point> | {} | 每字形锚点偏移（默认锚 = advance*0.5, 0；缩放/旋转/斜切围绕锚点） |
| scales | vector<Point> | {} | 每字形缩放，默认 (1,1) |
| rotations | vector<float> | {} | 每字形旋转角（度），默认 0 |
| skews | vector<float> | {} | 每字形斜切角（度，沿垂直轴），默认 0 |
| bounds | Rect | {} | 文本块在父坐标系边界；仅每个 Text 的首个 GlyphRun 携带整块 bounds |

**无通道注册**（ChannelsFor 无 GlyphRun case）：全部字段不可动画/不可绑定。
子节点：font: Font*（资源引用）。

### TextBox (NodeType::TextBox = 46)（继承 Group=47）

排版属性（自有）：

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| textAlign | TextAlign | Start | 行内方向对齐（横排=行内水平，竖排=列内垂直） | textAlign: String, Layout |
| paragraphAlign | ParagraphAlign | Near | 块流方向段落对齐（横排=行垂直布局，竖排=列水平布局） | paragraphAlign: String, Layout |
| writingMode | WritingMode | Horizontal | 横排/竖排（竖排列从右到左，传统 CJK） | writingMode: String, Layout |
| lineHeight | float | 0 | 行高 px；0=自动（ascent+descent+leading）；正值=固定行高；竖排时此逻辑属性控制列宽 | lineHeight: Float, Layout |
| wordWrap | bool | true | 自动换行（横排按宽、竖排按高；对应维度 NaN 时无效） | wordWrap: Bool, Layout |
| overflow | Overflow | Visible | 溢出行为；Hidden=排版时整行/整列丢弃（对应维度 NaN 时无效） | overflow: String, Layout |

继承自 Group 的变换/容器字段（AppendGroupCommonFields，PAGXNodeChannel.cpp:601-624）：

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| anchor | Point | {0, 0} | 变换锚点 | anchor.x / anchor.y: Float, Anim |
| position | Point | {0, 0} | 位移 | position.x / position.y: Float, AnimLayout |
| rotation | float | 0 | 旋转角（度） | rotation: Float, Anim |
| scale | Point | {1, 1} | 缩放 (scaleX, scaleY) | scale.x / scale.y: Float, Anim |
| skew | float | 0 | 斜切角（度） | skew: Float, Anim |
| skewAxis | float | 0 | 斜切轴角（度） | skewAxis: Float, Anim |
| alpha | float | 1 | 不透明度 0..1 | alpha: Float, Anim |
| elements | vector<Element*> | {} | 子元素容器（Text 等） | 无通道（容器） |
| padding | Padding | 全 0 | 内边距，内缩子元素约束参考系 | padding.left/top/right/bottom: Float, Layout（4 通道） |

+ LayoutNode ×10（width/height 即文本区尺寸，NaN=该维度自适应无边界）。
子节点：elements: Element*。排版后 TextBox 行为等同含定位 Text 的普通 Group。

### TextModifier (NodeType::TextModifier = 44)

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| anchor | Point | {0, 0} | 变换锚点 | anchor.x / anchor.y: Float, Anim |
| position | Point | {0, 0} | 选中字符位移 | position.x / position.y: Float, Anim |
| rotation | float | 0 | 选中字符旋转（度） | rotation: Float, Anim |
| scale | Point | {1, 1} | 选中字符缩放 | scale.x / scale.y: Float, Anim |
| skew | float | 0 | 选中字符斜切（度） | skew: Float, Anim |
| skewAxis | float | 0 | 斜切轴角（度） | skewAxis: Float, Anim |
| alpha | float | 1 | 选中字符不透明度 0..1 | alpha: Float, Anim |
| fillColor | optional<Color> | nullopt | 填充色覆盖 | fillColor: Color, Anim（optional；未设值时读取失败） |
| strokeColor | optional<Color> | nullopt | 描边色覆盖 | strokeColor: Color, Anim |
| strokeWidth | optional<float> | nullopt | 描边宽覆盖 | strokeWidth: Float, Anim |
| selectors | vector<TextSelector*> | {} | 选择器列表，决定哪些字符受影响 | 无通道（容器） |

子节点：selectors: TextSelector*（PAGX 内唯一具体子类为 RangeSelector）。无 LayoutNode 继承。

### TextSelector（抽象基类，无独立 NodeType 值）
直接继承 Node，无自有字段、无通道。PAGX 体系唯一具体子类：RangeSelector。（include/pag/file.h 的 TextRangeSelector/TextWigglySelector 属旧 PAG 文件模型，不在 PAGX 节点体系。）

### RangeSelector (NodeType::RangeSelector = 49)

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| start | float | 0 | 选择区间起点（单位由 unit 决定） | start: Float, Anim |
| end | float | 1 | 选择区间终点 | end: Float, Anim |
| offset | float | 0 | 区间平移 | offset: Float, Anim |
| unit | SelectorUnit | Percentage | start/end/offset 的单位（字符索引/百分比） | unit: String, NoFlags |
| shape | SelectorShape | Square | 选择衰减曲线形状 | shape: String, NoFlags |
| easeIn | float | 0 | 起端缓入量 0..1 | easeIn: Float, Anim |
| easeOut | float | 0 | 末端缓出量 0..1 | easeOut: Float, Anim |
| mode | SelectorMode | Add | 多选择器组合模式 | mode: String, NoFlags |
| weight | float | 1 | 本选择器影响权重 0..1 | weight: Float, Anim |
| randomOrder | bool | false | 随机化字符选择顺序 | randomOrder: Bool, NoFlags |
| randomSeed | int | 0 | 随机序种子 | randomSeed: Int, NoFlags |

子节点：无。

---

## 枚举总表（include/pagx/types/*.h，均无显式赋值，默认 0 起递增）

- PolystarType: Polygon=0, Star=1
- FillRule: Winding=0, EvenOdd=1
- LineCap: Butt=0, Round=1, Square=2
- LineJoin: Miter=0, Round=1, Bevel=2
- StrokeAlign: Center=0, Inside=1, Outside=2
- TrimType: Separate=0, Continuous=1
- MergePathMode: Append=0, Union=1, Intersect=2, Xor=3, Difference=4
- RepeaterOrder: BelowOriginal=0, AboveOriginal=1
- TextAlign: Start=0, Center=1, End=2, Justify=3
- TextAnchor: Start=0, Center=1, End=2
- TextBaseline: LineBox=0, Alphabetic=1
- WritingMode: Horizontal=0, Vertical=1
- ParagraphAlign: Near=0, Middle=1, Far=2
- Overflow: Visible=0, Hidden=1
- LayerPlacement: Background=0, Foreground=1
- SelectorUnit: Index=0, Percentage=1
- SelectorShape: Square=0, RampUp=1, RampDown=2, Triangle=3, Round=4, Smooth=5
- SelectorMode: Add=0, Subtract=1, Intersect=2, Min=3, Max=4, Difference=5
- ColorSpace（Color.colorSpace 用）: SRGB=0, DisplayP3=1
- BlendMode: Normal=0, Multiply=1, Screen=2, Overlay=3, Darken=4, Lighten=5, ColorDodge=6, ColorBurn=7, HardLight=8, SoftLight=9, Difference=10, Exclusion=11, Hue=12, Saturation=13, Color=14, Luminosity=15, PlusLighter=16, PlusDarker=17

## NodeType 数值（main 仓库 Node.h，0..54）

本范围：Rectangle=34, Ellipse=35, Polystar=36, Path=37, Text=38, Fill=39, Stroke=40, TrimPath=41, RoundCorner=42, MergePath=43, TextModifier=44, TextPath=45, TextBox=46, Group=47, Repeater=48, RangeSelector=49, GlyphRun=50。
完整：Document=0, PathData=1, Image=2, Composition=3, SolidColor=4, LinearGradient=5, RadialGradient=6, ConicGradient=7, DiamondGradient=8, ImagePattern=9, ColorStop=10, Font=11, Glyph=12, Animation=13, AnimationObject=14, Channel=15, StateMachine=16, StateRegion=17, State=18, StateTransition=19, TransitionCondition=20, StateMachineInput=21, Layer=22, DropShadowStyle=23, InnerShadowStyle=24, BackgroundBlurStyle=25, NoiseStyle=26, GlassStyle=27, BlurFilter=28, DropShadowFilter=29, InnerShadowFilter=30, BlendFilter=31, ColorMatrixFilter=32, NoiseFilter=33, Rectangle=34, Ellipse=35, Polystar=36, Path=37, Text=38, Fill=39, Stroke=40, TrimPath=41, RoundCorner=42, MergePath=43, TextModifier=44, TextPath=45, TextBox=46, Group=47, Repeater=48, RangeSelector=49, GlyphRun=50, ViewModel=51, ViewModelProperty=52, DataBind=53, DataConverter=54。

## [待确认] 事项

1. **worktree 分歧**（重要）：libpag_pagx_to_pag/（分支 feature/henryjpxie_pagx_pag，HEAD ee64dcbbf，含未跟踪 docs/pagx_to_pag_v2_summary.md）与 main 在规范相关处不一致：
   - Text 通道标记：main 为 AnimLayout（text/x/y/fontFamily/fontStyle/fontSize/letterSpacing/fauxBold/fauxItalic，带 TextHolder 运行时 writer 注释，PAGXNodeChannel.cpp:477-484）；worktree 全为 Layout。二进制若区分"可动画通道集合"需钉死基准树。**→ 已裁决：以 main 仓库为准（本规范前提）。**
   - worktree 的 Node.h 缺 StateMachine 6 枚举与 GlassStyle，全部 NodeType 数值偏移（worktree 中 Rectangle=27、Text=31、GlyphRun=43 等）。枚举数值以哪棵树为准必须确认。**→ 已裁决：以 main 仓库为准；NodeType 数值不写入二进制（TagCode 已区分节点类型），仅规范文档引用。**
   - worktree 的 Text.h 多 firstBaselineY()/layoutOrigin() 两个方法（PAG v2 Baker 用的运行时派生，非序列化字段），字段清单本身无差异。
2. Stroke.align 字段注释写 "(Center, Inner, or Outer)"，实际枚举值为 Center/Inside/Outside，以枚举为准。
3. Text.position 字段注释 "y is the baseline" 与 baseline=LineBox（默认，y=行盒顶部）冲突，以 baseline 字段注释为准。
4. Fill/Stroke 的 color 通道仅在 color 指针指向 SolidColor 时可用；源为渐变/图案时该通道读写失败。二进制侧若单独序列化 color 需区分"颜色值"与"颜色源结构"。
5. GlyphRun 无任何通道注册，所有字段不可动画/DataBind；Stroke.dashes（向量）同样无通道。

DataBind 补充语义：导入仅校验 source/target/channel 非空（PAGXImporter.cpp:3187-3203），未强制 Animatable；运行时经 RuntimeBinding::apply 走通道 writer，回读（ToSource/TwoWay）需通道有已注册 reader，否则跳过（DataBindRuntime.cpp:385-390）。
