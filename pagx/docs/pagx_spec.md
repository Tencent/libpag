# PAGX 格式规范 v1.0

## 1. Introduction（介绍）

**PAGX**（Portable Animated Graphics XML）是一种基于 XML 的矢量动画标记语言。它提供了统一且强大的矢量图形与动画描述能力，旨在成为跨所有主要工具与运行时的矢量动画交换标准。

**PAGX** (Portable Animated Graphics XML) is an XML-based markup language for describing animated vector graphics. It provides a unified and powerful representation of vector graphics and animations, designed to be the vector animation interchange standard across all major tools and runtimes.

### 1.1 设计目标

- **开放可读**：纯文本 XML 格式，易于阅读和编辑，天然支持版本控制与差异对比，便于调试及 AI 理解与生成。

- **特性完备**：完整覆盖矢量图形、图片、富文本、滤镜效果、混合模式、遮罩等能力，满足复杂动效的描述需求。

- **精简高效**：提供简洁且强大的统一结构，兼顾静态矢量与动画的优化描述，同时预留未来交互和脚本的扩展能力。

- **生态兼容**：可作为 After Effects、Figma、腾讯设计等设计工具的通用交换格式，实现设计资产无缝流转。

- **高效部署**：设计资产可一键导出并部署到研发环境，与二进制 PAG 格式双向互转，获得极高压缩比和运行时性能。

### 1.2 文件结构

PAGX 是纯 XML 文件（`.pagx`），可引用外部资源文件（图片、视频、音频、字体等），也支持通过数据 URI 内嵌资源。PAGX 与二进制 PAG 格式可双向互转：发布时转换为 PAG 以优化加载性能，调试时转换回 PAGX 以便阅读和编辑。

### 1.3 文档组织

本规范按以下顺序组织：

1. **基础数据类型**：定义文档中使用的基本数据格式
2. **文档结构**：描述 PAGX 文档的整体组织方式
3. **图层系统**：定义图层及其相关特性（样式、滤镜、遮罩）
4. **矢量元素系统**：定义图层内容的矢量元素及其处理模型

**附录**（方便速查）：

- **附录 A**：枚举类型汇总
- **附录 B**：节点定义速查
- **附录 C**：节点分类与包含关系
- **附录 D**：完整示例

---

## 2. Basic Data Types（基础数据类型）

本节定义 PAGX 文档中使用的基础数据类型和命名规范。

### 2.1 命名规范

| 类别 | 规范 | 示例 |
|------|------|------|
| 元素名 | PascalCase，不缩写 | `Group`、`Rectangle`、`Fill` |
| 属性名 | camelCase，尽量简短 | `antiAlias`、`blendMode`、`fontSize` |
| 属性节点 | camelCase | `<contents>`、`<styles>`、`<filters>` |
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
<ShapeLayer data-name="背景图层" data-figma-id="12:34" data-exported-by="PAGExporter">
  <contents>
    <Rectangle center="50,50" size="100,100"/>
    <Fill color="#FF0000"/>
  </contents>
</ShapeLayer>
```

### 2.4 基本数值类型

| 类型 | 说明 | 示例 |
|------|------|------|
| `float` | 浮点数 | `1.5`、`-0.5`、`100` |
| `int` | 整数 | `400`、`0`、`-1` |
| `bool` | 布尔值 | `true`、`false` |
| `string` | 字符串 | `"Arial"`、`"myLayer"` |
| `enum` | 枚举值 | `normal`、`multiply` |
| `idref` | ID 引用 | `#gradientId`、`#maskLayer` |

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

PAGX 支持多种颜色格式：

| 格式 | 示例 | 说明 |
|------|------|------|
| HEX | `#RGB`、`#RRGGBB`、`#RRGGBBAA` | 十六进制 |
| RGB | `rgb(255,0,0)`、`rgba(255,0,0,0.5)` | RGB 带可选透明度 |
| HSL | `hsl(0,100%,50%)`、`hsla(0,100%,50%,0.5)` | HSL 带可选透明度 |
| 色域 | `color(display-p3 1 0 0)` | 广色域颜色 |
| 引用 | `#resourceId` | 引用 Resources 中定义的颜色源 |

### 2.9 PathData（路径数据）

PathData 定义可复用的路径数据，放置在 Resources 中供 Path 元素和 TextPath 修改器引用。

```xml
<PathData id="curvePath" data="M 0 0 C 50 0 50 100 100 100"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `data` | string | (必填) | SVG 路径数据 |

**路径命令**（SVG 路径语法）：

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

### 2.10 外部资源引用（External Resource Reference）

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

### 2.11 Image（图片）

图片资源定义可在文档中引用的位图数据。

```xml
<Image source="photo.png"/>
<Image source="data:image/png;base64,..."/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `source` | string | (必填) | 文件路径或数据 URI（见 2.10 节） |

**支持格式**：PNG、JPEG、WebP、GIF

### 2.12 颜色源（Color Source）

颜色源定义可用于渲染的颜色，支持两种定义方式：

1. **共享定义**：在 `<Resources>` 中预定义，通过 `#id` 引用。适用于**被多处引用**的颜色源。
2. **内联定义**：直接嵌套在 `<Fill>` 或 `<Stroke>` 元素内部。适用于**仅使用一次**的颜色源，更简洁。

#### 2.12.1 SolidColor（纯色）

```xml
<SolidColor color="#FF0000"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `color` | color | (必填) | 颜色值 |

#### 2.12.2 LinearGradient（线性渐变）

线性渐变沿起点到终点的方向插值。

```xml
<LinearGradient startPoint="0,0" endPoint="100,0">
  <ColorStop offset="0" color="#FF0000"/>
  <ColorStop offset="1" color="#0000FF"/>
</LinearGradient>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `startPoint` | point | (必填) | 起点 |
| `endPoint` | point | (必填) | 终点 |
| `matrix` | string | 单位矩阵 | 变换矩阵 |

**计算**：对于点 P，其颜色由 P 在起点-终点连线上的投影位置决定。

#### 2.12.3 RadialGradient（径向渐变）

径向渐变从中心向外辐射。

```xml
<RadialGradient center="50,50" radius="50">
  <ColorStop offset="0" color="#FFFFFF"/>
  <ColorStop offset="1" color="#000000"/>
</RadialGradient>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `center` | point | 0,0 | 中心点 |
| `radius` | float | (必填) | 渐变半径 |
| `matrix` | string | 单位矩阵 | 变换矩阵 |

**计算**：对于点 P，其颜色由 `distance(P, center) / radius` 决定。

#### 2.12.4 ConicGradient（锥形渐变）

锥形渐变（也称扫描渐变）沿圆周方向插值。

```xml
<ConicGradient center="50,50" startAngle="0" endAngle="360">
  <ColorStop offset="0" color="#FF0000"/>
  <ColorStop offset="1" color="#0000FF"/>
</ConicGradient>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `center` | point | 0,0 | 中心点 |
| `startAngle` | float | 0 | 起始角度 |
| `endAngle` | float | 360 | 结束角度 |
| `matrix` | string | 单位矩阵 | 变换矩阵 |

**计算**：对于点 P，其颜色由 `atan2(P.y - center.y, P.x - center.x)` 在 `[startAngle, endAngle]` 范围内的比例决定。

#### 2.12.5 DiamondGradient（菱形渐变）

菱形渐变从中心向四角辐射。

```xml
<DiamondGradient center="50,50" halfDiagonal="50">
  <ColorStop offset="0" color="#FFFFFF"/>
  <ColorStop offset="1" color="#000000"/>
</DiamondGradient>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `center` | point | 0,0 | 中心点 |
| `halfDiagonal` | float | (必填) | 半对角线长度 |
| `matrix` | string | 单位矩阵 | 变换矩阵 |

**计算**：对于点 P，其颜色由曼哈顿距离 `(|P.x - center.x| + |P.y - center.y|) / halfDiagonal` 决定。

#### 2.12.6 ColorStop（渐变色标）

```xml
<ColorStop offset="0.5" color="#FF0000"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `offset` | float | (必填) | 位置 0.0~1.0 |
| `color` | color | (必填) | 色标颜色 |

**渐变通用规则**：

- **色标插值**：相邻色标之间使用线性插值
- **色标边界**：
  - `offset < 0` 的色标被视为 `offset = 0`
  - `offset > 1` 的色标被视为 `offset = 1`
  - 如果没有 `offset = 0` 的色标，使用第一个色标的颜色填充
  - 如果没有 `offset = 1` 的色标，使用最后一个色标的颜色填充
- **渐变变换**：`matrix` 属性对渐变坐标系应用变换

#### 2.12.7 颜色源坐标系统

所有颜色源（渐变、图案）的坐标系是**相对于几何元素的局部坐标系原点**。

**变换行为**：

1. **外部变换会同时作用于几何和颜色源**：Group 的变换、Layer 的矩阵等外部变换会整体作用于几何元素及其颜色源，两者一起缩放、旋转、平移。

2. **修改几何属性不影响颜色源**：直接修改几何元素的属性（如 Rectangle 的 width/height、Path 的路径数据）只改变几何内容本身，不会影响颜色源的坐标系。

**示例**：在 100×100 的区域内绘制一个从左到右的线性渐变：

```xml
<Resources>
  <LinearGradient id="grad" startPoint="0,0" endPoint="100,0">
    <ColorStop offset="0" color="#FF0000"/>
    <ColorStop offset="1" color="#0000FF"/>
  </LinearGradient>
</Resources>

<Layer>
  <contents>
    <Rectangle center="50,50" size="100,100"/>
    <Fill color="#grad"/>
  </contents>
</Layer>
```

- 对该图层应用 `scale(2, 2)` 变换：矩形变为 200×200，渐变也随之放大，视觉效果保持一致
- 直接将 Rectangle 的 size 改为 200,200：矩形变为 200×200，但渐变坐标不变，只覆盖矩形的左半部分

#### 2.12.8 ImagePattern（图片图案）

图片图案使用图片作为颜色源。

```xml
<ImagePattern image="#img1" tileModeX="repeat" tileModeY="repeat"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `image` | idref | (必填) | 图片引用 "#id" |
| `tileModeX` | TileMode | clamp | X 方向平铺模式（见下方） |
| `tileModeY` | TileMode | clamp | Y 方向平铺模式（见下方） |
| `sampling` | SamplingMode | linear | 采样模式（见下方） |
| `matrix` | string | 单位矩阵 | 变换矩阵 |

**TileMode（平铺模式）**：

| 值 | 说明 |
|------|------|
| `clamp` | 钳制：超出边界使用边缘像素颜色 |
| `repeat` | 重复：平铺图片 |
| `mirror` | 镜像：交替翻转平铺 |
| `decal` | 贴花：超出边界为透明 |

**SamplingMode（采样模式）**：

| 值 | 说明 |
|------|------|
| `nearest` | 最近邻：取最近像素，锐利但有锯齿 |
| `linear` | 双线性：平滑插值 |
| `mipmap` | 多级渐远：根据缩放级别选择合适的 mip 层 |

**图案变换**：

`matrix` 属性对图案应用变换，效果与对普通图形元素的变换一致：

- `matrix="2,0,0,2,0,0"`（缩放 2 倍）：图案视觉上**放大** 2 倍
- `matrix="0.5,0,0,0.5,0,0"`（缩放 0.5 倍）：图案视觉上**缩小**到原来的 1/2

**示例**：假设有一个 12×12 像素的棋盘格图片，希望每个 tile 显示为 24×24 像素：

```xml
<!-- 图片原始尺寸 12x12，显示为 24x24 需要放大 2 倍，因此 matrix 使用 2 缩放 -->
<ImagePattern image="#checker12" tileModeX="repeat" tileModeY="repeat" matrix="2,0,0,2,0,0"/>
```

---

## 3. Document Structure（文档结构）

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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="1920" height="1080">
  <Layer name="background"><!-- 图层内容 --></Layer>
  <Layer name="content"><!-- 图层内容 --></Layer>
  <Resources><!-- 资源定义 --></Resources>
</pagx>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `version` | string | (必填) | 格式版本 |
| `width` | float | (必填) | 画布宽度 |
| `height` | float | (必填) | 画布高度 |

**图层渲染顺序**：图层按文档顺序依次渲染，文档中靠前的图层先渲染（位于下方），靠后的图层后渲染（位于上方）。

### 3.3 Resources（资源区）

`<Resources>` 定义可复用的资源，包括图片、路径数据、颜色源和合成。资源通过 `id` 属性标识，在文档其他位置通过 `#id` 形式引用。

**元素位置**：Resources 元素可放置在根元素内的任意位置，对位置没有限制。解析器必须支持元素引用在文档后面定义的资源或图层（即前向引用）。

```xml
<Resources>
  <Image id="img1" source="photo.png"/>
  <PathData id="curvePath" data="M 0 0 C 50 0 50 100 100 100"/>
  <SolidColor id="brandRed" color="#FF0000"/>
  <LinearGradient id="skyGradient" startPoint="0,0" endPoint="0,100">
    <ColorStop offset="0" color="#87CEEB"/>
    <ColorStop offset="1" color="#E0F6FF"/>
  </LinearGradient>
  <ImagePattern id="texture" image="#img1" tileModeX="repeat" tileModeY="repeat"/>
  <Composition id="buttonComp" width="100" height="50"><!-- 合成内容 --></Composition>
</Resources>
```

**支持的资源类型**：

- `Image`：图片资源（见 2.11 节）
- `PathData`：路径数据（见 2.9 节），可被 Path 元素和 TextPath 修改器引用
- 颜色源（见 2.12 节）：`SolidColor`、`LinearGradient`、`RadialGradient`、`ConicGradient`、`DiamondGradient`、`ImagePattern`
- `Composition`：合成（见下方）

#### 3.3.1 Composition（合成）

合成用于内容复用（类似 After Effects 的 Pre-comp）。

```xml
<Composition id="buttonComp" width="100" height="50">
  <Layer name="button">
    <contents>
      <Rectangle center="50,25" size="100,50" roundness="10"/>
      <Fill color="#007AFF"/>
    </contents>
  </Layer>
</Composition>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `width` | float | (必填) | 合成宽度 |
| `height` | float | (必填) | 合成高度 |

### 3.4 文档层级结构

PAGX 文档采用层级结构组织内容：

```
<pagx>                          ← 根元素（定义画布尺寸）
├── <Layer>                     ← 图层（可多个）
│   ├── <contents>              ← 矢量内容（VectorElement 系统）
│   │   ├── 几何元素            ← Rectangle、Ellipse、Path、TextSpan 等
│   │   ├── 修改器              ← TrimPath、RoundCorner、TextModifier 等
│   │   ├── 绘制器              ← Fill、Stroke
│   │   └── <Group>             ← 矢量元素容器（可嵌套）
│   │
│   ├── <styles>                 ← 图层样式
│   │   └── <DropShadowStyle>   ← 投影、内阴影等
│   │
│   ├── <filters>               ← 滤镜
│   │   └── <BlurFilter>        ← 模糊、颜色矩阵等
│   │
│   └── <Layer>                 ← 子图层（递归结构）
│       └── ...
│
└── <Resources>                 ← 资源区（可选，定义可复用资源）
    ├── <Image>                 ← 图片资源
    ├── <PathData>              ← 路径数据资源
    ├── <SolidColor>            ← 纯色定义
    ├── <LinearGradient>        ← 渐变定义
    ├── <ImagePattern>          ← 图片图案定义
    └── <Composition>           ← 合成定义
        └── <Layer>             ← 合成内的图层
```

---

## 4. Layer System（图层系统）

图层（Layer）是 PAGX 内容组织的基本单元，提供了丰富的视觉效果控制能力。

### 4.1 Layer（图层）

`<Layer>` 是内容和子图层的基本容器。

```xml
<Layer name="MyLayer" visible="true" alpha="1" blendMode="normal" x="0" y="0" antiAlias="true">
  <contents>
    <Rectangle center="50,50" size="100,100"/>
    <Fill color="#FF0000"/>
  </contents>
  <styles>
    <DropShadowStyle offsetX="5" offsetY="5" blurrinessX="10" blurrinessY="10" color="#00000080"/>
  </styles>
  <filters>
    <BlurFilter blurrinessX="10" blurrinessY="10"/>
  </filters>
  <Layer name="Child">
    <contents>
      <Ellipse center="50,50" size="80,80"/>
      <Fill color="#00FF00"/>
    </contents>
  </Layer>
</Layer>
<Layer composition="#buttonComp" x="100" y="200"/>
```

#### contents 子节点

`<contents>` 是图层的矢量内容容器，本身相当于一个不带变换属性的 Group，可直接包含几何元素、修改器、绘制器等 VectorElement。

#### 图层属性

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `name` | string | "" | 显示名称 |
| `visible` | bool | true | 是否可见 |
| `alpha` | float | 1 | 透明度 0~1 |
| `blendMode` | BlendMode | normal | 混合模式（见下方） |
| `x` | float | 0 | X 位置 |
| `y` | float | 0 | Y 位置 |
| `matrix` | string | 单位矩阵 | 2D 变换 "a,b,c,d,tx,ty" |
| `matrix3D` | string | - | 3D 变换（16 个值，列优先） |
| `preserve3D` | bool | false | 保持 3D 变换 |
| `antiAlias` | bool | true | 边缘抗锯齿 |
| `groupOpacity` | bool | false | 组透明度 |
| `passThroughBackground` | bool | true | 是否允许背景透传给子图层 |
| `excludeChildEffectsInLayerStyle` | bool | false | 图层样式是否排除子图层效果 |
| `scrollRect` | string | - | 滚动裁剪区域 "x,y,w,h" |
| `mask` | idref | - | 遮罩图层引用 "#id" |
| `maskType` | MaskType | alpha | 遮罩类型（见下方） |
| `composition` | idref | - | 合成引用 "#id" |

**MaskType（遮罩类型）**：

| 值 | 说明 |
|------|------|
| `alpha` | Alpha 遮罩：使用遮罩的 alpha 通道 |
| `luminance` | 亮度遮罩：使用遮罩的亮度值 |
| `contour` | 轮廓遮罩：使用遮罩的轮廓进行裁剪 |

**BlendMode（混合模式）**：

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

#### 图层渲染流程

图层内容（填充、描边等）通过 `placement` 属性分为背景内容和前景内容，默认为背景内容。单个图层渲染时按以下顺序处理：

1. **图层样式（下方）**：渲染位于内容下方的图层样式（如投影阴影）
2. **图层背景内容**：渲染填充和描边
3. **子图层**：按文档顺序递归渲染所有子图层
4. **图层样式（上方）**：渲染位于内容上方的图层样式（如内阴影）
5. **图层前景内容**：渲染填充和描边
6. **滤镜**：应用滤镜链

**图层样式的参考内容**：图层样式计算时使用的参考内容包含背景内容和前景内容的完整形状。例如，当填充为背景、描边为前景时，描边会绘制在子图层之上，但投影阴影仍然基于包含填充和描边的完整形状计算。

#### 图层轮廓（Layer Contour）

**图层轮廓**是图层内容的形状信息，代表图层内容的外形边界。轮廓不包含实际的 RGBA 颜色数据，仅表示形状。图层轮廓主要用于：

- **图层样式**：投影阴影、内阴影、背景模糊等效果基于轮廓计算
- **遮罩**：`maskType="contour"` 使用遮罩图层的轮廓进行裁剪

**轮廓与可见内容的关系**：

- **透明度为 0 的绘制器仍参与轮廓**：即使 Fill 或 Stroke 的 `alpha="0"`，其形状仍然会参与图层轮廓的构建。这意味着完全透明的内容虽然不可见，但仍会影响图层样式的效果范围
- **几何元素必须有绘制器才能参与轮廓**：单独的几何元素（Rectangle、Ellipse 等）如果没有对应的 Fill 或 Stroke，则不会参与轮廓计算

**示例**：创建一个不可见但有阴影的图层：

```xml
<Layer name="InvisibleShadow">
  <contents>
    <Rectangle center="100,100" size="200,200"/>
    <Fill color="#000000" alpha="0"/>  <!-- 透明填充，不可见但参与轮廓 -->
  </contents>
  <styles>
    <DropShadowStyle offsetX="5" offsetY="5" blurrinessX="10" blurrinessY="10" color="#00000080"/>
  </styles>
</Layer>
```

上例中，矩形填充完全透明不可见，但投影阴影仍然会基于矩形的轮廓生成。

### 4.2 Layer Styles（图层样式）

图层样式在图层内容渲染完成后应用。

```xml
<styles>
  <DropShadowStyle offsetX="5" offsetY="5" blurrinessX="10" blurrinessY="10" color="#00000080" showBehindLayer="true"/>
  <InnerShadowStyle offsetX="2" offsetY="2" blurrinessX="5" blurrinessY="5" color="#00000040"/>
  <BackgroundBlurStyle blurrinessX="20" blurrinessY="20" tileMode="mirror"/>
</styles>
```

**所有 LayerStyle 共有属性**：

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `blendMode` | BlendMode | normal | 混合模式（见 4.1） |

#### 4.2.1 DropShadowStyle（投影阴影）

在图层外部绘制投影阴影。

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `offsetX` | float | 0 | X 偏移 |
| `offsetY` | float | 0 | Y 偏移 |
| `blurrinessX` | float | 0 | X 模糊半径 |
| `blurrinessY` | float | 0 | Y 模糊半径 |
| `color` | color | #000000 | 阴影颜色 |
| `showBehindLayer` | bool | true | 图层后面是否显示阴影 |

**渲染步骤**：
1. 将图层内容偏移 `(offsetX, offsetY)`
2. 应用高斯模糊 `(blurrinessX, blurrinessY)`
3. 将图层 alpha 通道替换为 `color` 的颜色

**showBehindLayer**：
- `true`：阴影完整显示，包括被图层内容遮挡的部分
- `false`：阴影被图层内容遮挡的部分会被挖空（仅显示图层轮廓外的阴影）

#### 4.2.2 InnerShadowStyle（内阴影）

在图层内部绘制内阴影。

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `offsetX` | float | 0 | X 偏移 |
| `offsetY` | float | 0 | Y 偏移 |
| `blurrinessX` | float | 0 | X 模糊半径 |
| `blurrinessY` | float | 0 | Y 模糊半径 |
| `color` | color | #000000 | 阴影颜色 |

**渲染步骤**：
1. 创建图层轮廓的反向遮罩
2. 偏移并模糊
3. 与图层内容求交集

#### 4.2.3 BackgroundBlurStyle（背景模糊）

对图层下方的背景应用模糊效果。

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `blurrinessX` | float | 0 | X 模糊半径 |
| `blurrinessY` | float | 0 | Y 模糊半径 |
| `tileMode` | TileMode | mirror | 平铺模式（见 2.12.8） |

**渲染步骤**：
1. 获取图层边界下方的背景内容
2. 应用高斯模糊 `(blurrinessX, blurrinessY)`
3. 使用图层轮廓作为遮罩裁剪模糊结果

### 4.3 Filter Effects（滤镜效果）

滤镜按文档顺序链式应用，每个滤镜的输出作为下一个滤镜的输入。

```xml
<filters>
  <BlurFilter blurrinessX="10" blurrinessY="10"/>
  <DropShadowFilter offsetX="5" offsetY="5" blurrinessX="10" blurrinessY="10" color="#00000080"/>
  <ColorMatrixFilter matrix="1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0"/>
</filters>
```

#### 4.3.1 BlurFilter（模糊滤镜）

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `blurrinessX` | float | (必填) | X 模糊半径 |
| `blurrinessY` | float | (必填) | Y 模糊半径 |
| `tileMode` | TileMode | decal | 平铺模式（见 2.12.8） |

#### 4.3.2 DropShadowFilter（投影阴影滤镜）

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `offsetX` | float | 0 | X 偏移 |
| `offsetY` | float | 0 | Y 偏移 |
| `blurrinessX` | float | 0 | X 模糊半径 |
| `blurrinessY` | float | 0 | Y 模糊半径 |
| `color` | color | #000000 | 阴影颜色 |
| `shadowOnly` | bool | false | 仅显示阴影 |

#### 4.3.3 InnerShadowFilter（内阴影滤镜）

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `offsetX` | float | 0 | X 偏移 |
| `offsetY` | float | 0 | Y 偏移 |
| `blurrinessX` | float | 0 | X 模糊半径 |
| `blurrinessY` | float | 0 | Y 模糊半径 |
| `color` | color | #000000 | 阴影颜色 |
| `shadowOnly` | bool | false | 仅显示阴影 |

#### 4.3.4 BlendFilter（混合滤镜）

将指定颜色以指定混合模式叠加到图层上。

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `color` | color | (必填) | 混合颜色 |
| `blendMode` | BlendMode | normal | 混合模式（见 4.1） |

#### 4.3.5 ColorMatrixFilter（颜色矩阵滤镜）

使用 4×5 颜色矩阵变换颜色。

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `matrix` | string | (必填) | 4x5 颜色矩阵（20 个逗号分隔的浮点数） |

**矩阵格式**（20 个值，行优先）：
```
| R' |   | m[0]  m[1]  m[2]  m[3]  m[4]  |   | R |
| G' |   | m[5]  m[6]  m[7]  m[8]  m[9]  |   | G |
| B' | = | m[10] m[11] m[12] m[13] m[14] | × | B |
| A' |   | m[15] m[16] m[17] m[18] m[19] |   | A |
                                            | 1 |
```

### 4.4 Clipping and Masking（裁剪与遮罩）

#### 4.4.1 scrollRect（滚动裁剪）

`scrollRect` 属性定义图层的可视区域，超出该区域的内容会被裁剪。

```xml
<Layer scrollRect="0,0,100,100">
  <contents>
    <Rectangle center="50,50" size="200,200"/>
    <Fill color="#FF0000"/>
  </contents>
</Layer>
```

#### 4.4.2 遮罩（Masking）

通过 `mask` 属性引用另一个图层作为遮罩。

```xml
<Layer id="maskShape" visible="false">
  <contents>
    <Ellipse center="100,100" size="150,150"/>
    <Fill color="#FFFFFF"/>
  </contents>
</Layer>
<Layer mask="#maskShape" maskType="alpha">
  <contents>
    <Rectangle center="100,100" size="200,200"/>
    <Fill color="#FF0000"/>
  </contents>
</Layer>
```

**遮罩类型**：MaskType 定义见 4.1 节。

**遮罩规则**：
- 遮罩图层自身不渲染（`visible` 属性被忽略）
- 遮罩图层的变换不影响被遮罩图层

---

## 5. VectorElement System（矢量元素系统）

矢量元素系统定义了图层 `<contents>` 内的矢量内容如何被处理和渲染。

### 5.1 Processing Model（处理模型）

VectorElement 系统采用**累积-渲染**的处理模型：几何元素在渲染上下文中累积，修改器对累积的几何进行变换，绘制器触发最终渲染。

#### 5.1.1 术语定义

| 术语 | 包含元素 | 说明 |
|------|----------|------|
| **几何元素** | Rectangle、Ellipse、Polystar、Path、TextSpan | 提供几何形状的元素，在上下文中累积为几何列表 |
| **修改器** | TrimPath、RoundCorner、MergePath、TextModifier、TextPath、TextLayout、Repeater | 对累积的几何进行变换 |
| **绘制器** | Fill、Stroke | 对累积的几何进行填充或描边渲染 |
| **容器** | Group | 创建独立作用域并应用矩阵变换，处理完成后合并 |

#### 5.1.2 几何元素的内部结构

几何元素在上下文中累积时，内部结构有所不同：

| 元素类型 | 内部结构 | 说明 |
|----------|----------|------|
| 形状元素（Rectangle、Ellipse、Polystar、Path） | 单个 Path | 每个形状元素产生一个路径 |
| 文本元素（TextSpan） | 字形列表 | 一个 TextSpan 经过塑形后产生多个字形 |

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
│ TextSpan │          │ TextPath │               │
└────┬─────┘          │TextLayout│               │
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
- 每个 TextSpan 贡献一个字形列表（包含多个字形）

#### 5.1.5 修改器的作用范围

不同修改器对几何列表中的元素有不同的作用范围：

| 修改器类型 | 作用对象 | 说明 |
|------------|----------|------|
| 形状修改器（TrimPath、RoundCorner、MergePath） | 仅 Path | 对文本触发强制转换 |
| 文本修改器（TextModifier、TextPath、TextLayout） | 仅字形列表 | 对 Path 无效 |
| 复制器（Repeater） | Path + 字形列表 | 同时作用于所有几何 |

### 5.2 Geometry Elements（几何元素）

几何元素提供可渲染的形状。

#### 5.2.1 Rectangle（矩形）

矩形从中心点定义，支持统一圆角。

```xml
<Rectangle center="100,100" size="200,150" roundness="10" reversed="false"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `center` | point | 0,0 | 中心点 |
| `size` | size | 100,100 | 尺寸 "width,height" |
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

**路径起点**：矩形路径从**右上角**开始，顺时针方向绘制（`reversed="false"` 时）。

#### 5.2.2 Ellipse（椭圆）

椭圆从中心点定义。

```xml
<Ellipse center="100,100" size="100,60" reversed="false"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `center` | point | 0,0 | 中心点 |
| `size` | size | 100,100 | 尺寸 "width,height" |
| `reversed` | bool | false | 反转路径方向 |

**计算规则**：
```
boundingRect.left   = center.x - size.width / 2
boundingRect.top    = center.y - size.height / 2
boundingRect.right  = center.x + size.width / 2
boundingRect.bottom = center.y + size.height / 2
```

**路径起点**：椭圆路径从**右侧中点**（3 点钟方向）开始。

#### 5.2.3 Polystar（多边形/星形）

支持正多边形和星形两种模式。

```xml
<Polystar center="100,100" polystarType="star" pointCount="5" outerRadius="100" innerRadius="50" rotation="0" outerRoundness="0" innerRoundness="0" reversed="false"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `center` | point | 0,0 | 中心点 |
| `polystarType` | PolystarType | star | 类型（见下方） |
| `pointCount` | float | 5 | 顶点数（支持小数） |
| `outerRadius` | float | 100 | 外半径 |
| `innerRadius` | float | 50 | 内半径（仅星形） |
| `rotation` | float | 0 | 旋转角度 |
| `outerRoundness` | float | 0 | 外角圆度 |
| `innerRoundness` | float | 0 | 内角圆度 |
| `reversed` | bool | false | 反转路径方向 |

**PolystarType（类型）**：

| 值 | 说明 |
|------|------|
| `polygon` | 正多边形：只使用外半径 |
| `star` | 星形：使用外半径和内半径交替 |

**多边形模式** (`type="polygon"`)：
- 只使用 `outerRadius` 和 `outerRoundness`
- `innerRadius` 和 `innerRoundness` 被忽略

**星形模式** (`type="star"`)：
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

#### 5.2.4 Path（路径）

使用 SVG 路径语法定义任意形状，支持内联数据或引用 Resources 中定义的 PathData。

```xml
<!-- 内联路径数据 -->
<Path data="M 0 0 L 100 0 L 100 100 Z" reversed="false"/>

<!-- 引用 PathData 资源 -->
<Path data="#curvePath" reversed="false"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `data` | string/idref | (必填) | SVG 路径数据或 PathData 资源引用 "#id"（语法见 2.9 节） |
| `reversed` | bool | false | 反转路径方向 |

#### 5.2.5 TextSpan（文本片段）

文本片段提供文本内容的几何形状。一个 TextSpan 经过塑形后会产生**字形列表**（多个字形），而非单一 Path。

```xml
<TextSpan x="100" y="200" font="Arial" fontSize="24" fontWeight="400" fontStyle="normal" tracking="0" baselineShift="0">
  <![CDATA[Hello World]]>
</TextSpan>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `x` | float | 0 | X 位置 |
| `y` | float | 0 | Y 位置 |
| `font` | string | (必填) | 字体族 |
| `fontSize` | float | 12 | 字号 |
| `fontWeight` | int | 400 | 字重（100-900） |
| `fontStyle` | enum | normal | normal 或 italic |
| `tracking` | float | 0 | 字距 |
| `baselineShift` | float | 0 | 基线偏移 |

**处理流程**：
1. 根据 `font`、`fontSize`、`fontWeight`、`fontStyle` 查找字体
2. 应用 `tracking`（字距调整）
3. 将文本塑形（shaping）为字形列表
4. 按 `x`、`y` 位置放置

**字体回退**：当指定字体不可用时，按平台默认字体回退链选择替代字体。

### 5.3 Painters（绘制器）

绘制器（Fill、Stroke）对**当前时刻**累积的所有几何（Path 和字形列表）进行渲染。

#### 5.3.1 Fill（填充）

填充使用指定的颜色源绘制几何的内部区域。

```xml
<!-- 纯色填充 -->
<Fill color="#FF0000" alpha="0.8" blendMode="normal" fillRule="winding" placement="background"/>

<!-- 引用共享颜色源 -->
<Fill color="#grad1"/>

<!-- 内联线性渐变（仅使用一次时推荐） -->
<Fill>
  <LinearGradient startPoint="0,0" endPoint="100,0">
    <ColorStop offset="0" color="#FF0000"/>
    <ColorStop offset="1" color="#0000FF"/>
  </LinearGradient>
</Fill>

<!-- 内联图片图案 -->
<Fill>
  <ImagePattern image="#img1" tileModeX="repeat" tileModeY="repeat"/>
</Fill>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `color` | color/idref | #000000 | 颜色值或颜色源引用，默认黑色 |
| `alpha` | float | 1 | 透明度 0~1 |
| `blendMode` | BlendMode | normal | 混合模式（见 4.1） |
| `fillRule` | FillRule | winding | 填充规则（见下方） |
| `placement` | LayerPlacement | background | 绘制位置（见 5.3.3） |

**FillRule（填充规则）**：

| 值 | 说明 |
|------|------|
| `winding` | 非零环绕规则：根据路径方向计数，非零则填充 |
| `evenOdd` | 奇偶规则：根据交叉次数，奇数则填充 |

**文本填充**：
- 文本以字形（glyph）为单位填充
- 支持通过 TextModifier 对单个字形应用颜色覆盖
- 颜色覆盖采用 alpha 混合：`finalColor = lerp(originalColor, overrideColor, overrideAlpha)`

#### 5.3.2 Stroke（描边）

描边沿几何边界绘制线条。

```xml
<!-- 基础描边 -->
<Stroke color="#000000" width="2" cap="round" join="miter" miterLimit="4"/>

<!-- 虚线描边 -->
<Stroke color="#0000FF" width="1" dashes="5,3" dashOffset="2"/>

<!-- 内联渐变描边 -->
<Stroke width="3">
  <LinearGradient startPoint="0,0" endPoint="100,0">
    <ColorStop offset="0" color="#FF0000"/>
    <ColorStop offset="1" color="#0000FF"/>
  </LinearGradient>
</Stroke>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `color` | color/idref | #000000 | 颜色值或颜色源引用，默认黑色 |
| `width` | float | 1 | 描边宽度 |
| `alpha` | float | 1 | 透明度 0~1 |
| `blendMode` | BlendMode | normal | 混合模式（见 4.1） |
| `cap` | LineCap | butt | 线帽样式（见下方） |
| `join` | LineJoin | miter | 线连接样式（见下方） |
| `miterLimit` | float | 4 | 斜接限制 |
| `dashes` | string | - | 虚线模式 "d1,d2,..." |
| `dashOffset` | float | 0 | 虚线偏移 |
| `align` | StrokeAlign | center | 描边对齐（见下方） |
| `placement` | LayerPlacement | background | 绘制位置（见 5.3.3） |

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

#### 5.3.3 LayerPlacement（绘制位置）

Fill 和 Stroke 的 `placement` 属性控制相对于子图层的绘制顺序：

| 值 | 说明 |
|------|------|
| `background` | 在子图层**下方**绘制（默认） |
| `foreground` | 在子图层**上方**绘制 |

### 5.4 Shape Modifiers（形状修改器）

形状修改器对累积的 Path 进行**原地变换**，对字形列表则触发强制转换为 Path。

#### 5.4.1 TrimPath（路径裁剪）

裁剪路径到指定的起止范围。

```xml
<TrimPath start="0" end="0.5" offset="0" type="separate"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `start` | float | 0 | 起始位置 0~1 |
| `end` | float | 1 | 结束位置 0~1 |
| `offset` | float | 0 | 偏移量（度） |
| `type` | TrimType | separate | 裁剪类型（见下方） |

**TrimType（裁剪类型）**：

| 值 | 说明 |
|------|------|
| `separate` | 独立模式：每个形状独立裁剪，使用相同的 start/end 参数 |
| `continuous` | 连续模式：所有形状视为一条连续路径，按总长度比例裁剪 |

**边界情况**：
- `start > end`：反向裁剪，路径方向反转
- 支持环绕：当裁剪范围超出 [0,1] 时，自动环绕到路径另一端
- 路径总长度为 0 时，不执行任何操作

**连续模式示例**：
```xml
<Path d="M0,0 L100,0"/>
<Path d="M0,0 L100,0"/>
<TrimPath start="0.25" end="0.75" type="continuous"/>
```

#### 5.4.2 RoundCorner（圆角）

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

#### 5.4.3 MergePath（路径合并）

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
- MergePath 会**清空之前渲染的所有样式**
- 合并时应用各形状的当前变换矩阵
- 合并后的形状变换矩阵重置为单位矩阵

**示例**：
```xml
<Rectangle center="50,50" size="100,100"/>
<Fill color="#FF0000"/>
<Ellipse center="100,100" size="80,80"/>
<MergePath mode="union"/>
<Fill color="#0000FF"/>
```

### 5.5 Text Modifiers（文本修改器）

文本修改器对文本中的独立字形进行变换。

#### 5.5.1 文本修改器处理

遇到文本修改器时，上下文中累积的**所有字形列表**会汇总为一个统一的字形列表进行操作：

```xml
<Group>
  <TextSpan font="Arial" fontSize="24"><![CDATA[Hello]]></TextSpan>
  <TextSpan font="Arial" fontSize="24"><![CDATA[World]]></TextSpan>
  <TextModifier position="0,-10"/>
  <Fill color="#333333"/>
</Group>
```

#### 5.5.2 文本转形状

当文本遇到形状修改器时，会强制转换为形状路径：

```
文本元素              形状修改器              后续修改器
┌──────────┐          ┌──────────┐
│ TextSpan │          │ TrimPath │
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
2. **合并为单个 Path**：一个 TextSpan 的所有字形合并为**一个** Path，而非每个字形产生一个独立 Path
3. **Emoji 丢失**：Emoji 无法转换为路径轮廓，转换时被丢弃
4. **不可逆转换**：转换后成为纯 Path，后续的文本修改器对其无效

**示例**：
```xml
<Group>
  <TextSpan font="Arial" fontSize="24"><![CDATA[Hello 😀]]></TextSpan>
  <TrimPath start="0" end="0.5"/>
  <TextModifier position="0,-10"/>
  <Fill color="#333333"/>
</Group>
```

#### 5.5.3 TextModifier（文本变换器）

对选定范围内的字形应用变换和样式覆盖。

```xml
<TextModifier anchorPoint="0,0" position="0,0" rotation="0" scale="1,1" skew="0" skewAxis="0" alpha="1" fillColor="#FF0000" strokeColor="#000000" strokeWidth="1">
  <RangeSelector start="0" end="1" offset="0" unit="percentage" shape="square" easeIn="0" easeOut="0" mode="add" weight="1" randomizeOrder="false" randomSeed="0"/>
</TextModifier>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `anchorPoint` | point | 0,0 | 锚点偏移 |
| `position` | point | 0,0 | 位置偏移 |
| `rotation` | float | 0 | 旋转 |
| `scale` | point | 1,1 | 缩放 |
| `skew` | float | 0 | 倾斜 |
| `skewAxis` | float | 0 | 倾斜轴 |
| `alpha` | float | 1 | 透明度 |
| `fillColor` | color | - | 填充颜色覆盖 |
| `strokeColor` | color | - | 描边颜色覆盖 |
| `strokeWidth` | float | - | 描边宽度覆盖 |

**选择器计算**：
1. 根据 RangeSelector 的 `start`、`end`、`offset` 计算选择范围（支持任意小数值，超出 [0,1] 范围时自动环绕）
2. 根据 `shape` 计算每个字形的影响因子（0~1）
3. 多个选择器按 `mode` 组合

**变换应用**：

选择器计算出的 `factor` 范围为 [-1, 1]，控制变换属性的应用程度：

```
factor = clamp(selectorFactor × weight, -1, 1)

// 位置和旋转：线性应用 factor
transform = translate(-anchorPoint × factor) 
          × scale(1 + (scale - 1) × factor)  // 缩放从 1 插值到目标值
          × skew(skew × factor, skewAxis)
          × rotate(rotation × factor)
          × translate(anchorPoint × factor)
          × translate(position × factor)

// 透明度：使用 factor 的绝对值
alphaFactor = 1 + (alpha - 1) × |factor|
finalAlpha = originalAlpha × max(0, alphaFactor)
```

**颜色覆盖**：

颜色覆盖使用 `factor` 的绝对值进行 alpha 混合：

```
blendFactor = overrideColor.alpha × |factor|
finalColor = blend(originalColor, overrideColor, blendFactor)
```

#### 5.5.4 RangeSelector（范围选择器）

范围选择器定义 TextModifier 影响的字形范围和影响程度。

```xml
<RangeSelector start="0" end="1" offset="0" unit="percentage" shape="square" easeIn="0" easeOut="0" mode="add" weight="1" randomizeOrder="false" randomSeed="0"/>
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
| `randomizeOrder` | bool | false | 随机顺序 |
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
| `add` | 相加：累加选择器权重 |
| `subtract` | 相减：减去选择器权重 |
| `intersect` | 交集：取最小权重 |
| `min` | 最小：取最小值 |
| `max` | 最大：取最大值 |
| `difference` | 差值：取绝对差值 |

#### 5.5.5 TextPath（文本路径）

将文本沿指定路径排列。

```xml
<TextPath path="#curvePath" align="start" firstMargin="0" lastMargin="0" perpendicularToPath="true" reversed="false" forceAlignment="false"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `path` | idref | (必填) | PathData 资源引用 "#id"（见 2.9 节） |
| `align` | TextPathAlign | start | 对齐模式（见下方） |
| `firstMargin` | float | 0 | 起始边距 |
| `lastMargin` | float | 0 | 结束边距 |
| `perpendicularToPath` | bool | true | 垂直于路径 |
| `reversed` | bool | false | 反转方向 |
| `forceAlignment` | bool | false | 强制对齐 |

**TextPathAlign（对齐模式）**：

| 值 | 说明 |
|------|------|
| `start` | 从路径起点开始排列 |
| `center` | 文本居中于路径 |
| `end` | 文本结束于路径终点 |

**边距**：
- `firstMargin`：起点边距（从路径起点向内偏移）
- `lastMargin`：终点边距（从路径终点向内偏移）

**字形定位**：
1. 计算字形中心在路径上的位置
2. 获取该位置的路径切线方向
3. 如果 `perpendicularToPath="true"`，旋转字形使其垂直于路径

**闭合路径**：对于闭合路径，超出范围的字形会环绕到路径另一端。

**强制对齐**：`forceAlignment="true"` 时，自动调整字间距以填满可用路径长度（减去边距）。

#### 5.5.6 TextLayout（文本排版）

文本排版修改器对累积的文本元素应用段落排版，是 PAGX 格式特有的元素。与 TextPath 类似，TextLayout 作用于累积的字形列表，为其应用自动换行和对齐。

渲染时会由附加的文字排版模块预先排版，重新计算每个字形的位置。转换为 PAG 二进制格式时，TextLayout 会被预排版展开，字形位置直接写入 TextSpan。

```xml
<Group>
  <TextSpan font="Arial" fontSize="16">第一段内容...</TextSpan>
  <TextSpan font="Arial" fontSize="16" fontWeight="700">粗体</TextSpan>
  <TextSpan font="Arial" fontSize="16">普通文本。</TextSpan>
  <TextLayout width="300" height="200"
              align="left"
              verticalAlign="top"
              lineHeight="1.5"
              indent="20"
              overflow="clip"/>
  <Fill color="#333333"/>
</Group>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `width` | float | (必填) | 文本框宽度 |
| `height` | float | (必填) | 文本框高度 |
| `align` | TextAlign | left | 水平对齐（见下方） |
| `verticalAlign` | VerticalAlign | top | 垂直对齐（见下方） |
| `lineHeight` | float | 1.2 | 行高倍数 |
| `indent` | float | 0 | 首行缩进 |
| `overflow` | Overflow | clip | 溢出处理（见下方） |

**TextAlign（水平对齐）**：

| 值 | 说明 |
|------|------|
| `left` | 左对齐 |
| `center` | 居中对齐 |
| `right` | 右对齐 |
| `justify` | 两端对齐 |

**VerticalAlign（垂直对齐）**：

| 值 | 说明 |
|------|------|
| `top` | 顶部对齐 |
| `center` | 垂直居中 |
| `bottom` | 底部对齐 |

**Overflow（溢出处理）**：

| 值 | 说明 |
|------|------|
| `clip` | 裁剪：超出部分不显示 |
| `visible` | 可见：超出部分仍然显示 |
| `ellipsis` | 省略：超出部分显示省略号 |

#### 5.5.7 富文本

富文本通过 Group 内的多个 TextSpan 元素组合，共享 Fill/Stroke 样式。

```xml
<Group>
  <TextSpan x="0" y="24" font="Arial" fontSize="24">Hello </TextSpan>
  <TextSpan x="60" y="24" font="Arial" fontSize="24" fontWeight="700">World</TextSpan>
  <Fill color="#000000"/>
</Group>
```

### 5.6 Repeater（复制器）

复制累积的内容和已渲染的样式，对每个副本应用渐进变换。Repeater 对 Path 和字形列表同时生效，且不会触发文本转形状。

```xml
<Repeater copies="5" offset="1" order="belowOriginal" anchorPoint="0,0" position="50,0" rotation="0" scale="1,1" startAlpha="1" endAlpha="0.2"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `copies` | float | 3 | 副本数 |
| `offset` | float | 0 | 起始偏移 |
| `order` | RepeaterOrder | belowOriginal | 堆叠顺序（见下方） |
| `anchorPoint` | point | 0,0 | 锚点 |
| `position` | point | 100,100 | 每个副本的位置偏移 |
| `rotation` | float | 0 | 每个副本的旋转 |
| `scale` | point | 1,1 | 每个副本的缩放 |
| `startAlpha` | float | 1 | 首个副本透明度 |
| `endAlpha` | float | 1 | 末个副本透明度 |

**变换计算**（第 i 个副本，i 从 0 开始）：
```
progress = i + offset
matrix = translate(-anchorPoint) 
       × scale(scale^progress)      // 指数缩放
       × rotate(rotation × progress) // 线性旋转
       × translate(position × progress) // 线性位移
       × translate(anchorPoint)
```

**透明度插值**：
```
t = progress / copies
alpha = lerp(startAlpha, endAlpha, t)
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

```xml
<Group>
  <TextSpan font="Arial" fontSize="24"><![CDATA[Hi]]></TextSpan>  <!-- 累积字形列表 -->
  <Fill color="#333333"/>            <!-- 渲染填充 -->
  <Repeater copies="3"/> <!-- 复制字形列表和已渲染的填充 -->
  <TextModifier position="0,-5"/>    <!-- 仍可对复制后的字形列表生效 -->
</Group>
```

### 5.7 Group（容器）

Group 是带变换属性的矢量元素容器。

```xml
<Group anchorPoint="50,50" position="100,200" rotation="45" scale="1,1" skew="0" skewAxis="0" alpha="1">
  <!-- 子元素 -->
</Group>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `anchorPoint` | point | 0,0 | 锚点 "x,y" |
| `position` | point | 0,0 | 位置 "x,y" |
| `rotation` | float | 0 | 旋转角度 |
| `scale` | point | 1,1 | 缩放 "sx,sy" |
| `skew` | float | 0 | 倾斜量 |
| `skewAxis` | float | 0 | 倾斜轴角度 |
| `alpha` | float | 1 | 透明度 0~1 |

#### 变换顺序

变换按以下顺序应用（后应用的变换先计算）：

1. 平移到锚点的负方向（`translate(-anchorPoint)`）
2. 缩放（`scale`）
3. 倾斜（`skew` 沿 `skewAxis` 方向）
4. 旋转（`rotation`）
5. 平移到位置（`translate(position)`）

**变换矩阵**：
```
M = translate(position) × rotate(rotation) × skew(skew, skewAxis) × scale(scale) × translate(-anchorPoint)
```

**倾斜变换**：
```
skewMatrix = rotate(skewAxis) × shearX(tan(skew)) × rotate(-skewAxis)
```

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

**示例 1 - 基本隔离**：
```xml
<Group alpha="0.5">
  <Rectangle center="50,50" size="100,100"/>    <!-- 只在组内累积 -->
  <Fill color="#FF0000"/> <!-- 只填充组内的矩形，alpha=0.5 -->
</Group>
<Ellipse center="150,50" size="80,80"/>        <!-- 在组外累积 -->
<Fill color="#0000FF"/>  <!-- 只填充椭圆，alpha=1.0 -->
```

**示例 2 - 子 Group 几何向上累积**：
```xml
<contents>
  <Group>
    <Rectangle center="50,50" size="100,100"/>    <!-- 累积矩形 -->
    <Fill color="#FF0000"/> <!-- 填充矩形 -->
  </Group>
  <Group>
    <Ellipse center="150,50" size="80,80"/>       <!-- 累积椭圆 -->
    <Fill color="#00FF00"/> <!-- 填充椭圆 -->
  </Group>
  <Fill color="#0000FF"/>  <!-- 填充矩形+椭圆（所有子 Group 的几何） -->
</contents>
```

**示例 3 - 多个绘制器复用几何**：
```xml
<Rectangle center="50,50" size="100,100"/>
<Fill color="#FF0000"/>    <!-- 填充矩形 -->
<Stroke color="#000000"/> <!-- 描边同一矩形（几何未清空） -->
```

#### 多重填充与描边

由于绘制器不清空几何列表，同一几何可连续应用多个 Fill 和 Stroke。

**示例 4 - 多重填充**：
```xml
<Rectangle center="100,100" size="200,100" roundness="10"/>
<Fill>
  <ImagePattern image="#checkerboard" tileModeX="repeat" tileModeY="repeat"/>
</Fill>
<Fill color="#FF000080"/>
```

**示例 5 - 多重描边**：
```xml
<Path d="M10,50 Q50,10 90,50 T170,50"/>
<Stroke color="#0088FF40" width="12" cap="round" join="round"/>
<Stroke color="#0088FF80" width="6" cap="round" join="round"/>
<Stroke color="#0088FF" width="2" cap="round" join="round"/>
```

**示例 6 - 混合叠加**：
```xml
<Ellipse center="100,100" size="180,180"/>
<Fill>
  <RadialGradient center="100,100" radius="90">
    <ColorStop offset="0" color="#FFFFFF"/>
    <ColorStop offset="1" color="#3366FF"/>
  </RadialGradient>
</Fill>
<Fill alpha="0.3">
  <ImagePattern image="#noiseTexture" tileModeX="repeat" tileModeY="repeat"/>
</Fill>
<Stroke color="#1a3366" width="3"/>
```

**渲染顺序**：多个绘制器按文档顺序渲染，先出现的位于下方。

---

## 6. Conformance（一致性）

### 6.1 解析规则

- 未知元素跳过不报错（向前兼容）
- 未知属性忽略
- 缺失的可选属性使用默认值
- 无效值尽可能回退到默认值

### 6.2 格式互转

PAGX 与 PAG 二进制格式可双向无损转换。

**PAGX → PAG**：

1. 内嵌外部资源
2. 预排版文本（转换为字形坐标）
3. 静态图层可合并优化
4. 合成内联展开
5. 排版相关文本属性预计算为位置偏移

**PAG → PAGX**：

1. 提取内嵌资源为外部文件
2. 还原图层结构和属性
3. 重建合成引用关系

---

## Appendix A. Enumerations（枚举类型汇总）

本附录汇总规范中所有枚举类型，方便速查。

### A.1 图层相关

| 枚举 | 值 | 定义位置 |
|------|------|----------|
| **BlendMode** | `normal`, `multiply`, `screen`, `overlay`, `darken`, `lighten`, `colorDodge`, `colorBurn`, `hardLight`, `softLight`, `difference`, `exclusion`, `hue`, `saturation`, `color`, `luminosity`, `plusLighter`, `plusDarker` | 4.1 |
| **MaskType** | `alpha`, `luminance`, `contour` | 4.1 |
| **TileMode** | `clamp`, `repeat`, `mirror`, `decal` | 2.12.8 |
| **SamplingMode** | `nearest`, `linear`, `mipmap` | 2.12.8 |

### A.2 绘制器相关

| 枚举 | 值 | 定义位置 |
|------|------|----------|
| **FillRule** | `winding`, `evenOdd` | 5.3.1 |
| **LineCap** | `butt`, `round`, `square` | 5.3.2 |
| **LineJoin** | `miter`, `round`, `bevel` | 5.3.2 |
| **StrokeAlign** | `center`, `inside`, `outside` | 5.3.2 |
| **LayerPlacement** | `background`, `foreground` | 5.3.3 |

### A.3 几何元素相关

| 枚举 | 值 | 定义位置 |
|------|------|----------|
| **PolystarType** | `polygon`, `star` | 5.2.3 |

### A.4 修改器相关

| 枚举 | 值 | 定义位置 |
|------|------|----------|
| **TrimType** | `separate`, `continuous` | 5.4.1 |
| **MergePathOp** | `append`, `union`, `intersect`, `xor`, `difference` | 5.4.3 |
| **SelectorUnit** | `index`, `percentage` | 5.5.4 |
| **SelectorShape** | `square`, `rampUp`, `rampDown`, `triangle`, `round`, `smooth` | 5.5.4 |
| **SelectorMode** | `add`, `subtract`, `intersect`, `min`, `max`, `difference` | 5.5.4 |
| **TextPathAlign** | `start`, `center`, `end` | 5.5.5 |
| **TextAlign** | `left`, `center`, `right`, `justify` | 5.5.6 |
| **VerticalAlign** | `top`, `center`, `bottom` | 5.5.6 |
| **Overflow** | `clip`, `visible`, `ellipsis` | 5.5.6 |
| **RepeaterOrder** | `belowOriginal`, `aboveOriginal` | 5.6 |

---

## Appendix B. Node Reference（节点定义速查）

本附录列出所有节点的属性定义，省略详细说明。

**注意**：`id` 和 `data-*` 属性为通用属性，可用于任意元素（见 2.3 节），各表中不再重复列出。

### B.1 文档结构节点

#### pagx
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `version` | string | (必填) |
| `width` | float | (必填) |
| `height` | float | (必填) |

#### Composition
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `width` | float | (必填) |
| `height` | float | (必填) |

### B.2 资源节点

#### Image
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `source` | string | (必填) |

#### PathData
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `data` | string | (必填) |

#### SolidColor
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `color` | color | (必填) |

#### LinearGradient
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `startPoint` | point | (必填) |
| `endPoint` | point | (必填) |
| `matrix` | string | 单位矩阵 |

#### RadialGradient
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `center` | point | 0,0 |
| `radius` | float | (必填) |
| `matrix` | string | 单位矩阵 |

#### ConicGradient
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `center` | point | 0,0 |
| `startAngle` | float | 0 |
| `endAngle` | float | 360 |
| `matrix` | string | 单位矩阵 |

#### DiamondGradient
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `center` | point | 0,0 |
| `halfDiagonal` | float | (必填) |
| `matrix` | string | 单位矩阵 |

#### ColorStop
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `offset` | float | (必填) |
| `color` | color | (必填) |

#### ImagePattern
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `image` | idref | (必填) |
| `tileModeX` | TileMode | clamp |
| `tileModeY` | TileMode | clamp |
| `sampling` | SamplingMode | linear |
| `matrix` | string | 单位矩阵 |

### B.3 图层节点

#### Layer
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `name` | string | "" |
| `visible` | bool | true |
| `alpha` | float | 1 |
| `blendMode` | BlendMode | normal |
| `x` | float | 0 |
| `y` | float | 0 |
| `matrix` | string | 单位矩阵 |
| `matrix3D` | string | - |
| `preserve3D` | bool | false |
| `antiAlias` | bool | true |
| `groupOpacity` | bool | false |
| `passThroughBackground` | bool | true |
| `excludeChildEffectsInLayerStyle` | bool | false |
| `scrollRect` | string | - |
| `mask` | idref | - |
| `maskType` | MaskType | alpha |
| `composition` | idref | - |

### B.4 图层样式节点

#### DropShadowStyle
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `offsetX` | float | 0 |
| `offsetY` | float | 0 |
| `blurrinessX` | float | 0 |
| `blurrinessY` | float | 0 |
| `color` | color | #000000 |
| `showBehindLayer` | bool | true |
| `blendMode` | BlendMode | normal |

#### InnerShadowStyle
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `offsetX` | float | 0 |
| `offsetY` | float | 0 |
| `blurrinessX` | float | 0 |
| `blurrinessY` | float | 0 |
| `color` | color | #000000 |
| `blendMode` | BlendMode | normal |

#### BackgroundBlurStyle
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `blurrinessX` | float | 0 |
| `blurrinessY` | float | 0 |
| `tileMode` | TileMode | mirror |
| `blendMode` | BlendMode | normal |

### B.5 滤镜节点

#### BlurFilter
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `blurrinessX` | float | (必填) |
| `blurrinessY` | float | (必填) |
| `tileMode` | TileMode | decal |

#### DropShadowFilter
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `offsetX` | float | 0 |
| `offsetY` | float | 0 |
| `blurrinessX` | float | 0 |
| `blurrinessY` | float | 0 |
| `color` | color | #000000 |
| `shadowOnly` | bool | false |

#### InnerShadowFilter
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `offsetX` | float | 0 |
| `offsetY` | float | 0 |
| `blurrinessX` | float | 0 |
| `blurrinessY` | float | 0 |
| `color` | color | #000000 |
| `shadowOnly` | bool | false |

#### BlendFilter
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `color` | color | (必填) |
| `blendMode` | BlendMode | normal |

#### ColorMatrixFilter
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `matrix` | string | (必填) |

### B.6 几何元素节点

#### Rectangle
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `center` | point | 0,0 |
| `size` | size | 100,100 |
| `roundness` | float | 0 |
| `reversed` | bool | false |

#### Ellipse
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `center` | point | 0,0 |
| `size` | size | 100,100 |
| `reversed` | bool | false |

#### Polystar
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `center` | point | 0,0 |
| `polystarType` | PolystarType | star |
| `pointCount` | float | 5 |
| `outerRadius` | float | 100 |
| `innerRadius` | float | 50 |
| `rotation` | float | 0 |
| `outerRoundness` | float | 0 |
| `innerRoundness` | float | 0 |
| `reversed` | bool | false |

#### Path
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `data` | string/idref | (必填) |
| `reversed` | bool | false |

#### TextSpan
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `x` | float | 0 |
| `y` | float | 0 |
| `font` | string | (必填) |
| `fontSize` | float | 12 |
| `fontWeight` | int | 400 |
| `fontStyle` | enum | normal |
| `tracking` | float | 0 |
| `baselineShift` | float | 0 |

### B.7 绘制器节点

#### Fill
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `color` | color/idref | #000000 |
| `alpha` | float | 1 |
| `blendMode` | BlendMode | normal |
| `fillRule` | FillRule | winding |
| `placement` | LayerPlacement | background |

#### Stroke
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `color` | color/idref | #000000 |
| `width` | float | 1 |
| `alpha` | float | 1 |
| `blendMode` | BlendMode | normal |
| `cap` | LineCap | butt |
| `join` | LineJoin | miter |
| `miterLimit` | float | 4 |
| `dashes` | string | - |
| `dashOffset` | float | 0 |
| `align` | StrokeAlign | center |
| `placement` | LayerPlacement | background |

### B.8 形状修改器节点

#### TrimPath
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `start` | float | 0 |
| `end` | float | 1 |
| `offset` | float | 0 |
| `type` | TrimType | separate |

#### RoundCorner
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `radius` | float | 10 |

#### MergePath
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `mode` | MergePathOp | append |

### B.9 文本修改器节点

#### TextModifier
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `anchorPoint` | point | 0,0 |
| `position` | point | 0,0 |
| `rotation` | float | 0 |
| `scale` | point | 1,1 |
| `skew` | float | 0 |
| `skewAxis` | float | 0 |
| `alpha` | float | 1 |
| `fillColor` | color | - |
| `strokeColor` | color | - |
| `strokeWidth` | float | - |

#### RangeSelector
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `start` | float | 0 |
| `end` | float | 1 |
| `offset` | float | 0 |
| `unit` | SelectorUnit | percentage |
| `shape` | SelectorShape | square |
| `easeIn` | float | 0 |
| `easeOut` | float | 0 |
| `mode` | SelectorMode | add |
| `weight` | float | 1 |
| `randomizeOrder` | bool | false |
| `randomSeed` | int | 0 |

#### TextPath
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `path` | idref | (必填) |
| `align` | TextPathAlign | start |
| `firstMargin` | float | 0 |
| `lastMargin` | float | 0 |
| `perpendicularToPath` | bool | true |
| `reversed` | bool | false |
| `forceAlignment` | bool | false |

#### TextLayout
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `width` | float | (必填) |
| `height` | float | (必填) |
| `align` | TextAlign | left |
| `verticalAlign` | VerticalAlign | top |
| `lineHeight` | float | 1.2 |
| `indent` | float | 0 |
| `overflow` | Overflow | clip |

### B.10 其他节点

#### Repeater
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `copies` | float | 3 |
| `offset` | float | 0 |
| `order` | RepeaterOrder | belowOriginal |
| `anchorPoint` | point | 0,0 |
| `position` | point | 100,100 |
| `rotation` | float | 0 |
| `scale` | point | 1,1 |
| `startAlpha` | float | 1 |
| `endAlpha` | float | 1 |

#### Group
| 属性 | 类型 | 默认值 |
|------|------|--------|
| `anchorPoint` | point | 0,0 |
| `position` | point | 0,0 |
| `rotation` | float | 0 |
| `scale` | point | 1,1 |
| `skew` | float | 0 |
| `skewAxis` | float | 0 |
| `alpha` | float | 1 |

---

## Appendix C. Node Hierarchy（节点分类与包含关系）

本附录描述节点的分类和嵌套规则。

### C.1 节点分类

| 分类 | 节点 |
|------|------|
| **文档根** | `pagx` |
| **资源** | `Resources`, `Image`, `PathData`, `SolidColor`, `LinearGradient`, `RadialGradient`, `ConicGradient`, `DiamondGradient`, `ColorStop`, `ImagePattern`, `Composition` |
| **图层** | `Layer` |
| **图层样式** | `DropShadowStyle`, `InnerShadowStyle`, `BackgroundBlurStyle` |
| **滤镜** | `BlurFilter`, `DropShadowFilter`, `InnerShadowFilter`, `BlendFilter`, `ColorMatrixFilter` |
| **几何元素** | `Rectangle`, `Ellipse`, `Polystar`, `Path`, `TextSpan` |
| **绘制器** | `Fill`, `Stroke` |
| **形状修改器** | `TrimPath`, `RoundCorner`, `MergePath` |
| **文本修改器** | `TextModifier`, `RangeSelector`, `TextPath`, `TextLayout` |
| **其他** | `Repeater`, `Group` |

### C.2 包含关系

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
│   └── Composition → Layer*
│
└── Layer*
    ├── contents
    │   └── VectorElement*（见下方）
    ├── styles
    │   ├── DropShadowStyle
    │   ├── InnerShadowStyle
    │   └── BackgroundBlurStyle
    ├── filters
    │   ├── BlurFilter
    │   ├── DropShadowFilter
    │   ├── InnerShadowFilter
    │   ├── BlendFilter
    │   └── ColorMatrixFilter
    └── Layer*（子图层）
```

### C.3 VectorElement 包含关系

`contents` 和 `Group` 可包含以下 VectorElement：

```
contents / Group
├── Rectangle
├── Ellipse
├── Polystar
├── Path
├── TextSpan
├── Fill（可内嵌颜色源）
│   └── SolidColor / LinearGradient / RadialGradient / ConicGradient / DiamondGradient / ImagePattern
├── Stroke（可内嵌颜色源）
│   └── SolidColor / LinearGradient / RadialGradient / ConicGradient / DiamondGradient / ImagePattern
├── TrimPath
├── RoundCorner
├── MergePath
├── TextModifier → RangeSelector*
├── TextPath
├── TextLayout
├── Repeater
└── Group*（递归）
```

---

## Appendix D. Examples（示例）

### D.1 Complete Example（完整示例）

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="400" height="300">
  
  <!-- 背景 -->
  <Layer name="Background">
    <contents>
      <Rectangle center="200,150" size="400,300"/>
      <Fill color="#skyGradient"/>
    </contents>
  </Layer>
  
  <!-- 标题：使用 Group 是因为需要整体变换 -->
  <Layer name="Title">
    <contents>
      <Group anchorPoint="100,20" position="200,50">
        <TextSpan x="0" y="32" font="Helvetica" fontSize="32" fontWeight="700">
          <![CDATA[Hello PAGX!]]>
        </TextSpan>
        <Fill color="#333333"/>
        <Stroke color="#FFFFFF" width="2" placement="foreground"/>
      </Group>
    </contents>
    <styles>
      <DropShadowStyle offsetX="2" offsetY="2" blurrinessX="4" blurrinessY="4" color="#00000040"/>
    </styles>
  </Layer>
  
  <!-- 使用合成的星星 -->
  <Layer composition="#star" x="50" y="80"/>
  <Layer composition="#star" x="320" y="100" alpha="0.7"/>
  
  <!-- 遮罩示例 -->
  <Layer id="maskShape" name="Mask" visible="false">
    <contents>
      <Ellipse center="200,200" size="150,150"/>
      <Fill color="#FFFFFF"/>
    </contents>
  </Layer>
  
  <Layer name="MaskedContent" mask="#maskShape" maskType="alpha">
    <contents>
      <Rectangle center="200,200" size="200,200"/>
      <Fill color="#FF6B6B"/>
    </contents>
  </Layer>
  
  <Resources>
    <LinearGradient id="skyGradient" startPoint="0,0" endPoint="0,300">
      <ColorStop offset="0" color="#87CEEB"/>
      <ColorStop offset="1" color="#E0F6FF"/>
    </LinearGradient>
    
    <Composition id="star" width="50" height="50">
      <Layer name="starLayer">
        <contents>
          <Polystar center="25,25" polystarType="star" pointCount="5"
                    outerRadius="25" innerRadius="10"/>
          <Fill color="#FFD700"/>
        </contents>
      </Layer>
    </Composition>
  </Resources>
  
</pagx>
```

### D.2 Feature Examples（功能示例）

以下示例省略外层 `<?xml>`、`<pagx>`、`<Resources>` 等样板代码，仅展示核心片段。

#### D.2.1 多重填充/描边

```xml
<!-- 多重填充：棋盘格图案 + 半透明红色覆盖 -->
<Rectangle center="100,100" size="200,100" roundness="10"/>
<Fill>
  <ImagePattern image="#checkerboard" tileModeX="repeat" tileModeY="repeat"/>
</Fill>
<Fill color="#FF000080"/>

<!-- 多重描边：由粗到细渐变发光效果 -->
<Path data="M10,50 Q50,10 90,50 T170,50"/>
<Stroke color="#0088FF40" width="12" cap="round" join="round"/>
<Stroke color="#0088FF80" width="6" cap="round" join="round"/>
<Stroke color="#0088FF" width="2" cap="round" join="round"/>
```

#### D.2.2 TrimPath 路径裁剪

```xml
<!-- 独立模式：每个形状独立裁剪 50% -->
<Rectangle center="50,50" size="80,80"/>
<Ellipse center="150,50" size="80,80"/>
<TrimPath start="0" end="0.5" type="separate"/>
<Stroke color="#333333" width="3"/>

<!-- 连续模式：所有路径视为整体，显示中间 50% -->
<Path data="M0,100 L100,100"/>
<Path data="M0,120 L100,120"/>
<TrimPath start="0.25" end="0.75" type="continuous"/>
<Stroke color="#FF6600" width="2"/>
```

#### D.2.3 Repeater 阵列效果

```xml
<!-- 放射阵列：12 个圆点围成圆形 -->
<Group>
  <Ellipse center="80,0" size="10,10"/>
  <Fill color="#3366FF"/>
  <Repeater copies="12" position="0,0" rotation="30" anchorPoint="0,0"/>
</Group>

<!-- 渐隐阵列：透明度从 1 渐变到 0.2 -->
<Rectangle center="0,0" size="30,30"/>
<Fill color="#FF3366"/>
<Repeater copies="5" position="40,0" startAlpha="1" endAlpha="0.2"/>
```

#### D.2.4 TextModifier 逐字变换

```xml
<!-- 波浪文字：逐字上下偏移 -->
<TextSpan x="0" y="50" font="Arial" fontSize="32">
  <![CDATA[WAVE TEXT]]>
</TextSpan>
<TextModifier position="0,-20">
  <RangeSelector start="0" end="1" shape="triangle"/>
</TextModifier>
<Fill color="#333333"/>

<!-- 颜色渐变文字 -->
<TextSpan x="0" y="100" font="Arial" fontSize="32">
  <![CDATA[GRADIENT]]>
</TextSpan>
<TextModifier fillColor="#FF0000">
  <RangeSelector start="0" end="1" shape="rampUp"/>
</TextModifier>
<Fill color="#0000FF"/>
```

#### D.2.5 TextLayout 富文本排版

```xml
<Group>
  <TextSpan font="Arial" fontSize="16">This is </TextSpan>
  <TextSpan font="Arial" fontSize="16" fontWeight="700">bold</TextSpan>
  <TextSpan font="Arial" fontSize="16"> and </TextSpan>
  <TextSpan font="Arial" fontSize="16" fontStyle="italic">italic</TextSpan>
  <TextSpan font="Arial" fontSize="16"> text in a paragraph that will automatically wrap to fit the container width.</TextSpan>
  <TextLayout width="200" height="150" align="justify" lineHeight="1.5" indent="20"/>
  <Fill color="#333333"/>
</Group>
```

#### D.2.6 TextPath 沿路径文本

```xml
<!-- 需要在 Resources 中定义: <PathData id="arc" data="M0,100 Q100,0 200,100"/> -->
<TextSpan font="Arial" fontSize="18">
  <![CDATA[Text along a curved path]]>
</TextSpan>
<TextPath path="#arc" align="center"/>
<Fill color="#336699"/>
```
