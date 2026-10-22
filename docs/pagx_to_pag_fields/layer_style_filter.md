# PAGX 节点字段提取：图层与样式/滤镜

> M0-1 产出。来源：include/pagx/nodes/*.h、include/pagx/types/*.h、src/pagx/PAGXNodeChannel.cpp。

# PAGX→PAG 二进制规范字段提取：图层与样式/滤镜

来源：include/pagx/nodes/*.h、include/pagx/types/*.h、src/pagx/PAGXNodeChannel.cpp（全部逐一读取原文，无猜测）。

## 通用约定

**ChannelValueType**（include/pagx/nodes/KeyValue.h:52，KeyValue variant 顺序与之一致）：
- Float=0, Bool=1, Int=2, String=3, ImageRef=4, Color=5, Matrix=6, PAGImage=7

**ChannelFlags**（src/pagx/PAGXChannelTable.h:33）：
- None=0, Animatable=1<<0（有运行时 writer，可被动画/DataBind 原地驱动）, RequiresLayout=1<<1（文档编辑后需 layout pass 才生效）
- 组合语义：Anim=可动画；Layout=仅可编辑（不可动画）；AnimLayout=两者（编辑需 layout）；NoFlags=可反射编辑（无需 layout）但不可动画
- 可写入性判定：注册了通道的字段都可经 SetNodeChannel 写入（DataBind/编辑路径）；带 Animatable 的才能被动画驱动；未注册通道的字段动画/DataBind 都不可写
- 枚举通道经 StringParser 以字符串名承载（与 XML 属性一致），ChannelValueType=String

**Node 基类字段**（include/pagx/nodes/Node.h:127，所有节点继承，均无通道注册）：
- id: std::string, 默认 {}，节点唯一标识，"@id" 引用
- customData: std::unordered_map<std::string, std::string>, 默认 {}，data-* 自定义属性（键不含前缀，仅工具间传元数据，运行时不处理）
- sourceLine: int, 默认 -1，源 XML 行号（解析器填写，-1=程序创建）

**Color 默认值**（types/Color.h）: {red=0, green=0, blue=0, alpha=1, colorSpace=SRGB}（不透明黑）。**Matrix 默认值**: {a=1,b=0,c=0,d=1,tx=0,ty=0}（单位阵）。**Matrix3D 默认值**: values[16]={1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}（单位阵）。**Rect 默认值**: {x=0,y=0,width=0,height=0}。**Point 默认值**: {x=0,y=0}。**Padding 默认值**: {top=0,right=0,bottom=0,left=0}（四值简写顺序为 top,right,bottom,left）。

---

### Layer (NodeType::Layer=22)
继承 Node + LayoutNode。图层节点，承载矢量元素、样式、滤镜与子图层。

字段表（通道列格式：ChannelValueType/标记）：

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| id（继承 Node） | std::string | {} | 节点唯一标识 | 无通道 |
| customData（继承 Node） | unordered_map<string,string> | {} | data-* 自定义属性 | 无通道 |
| sourceLine（继承 Node） | int | -1 | 源 XML 行号 | 无通道 |
| name | std::string | {} | 图层显示名 | String/NoFlags |
| visible | bool | true | 是否可见 | Bool/Anim |
| alpha | float | 1.0f | 不透明度 [0,1] | Float/Anim |
| blendMode | BlendMode | Normal | 合成混合模式 | String(BlendMode)/Anim |
| x | float | 0 | 图层位置 X 坐标（布局输入，渲染经 renderPosition 解析） | Float/AnimLayout |
| y | float | 0 | 图层位置 Y 坐标（同上） | Float/AnimLayout |
| matrix | Matrix | 单位阵 | 2D 变换矩阵（渲染字段） | 无通道 |
| matrix3D | Matrix3D | 单位阵 | 3D 变换矩阵（渲染字段） | 无通道 |
| preserve3D | bool | false | 子层是否保留 3D 变换 | Bool/NoFlags |
| antiAlias | bool | true | 图层边缘抗锯齿 | Bool/NoFlags |
| groupOpacity | bool | true | 图层及其子内容是否组透明度模式 | Bool/NoFlags |
| passThroughBackground | bool | true | 图层效果是否穿透到背景 | Bool/NoFlags |
| scrollRect | Rect | {0,0,0,0} | 滚动裁剪矩形（渲染字段） | 无通道 |
| hasScrollRect | bool | false | 是否应用 scrollRect | 无通道 |
| clipToBounds | bool | false | 裁剪到自身 bounds；true 时自动布局展开为 scrollRect；已有 scrollRect 或两维均未解析尺寸时忽略 | Bool/Layout |
| mask | Layer* | nullptr | 蒙版图层引用 | 无通道（引用字段） |
| maskType | MaskType | Alpha | 蒙版类型 Alpha/Luminance/Contour | String(MaskType)/NoFlags |
| composition | Composition* | nullptr | 作为图层内容的合成引用 | 无通道（引用字段） |
| compositionFilePath | std::string | {} | composition 属性为文件路径时的外部 PAGX 路径，由 PAGXDocument::loadFileData 解析 | 无通道 |
| externalDoc | std::shared_ptr<PAGXDocument> | nullptr | 外部加载文档作为合成内容；timelines 目标查找在该文档内解析 | 无通道 |
| timelines | std::vector<std::unique_ptr<Timeline>> | {} | 引用合成时激活的时间线（v1 为 AnimationTimeline）；空表=静态渲染无时间线 | 无通道（子节点容器） |
| vmContext | std::string | {} | DataBind 源解析的 ViewModel 上下文路径（如 "$vm.propertyName"） | 无通道 |
| contents | std::vector<Element*> | {} | 矢量元素（几何/画笔/修饰符） | 子节点容器，无通道 |
| styles | std::vector<LayerStyle*> | {} | 图层样式列表 | 子节点容器，无通道 |
| filters | std::vector<LayerFilter*> | {} | 滤镜列表 | 子节点容器，无通道 |
| children | std::vector<Layer*> | {} | 子图层列表 | 子节点容器，无通道 |
| layout | LayoutMode | None | 容器布局模式；Horizontal/Vertical 时子层沿该轴自动排列 | String(LayoutMode)/Layout |
| gap | float | 0 | 布局方向相邻子层间距 | Float/Layout |
| flex | float | 0 | 主轴伸缩权重；无显式主轴尺寸时 flex>0 按权重分剩余空间；有显式 width/height 时忽略 | Float/Layout |
| padding | Padding | 全 0 | 容器内边距 | padding.left/top/right/bottom 各 Float/Layout |
| alignment | Alignment | Stretch | 子层交叉轴对齐；有显式/百分比交叉轴尺寸的子层不被 Stretch 拉伸 | String(Alignment)/Layout |
| arrangement | Arrangement | Start | 子层主轴排列；显式/百分比主轴尺寸先保留，仅剩余空间按此分配给 flex 子层 | String(Arrangement)/Layout |
| includeInLayout | bool | true | 是否参与父容器布局（false=不测量不排位，保留自身 x/y 可见） | Bool/Layout |
| importDirective.source | std::string | {} | 导入指令：外部文件路径（如 SVG），相对 PAGX 文件 | 无通道 |
| importDirective.format | std::string | {} | 强制导入格式（如 "svg"）；空=按扩展名/子标签推断 | 无通道 |
| importDirective.content | std::string | {} | 内联导入原文（如 <svg>...</svg>），解析器填 | 无通道 |
| importDirective.resolvedFrom | std::string | {} | 解析后来源描述（导出为 XML 注释），pagx resolve 填 | 无通道 |
| left/right/top/bottom（继承 LayoutNode） | float | NAN | 距父容器各边偏移，NaN=未设 | Float/Layout |
| centerX/centerY（继承 LayoutNode） | float | NAN | 距父中心偏移，正=右/下；优先于 left/right、top/bottom | Float/Layout |
| percentWidth/percentHeight（继承 LayoutNode） | float | NAN | 父容器布局宽高（去 padding）百分比，期望 [0,100]；优先于显式尺寸，被双边约束覆盖 | Float/Layout |
| width/height（继承 LayoutNode） | float | NAN | 显式布局尺寸；对可缩放元素（Path/Polystar/Text/TextPath）定义目标尺寸并等比缩放 | Float/Layout |

LayoutNode 私有成员 preferredX/Y/Width/Height、layoutX/Y/Width/Height 为布局计算输出（两阶段：自底向上 measure + 自顶向下约束解析），非输入字段，不应入二进制。

**规范扩展新增（§6.2）**：anchor/rotation/scale/skew/skewAxis —— 当前 Layer.h 中**不存在**这些字段（Layer 变换由 x/y + matrix/matrix3D 表达），为规范扩展新增。注意 Group 已有同名字段（见下）。

**通道未覆盖说明**：ChannelValueType 含 Matrix=6 但当前无任何节点注册 Matrix 通道（Layer.matrix/matrix3D 均未注册）。

---

### Group (NodeType::Group=47)
继承 Element + LayoutNode。带自身变换的矢量元素容器，可含几何、画笔、修饰符与嵌套 Group。

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| id/customData/sourceLine（继承 Node） | — | — | 同 Node 基类 | 无通道 |
| anchor | Point | {0,0} | 变换锚点 | anchor.x/y 各 Float/Anim |
| position | Point | {0,0} | 位置偏移（布局输入，渲染经 renderPosition） | position.x/y 各 Float/AnimLayout |
| rotation | float | 0 | 旋转角（度） | Float/Anim |
| scale | Point | {1,1} | 缩放 (scaleX, scaleY) | scale.x/y 各 Float/Anim |
| skew | float | 0 | 斜切角（度） | Float/Anim |
| skewAxis | float | 0 | 斜切轴角（度） | Float/Anim |
| alpha | float | 1.0f | 不透明度 [0,1] | Float/Anim |
| elements | std::vector<Element*> | {} | 子元素列表 | 子节点容器，无通道 |
| padding | Padding | 全 0 | 组内边距，内缩子元素约束参考系 | padding.* 各 Float/Layout |
| left/right/top/bottom/centerX/centerY/percentWidth/percentHeight/width/height（继承 LayoutNode） | float | NAN | 同 LayoutNode 语义 | Float/Layout（各注册一通道） |

子节点：- elements: Element*

---

### Element（抽象基类，无 NodeType 独立值）
Node 派生，Layer/Group 内所有矢量元素（Rectangle/Ellipse/Polystar/Path/Text/Fill/Stroke/TrimPath/RoundCorner/MergePath/TextModifier/TextPath/TextBox/Group/Repeater）的基类。**无自有公共字段**，仅继承 Node 的 id/customData/sourceLine（无通道）。

---

### LayerStyle（基类，无独立 NodeType 值）
DropShadowStyle/InnerShadowStyle/BackgroundBlurStyle/NoiseStyle/GlassStyle 的公共基类，字段并入各派生类通道表注册。

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| blendMode | BlendMode | Normal | 样式合成混合模式 | String(BlendMode)/NoFlags（各派生类表内注册） |
| excludeChildEffects | bool | false | 计算样式时是否排除子图层效果 | Bool/NoFlags（各派生类表内注册） |

### DropShadowStyle (NodeType::DropShadowStyle=23)
图层后方投影样式。无子节点容器。

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| blendMode（基类） | BlendMode | Normal | 样式混合模式 | String(BlendMode)/NoFlags |
| excludeChildEffects（基类） | bool | false | 排除子图层效果 | Bool/NoFlags |
| offsetX | float | 0 | 阴影水平偏移（px） | Float/Anim |
| offsetY | float | 0 | 阴影垂直偏移（px） | Float/Anim |
| blurX | float | 0 | 阴影水平模糊半径（px） | Float/Anim |
| blurY | float | 0 | 阴影垂直模糊半径（px） | Float/Anim |
| color | Color | 不透明黑 | 阴影颜色 | Color/Anim |
| showBehindLayer | bool | true | 阴影是否显示在图层后方 | Bool/Anim |

### InnerShadowStyle (NodeType::InnerShadowStyle=24)
图层内容内部阴影样式。无子节点容器。

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| blendMode/excludeChildEffects（基类） | — | Normal/false | 同基类 | NoFlags |
| offsetX/offsetY | float | 0 | 阴影偏移（px） | Float/Anim |
| blurX/blurY | float | 0 | 阴影模糊半径（px） | Float/Anim |
| color | Color | 不透明黑 | 阴影颜色 | Color/Anim |

### BackgroundBlurStyle (NodeType::BackgroundBlurStyle=25)
背景（图层后方内容）模糊样式。无子节点容器。

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| blendMode/excludeChildEffects（基类） | — | Normal/false | 同基类 | NoFlags |
| blurX | float | 0 | 水平模糊半径（px） | Float/Anim |
| blurY | float | 0 | 垂直模糊半径（px） | Float/Anim |
| tileMode | TileMode | Mirror | 模糊边缘平铺模式 | String(TileMode)/NoFlags |

### NoiseStyle (NodeType::NoiseStyle=26)
图层内容上方叠加 Perlin 噪声；Mono（单色）/Duo（双色互补）/Multi（保留噪声 RGB 增强对比）三模式，非当前模式的字段被忽略。无子节点容器。

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| blendMode/excludeChildEffects（基类） | — | Normal/false | 同基类 | NoFlags |
| mode | NoiseMode | Mono | 噪声模式 | String(NoiseMode)/NoFlags |
| size | float | 4.0 | 噪声颗粒尺寸，越大越粗，须为正 | Float/Anim |
| density | float | 0.5 | 噪声密度 [0,1]，可见噪声像素占比 | Float/Anim |
| seed | float | 0 | 噪声随机种子 | Float/Anim |
| color | Color | 不透明黑 | Mono 模式噪声色，alpha 控噪声不透明度 | Color/Anim |
| firstColor | Color | 不透明黑 | Duo 模式第一色 | Color/Anim |
| secondColor | Color | 不透明白 | Duo 模式第二色 | Color/Anim |
| opacity | float | 0.15 | Multi 模式整体不透明度 [0,1] | Float/Anim |

注意（源码注释）：颜色/opacity 通道按 mode 绑定运行时 writer（镜像 LayerBuilder::bindNoiseStyleChannels），非当前 mode 的字段无运行时 writer；表本身与 mode 无关。

### GlassStyle (NodeType::GlassStyle=27)
玻璃样式：折射、色散、磨砂模糊、高光；背景隐式取自该图层下方的完整图层树，无背景引用属性。七参数均为可动画 float 通道，字段与 tgfx::GlassStyle 对齐。无子节点容器。

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| blendMode/excludeChildEffects（基类） | — | Normal/false | 同基类 | NoFlags |
| refraction | float | 80 | 沿弯曲边缘的光学扭曲量 [0,100] | Float/Anim |
| depth | float | 20 | 从边缘向内的折射区宽度 [1,100] | Float/Anim |
| frost | float | 5 | 背景模糊量（磨砂）[0,100] | Float/Anim |
| dispersion | float | 50 | 色散强度（彩虹棱镜）[0,100] | Float/Anim |
| splay | float | 0 | 折射方向混合因子 [0,100]；0=沿边缘曲率，100=指向形状中心 | Float/Anim |
| lightAngle | float | 45 | 光源方向角（度）；0=正上，正=顺时针，[-179,180] | Float/Anim |
| lightIntensity | float | 80 | 边缘高光亮度 [0,100] | Float/Anim |

---

### LayerFilter（基类，无独立 NodeType 值）
六个滤镜的公共基类，**无自有字段**。

### BlurFilter (NodeType::BlurFilter=28)
高斯模糊滤镜。无子节点容器。

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| blurX | float | 0 | 水平模糊半径（px） | Float/Anim |
| blurY | float | 0 | 垂直模糊半径（px） | Float/Anim |
| tileMode | TileMode | Decal | 模糊边缘平铺模式 | String(TileMode)/NoFlags |

### DropShadowFilter (NodeType::DropShadowFilter=29)
投影滤镜。无子节点容器。

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| offsetX/offsetY | float | 0 | 阴影偏移（px） | Float/Anim |
| blurX/blurY | float | 0 | 阴影模糊半径（px） | Float/Anim |
| color | Color | 不透明黑 | 阴影颜色 | Color/Anim |
| shadowOnly | bool | false | 是否只渲染阴影不渲染原内容 | Bool/Anim |

### InnerShadowFilter (NodeType::InnerShadowFilter=30)
内阴影滤镜。无子节点容器。字段与 DropShadowFilter 完全相同（offsetX/offsetY/blurX/blurY/color 均 Anim，shadowOnly bool/Anim，默认 false）。

### BlendFilter (NodeType::BlendFilter=31)
用指定混合模式将颜色与图层内容混合的滤镜。无子节点容器。

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| color | Color | 不透明黑 | 参与混合的颜色 | Color/Anim |
| blendMode | BlendMode | Normal | 颜色与图层的混合模式 | String(BlendMode)/NoFlags |

### ColorMatrixFilter (NodeType::ColorMatrixFilter=32)
4x5 颜色矩阵滤镜，[R' G' B' A'] = [R G B A 1] * matrix。无子节点容器。

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| matrix | std::array<float,20> | 单位阵（行主序：每 5 元素一行，末元素为该通道加性偏置；行 0-4=R、5-9=G、10-14=B、15-19=A） | 颜色变换矩阵 | **无通道**（ChannelsFor 的 switch 无此 case，走 default 空表；不可动画、不可 DataBind 反射写入） |

### NoiseFilter (NodeType::NoiseFilter=33)
在图层上叠加 Perlin 噪声的滤镜；模式语义同 NoiseStyle。无子节点容器。

| 字段 | 类型 | 默认值 | 语义 | 通道 |
|---|---|---|---|---|
| mode | NoiseMode | Mono | 噪声模式 | String(NoiseMode)/NoFlags |
| size | float | 4.0 | 颗粒尺寸，须为正 | Float/Anim |
| density | float | 0.5 | 密度 [0,1] | Float/Anim |
| seed | float | 0 | 随机种子 | Float/Anim |
| blendMode | BlendMode | Normal | 噪声与源图像的合成混合模式（滤镜自带，NoiseStyle 无此语义对应——NoiseStyle 的 blendMode 在基类） | String(BlendMode)/NoFlags |
| color | Color | 不透明黑 | Mono 模式噪声色 | Color/Anim |
| firstColor | Color | 不透明黑 | Duo 模式第一色 | Color/Anim |
| secondColor | Color | 不透明白 | Duo 模式第二色 | Color/Anim |
| opacity | float | 0.15 | Multi 模式整体不透明度 [0,1] | Float/Anim |

注意（源码注释）：NoiseFilter 与 NoiseStyle 参数相同但自带 blendMode、无 excludeChildEffects，两表不能共享；颜色/opacity 按 mode 绑定，同 NoiseStyle。

---

## 枚举总表（含数值，均无显式赋值，按声明顺序从 0 递增）

- **ChannelValueType**（KeyValue.h）: Float=0, Bool=1, Int=2, String=3, ImageRef=4, Color=5, Matrix=6, PAGImage=7
- **ChannelFlags**（PAGXChannelTable.h）: None=0, Animatable=1, RequiresLayout=2
- **BlendMode**: Normal=0, Multiply=1, Screen=2, Overlay=3, Darken=4, Lighten=5, ColorDodge=6, ColorBurn=7, HardLight=8, SoftLight=9, Difference=10, Exclusion=11, Hue=12, Saturation=13, Color=14, Luminosity=15, PlusLighter=16, PlusDarker=17
- **MaskType**: Alpha=0, Luminance=1, Contour=2
- **LayerPlacement**（Fill/Stroke 的 placement）: Background=0, Foreground=1
- **FilterMode**（ImagePattern 采样）: Nearest=0, Linear=1
- **NoiseMode**: Mono=0, Duo=1, Multi=2
- **Alignment**: Start=0, Center=1, End=2, Stretch=3
- **Arrangement**: Start=0, Center=1, End=2, SpaceBetween=3, SpaceEvenly=4, SpaceAround=5
- **LayoutMode**: None=0, Horizontal=1, Vertical=2
- **Overflow**（TextBox）: Visible=0, Hidden=1
- **MipmapMode**（ImagePattern）: None=0, Nearest=1, Linear=2
- **TileMode**: Clamp=0, Repeat=1, Mirror=2, Decal=3

## NodeType 数值校验
enum 共 55 个成员（0-54），无显式赋值。本范围相关：Layer=22, DropShadowStyle=23, InnerShadowStyle=24, BackgroundBlurStyle=25, NoiseStyle=26, GlassStyle=27, BlurFilter=28, DropShadowFilter=29, InnerShadowFilter=30, BlendFilter=31, ColorMatrixFilter=32, NoiseFilter=33, Group=47。

## 待确认/注意项
1. Layer 的 anchor/rotation/scale/skew/skewAxis 为规范扩展新增（§6.2），当前 Layer.h 不存在；Group 有同名字段且均已注册 Anim 通道。
2. ColorMatrixFilter 是唯一无通道注册的样式/滤镜节点（matrix 不可动画）。
3. Layer.matrix/matrix3D/scrollRect/hasScrollRect/mask/composition/timelines/vmContext/id 等均无通道：动画与 DataBind 都不可写，二进制中作为静态字段/引用序列化。
4. NoiseStyle/NoiseFilter 的颜色与 opacity 通道标记为 Anim 但按 mode 绑定运行时 writer——二进制侧若允许对这些通道建动画，需沿用同一 mode 相关绑定语义。
5. Padding 四值简写顺序为 top,right,bottom,left；通道按 padding.left/top/right/bottom 四个 Float 拆分注册。
