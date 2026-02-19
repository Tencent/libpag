# PAGX 格式规范

## 1. 介绍（Introduction）

**PAGX**（Portable Animated Graphics XML）是一种基于 XML 的矢量动画标记语言。它提供了统一且强大的矢量图形与动画描述能力，旨在成为跨所有主要工具与运行时的矢量动画交换标准。

### 1.1 设计目标

- **开放可读**：纯文本 XML 格式，易于阅读和编辑，天然支持版本控制与差异对比，便于调试及 AI 理解与生成。

- **特性完备**：完整覆盖矢量图形、图片、富文本、滤镜效果、混合模式、遮罩等能力，满足复杂动效的描述需求。

- **精简高效**：提供简洁且强大的统一结构，兼顾静态矢量与动画的优化描述，同时预留未来交互和脚本的扩展能力。

- **生态兼容**：可作为 After Effects、Figma、腾讯设计等设计工具的通用交换格式，实现设计资产无缝流转。

- **高效部署**：设计资产可一键导出并部署到研发环境，转换为二进制 PAG 格式后获得极高压缩比和运行时性能。

### 1.2 文件结构

PAGX 是纯 XML 文件（`.pagx`），可引用外部资源文件（图片、视频、音频、字体等），也支持通过数据 URI 内嵌资源。PAGX 与二进制 PAG 格式可双向互转：发布时转换为 PAG 以优化加载性能；开发、审查或编辑时可使用 PAGX 格式以便阅读和修改。

### 1.3 文档组织

本规范按以下顺序组织：

1. **基础数据类型**：定义文档中使用的基本数据格式
2. **文档结构**：描述 PAGX 文档的整体组织方式
3. **图层系统**：定义图层及其相关特性（样式、滤镜、遮罩）
4. **矢量元素系统**：定义图层内容的矢量元素及其处理模型

**附录**（方便速查）：

- **附录 A**：节点层级与包含关系
- **附录 B**：枚举类型速查
- **附录 C**：常见用法示例

---

## 2. 基础数据类型（Basic Data Types）

本节定义 PAGX 文档中使用的基础数据类型和命名规范。

### 2.1 命名规范

| 类别 | 规范 | 示例 |
|------|------|------|
| 元素名 | PascalCase，不缩写 | `Group`、`Rectangle`、`Fill` |
| 属性名 | camelCase，尽量简短 | `antiAlias`、`blendMode`、`fontSize` |
| 默认单位 | 像素（无需标注） | `width="100"` |
| 角度单位 | 度 | `rotation="45"` |

### 2.2 属性表格约定

本规范中的属性表格统一使用"默认值"列描述属性的必填性：

| 默认值格式 | 含义 |
|------------|------|
| `(必填)` | 属性必须指定，没有默认值 |
| 具体值（如 `0`、`true`、`normal`） | 属性可选，未指定时使用该默认值 |
| `-` | 属性可选，未指定时不生效 |

### 2.3 通用属性

以下属性可用于任意元素，不在各节点的属性表中重复列出：

| 属性 | 类型 | 说明 |
|------|------|------|
| `id` | string | 唯一标识符，用于被其他元素引用（如遮罩、颜色源）。在文档中必须唯一，不能为空或包含空白字符 |
| `data-*` | string | 自定义数据属性，用于存储应用特定的私有数据。`*` 可替换为任意名称（如 `data-name`、`data-layer-id`），运行时忽略这些属性 |

**自定义属性说明**：

- 属性名必须以 `data-` 开头，后跟至少一个字符
- 属性名只能包含小写字母、数字和连字符（`-`），不能以连字符结尾
- 属性值为任意字符串，由创建该属性的应用自行解释
- 运行时不处理这些属性，仅用于工具间传递元数据或存储调试信息

**示例**：

```xml
<Layer data-name="背景图层" data-figma-id="12:34" data-exported-by="PAGExporter">
  <Rectangle center="50,50" size="100,100"/>
  <Fill color="#FF0000"/>
</Layer>
```

### 2.4 基本数值类型

| 类型 | 说明 | 示例 |
|------|------|------|
| `float` | 浮点数 | `1.5`、`-0.5`、`100` |
| `int` | 整数 | `400`、`0`、`-1` |
| `bool` | 布尔值 | `true`、`false` |
| `string` | 字符串 | `"Arial"`、`"myLayer"` |
| `enum` | 枚举值 | `normal`、`multiply` |
| `idref` | ID 引用 | `@gradientId`、`@maskLayer` |

### 2.5 点（Point）

点使用逗号分隔的两个浮点数表示：

```
"x,y"
```

**示例**：`"100,200"`、`"0.5,0.5"`、`"-50,100"`

### 2.6 矩形（Rect）

矩形使用逗号分隔的四个浮点数表示：

```
"x,y,width,height"
```

**示例**：`"0,0,100,100"`、`"10,20,200,150"`

### 2.7 变换矩阵（Matrix）

#### 2D 变换矩阵

2D 变换使用 6 个逗号分隔的浮点数表示，对应标准 2D 仿射变换矩阵：

```
"a,b,c,d,tx,ty"
```

矩阵形式：
```
| a  c  tx |
| b  d  ty |
| 0  0  1  |
```

**单位矩阵**：`"1,0,0,1,0,0"`

#### 3D 变换矩阵

3D 变换使用 16 个逗号分隔的浮点数表示，列优先顺序：

```
"m00,m10,m20,m30,m01,m11,m21,m31,m02,m12,m22,m32,m03,m13,m23,m33"
```

### 2.8 颜色（Color）

PAGX 支持两种颜色表示方式：

#### HEX 格式（十六进制）

HEX 格式用于表示 sRGB 色域的颜色，使用 `#` 前缀的十六进制值：

| 格式 | 示例 | 说明 |
|------|------|------|
| `#RGB` | `#F00` | 3 位简写，每位扩展为两位（等价于 `#FF0000`） |
| `#RRGGBB` | `#FF0000` | 6 位标准格式，不透明 |
| `#RRGGBBAA` | `#FF000080` | 8 位带 Alpha，Alpha 在末尾（与 CSS 一致） |

#### 浮点数格式

浮点数格式使用 `色域(r, g, b)` 或 `色域(r, g, b, a)` 的形式表示颜色，支持 sRGB 和 Display P3 两种色域：

| 色域 | 格式 | 示例 | 说明 |
|------|------|------|------|
| sRGB | `srgb(r, g, b)` | `srgb(1.0, 0.5, 0.2)` | sRGB 色域，各分量 0.0~1.0 |
| sRGB | `srgb(r, g, b, a)` | `srgb(1.0, 0.5, 0.2, 0.8)` | 带透明度 |
| Display P3 | `p3(r, g, b)` | `p3(1.0, 0.5, 0.2)` | Display P3 广色域 |
| Display P3 | `p3(r, g, b, a)` | `p3(1.0, 0.5, 0.2, 0.8)` | 带透明度 |

**注意**：
- 色域标识符（`srgb` 或 `p3`）和括号**不能省略**
- 广色域颜色（Display P3）的分量值可以超出 [0, 1] 范围，以表示超出 sRGB 色域的颜色
- sRGB 浮点格式与 HEX 格式表示相同的色域，可根据需要选择

#### 颜色源引用

使用 `@resourceId` 引用 Resources 中定义的颜色源（渐变、图案等）。

### 2.9 混合模式（Blend Mode）

混合模式定义源颜色（S）如何与目标颜色（D）组合。

| 值 | 公式 | 说明 |
|------|------|------|
| `normal` | S | 正常（覆盖） |
| `multiply` | S × D | 正片叠底 |
| `screen` | 1 - (1-S)(1-D) | 滤色 |
| `overlay` | multiply/screen 组合 | 叠加 |
| `darken` | min(S, D) | 变暗 |
| `lighten` | max(S, D) | 变亮 |
| `colorDodge` | D / (1-S) | 颜色减淡 |
| `colorBurn` | 1 - (1-D)/S | 颜色加深 |
| `hardLight` | multiply/screen 反向组合 | 强光 |
| `softLight` | 柔和版叠加 | 柔光 |
| `difference` | \|S - D\| | 差值 |
| `exclusion` | S + D - 2SD | 排除 |
| `hue` | D 的饱和度和亮度 + S 的色相 | 色相 |
| `saturation` | D 的色相和亮度 + S 的饱和度 | 饱和度 |
| `color` | D 的亮度 + S 的色相和饱和度 | 颜色 |
| `luminosity` | S 的亮度 + D 的色相和饱和度 | 亮度 |
| `plusLighter` | S + D | 相加（趋向白色） |
| `plusDarker` | S + D - 1 | 相加减一（趋向黑色） |

### 2.10 路径数据语法（Path Data Syntax）

路径数据使用 SVG 路径语法，由一系列命令和坐标组成。

**路径命令**：

| 命令 | 参数 | 说明 |
|------|------|------|
| M/m | x y | 移动到（绝对/相对） |
| L/l | x y | 直线到 |
| H/h | x | 水平线到 |
| V/v | y | 垂直线到 |
| C/c | x1 y1 x2 y2 x y | 三次贝塞尔曲线 |
| S/s | x2 y2 x y | 平滑三次贝塞尔 |
| Q/q | x1 y1 x y | 二次贝塞尔曲线 |
| T/t | x y | 平滑二次贝塞尔 |
| A/a | rx ry rotation large-arc sweep x y | 椭圆弧 |
| Z/z | - | 闭合路径 |

### 2.11 外部资源引用（External Resource Reference）

外部资源通过相对路径或数据 URI 引用，适用于图片、视频、音频、字体等文件。

```xml
<!-- 相对路径引用 -->
<Image source="photo.png"/>
<Image source="assets/icons/logo.png"/>

<!-- 数据 URI 内嵌 -->
<Image source="data:image/png;base64,iVBORw0KGgo..."/>
```

**路径解析规则**：

- **相对路径**：相对于 PAGX 文件所在目录解析，支持 `../` 引用父目录
- **数据 URI**：以 `data:` 开头，格式为 `data:<mediatype>;base64,<data>`，仅支持 base64 编码
- 路径分隔符统一使用 `/`（正斜杠），不支持 `\`（反斜杠）

---

## 3. 文档结构（Document Structure）

本节定义 PAGX 文档的整体结构。

### 3.1 坐标系统

PAGX 使用标准的 2D 笛卡尔坐标系：

- **原点**：位于画布左上角
- **X 轴**：向右为正方向
- **Y 轴**：向下为正方向
- **角度**：顺时针方向为正（0° 指向 X 轴正方向）
- **单位**：所有长度值默认为像素，角度值默认为度

### 3.2 根元素（pagx）

`<pagx>` 是 PAGX 文档的根元素，定义画布尺寸并直接包含图层列表。

> [Sample](samples/3.2_document_structure.pagx)

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `version` | string | (必填) | 格式版本 |
| `width` | float | (必填) | 画布宽度 |
| `height` | float | (必填) | 画布高度 |

**图层渲染顺序**：图层按文档顺序依次渲染，文档中靠前的图层先渲染（位于下方），靠后的图层后渲染（位于上方）。

### 3.3 资源区（Resources）

`<Resources>` 定义可复用的资源，包括图片、路径数据、颜色源和合成。资源通过 `id` 属性标识，在文档其他位置通过 `@id` 形式引用。

**元素位置**：Resources 元素可放置在根元素内的任意位置，对位置没有限制。解析器必须支持元素引用在文档后面定义的资源或图层（即前向引用）。

> [Sample](samples/3.3_resources.pagx)

#### 3.3.1 图片（Image）

图片资源定义可在文档中引用的位图数据。

```xml
<Image source="photo.png"/>
<Image source="data:image/png;base64,..."/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `source` | string | (必填) | 文件路径或数据 URI |

**支持格式**：PNG、JPEG、WebP、GIF

#### 3.3.2 路径数据（PathData）

PathData 定义可复用的路径数据，供 Path 元素和 TextPath 修改器引用。

```xml
<PathData id="curvePath" data="M 0 0 C 50 0 50 100 100 100"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `data` | string | (必填) | SVG 路径数据 |

#### 3.3.3 颜色源（Color Source）

颜色源定义可用于填充和描边的颜色，支持两种使用方式：

1. **共享定义**：在 `<Resources>` 中预定义，通过 `@id` 引用。适用于**被多处引用**的颜色源。
2. **内联定义**：直接嵌套在 `<Fill>` 或 `<Stroke>` 元素内部。适用于**仅使用一次**的颜色源，更简洁。

##### 纯色（SolidColor）

```xml
<SolidColor color="#FF0000"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `color` | Color | (必填) | 颜色值 |

##### 线性渐变（LinearGradient）

线性渐变沿起点到终点的方向插值。

> [Sample](samples/3.3.3_linear_gradient.pagx)

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `startPoint` | Point | (必填) | 起点 |
| `endPoint` | Point | (必填) | 终点 |
| `matrix` | Matrix | 单位矩阵 | 变换矩阵 |

**计算**：对于点 P，其颜色由 P 在起点-终点连线上的投影位置决定。

##### 径向渐变（RadialGradient）

径向渐变从中心向外辐射。

> [Sample](samples/3.3.3_radial_gradient.pagx)

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `center` | Point | 0,0 | 中心点 |
| `radius` | float | (必填) | 渐变半径 |
| `matrix` | Matrix | 单位矩阵 | 变换矩阵 |

**计算**：对于点 P，其颜色由 `distance(P, center) / radius` 决定。

##### 锥形渐变（ConicGradient）

锥形渐变（也称扫描渐变）沿圆周方向插值。

> [Sample](samples/3.3.3_conic_gradient.pagx)

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `center` | Point | 0,0 | 中心点 |
| `startAngle` | float | 0 | 起始角度 |
| `endAngle` | float | 360 | 结束角度 |
| `matrix` | Matrix | 单位矩阵 | 变换矩阵 |

**计算**：对于点 P，其颜色由 `atan2(P.y - center.y, P.x - center.x)` 在 `[startAngle, endAngle]` 范围内的比例决定。

**角度约定**：遵循全局坐标系约定（见 §3.1）：0° 指向**右侧**（X 轴正方向），角度沿**顺时针**递增。这与 CSS `conic-gradient`（0° 指向顶部）不同。常用参考角度：

| 角度 | 方向 |
|------|------|
| 0°   | 右 (3 点钟) |
| 90°  | 下 (6 点钟) |
| 180° | 左 (9 点钟) |
| 270° | 上 (12 点钟) |

##### 菱形渐变（DiamondGradient）

菱形渐变从中心向四角辐射。

> [Sample](samples/3.3.3_diamond_gradient.pagx)

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `center` | Point | 0,0 | 中心点 |
| `radius` | float | (必填) | 渐变半径 |
| `matrix` | Matrix | 单位矩阵 | 变换矩阵 |

**计算**：对于点 P，其颜色由切比雪夫距离 `max(|P.x - center.x|, |P.y - center.y|) / radius` 决定。

##### 渐变色标（ColorStop）

```xml
<ColorStop offset="0.5" color="#FF0000"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `offset` | float | (必填) | 位置 0.0~1.0 |
| `color` | Color | (必填) | 色标颜色 |

**渐变通用规则**：

- **色标插值**：相邻色标之间使用线性插值
- **色标边界**：
  - `offset < 0` 的色标被视为 `offset = 0`
  - `offset > 1` 的色标被视为 `offset = 1`
  - 如果没有 `offset = 0` 的色标，使用第一个色标的颜色填充
  - 如果没有 `offset = 1` 的色标，使用最后一个色标的颜色填充

##### 图片填充（ImagePattern）

图片图案使用图片作为颜色源。

```xml
<ImagePattern image="@img1" tileModeX="repeat" tileModeY="repeat"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `image` | idref | (必填) | 图片引用 "@id" |
| `tileModeX` | TileMode | clamp | X 方向平铺模式 |
| `tileModeY` | TileMode | clamp | Y 方向平铺模式 |
| `filterMode` | FilterMode | linear | 纹理过滤模式 |
| `mipmapMode` | MipmapMode | linear | Mipmap 模式 |
| `matrix` | Matrix | 单位矩阵 | 变换矩阵 |

**TileMode（平铺模式）**：`clamp`（钳制）、`repeat`（重复）、`mirror`（镜像）、`decal`（贴花）

**FilterMode（纹理过滤模式）**：`nearest`（最近邻）、`linear`（双线性插值）

**MipmapMode（Mipmap 模式）**：`none`（禁用）、`nearest`（最近级别）、`linear`（线性插值）

**完整示例**：演示不同平铺模式的图片填充

> [Sample](samples/3.3.3_image_pattern.pagx)

##### 颜色源坐标系统

除纯色外，所有颜色源（渐变、图片填充）都有坐标系的概念，其坐标系**相对于几何元素的局部坐标系原点**。可通过 `matrix` 属性对颜色源坐标系应用变换。

**变换行为**：

1. **外部变换会同时作用于几何和颜色源**：Group 的变换、Layer 的矩阵等外部变换会整体作用于几何元素及其颜色源，两者一起缩放、旋转、平移。

2. **修改几何属性不影响颜色源**：直接修改几何元素的属性（如 Rectangle 的 width/height、Path 的路径数据）只改变几何内容本身，不会影响颜色源的坐标系。

**示例**：在 300×300 的区域内绘制一个对角线方向的线性渐变：

> [Sample](samples/3.3.3_color_source_coordinates.pagx)

- 对该图层应用 `scale(2, 2)` 变换：矩形变为 600×600，渐变也随之放大，视觉效果保持一致
- 直接将 Rectangle 的 size 改为 600,600：矩形变为 600×600，但渐变坐标不变，只覆盖矩形的左上四分之一

#### 3.3.4 合成（Composition）

合成用于内容复用（类似 After Effects 的 Pre-comp）。

> [Sample](samples/3.3.4_composition.pagx)

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `width` | float | (必填) | 合成宽度 |
| `height` | float | (必填) | 合成高度 |

#### 3.3.5 字体（Font）

Font 定义嵌入字体资源，包含子集化的字形数据（矢量轮廓或位图）。PAGX 文件通过嵌入字形数据实现完全自包含，确保跨平台渲染一致性。

```xml
<!-- 嵌入矢量字体 -->
<Font id="myFont" unitsPerEm="1000">
  <Glyph advance="600" path="M 50 0 L 300 700 L 550 0 Z"/>
  <Glyph advance="550" path="M 100 0 L 100 700 L 400 700 C 550 700 550 400 400 400 Z"/>
</Font>

<!-- 嵌入位图字体（Emoji） -->
<Font id="emojiFont" unitsPerEm="136">
  <Glyph advance="136" image="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUA..."/>
  <Glyph advance="136" image="emoji/heart.png" offset="0,-5"/>
</Font>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `unitsPerEm` | int | 1000 | 字体设计空间单位。渲染时按 `fontSize / unitsPerEm` 缩放 |

**一致性约束**：同一 Font 内的所有 Glyph 必须使用相同类型（全部 `path` 或全部 `image`），不允许混用。

**GlyphID 规则**：
- **GlyphID 从 1 开始**：Glyph 列表的索引 + 1 = GlyphID
- **GlyphID 0 保留**：表示缺失字形，不渲染

##### 字形（Glyph）

Glyph 定义单个字形的渲染数据。`path` 和 `image` 二选一必填，不能同时指定。

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `advance` | float | (必填) | 水平步进宽度，设计空间坐标（unitsPerEm 单位） |
| `path` | string | - | SVG 路径数据（矢量轮廓） |
| `image` | string | - | 图片数据（base64 数据 URI）或外部文件路径 |
| `offset` | Point | 0,0 | 字形偏移量，设计空间坐标（通常用于位图字形） |

**字形类型**：
- **矢量字形**：指定 `path` 属性，使用 SVG 路径语法描述轮廓
- **位图字形**：指定 `image` 属性，用于 Emoji 等彩色字形，可通过 `offset` 调整位置

**坐标系说明**：字形路径、偏移和步进均使用设计空间坐标。渲染时根据 GlyphRun 的 `fontSize` 和 Font 的 `unitsPerEm` 计算缩放比例：`scale = fontSize / unitsPerEm`。

### 3.4 文档层级结构

PAGX 文档采用层级结构组织内容：

```
<pagx>                          ← 根元素（定义画布尺寸）
├── <Layer>                     ← 图层（可多个）
│   ├── 几何元素                ← Rectangle、Ellipse、Path、Text 等
│   ├── 修改器                  ← TrimPath、RoundCorner、TextModifier 等
│   ├── 绘制器                  ← Fill、Stroke
│   ├── <Group>                 ← 矢量元素容器（可嵌套）
│   ├── LayerStyle              ← DropShadowStyle、InnerShadowStyle 等
│   ├── LayerFilter             ← BlurFilter、ColorMatrixFilter 等
│   └── <Layer>                 ← 子图层（递归结构）
│       └── ...
│
└── <Resources>                 ← 资源区（可选，定义可复用资源）
    ├── <Image>                 ← 图片资源
    ├── <PathData>              ← 路径数据资源
    ├── <SolidColor>            ← 纯色定义
    ├── <LinearGradient>        ← 渐变定义
    ├── <ImagePattern>          ← 图片图案定义
    ├── <Font>                  ← 字体资源（嵌入字体）
    │   └── <Glyph>             ← 字形定义
    └── <Composition>           ← 合成定义
        └── <Layer>             ← 合成内的图层
```

---

## 4. 图层系统（Layer System）

图层（Layer）是 PAGX 内容组织的基本单元，提供了丰富的视觉效果控制能力。

### 4.1 核心概念

本节介绍图层系统的核心概念，这些概念是理解图层样式、滤镜和遮罩等功能的基础。

#### 图层渲染流程

图层绑定的绘制器（Fill、Stroke 等）通过 `placement` 属性分为下层内容和上层内容，默认为下层内容。单个图层渲染时按以下顺序处理：

1. **图层样式（下方）**：渲染位于内容下方的图层样式（如投影阴影）
2. **下层内容**：渲染 `placement="background"` 的 Fill 和 Stroke
3. **子图层**：按文档顺序递归渲染所有子图层
4. **图层样式（上方）**：渲染位于内容上方的图层样式（如内阴影）
5. **上层内容**：渲染 `placement="foreground"` 的 Fill 和 Stroke（绘制在子图层之上）
6. **图层滤镜**：将前面步骤的整体输出作为滤镜链的输入，依次应用所有滤镜

#### 图层内容（Layer Content）

**图层内容**是指图层的下层内容、子图层和上层内容的完整渲染结果（渲染流程中的步骤 2 到 5），不包含图层样式和图层滤镜。

图层样式基于图层内容计算效果。例如，当填充位于下层内容、描边位于上层内容时，描边会绘制在子图层之上，但投影阴影仍然基于包含填充、子图层和描边的完整图层内容计算。

#### 图层轮廓（Layer Contour）

**图层轮廓**是基于图层内容生成的一个二值（不透明或完全透明）遮罩。与普通图层内容相比，图层轮廓有以下区别：

1. **包含 alpha=0 的填充**：填充透明度完全为 0 的几何形状也会加入轮廓计算
2. **纯色填充和渐变填充**：原始颜色替换为不透明白色
3. **图片填充**：完全透明的像素保留透明；其余像素均转为完全不透明

注意：没有绘制器的几何元素（独立的 Rectangle、Ellipse 等）不参与轮廓计算。

#### 图层背景（Layer Background）

**图层背景**是指当前图层下方所有已渲染内容的合成结果，包括：
- 当前图层下方的所有兄弟图层及其子树的渲染结果
- 当前图层已绘制的下方图层样式（不包括 BackgroundBlurStyle 本身）

图层背景主要用于：

- **图层样式**：部分图层样式需要背景作为其中一个输入源
- **混合模式**：部分混合模式需要背景信息才能正确渲染

**背景透传**：通过 `passThroughBackground` 属性控制是否允许图层背景透传给子图层。当设置为 `false` 时，子图层的背景依赖样式将无法获取到正确的图层背景。以下情况会自动禁用背景透传：
- 图层使用了非 `normal` 的混合模式
- 图层应用了滤镜
- 图层使用了 3D 变换或投影变换

### 4.2 图层（Layer）

`<Layer>` 是内容和子图层的基本容器。

> [Sample](samples/4.2_layer.pagx)

#### 子元素

Layer 的子元素按类型自动归类为四个集合：

| 子元素类型 | 归类 | 说明 |
|-----------|------|------|
| VectorElement | contents | 几何元素、修改器、绘制器（参与累积处理） |
| LayerStyle | styles | DropShadowStyle、InnerShadowStyle、BackgroundBlurStyle |
| LayerFilter | filters | BlurFilter、DropShadowFilter 等滤镜 |
| Layer | children | 嵌套子图层 |

**建议顺序**：虽然子元素顺序不影响解析结果，但建议按 VectorElement → LayerStyle → LayerFilter → 子Layer 的顺序书写，以提高可读性。

#### 图层属性

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `name` | string | "" | 显示名称 |
| `visible` | bool | true | 是否可见 |
| `alpha` | float | 1 | 透明度 0~1 |
| `blendMode` | BlendMode | normal | 混合模式 |
| `x` | float | 0 | X 位置 |
| `y` | float | 0 | Y 位置 |
| `matrix` | Matrix | 单位矩阵 | 2D 变换 "a,b,c,d,tx,ty" |
| `matrix3D` | Matrix | - | 3D 变换（16 个值，列优先） |
| `preserve3D` | bool | false | 保持 3D 变换 |
| `antiAlias` | bool | true | 边缘抗锯齿 |
| `groupOpacity` | bool | false | 组透明度 |
| `passThroughBackground` | bool | true | 是否允许背景透传给子图层 |
| `excludeChildEffectsInLayerStyle` | bool | false | 图层样式是否排除子图层效果 |
| `scrollRect` | Rect | - | 滚动裁剪区域 "x,y,w,h" |
| `mask` | idref | - | 遮罩图层引用 "@id" |
| `maskType` | MaskType | alpha | 遮罩类型 |
| `composition` | idref | - | 合成引用 "@id" |

**groupOpacity**：当值为 `false`（默认）时，图层的 `alpha` 独立应用到每个子元素，重叠的半透明子元素在交叉处可能显得更深。当值为 `true` 时，所有图层内容先合成到离屏缓冲区，再将 `alpha` 整体应用到缓冲区，使整个图层呈现均匀的透明效果。

**preserve3D**：当值为 `false`（默认）时，具有 3D 变换的子图层在合成前会被压平到父级的 2D 平面。当值为 `true` 时，子图层保留其 3D 位置，在共享的 3D 空间中渲染，实现基于深度的交叉和正确的兄弟层 Z 排序。类似于 CSS 的 `transform-style: preserve-3d`。

**excludeChildEffectsInLayerStyle**：当值为 `false`（默认）时，图层样式（如 DropShadowStyle）基于完整的图层内容计算，包含子图层的渲染结果。当值为 `true` 时，子图层的图层样式和图层滤镜不参与父级图层样式的计算，但子图层的基础渲染结果仍然包含在内。

**变换属性优先级**：`x`/`y`、`matrix`、`matrix3D` 三者存在覆盖关系：
- 仅设置 `x`/`y`：使用 `x`/`y` 作为平移
- 设置 `matrix`：`matrix` 覆盖 `x`/`y` 的值
- 设置 `matrix3D`：`matrix3D` 覆盖 `matrix` 和 `x`/`y` 的值

**MaskType（遮罩类型）**：

| 值 | 说明 |
|------|------|
| `alpha` | Alpha 遮罩：使用遮罩的 alpha 通道 |
| `luminance` | 亮度遮罩：使用遮罩的亮度值 |
| `contour` | 轮廓遮罩：使用遮罩的轮廓进行裁剪 |

**BlendMode**：见 2.9 节混合模式完整表格。

### 4.3 图层样式（Layer Styles）

图层样式在图层内容的上方或下方添加视觉效果，不会替换原有内容。

**图层样式的输入源**：

所有图层样式都基于**图层内容**计算效果。计算时，图层内容会被转换为对应的不透明版本：使用正常的填充方式渲染几何形状，然后将所有半透明像素转换为完全不透明（完全透明的像素保留）。这意味着半透明填充产生的阴影效果与完全不透明填充相同。

部分图层样式还会额外使用**图层轮廓**或**图层背景**作为输入（详见各样式说明）。图层轮廓和图层背景的定义参见 4.1 节。

> [Sample](samples/4.3_layer_styles.pagx)

**所有 LayerStyle 共有属性**：

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `blendMode` | BlendMode | normal | 混合模式（见 2.9 节） |

#### 4.3.1 投影阴影（DropShadowStyle）

在图层**下方**绘制投影阴影。基于不透明图层内容计算阴影形状。

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `offsetX` | float | 0 | X 偏移 |
| `offsetY` | float | 0 | Y 偏移 |
| `blurX` | float | 0 | X 模糊半径 |
| `blurY` | float | 0 | Y 模糊半径 |
| `color` | Color | #000000 | 阴影颜色 |
| `showBehindLayer` | bool | true | 图层后面是否显示阴影 |

**渲染步骤**：
1. 获取不透明图层内容并偏移 `(offsetX, offsetY)`
2. 对偏移后的内容应用高斯模糊 `(blurX, blurY)`
3. 使用 `color` 的颜色填充阴影区域
4. 如果 `showBehindLayer="false"`，使用图层轮廓作为擦除遮罩挖空被遮挡部分

**showBehindLayer**：
- `true`：阴影完整显示，包括被图层内容遮挡的部分
- `false`：阴影被图层内容遮挡的部分会被挖空（使用图层轮廓作为擦除遮罩）

#### 4.3.2 背景模糊（BackgroundBlurStyle）

在图层**下方**对图层背景应用模糊效果。基于**图层背景**计算效果，使用不透明图层内容作为遮罩裁剪。

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `blurX` | float | 0 | X 模糊半径 |
| `blurY` | float | 0 | Y 模糊半径 |
| `tileMode` | TileMode | mirror | 平铺模式 |

**渲染步骤**：
1. 获取图层边界下方的图层背景
2. 对图层背景应用高斯模糊 `(blurX, blurY)`
3. 使用不透明图层内容作为遮罩裁剪模糊结果

#### 4.3.3 内阴影（InnerShadowStyle）

在图层**上方**绘制内阴影，效果呈现在图层内容之内。基于不透明图层内容计算阴影范围。

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `offsetX` | float | 0 | X 偏移 |
| `offsetY` | float | 0 | Y 偏移 |
| `blurX` | float | 0 | X 模糊半径 |
| `blurY` | float | 0 | Y 模糊半径 |
| `color` | Color | #000000 | 阴影颜色 |

**渲染步骤**：
1. 获取不透明图层内容并偏移 `(offsetX, offsetY)`
2. 对偏移后内容的反向（内容外部区域）应用高斯模糊 `(blurX, blurY)`
3. 使用 `color` 的颜色填充阴影区域
4. 与不透明图层内容求交集，仅保留内容内部的阴影

### 4.4 图层滤镜（Layer Filters）

图层滤镜是图层渲染的最后一个环节，所有之前按顺序渲染的结果（包含图层样式）累积起来作为滤镜的输入。滤镜按文档顺序链式应用，每个滤镜的输出作为下一个滤镜的输入。

与图层样式（4.3 节）的关键区别：图层样式在图层内容的上方或下方**独立渲染**视觉效果，而滤镜**修改**图层的整体渲染输出。图层样式先于滤镜应用。

> [Sample](samples/4.4_layer_filters.pagx)

#### 4.4.1 模糊滤镜（BlurFilter）

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `blurX` | float | (必填) | X 模糊半径 |
| `blurY` | float | (必填) | Y 模糊半径 |
| `tileMode` | TileMode | decal | 平铺模式 |

#### 4.4.2 投影阴影滤镜（DropShadowFilter）

基于滤镜输入生成阴影效果。与 DropShadowStyle 的核心区别：滤镜基于原始渲染内容投影，支持半透明度；而样式基于不透明图层内容投影。

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `offsetX` | float | 0 | X 偏移 |
| `offsetY` | float | 0 | Y 偏移 |
| `blurX` | float | 0 | X 模糊半径 |
| `blurY` | float | 0 | Y 模糊半径 |
| `color` | Color | #000000 | 阴影颜色 |
| `shadowOnly` | bool | false | 仅显示阴影 |

**渲染步骤**：
1. 将滤镜输入偏移 `(offsetX, offsetY)`
2. 提取 alpha 通道并应用高斯模糊 `(blurX, blurY)`
3. 使用 `color` 的颜色填充阴影区域
4. 将阴影与滤镜输入合成（`shadowOnly=false`）或仅输出阴影（`shadowOnly=true`）

#### 4.4.3 内阴影滤镜（InnerShadowFilter）

在滤镜输入的内部绘制阴影。

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `offsetX` | float | 0 | X 偏移 |
| `offsetY` | float | 0 | Y 偏移 |
| `blurX` | float | 0 | X 模糊半径 |
| `blurY` | float | 0 | Y 模糊半径 |
| `color` | Color | #000000 | 阴影颜色 |
| `shadowOnly` | bool | false | 仅显示阴影 |

**渲染步骤**：
1. 创建滤镜输入 alpha 通道的反向遮罩
2. 偏移并应用高斯模糊
3. 与滤镜输入的 alpha 通道求交集
4. 将结果与滤镜输入合成（`shadowOnly=false`）或仅输出阴影（`shadowOnly=true`）

#### 4.4.4 混合滤镜（BlendFilter）

将指定颜色以指定混合模式叠加到图层上。

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `color` | Color | (必填) | 混合颜色 |
| `blendMode` | BlendMode | normal | 混合模式（见 2.9 节） |

#### 4.4.5 颜色矩阵滤镜（ColorMatrixFilter）

使用 4×5 颜色矩阵变换颜色。

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `matrix` | Matrix | (必填) | 4x5 颜色矩阵（20 个逗号分隔的浮点数） |

**矩阵格式**（20 个值，行优先）：
```
| R' |   | m[0]  m[1]  m[2]  m[3]  m[4]  |   | R |
| G' |   | m[5]  m[6]  m[7]  m[8]  m[9]  |   | G |
| B' | = | m[10] m[11] m[12] m[13] m[14] | × | B |
| A' |   | m[15] m[16] m[17] m[18] m[19] |   | A |
                                            | 1 |
```

### 4.5 裁剪与遮罩（Clipping and Masking）

#### 4.5.1 scrollRect（滚动裁剪）

`scrollRect` 属性定义图层的可视区域，超出该区域的内容会被裁剪。

> [Sample](samples/4.5.1_scroll_rect.pagx)

#### 4.5.2 遮罩（Masking）

通过 `mask` 属性引用另一个图层作为遮罩。

> [Sample](samples/4.5.2_masking.pagx)

**遮罩规则**：
- 遮罩图层自身不渲染（`visible` 属性被忽略）
- 遮罩图层的变换不影响被遮罩图层

---

## 5. 矢量元素系统（VectorElement System）

矢量元素系统定义了 Layer 内的矢量内容如何被处理和渲染。

### 5.1 处理模型（Processing Model）

VectorElement 系统采用**累积-渲染**的处理模型：几何元素在渲染上下文中累积，修改器对累积的几何进行变换，绘制器触发最终渲染。

#### 5.1.1 术语定义

| 术语 | 包含元素 | 说明 |
|------|----------|------|
| **几何元素** | Rectangle、Ellipse、Polystar、Path、Text | 提供几何形状的元素，在上下文中累积为几何列表 |
| **修改器** | TrimPath、RoundCorner、MergePath、TextModifier、TextPath、TextBox、Repeater | 对累积的几何进行变换 |
| **绘制器** | Fill、Stroke | 对累积的几何进行填充或描边渲染 |
| **容器** | Group | 创建独立作用域并应用矩阵变换，处理完成后合并 |

#### 5.1.2 几何元素的内部结构

几何元素在上下文中累积时，内部结构有所不同：

| 元素类型 | 内部结构 | 说明 |
|----------|----------|------|
| 形状元素（Rectangle、Ellipse、Polystar、Path） | 单个 Path | 每个形状元素产生一个路径 |
| 文本元素（Text） | 字形列表 | 一个 Text 经过塑形后产生多个字形 |

#### 5.1.3 处理与渲染顺序

VectorElement 按**文档顺序**依次处理，文档中靠前的元素先处理。默认情况下，先处理的绘制器先渲染（位于下方）。

由于 Fill 和 Stroke 可通过 `placement` 属性指定渲染到图层的背景或前景，因此最终渲染顺序可能与文档顺序不完全一致。但在默认情况下（所有内容均为背景），渲染顺序与文档顺序一致。

#### 5.1.4 统一处理流程

```
几何元素                修改器                  绘制器
┌──────────┐          ┌──────────┐          ┌──────────┐
│Rectangle │          │ TrimPath │          │   Fill   │
│ Ellipse  │          │RoundCorn │          │  Stroke  │
│ Polystar │          │MergePath │          └────┬─────┘
│   Path   │          │TextModif │               │
│   Text   │          │ TextPath │               │
└────┬─────┘          │TextBox   │               │
     │                │ Repeater │               │
     │                └────┬─────┘               │
     │                     │                     │
     │ 累积几何            │ 变换几何             │ 渲染
     ▼                     ▼                     ▼
┌─────────────────────────────────────────────────────────┐
│ 几何列表 [Path1, Path2, 字形列表1, 字形列表2...]          │
└─────────────────────────────────────────────────────────┘
```

**渲染上下文**累积的是一个几何列表，其中：
- 每个形状元素贡献一个 Path
- 每个 Text 贡献一个字形列表（包含多个字形）

#### 5.1.5 修改器的作用范围

不同修改器对几何列表中的元素有不同的作用范围：

| 修改器类型 | 作用对象 | 说明 |
|------------|----------|------|
| 形状修改器（TrimPath、RoundCorner、MergePath） | 仅 Path | 对文本触发强制转换 |
| 文本修改器（TextModifier、TextPath、TextBox） | 仅字形列表 | 对 Path 无效 |
| 复制器（Repeater） | Path + 字形列表 | 同时作用于所有几何 |

### 5.2 几何元素（Geometry Elements）

几何元素提供可渲染的形状。

#### 5.2.1 矩形（Rectangle）

矩形从中心点定义，支持统一圆角。

```xml
<Rectangle center="100,100" size="200,150" roundness="10" reversed="false"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `center` | Point | 0,0 | 中心点 |
| `size` | Size | 100,100 | 尺寸 "width,height" |
| `roundness` | float | 0 | 圆角半径 |
| `reversed` | bool | false | 反转路径方向 |

**计算规则**：
```
rect.left   = center.x - size.width / 2
rect.top    = center.y - size.height / 2
rect.right  = center.x + size.width / 2
rect.bottom = center.y + size.height / 2
```

**圆角处理**：
- `roundness` 值自动限制为 `min(roundness, size.width/2, size.height/2)`
- 当 `roundness >= min(size.width, size.height) / 2` 时，短边方向呈半圆形

**示例**:

> [Sample](samples/5.2.1_rectangle.pagx)

**路径起点**：矩形路径从**右上角**开始，顺时针方向绘制（`reversed="false"` 时）。

#### 5.2.2 椭圆（Ellipse）

椭圆从中心点定义。

```xml
<Ellipse center="100,100" size="100,60" reversed="false"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `center` | Point | 0,0 | 中心点 |
| `size` | Size | 100,100 | 尺寸 "width,height" |
| `reversed` | bool | false | 反转路径方向 |

**计算规则**：
```
boundingRect.left   = center.x - size.width / 2
boundingRect.top    = center.y - size.height / 2
boundingRect.right  = center.x + size.width / 2
boundingRect.bottom = center.y + size.height / 2
```

**示例**:

> [Sample](samples/5.2.2_ellipse.pagx)

**路径起点**：椭圆路径从**右侧中点**（3 点钟方向）开始。

#### 5.2.3 多边形/星形（Polystar）

支持正多边形和星形两种模式。

```xml
<Polystar center="100,100" type="star" pointCount="5" outerRadius="100" innerRadius="50" rotation="0" outerRoundness="0" innerRoundness="0" reversed="false"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `center` | Point | 0,0 | 中心点 |
| `type` | PolystarType | star | 类型（见下方） |
| `pointCount` | float | 5 | 顶点数（支持小数） |
| `outerRadius` | float | 100 | 外半径 |
| `innerRadius` | float | 50 | 内半径（仅星形） |
| `rotation` | float | 0 | 旋转角度 |
| `outerRoundness` | float | 0 | 外角圆度 0~1 |
| `innerRoundness` | float | 0 | 内角圆度 0~1 |
| `reversed` | bool | false | 反转路径方向 |

**PolystarType（类型）**：

| 值 | 说明 |
|------|------|
| `polygon` | 正多边形：只使用外半径 |
| `star` | 星形：使用外半径和内半径交替 |

**多边形模式** (`type="polygon"`):
- 只使用 `outerRadius` 和 `outerRoundness`
- `innerRadius` 和 `innerRoundness` 被忽略

**星形模式** (`type="star"`):
- 外顶点位于 `outerRadius` 处
- 内顶点位于 `innerRadius` 处
- 顶点交替连接形成星形

**顶点计算**（第 i 个外顶点）：
```
angle = rotation + (i / pointCount) * 360°
x = center.x + outerRadius * cos(angle)
y = center.y + outerRadius * sin(angle)
```

**小数点数**：
- `pointCount` 支持小数值（如 `5.5`）
- 小数部分表示最后一个顶点的"完成度"，产生不完整的最后一个角
- `pointCount <= 0` 时不生成任何路径

**圆度处理**：
- `outerRoundness` 和 `innerRoundness` 取值范围 0~1
- 0 表示尖角，1 表示完全圆滑
- 圆度通过在顶点处添加贝塞尔控制点实现

**示例**:

> [Sample](samples/5.2.3_polystar.pagx)

#### 5.2.4 路径（Path）

使用 SVG 路径语法定义任意形状，支持内联数据或引用 Resources 中定义的 PathData。

```xml
<!-- 内联路径数据 -->
<Path data="M 0 0 L 100 0 L 100 100 Z" reversed="false"/>

<!-- 引用 PathData 资源 -->
<Path data="@curvePath" reversed="false"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `data` | string/idref | (必填) | SVG 路径数据或 PathData 资源引用 "@id" |
| `reversed` | bool | false | 反转路径方向 |

**示例**:

> [Sample](samples/5.2.4_path.pagx)

#### 5.2.5 文本（Text）

文本元素提供文本内容的几何形状。与形状元素产生单一 Path 不同，Text 经过塑形后会产生**字形列表**（多个字形）并累积到渲染上下文的几何列表中，供后续修改器变换或绘制器渲染。

```xml
<Text text="Hello World" position="100,200" fontFamily="Arial" fontStyle="Regular" fauxBold="true" fauxItalic="false" fontSize="24" letterSpacing="0" textAnchor="start"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `text` | string | "" | 文本内容 |
| `position` | Point | 0,0 | 文本起点位置，y 为基线 |
| `fontFamily` | string | 系统默认 | 字体族 |
| `fontStyle` | string | "Regular" | 字体变体（Regular, Bold, Italic, Bold Italic 等） |
| `fontSize` | float | 12 | 字号 |
| `letterSpacing` | float | 0 | 字间距 |
| `fauxBold` | bool | false | 仿粗体效果 |
| `fauxItalic` | bool | false | 仿斜体效果 |
| `textAnchor` | TextAnchor | start | 文本锚点对齐——控制文本相对原点的位置（见下方）。有 TextBox 排版时忽略 |

子元素：`CDATA` 文本、`GlyphRun`*

**文本内容**：通常使用 `text` 属性指定文本内容。当文本包含 XML 特殊字符（`<`、`>`、`&` 等）或需要保留多行格式时，可使用 CDATA 子节点替代 `text` 属性。Text 不允许直接包含纯文本子节点，必须用 CDATA 包裹。

```xml
<!-- 简单文本：使用 text 属性 -->
<Text text="Hello World" fontFamily="Arial" fontSize="24"/>

<!-- 包含特殊字符：使用 CDATA -->
<Text fontFamily="Arial" fontSize="24"><![CDATA[A < B & C > D]]></Text>

<!-- 多行文本：使用 CDATA 保留格式 -->
<Text fontFamily="Arial" fontSize="24">
<![CDATA[Line 1
Line 2
Line 3]]>
</Text>
```

**渲染模式**：Text 支持**预排版**和**运行时排版**两种模式。预排版通过 GlyphRun 子节点提供预计算的字形和位置，使用嵌入字体渲染，确保跨平台完全一致。运行时排版在运行时进行塑形和排版，因各平台字体和排版特性差异，可能存在细微不一致。如需精确还原设计工具的排版效果，建议使用预排版。

**TextAnchor（文本锚点对齐）**：

控制文本相对其原点的定位方式。

| 值 | 说明 |
|------|------|
| `start` | 原点位于文本起始位置，不做偏移 |
| `center` | 原点位于文本中心位置，文本偏移半个宽度使其居中于原点 |
| `end` | 原点位于文本结束位置，文本偏移整个宽度使其终点对齐原点 |

**运行时排版渲染流程**：
1. 根据 `fontFamily` 和 `fontStyle` 查找系统字体，不可用时按运行时配置的回退列表选择替代字体
2. 使用 `text` 属性（或 CDATA 子节点）进行塑形，换行符触发换行（默认 1.2 倍字号行高，可通过 TextBox 自定义）
3. 应用 `fontSize`、`letterSpacing` 等排版参数
4. 构造字形列表累积到渲染上下文

**运行时排版示例**：

> [Sample](samples/5.2.5_text.pagx)

##### 预排版数据（GlyphRun）

GlyphRun 定义一组字形的预排版数据，每个 GlyphRun 独立引用一个字体资源。

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `font` | idref | (必填) | 引用 Font 资源 `@id` |
| `fontSize` | float | 12 | 渲染字号。实际缩放比例 = `fontSize / font.unitsPerEm` |
| `glyphs` | string | (必填) | GlyphID 序列，逗号分隔（0 表示缺失字形） |
| `x` | float | 0 | 总体 X 偏移 |
| `y` | float | 0 | 总体 Y 偏移 |
| `xOffsets` | string | - | 每字形 X 偏移，逗号分隔 |
| `positions` | string | - | 每字形 (x,y) 偏移，分号分隔 |
| `anchors` | string | - | 每字形锚点偏移 (x,y)，分号分隔。锚点是缩放、旋转和斜切变换的中心点。默认锚点为 (advance×0.5, 0) |
| `scales` | string | - | 每字形缩放 (sx,sy)，分号分隔。缩放围绕锚点进行。默认 1,1 |
| `rotations` | string | - | 每字形旋转角度（度），逗号分隔。旋转围绕锚点进行。默认 0 |
| `skews` | string | - | 每字形斜切角度（度），逗号分隔。斜切围绕锚点进行。默认 0 |

所有属性均为可选，可任意组合使用。当属性数组长度小于字形数量时，缺失的值使用默认值。

**位置计算**：

```
finalX[i] = x + xOffsets[i] + positions[i].x
finalY[i] = y + positions[i].y
```

- 未指定 `xOffsets` 时，`xOffsets[i]` 视为 0
- 未指定 `positions` 时，`positions[i]` 视为 (0, 0)
- 不指定 `xOffsets` 和 `positions` 时：首个字形位于 (x, y)，后续字形依次累加 advance

**变换应用顺序**：

当字形有 scale、rotation 或 skew 变换时，按以下顺序应用（与 TextModifier 一致）：

1. 平移到锚点（`translate(-anchor)`）
2. 缩放（`scale`）
3. 斜切（`skew`，沿垂直轴方向）
4. 旋转（`rotation`）
5. 平移回锚点（`translate(anchor)`）
6. 平移到位置（`translate(position)`）

**锚点**：

- 每个字形的**默认锚点**位于 `(advance × 0.5, 0)`，即字形水平中心的基线位置
- `anchors` 属性记录的是相对于默认锚点的偏移，最终锚点 = 默认锚点 + anchors[i]

**预排版示例**：

> [Sample](samples/5.2.5_glyph_run.pagx)

### 5.3 绘制器（Painters）

绘制器（Fill、Stroke）对**当前时刻**累积的所有几何（Path 和字形列表）进行渲染。

#### 5.3.1 填充（Fill）

填充使用指定的颜色源绘制几何的内部区域。

> [Sample](samples/5.3.1_fill.pagx)

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `color` | Color/idref | #000000 | 颜色值或颜色源引用，默认黑色 |
| `alpha` | float | 1 | 透明度 0~1 |
| `blendMode` | BlendMode | normal | 混合模式（见 2.9 节） |
| `fillRule` | FillRule | winding | 填充规则（见下方） |
| `placement` | LayerPlacement | background | 绘制位置（见 5.3.3 节） |

子元素：可内嵌一个颜色源（SolidColor、LinearGradient、RadialGradient、ConicGradient、DiamondGradient、ImagePattern）

**FillRule（填充规则）**：

| 值 | 说明 |
|------|------|
| `winding` | 非零环绕规则：根据路径方向计数，非零则填充 |
| `evenOdd` | 奇偶规则：根据交叉次数，奇数则填充 |

**文本填充**：
- 文本以字形（glyph）为单位填充
- 支持通过 TextModifier 对单个字形应用颜色覆盖
- 颜色覆盖采用 alpha 混合：`finalColor = lerp(originalColor, overrideColor, overrideAlpha)`

#### 5.3.2 描边（Stroke）

描边沿几何边界绘制线条。

> [Sample](samples/5.3.2_stroke.pagx)

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `color` | Color/idref | #000000 | 颜色值或颜色源引用，默认黑色 |
| `width` | float | 1 | 描边宽度 |
| `alpha` | float | 1 | 透明度 0~1 |
| `blendMode` | BlendMode | normal | 混合模式（见 2.9 节） |
| `cap` | LineCap | butt | 线帽样式（见下方） |
| `join` | LineJoin | miter | 线连接样式（见下方） |
| `miterLimit` | float | 4 | 斜接限制 |
| `dashes` | string | - | 虚线模式 "d1,d2,..." |
| `dashOffset` | float | 0 | 虚线偏移 |
| `dashAdaptive` | bool | false | 等长虚线段缩放 |
| `align` | StrokeAlign | center | 描边对齐（见下方） |
| `placement` | LayerPlacement | background | 绘制位置（见 5.3.3 节） |

**LineCap（线帽样式）**：

| 值 | 说明 |
|------|------|
| `butt` | 平头：线条不超出端点 |
| `round` | 圆头：以半圆形扩展端点 |
| `square` | 方头：以方形扩展端点 |

**LineJoin（线连接样式）**：

| 值 | 说明 |
|------|------|
| `miter` | 斜接：延伸外边缘形成尖角 |
| `round` | 圆角：以圆弧连接 |
| `bevel` | 斜角：以三角形填充连接处 |

**StrokeAlign（描边对齐）**：

| 值 | 说明 |
|------|------|
| `center` | 描边居中于路径（默认） |
| `inside` | 描边在闭合路径内侧 |
| `outside` | 描边在闭合路径外侧 |

内侧/外侧描边通过以下方式实现：
1. 以双倍宽度描边
2. 与原始形状进行布尔运算（内侧用交集，外侧用差集）

**虚线模式**：
- `dashes`：定义虚线段长度序列，如 `"5,3"` 表示 5px 实线 + 3px 空白
- `dashOffset`：虚线起始偏移量
- `dashAdaptive`：为 true 时，缩放虚线间隔使各虚线段保持等长

#### 5.3.3 绘制位置（LayerPlacement）

Fill 和 Stroke 的 `placement` 属性控制相对于子图层的绘制顺序：

| 值 | 说明 |
|------|------|
| `background` | 在子图层**下方**绘制（默认） |
| `foreground` | 在子图层**上方**绘制 |

### 5.4 形状修改器（Shape Modifiers）

形状修改器对累积的 Path 进行**原地变换**，对字形列表则触发强制转换为 Path。

#### 5.4.1 路径裁剪（TrimPath）

裁剪路径到指定的起止范围。

```xml
<TrimPath start="0" end="0.5" offset="0" type="separate"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `start` | float | 0 | 起始位置 0~1 |
| `end` | float | 1 | 结束位置 0~1 |
| `offset` | float | 0 | 偏移量（度），360 度表示完整路径长度的一个周期。例如，180 度将裁剪范围偏移半个路径长度 |
| `type` | TrimType | separate | 裁剪类型（见下方） |

**TrimType（裁剪类型）**：

| 值 | 说明 |
|------|------|
| `separate` | 独立模式：每个形状独立裁剪，使用相同的 start/end 参数 |
| `continuous` | 连续模式：所有形状视为一条连续路径，按总长度比例裁剪 |

**边界情况**：
- `start > end`：对 start 和 end 值取镜像（`start = 1 - start`，`end = 1 - end`）并反转所有路径方向，然后执行正常裁剪。视觉效果为路径的互补段且方向相反
- 支持环绕：当裁剪范围超出 [0,1] 时，自动环绕到路径另一端
- 路径总长度为 0 时，不执行任何操作

> [Sample](samples/5.4.1_trim_path.pagx)

#### 5.4.2 圆角（RoundCorner）

将路径的尖角转换为圆角。

```xml
<RoundCorner radius="10"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `radius` | float | 10 | 圆角半径 |

**处理规则**：
- 只影响尖角（非平滑连接的顶点）
- 圆角半径自动限制为不超过相邻边长度的一半
- `radius <= 0` 时不执行任何操作

**示例**:

> [Sample](samples/5.4.2_round_corner.pagx)

#### 5.4.3 路径合并（MergePath）

将所有形状合并为单个形状。

```xml
<MergePath mode="append"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `mode` | MergePathOp | append | 合并操作（见下方） |

**MergePathOp（路径合并操作）**：

| 值 | 说明 |
|------|------|
| `append` | 追加：简单合并所有路径，不进行布尔运算（默认） |
| `union` | 并集：合并所有形状的覆盖区域 |
| `intersect` | 交集：只保留所有形状的重叠区域 |
| `xor` | 异或：保留非重叠区域 |
| `difference` | 差集：从第一个形状中减去后续形状 |

**重要行为**：
- MergePath 会**清空当前作用域中之前累积的所有 Fill 和 Stroke 效果**，几何列表中仅保留合并后的路径
- 合并时应用各形状的当前变换矩阵
- 合并后的形状变换矩阵重置为单位矩阵

**示例**：

> [Sample](samples/5.4.3_merge_path.pagx)

### 5.5 文本修改器（Text Modifiers）

文本修改器对文本中的独立字形进行变换。

#### 5.5.1 文本修改器处理

遇到文本修改器时，上下文中累积的**所有字形列表**会汇总为一个统一的字形列表进行操作：

```xml
<Group>
  <Text text="Hello " fontFamily="Arial" fontSize="24"/>
  <Text text="World" fontFamily="Arial" fontSize="24"/>
  <TextModifier position="0,-5"/>
  <TextBox position="100,50" textAlign="center"/>
  <Fill color="#333333"/>
</Group>
```

#### 5.5.2 文本转形状

当文本遇到形状修改器时，会强制转换为形状路径：

```
文本元素              形状修改器              后续修改器
┌──────────┐          ┌──────────┐
│   Text   │          │ TrimPath │
└────┬─────┘          │RoundCorn │
     │                │MergePath │
     │ 累积字形列表    └────┬─────┘
     ▼                     │
┌──────────────┐           │ 触发转换
│ 字形列表     │───────────┼──────────────────────┐
│ [H,e,l,l,o]  │           │                      │
└──────────────┘           ▼                      ▼
                  ┌──────────────┐      ┌──────────────────┐
                  │ 合并为单个   │      │ Emoji 被丢弃     │
                  │   Path       │      │ (无法转为路径)    │
                  └──────────────┘      └──────────────────┘
                           │
                           │ 后续文本修改器不再生效
                           ▼
                  ┌──────────────┐
                  │ TextModifier │ → 跳过（已是 Path）
                  └──────────────┘
```

**转换规则**：

1. **触发条件**：文本遇到 TrimPath、RoundCorner、MergePath 时触发转换
2. **合并为单个 Path**：一个 Text 的所有字形合并为**一个** Path，而非每个字形产生一个独立 Path
3. **Emoji 丢失**：Emoji 无法转换为路径轮廓，转换时被丢弃
4. **不可逆转换**：转换后成为纯 Path，后续的文本修改器对其无效

**示例**：
```xml
<Group>
  <Text fontFamily="Arial" fontSize="24"><![CDATA[Hello 😀]]></Text>
  <TrimPath start="0" end="0.5"/>
  <TextModifier position="0,-10"/>
  <Fill color="#333333"/>
</Group>
```

#### 5.5.3 文本变换器（TextModifier）

对选定范围内的字形应用变换和样式覆盖。TextModifier 可包含多个 RangeSelector 子元素，用于定义不同的选择范围和影响因子。

```xml
<TextModifier anchor="0,0" position="0,0" rotation="0" scale="1,1" skew="0" skewAxis="0" alpha="1" fillColor="#FF0000" strokeColor="#000000" strokeWidth="1">
  <RangeSelector start="0" end="0.5" shape="rampUp"/>
  <RangeSelector start="0.5" end="1" shape="rampDown"/>
</TextModifier>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `anchor` | Point | 0,0 | 锚点偏移，相对于字形默认锚点位置。每个字形的默认锚点位于 `(advance × 0.5, 0)`，即字形水平中心的基线位置 |
| `position` | Point | 0,0 | 位置偏移 |
| `rotation` | float | 0 | 旋转 |
| `scale` | Point | 1,1 | 缩放 |
| `skew` | float | 0 | 倾斜角度（度），沿 skewAxis 方向应用 |
| `skewAxis` | float | 0 | 倾斜轴角度（度），定义倾斜的作用方向 |
| `alpha` | float | 1 | 透明度 |
| `fillColor` | Color | - | 填充颜色覆盖 |
| `strokeColor` | Color | - | 描边颜色覆盖 |
| `strokeWidth` | float | - | 描边宽度覆盖 |

**选择器计算**：
1. 根据 RangeSelector 的 `start`、`end`、`offset` 计算选择范围（支持任意小数值，超出 [0,1] 范围时自动环绕）
2. 根据 `shape` 计算每个字形的原始影响值（0~1），然后乘以 `weight`
3. 多个选择器按 `mode` 组合，组合结果限制到 [-1, 1]

```
factor = clamp(combine(rawInfluence₁ × weight₁, rawInfluence₂ × weight₂, ...), -1, 1)
```

**变换应用**：

位置和旋转线性应用 factor。变换按以下顺序应用：

1. 平移到锚点的负方向（`translate(-anchor × factor)`）
2. 从单位矩阵插值缩放（`scale(1 + (scale - 1) × factor)`）
3. 倾斜（`skew(skew × factor, skewAxis)`）
4. 旋转（`rotate(rotation × factor)`）
5. 平移回锚点（`translate(anchor × factor)`）
6. 平移到位置（`translate(position × factor)`）

透明度使用 factor 的绝对值：

```
alphaFactor = 1 + (alpha - 1) × |factor|
finalAlpha = originalAlpha × max(0, alphaFactor)
```

**颜色覆盖**：

颜色覆盖使用 `factor` 的绝对值进行 alpha 混合：

```
blendFactor = overrideColor.alpha × |factor|
finalColor = blend(originalColor, overrideColor, blendFactor)
```

**示例**：

> [Sample](samples/5.5.3_text_modifier.pagx)

#### 5.5.4 范围选择器（RangeSelector）

范围选择器定义 TextModifier 影响的字形范围和影响程度。

```xml
<RangeSelector start="0" end="1" offset="0" unit="percentage" shape="square" easeIn="0" easeOut="0" mode="add" weight="1" randomOrder="false" randomSeed="0"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `start` | float | 0 | 选择起始 |
| `end` | float | 1 | 选择结束 |
| `offset` | float | 0 | 选择偏移 |
| `unit` | SelectorUnit | percentage | 单位（见下方） |
| `shape` | SelectorShape | square | 形状（见下方） |
| `easeIn` | float | 0 | 缓入量 |
| `easeOut` | float | 0 | 缓出量 |
| `mode` | SelectorMode | add | 组合模式（见下方） |
| `weight` | float | 1 | 选择器权重 |
| `randomOrder` | bool | false | 随机顺序 |
| `randomSeed` | int | 0 | 随机种子 |

**SelectorUnit（单位）**：

| 值 | 说明 |
|------|------|
| `index` | 索引：按字形索引计算范围 |
| `percentage` | 百分比：按字形总数的百分比计算范围 |

**SelectorShape（形状）**：

| 值 | 说明 |
|------|------|
| `square` | 矩形：范围内为 1，范围外为 0 |
| `rampUp` | 上升斜坡：从 0 线性增加到 1 |
| `rampDown` | 下降斜坡：从 1 线性减少到 0 |
| `triangle` | 三角形：中心为 1，两端为 0 |
| `round` | 圆形：正弦曲线过渡 |
| `smooth` | 平滑：更平滑的过渡曲线 |

**SelectorMode（组合模式）**：

| 值 | 说明 |
|------|------|
| `add` | 相加：`result = a + b` |
| `subtract` | 相减：`result = b ≥ 0 ? a × (1 − b) : a × (−1 − b)` |
| `intersect` | 交集：`result = a × b` |
| `min` | 最小：`result = min(a, b)` |
| `max` | 最大：`result = max(a, b)` |
| `difference` | 差值：`result = |a − b|` |

#### 5.5.5 路径文本（TextPath）

将文本沿指定路径排列。路径可以通过引用 Resources 中定义的 PathData，也可以内联路径数据。TextPath 使用
基线（由 baselineOrigin 和 baselineAngle 定义的直线）作为文本的参考线：字形从基线上的位置映射到路径曲线上
的对应位置，保持相对间距和偏移。当 forceAlignment 启用时，忽略原始字形位置，将字形均匀分布以填满可用路径长度。

> [Sample](samples/5.5.5_text_path.pagx)

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `path` | string/idref | (必填) | SVG 路径数据或 PathData 资源引用 "@id" |
| `baselineOrigin` | Point | 0,0 | 基线原点，文本参考线的起点坐标 |
| `baselineAngle` | float | 0 | 基线角度（度），0 为水平，90 为垂直 |
| `firstMargin` | float | 0 | 起始边距 |
| `lastMargin` | float | 0 | 结束边距 |
| `perpendicular` | bool | true | 垂直于路径 |
| `reversed` | bool | false | 反转方向 |
| `forceAlignment` | bool | false | 强制拉伸文本填满路径 |

**基线**：
- `baselineOrigin`：基线的起点坐标，相对于 TextPath 的本地坐标空间
- `baselineAngle`：基线的角度（度数），0 表示水平基线（文本从左到右沿 X 轴），90 表示垂直基线（文本从上到下沿 Y 轴）
- 字形沿基线的距离决定其在曲线上的位置，字形垂直于基线的偏移量保持为垂直于曲线的偏移量

**边距**：
- `firstMargin`：起点边距（从路径起点向内偏移）
- `lastMargin`：终点边距（从路径终点向内偏移）

**强制对齐**：
- 当 `forceAlignment="true"` 时，字形按其推进宽度依次排列，然后按比例调整间距以填满 firstMargin 和 lastMargin 之间的路径区域

**字形定位**：
1. 计算字形中心在路径上的位置
2. 获取该位置的路径切线方向
3. 如果 `perpendicular="true"`，旋转字形使其垂直于路径

**闭合路径**：对于闭合路径，超出范围的字形会环绕到路径另一端。

#### 5.5.6 文本框（TextBox）

TextBox 是文本框排版器，对累积的 Text 元素应用排版。它根据自身的 position、size 和对齐设置重新排版所有字形位置，排版结果通过反向变换补偿写入每个 Text 元素的 GlyphRun 数据，因此 Text 自身的 position 和父级 Group 变换在渲染管线中仍然有效。首行使用行框模型定位：行框近端贴齐文本区域近端边缘，基线位于近端下方 `halfLeading + ascent` 处，其中 `halfLeading = (lineHeight - metricsHeight) / 2`，`metricsHeight = ascent + descent + leading`。遵循 CSS Writing Modes 的惯例，`lineHeight` 是逻辑属性，始终作用于行框的块轴方向尺寸。竖排模式下，它控制的是列宽而非行高。列间距为 `lineHeight`（中心到中心的距离）。当 `lineHeight` 为 0（自动）时，列宽根据字体 metrics 计算（ascent + descent + leading），与横排自动行高的算法一致。列从右往左排列。

TextBox 是**仅参与预排版**的节点：它在渲染前的排版阶段被处理，不会在渲染树中实例化。如果累积的所有 Text 元素都已包含嵌入的 GlyphRun 数据，则排版阶段会跳过 TextBox。但即使已填写嵌入的 GlyphRun 数据和字体，仍建议保留 TextBox 节点，因为设计工具导入时需要读取其排版属性（size、对齐方式、wordWrap 等）用于编辑展示。

与其他修改器不同（如 TrimPath 对累积结果进行链式操作），TextBox 只影响 Text 元素的**初始排版**。它在修改器链开始之前确定字形位置，后续的 TextPath、TextModifier 等修改器在 TextBox 的排版结果基础上工作。TextBox 在节点顺序中的位置不影响这一行为。

> [Sample](samples/5.5.6_text_box.pagx)

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `position` | Point | 0,0 | 文本区域左上角坐标 |
| `size` | Size | 0,0 | 排版尺寸。当宽度或高度为 0 时，该维度上文本无边界，可能导致 wordWrap 或 overflow 无效果 |
| `textAlign` | TextAlign | start | 文本对齐——沿行内方向对齐文本（见下方） |
| `paragraphAlign` | ParagraphAlign | near | 段落对齐——沿块流方向对齐文本行/列（见下方） |
| `writingMode` | WritingMode | horizontal | 排版方向（见下方） |
| `lineHeight` | float | 0 | 行高（像素值）。0 表示自动（根据字体 metrics 计算：ascent + descent + leading）。遵循 CSS Writing Modes 的逻辑属性惯例，竖排模式下控制列宽 |
| `wordWrap` | boolean | true | 是否启用自动换行，在盒子宽度边界（横排）或高度边界（竖排）处换行。当该维度的 size 为 0 时无效果 |
| `overflow` | Overflow | visible | 文本超出盒子高度（横排）或宽度（竖排）时的溢出行为。当该维度的 size 为 0 时无效果 |

**TextAlign（文本对齐）**：

| 值 | 说明 |
|------|------|
| `start` | 起始对齐 |
| `center` | 居中对齐 |
| `end` | 结束对齐 |
| `justify` | 两端对齐（最后一行起始对齐） |

**ParagraphAlign（段落对齐）**：

沿块流方向（block-flow direction）对齐文本行或列。使用方向中立的 Near/Far 而非 Top/Bottom，在横排和竖排模式下语义一致。横排模式下控制垂直定位，竖排模式下控制水平定位。

| 值 | 说明 |
|------|------|
| `near` | 近端对齐（横排时为顶部，竖排时为右侧）。使用行框模型，首行行框近端贴齐文本区域近端边缘。基线位于近端下方 `halfLeading + ascent` 处，其中 `halfLeading = (lineHeight - metricsHeight) / 2`。 |
| `middle` | 居中对齐。整体文本块尺寸（所有行高/列宽之和）在对应维度内居中。 |
| `far` | 远端对齐（横排时为底部，竖排时为左侧）。末行行框远端对齐文本区域远端边缘。 |

**WritingMode（排版方向）**：

| 值 | 说明 |
|------|------|
| `horizontal` | 横排文本 |
| `vertical` | 竖排文本（列从右到左排列，传统中日文竖排） |

**Overflow（溢出行为）**：

| 值 | 说明 |
|------|------|
| `visible` | 超出盒子边界的文本仍然渲染（默认） |
| `hidden` | 超出盒子高度（横排）或宽度（竖排）的整行/整列被丢弃，不会显示被截断的半行/半列。当该维度的 size 为 0 时无效果 |

#### 5.5.7 富文本

富文本通过 Group 内的多个 Text 元素组合，每个 Text 可以有独立的 Fill/Stroke 样式。使用 TextBox 进行统一排版。

> [Sample](samples/5.5.7_rich_text.pagx)

**说明**：每个 Group 内的 Text + Fill/Stroke 定义一段样式独立的文本片段，TextBox 将所有片段作为整体进行排版，实现自动换行和对齐。

### 5.6 复制器（Repeater）

复制累积的内容和已渲染的样式，对每个副本应用渐进变换。Repeater 对 Path 和字形列表同时生效，且不会触发文本转形状。

```xml
<Repeater copies="5" offset="1" order="belowOriginal" anchor="0,0" position="50,0" rotation="0" scale="1,1" startAlpha="1" endAlpha="0.2"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `copies` | float | 3 | 副本数 |
| `offset` | float | 0 | 起始偏移 |
| `order` | RepeaterOrder | belowOriginal | 堆叠顺序（见下方） |
| `anchor` | Point | 0,0 | 锚点 |
| `position` | Point | 100,100 | 每个副本的位置偏移 |
| `rotation` | float | 0 | 每个副本的旋转 |
| `scale` | Point | 1,1 | 每个副本的缩放 |
| `startAlpha` | float | 1 | 首个副本透明度 |
| `endAlpha` | float | 1 | 末个副本透明度 |

**变换计算**（第 i 个副本，i 从 0 开始）：
```
progress = i + offset
```

变换按以下顺序应用：

1. 平移到锚点的负方向（`translate(-anchor)`）
2. 指数缩放（`scale(scale^progress)`）
3. 线性旋转（`rotate(rotation × progress)`）
4. 线性位移（`translate(position × progress)`）
5. 平移回锚点（`translate(anchor)`）

**透明度插值**：
```
maxCount = ceil(copies)
t = progress / maxCount
alpha = lerp(startAlpha, endAlpha, t)
// 最后一个副本的 alpha 还需乘以 copies 的小数部分（见下文）
```

**RepeaterOrder（堆叠顺序）**：

| 值 | 说明 |
|------|------|
| `belowOriginal` | 副本在原件下方。索引 0 在最上 |
| `aboveOriginal` | 副本在原件上方。索引 N-1 在最上 |

**小数副本数**：

当 `copies` 为小数时（如 `3.5`），采用**叠加半透明**的方式实现部分副本效果：

1. **几何复制**：形状和文本按 `ceil(copies)` 个复制（即 4 个），几何本身不做缩放或裁剪
2. **透明度调整**：最后一个副本的透明度乘以小数部分（如 0.5），产生半透明效果
3. **视觉效果**：通过透明度渐变模拟"部分存在"的副本

**示例**：`copies="2.3"` 时
- 复制 3 个完整的几何副本
- 第 1、2 个副本正常渲染
- 第 3 个副本透明度 × 0.3，呈现半透明效果

**边界情况**：
- `copies < 0`：不执行任何操作
- `copies = 0`：清空所有累积的内容和已渲染的样式

**Repeater 特性**：
- **同时作用**：复制所有累积的 Path 和字形列表
- **保留文本属性**：字形列表复制后仍保留字形信息，后续文本修改器仍可作用
- **复制已渲染样式**：同时复制已渲染的填充和描边

> [Sample](samples/5.6_repeater.pagx)

### 5.7 容器（Group）

Group 是带变换属性的矢量元素容器。

> [Sample](samples/5.7_group.pagx)

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `anchor` | Point | 0,0 | 锚点 "x,y" |
| `position` | Point | 0,0 | 位置 "x,y" |
| `rotation` | float | 0 | 旋转角度 |
| `scale` | Point | 1,1 | 缩放 "sx,sy" |
| `skew` | float | 0 | 倾斜量 |
| `skewAxis` | float | 0 | 倾斜轴角度 |
| `alpha` | float | 1 | 透明度 0~1 |

#### 变换顺序

变换按以下顺序应用：

1. 平移到锚点的负方向（`translate(-anchor)`）
2. 缩放（`scale`）
3. 倾斜（`skew` 沿 `skewAxis` 方向）
4. 旋转（`rotation`）
5. 平移到位置（`translate(position)`）

**倾斜变换**：

倾斜变换按以下顺序应用：

1. 旋转到倾斜轴方向（`rotate(skewAxis)`）
2. 沿 X 轴剪切（`shearX(tan(skew))`）
3. 旋转回原方向（`rotate(-skewAxis)`）

#### 作用域隔离

Group 创建独立的作用域，用于隔离几何累积和渲染：

- 组内的几何元素只在组内累积
- 组内的绘制器只渲染组内累积的几何
- 组内的修改器只影响组内累积的几何
- 组的变换矩阵应用到组内所有内容
- 组的 `alpha` 属性应用到组内所有渲染内容

**几何累积规则**：

- **绘制器不清空几何**：Fill 和 Stroke 渲染后，几何列表保持不变，后续绘制器仍可渲染相同的几何
- **子 Group 几何向上累积**：子 Group 处理完成后，其几何会累积到父作用域，父级末尾的绘制器可以渲染所有子 Group 的几何
- **同级 Group 互不影响**：每个 Group 创建独立的累积起点，不会看到后续兄弟 Group 的几何
- **隔离渲染范围**：Group 内的绘制器只能渲染到当前位置已累积的几何，包括本组和已完成的子 Group
- **Layer 是累积终止点**：几何向上累积直到遇到 Layer 边界，不会跨 Layer 传递

**示例 1 - 基本隔离**：
> [Sample](samples/5.7_group_isolation.pagx)

**示例 2 - 子 Group 几何向上累积**：
> [Sample](samples/5.7_group_propagation.pagx)

**示例 3 - 多个绘制器复用几何**：
> [Sample](samples/5.7_multiple_painters.pagx)

#### 多重填充与描边

由于绘制器不清空几何列表，同一几何可连续应用多个 Fill 和 Stroke。

**示例 4 - 多重填充**：
> [Sample](samples/5.7_multiple_fills.pagx)

**示例 5 - 多重描边**：
> [Sample](samples/5.7_multiple_strokes.pagx)

**示例 6 - 混合叠加**：
> [Sample](samples/5.7_mixed_overlay.pagx)

**渲染顺序**：多个绘制器按文档顺序渲染，先出现的位于下方。

---

## 附录 A. 节点层级与包含关系（Node Hierarchy）

本附录描述节点的分类和嵌套规则。

### A.1 节点分类

| 分类 | 节点 |
|------|------|
| **容器** | `pagx`, `Resources`, `Layer`, `Group` |
| **资源** | `Image`, `PathData`, `Composition`, `Font`, `Glyph` |
| **颜色源** | `SolidColor`, `LinearGradient`, `RadialGradient`, `ConicGradient`, `DiamondGradient`, `ImagePattern`, `ColorStop` |
| **图层样式** | `DropShadowStyle`, `InnerShadowStyle`, `BackgroundBlurStyle` |
| **图层滤镜** | `BlurFilter`, `DropShadowFilter`, `InnerShadowFilter`, `BlendFilter`, `ColorMatrixFilter` |
| **几何元素** | `Rectangle`, `Ellipse`, `Polystar`, `Path`, `Text`, `GlyphRun` |
| **修改器** | `TrimPath`, `RoundCorner`, `MergePath`, `TextModifier`, `RangeSelector`, `TextPath`, `TextBox`, `Repeater` |
| **绘制器** | `Fill`, `Stroke` |

### A.2 文档包含关系

```
pagx
├── Resources
│   ├── Image
│   ├── PathData
│   ├── SolidColor
│   ├── LinearGradient → ColorStop*
│   ├── RadialGradient → ColorStop*
│   ├── ConicGradient → ColorStop*
│   ├── DiamondGradient → ColorStop*
│   ├── ImagePattern
│   ├── Font → Glyph*
│   └── Composition → Layer*
│
└── Layer*
    ├── VectorElement*（见 A.3）
    ├── DropShadowStyle*
    ├── InnerShadowStyle*
    ├── BackgroundBlurStyle*
    ├── BlurFilter*
    ├── DropShadowFilter*
    ├── InnerShadowFilter*
    ├── BlendFilter*
    ├── ColorMatrixFilter*
    └── Layer*（子图层）
```

### A.3 VectorElement 包含关系

`Layer` 和 `Group` 可包含以下 VectorElement：

```
Layer / Group
├── Rectangle
├── Ellipse
├── Polystar
├── Path
├── Text → GlyphRun*（预排版模式）
├── Fill（可内嵌颜色源）
│   └── SolidColor / LinearGradient / RadialGradient / ConicGradient / DiamondGradient / ImagePattern
├── Stroke（可内嵌颜色源）
│   └── SolidColor / LinearGradient / RadialGradient / ConicGradient / DiamondGradient / ImagePattern
├── TrimPath
├── RoundCorner
├── MergePath
├── TextModifier → RangeSelector*
├── TextPath
├── TextBox
├── Repeater
└── Group*（递归）
```

---

## 附录 B. 枚举类型（Enumeration Types）

### 图层相关

| 枚举 | 值 |
|------|------|
| **BlendMode** | `normal`, `multiply`, `screen`, `overlay`, `darken`, `lighten`, `colorDodge`, `colorBurn`, `hardLight`, `softLight`, `difference`, `exclusion`, `hue`, `saturation`, `color`, `luminosity`, `plusLighter`, `plusDarker` |
| **MaskType** | `alpha`, `luminance`, `contour` |
| **TileMode** | `clamp`, `repeat`, `mirror`, `decal` |
| **FilterMode** | `nearest`, `linear` |
| **MipmapMode** | `none`, `nearest`, `linear` |

### 绘制器相关

| 枚举 | 值 |
|------|------|
| **FillRule** | `winding`, `evenOdd` |
| **LineCap** | `butt`, `round`, `square` |
| **LineJoin** | `miter`, `round`, `bevel` |
| **StrokeAlign** | `center`, `inside`, `outside` |
| **LayerPlacement** | `background`, `foreground` |

### 几何元素相关

| 枚举 | 值 |
|------|------|
| **PolystarType** | `polygon`, `star` |

### 修改器相关

| 枚举 | 值 |
|------|------|
| **TrimType** | `separate`, `continuous` |
| **MergePathOp** | `append`, `union`, `intersect`, `xor`, `difference` |
| **SelectorUnit** | `index`, `percentage` |
| **SelectorShape** | `square`, `rampUp`, `rampDown`, `triangle`, `round`, `smooth` |
| **SelectorMode** | `add`, `subtract`, `intersect`, `min`, `max`, `difference` |
| **TextAlign** | `start`, `center`, `end`, `justify` |
| **TextAnchor** | `start`, `center`, `end` |
| **ParagraphAlign** | `near`, `middle`, `far` |
| **WritingMode** | `horizontal`, `vertical` |
| **RepeaterOrder** | `belowOriginal`, `aboveOriginal` |
---

## 附录 C. 常见用法示例（Examples）

### C.1 完整示例

以下示例涵盖 PAGX 的所有主要节点类型，展示完整的文档结构。

> [Sample](samples/C.1_complete_example.pagx)

### C.2 RPG 角色面板

一个奇幻 RPG 风格的角色状态面板，展示了复杂的 UI 组合，包含嵌套图层、渐变和装饰元素。

> [Sample](samples/C.2_rpg_character_panel.pagx)

### C.3 星云学员

一个太空主题的学员资料卡片，展示了星云效果、星空背景和现代 UI 设计模式。

> [Sample](samples/C.3_nebula_cadet.pagx)

### C.4 游戏 HUD

一个游戏平视显示器（HUD），展示了血条、分数显示和游戏界面元素。

> [Sample](samples/C.4_game_hud.pagx)

### C.5 PAGX 特性概览

PAGX 格式能力的综合展示，包括渐变、效果、文本样式和矢量图形。

> [Sample](samples/C.5_pagx_features.pagx)

