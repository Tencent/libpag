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
- **附录 B**：常见用法示例
- **附录 C**：节点与属性速查

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

### 2.9 路径数据语法（Path Data Syntax）

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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="400" height="300">
  <Layer name="background">
    <Rectangle center="200,150" size="400,300"/>
    <Fill color="#F0F0F0"/>
  </Layer>
  <Layer name="content">
    <Rectangle center="200,150" size="200,100" roundness="10"/>
    <Fill color="#3366FF"/>
  </Layer>
</pagx>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `version` | string | (必填) | 格式版本 |
| `width` | float | (必填) | 画布宽度 |
| `height` | float | (必填) | 画布高度 |

**图层渲染顺序**：图层按文档顺序依次渲染，文档中靠前的图层先渲染（位于下方），靠后的图层后渲染（位于上方）。

### 3.3 资源区（Resources）

`<Resources>` 定义可复用的资源，包括图片、路径数据、颜色源和合成。资源通过 `id` 属性标识，在文档其他位置通过 `@id` 形式引用。

**元素位置**：Resources 元素可放置在根元素内的任意位置，对位置没有限制。解析器必须支持元素引用在文档后面定义的资源或图层（即前向引用）。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="300" height="200">
  <Layer>
    <Rectangle center="150,100" size="200,120"/>
    <Fill color="@skyGradient"/>
  </Layer>
  <Resources>
    <PathData id="curvePath" data="M 0 0 C 50 0 50 100 100 100"/>
    <SolidColor id="brandRed" color="#FF0000"/>
    <LinearGradient id="skyGradient" startPoint="0,0" endPoint="0,200">
      <ColorStop offset="0" color="#87CEEB"/>
      <ColorStop offset="1" color="#E0F6FF"/>
    </LinearGradient>
  </Resources>
</pagx>
```

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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="100">
  <Layer>
    <Rectangle center="100,50" size="200,100"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="200,0">
        <ColorStop offset="0" color="#FF0000"/>
        <ColorStop offset="1" color="#0000FF"/>
      </LinearGradient>
    </Fill>
  </Layer>
</pagx>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `startPoint` | Point | (必填) | 起点 |
| `endPoint` | Point | (必填) | 终点 |
| `matrix` | Matrix | 单位矩阵 | 变换矩阵 |

**计算**：对于点 P，其颜色由 P 在起点-终点连线上的投影位置决定。

##### 径向渐变（RadialGradient）

径向渐变从中心向外辐射。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer>
    <Rectangle center="100,100" size="200,200"/>
    <Fill>
      <RadialGradient center="100,100" radius="100">
        <ColorStop offset="0" color="#FFFFFF"/>
        <ColorStop offset="1" color="#000000"/>
      </RadialGradient>
    </Fill>
  </Layer>
</pagx>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `center` | Point | 0,0 | 中心点 |
| `radius` | float | (必填) | 渐变半径 |
| `matrix` | Matrix | 单位矩阵 | 变换矩阵 |

**计算**：对于点 P，其颜色由 `distance(P, center) / radius` 决定。

##### 锥形渐变（ConicGradient）

锥形渐变（也称扫描渐变）沿圆周方向插值。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer>
    <Ellipse center="100,100" size="180,180"/>
    <Fill>
      <ConicGradient center="100,100" startAngle="0" endAngle="360">
        <ColorStop offset="0" color="#FF0000"/>
        <ColorStop offset="0.33" color="#00FF00"/>
        <ColorStop offset="0.66" color="#0000FF"/>
        <ColorStop offset="1" color="#FF0000"/>
      </ConicGradient>
    </Fill>
  </Layer>
</pagx>
```

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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer>
    <Rectangle center="100,100" size="180,180"/>
    <Fill>
      <DiamondGradient center="100,100" radius="90">
        <ColorStop offset="0" color="#FFFFFF"/>
        <ColorStop offset="1" color="#000000"/>
      </DiamondGradient>
    </Fill>
  </Layer>
</pagx>
```

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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="400" height="300">
  <!-- clamp 模式：图片边缘颜色延伸 -->
  <Layer name="ClampFill" x="100" y="80">
    <Rectangle center="0,0" size="150,120" roundness="8"/>
    <Fill>
      <ImagePattern image="@logo" tileModeX="clamp" tileModeY="clamp"/>
    </Fill>
  </Layer>
  
  <!-- repeat 模式：图片重复平铺 -->
  <Layer name="RepeatFill" x="300" y="80">
    <Rectangle center="0,0" size="150,120" roundness="8"/>
    <Fill>
      <ImagePattern image="@logo" tileModeX="repeat" tileModeY="repeat" matrix="0.25,0,0,0.25,0,0"/>
    </Fill>
  </Layer>
  
  <!-- mirror 模式：图片镜像平铺 -->
  <Layer name="MirrorFill" x="100" y="220">
    <Rectangle center="0,0" size="150,120" roundness="8"/>
    <Fill>
      <ImagePattern image="@logo" tileModeX="mirror" tileModeY="mirror" matrix="0.25,0,0,0.25,0,0"/>
    </Fill>
  </Layer>
  
  <Resources>
    <Image id="logo" source="pag_logo.png"/>
  </Resources>
</pagx>
```

##### 颜色源坐标系统

除纯色外，所有颜色源（渐变、图片填充）都有坐标系的概念，其坐标系**相对于几何元素的局部坐标系原点**。可通过 `matrix` 属性对颜色源坐标系应用变换。

**变换行为**：

1. **外部变换会同时作用于几何和颜色源**：Group 的变换、Layer 的矩阵等外部变换会整体作用于几何元素及其颜色源，两者一起缩放、旋转、平移。

2. **修改几何属性不影响颜色源**：直接修改几何元素的属性（如 Rectangle 的 width/height、Path 的路径数据）只改变几何内容本身，不会影响颜色源的坐标系。

**示例**：在 100×100 的区域内绘制一个从左到右的线性渐变：

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer>
    <Rectangle center="100,100" size="100,100"/>
    <Fill color="@grad"/>
  </Layer>
  <Resources>
    <LinearGradient id="grad" startPoint="0,0" endPoint="100,0">
      <ColorStop offset="0" color="#FF0000"/>
      <ColorStop offset="1" color="#0000FF"/>
    </LinearGradient>
  </Resources>
</pagx>
```

- 对该图层应用 `scale(2, 2)` 变换：矩形变为 200×200，渐变也随之放大，视觉效果保持一致
- 直接将 Rectangle 的 size 改为 200,200：矩形变为 200×200，但渐变坐标不变，只覆盖矩形的左半部分

#### 3.3.4 合成（Composition）

合成用于内容复用（类似 After Effects 的 Pre-comp）。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="300" height="200">
  <Layer composition="@buttonComp" x="100" y="75"/>
  <Resources>
    <Composition id="buttonComp" width="100" height="50">
      <Layer name="button">
        <Rectangle center="50,25" size="100,50" roundness="10"/>
        <Fill color="#007AFF"/>
      </Layer>
    </Composition>
  </Resources>
</pagx>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `width` | float | (必填) | 合成宽度 |
| `height` | float | (必填) | 合成高度 |

#### 3.3.5 字体（Font）

Font 定义嵌入字体资源，包含子集化的字形数据（矢量轮廓或位图）。PAGX 文件通过嵌入字形数据实现完全自包含，确保跨平台渲染一致性。

```xml
<!-- 嵌入矢量字体 -->
<Font id="myFont" unitsPerEm="1000">
  <Glyph path="M 50 0 L 300 700 L 550 0 Z" advance="600"/>
  <Glyph path="M 100 0 L 100 700 L 400 700 C 550 700 550 400 400 400 Z" advance="550"/>
</Font>

<!-- 嵌入位图字体（Emoji） -->
<Font id="emojiFont" unitsPerEm="136">
  <Glyph image="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUA..." advance="136"/>
  <Glyph image="emoji/heart.png" offset="0,-5" advance="136"/>
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
5. **上层内容**：渲染 `placement="foreground"` 的 Fill 和 Stroke
6. **图层滤镜**：将前面步骤的整体输出作为滤镜链的输入，依次应用所有滤镜

#### 图层内容（Layer Content）

**图层内容**是指图层的下层内容、子图层和上层内容的完整渲染结果。图层样式基于图层内容计算效果。例如，当填充为下层、描边为上层时，描边会绘制在子图层之上，但投影阴影仍然基于包含填充、子图层和描边的完整图层内容计算。

#### 图层轮廓（Layer Contour）

**图层轮廓**用于遮罩和部分图层样式。与正常图层内容相比，图层轮廓有以下区别：

1. **包含 alpha=0 的几何绘制**：填充透明度完全为 0 的几何形状也会加入轮廓
2. **纯色填充和渐变填充**：忽略原始填充，替换为不透明白色绘制
3. **图片填充**：保留原始像素，但将半透明像素转换为完全不透明（完全透明的像素保留）

注意：几何元素必须有绘制器才能参与轮廓，单独的几何元素（Rectangle、Ellipse 等）如果没有对应的 Fill 或 Stroke，则不会参与轮廓计算。

图层轮廓主要用于：

- **图层样式**：部分图层样式需要轮廓作为其中一个输入源
- **遮罩**：`maskType="contour"` 使用遮罩图层的轮廓进行裁剪

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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer name="MyLayer" visible="true" alpha="1" blendMode="normal" x="20" y="20" antiAlias="true">
    <Rectangle center="50,50" size="100,100"/>
    <Fill color="#FF0000"/>
    <DropShadowStyle offsetX="5" offsetY="5" blurX="10" blurY="10" color="#00000080"/>
    <Layer name="Child">
      <Ellipse center="60,60" size="60,60"/>
      <Fill color="#00FF00"/>
    </Layer>
  </Layer>
</pagx>
```

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

### 4.3 图层样式（Layer Styles）

图层样式在图层内容的上方或下方添加视觉效果，不会替换原有内容。

**图层样式的输入源**：

所有图层样式都基于**图层内容**计算效果。在图层样式中，图层内容会自动转换为 **不透明模式**：使用正常的填充方式渲染几何形状，然后将所有半透明像素转换为完全不透明（完全透明的像素保留）。这意味着半透明填充产生的阴影效果与完全不透明填充相同。

部分图层样式还会额外使用**图层轮廓**或**图层背景**作为输入（详见各样式说明）。图层轮廓和图层背景的定义参见 4.1 节。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer x="50" y="50">
    <Rectangle center="50,50" size="100,100"/>
    <Fill color="#FF0000"/>
    <DropShadowStyle offsetX="5" offsetY="5" blurX="10" blurY="10" color="#00000080" showBehindLayer="true"/>
    <InnerShadowStyle offsetX="2" offsetY="2" blurX="5" blurY="5" color="#00000040"/>
  </Layer>
</pagx>
```

**所有 LayerStyle 共有属性**：

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `blendMode` | BlendMode | normal | 混合模式（见 4.1 节） |

#### 4.3.1 投影阴影（DropShadowStyle）

在图层**下方**绘制投影阴影。基于不透明图层内容计算阴影形状。当 `showBehindLayer="false"` 时，额外使用**图层轮廓**作为擦除遮罩挖空被图层遮挡的部分。

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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer x="50" y="50">
    <Rectangle center="50,50" size="100,100"/>
    <Fill color="#FF0000"/>
    <BlurFilter blurX="5" blurY="5"/>
    <DropShadowFilter offsetX="5" offsetY="5" blurX="10" blurY="10" color="#00000080"/>
  </Layer>
</pagx>
```

#### 4.4.1 模糊滤镜（BlurFilter）

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `blurX` | float | (必填) | X 模糊半径 |
| `blurY` | float | (必填) | Y 模糊半径 |
| `tileMode` | TileMode | decal | 平铺模式 |

#### 4.4.2 投影阴影滤镜（DropShadowFilter）

基于滤镜输入生成阴影效果。与 DropShadowStyle 的核心区别：滤镜基于原始渲染内容投影，支持半透明度；而样式基于不透明图层内容投影。此外两者支持的属性功能也有所不同。

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
| `blendMode` | BlendMode | normal | 混合模式（见 4.1 节） |

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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer scrollRect="50,50,100,100">
    <Rectangle center="100,100" size="200,200"/>
    <Fill color="#FF0000"/>
  </Layer>
</pagx>
```

#### 4.5.2 遮罩（Masking）

通过 `mask` 属性引用另一个图层作为遮罩。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer id="maskShape" visible="false">
    <Ellipse center="100,100" size="150,150"/>
    <Fill color="#FFFFFF"/>
  </Layer>
  <Layer mask="@maskShape" maskType="alpha">
    <Rectangle center="100,100" size="200,200"/>
    <Fill color="#FF0000"/>
  </Layer>
</pagx>
```

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
| **修改器** | TrimPath、RoundCorner、MergePath、TextModifier、TextPath、TextLayout、Repeater | 对累积的几何进行变换 |
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
- 每个 Text 贡献一个字形列表（包含多个字形）

#### 5.1.5 修改器的作用范围

不同修改器对几何列表中的元素有不同的作用范围：

| 修改器类型 | 作用对象 | 说明 |
|------------|----------|------|
| 形状修改器（TrimPath、RoundCorner、MergePath） | 仅 Path | 对文本触发强制转换 |
| 文本修改器（TextModifier、TextPath、TextLayout） | 仅字形列表 | 对 Path 无效 |
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
| `outerRoundness` | float | 0 | 外角圆度 |
| `innerRoundness` | float | 0 | 内角圆度 |
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

#### 5.2.5 文本（Text）

文本元素提供文本内容的几何形状。与形状元素产生单一 Path 不同，Text 经过塑形后会产生**字形列表**（多个字形）并累积到渲染上下文的几何列表中，供后续修改器变换或绘制器渲染。

```xml
<Text text="Hello World" position="100,200" fontFamily="Arial" fontStyle="Bold" fontSize="24" letterSpacing="0" baselineShift="0"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `text` | string | "" | 文本内容 |
| `position` | Point | 0,0 | 文本起点位置，y 为基线（可被 TextLayout 覆盖） |
| `fontFamily` | string | 系统默认 | 字体族 |
| `fontStyle` | string | "Regular" | 字体变体（Regular, Bold, Italic, Bold Italic 等） |
| `fontSize` | float | 12 | 字号 |
| `letterSpacing` | float | 0 | 字间距 |
| `baselineShift` | float | 0 | 基线偏移（正值上移，负值下移） |

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

**运行时排版渲染流程**：
1. 根据 `fontFamily` 和 `fontStyle` 查找系统字体，不可用时按运行时配置的回退列表选择替代字体
2. 使用 `text` 属性（或 CDATA 子节点）进行塑形，换行符触发换行（默认 1.2 倍字号行高，可通过 TextLayout 自定义）
3. 应用 `fontSize`、`letterSpacing`、`baselineShift` 等排版参数
4. 构造字形列表累积到渲染上下文

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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="300" height="200">
  <Resources>
    <!-- 嵌入字体：包含 H, e, l, o 四个字形 -->
    <Font id="myFont" unitsPerEm="1000">
      <Glyph path="M 0 0 L 0 700 M 0 350 L 400 350 M 400 0 L 400 700" advance="500"/>
      <Glyph path="M 50 250 C 50 450 350 450 350 250 C 350 50 50 50 50 250 Z" advance="400"/>
      <Glyph path="M 100 0 L 100 700 L 350 700" advance="350"/>
      <Glyph path="M 200 350 C 200 550 0 550 0 350 C 0 150 200 150 200 350 Z" advance="400"/>
    </Font>
  </Resources>

  <!-- 示例 1：基本用法（依次累加 advance） -->
  <Layer>
    <Text fontFamily="Arial" fontSize="24">
      <GlyphRun font="@myFont" fontSize="24" glyphs="1,2,3,3,4" x="20" y="50"/>
    </Text>
    <Fill color="#333333"/>
  </Layer>

  <!-- 示例 2：自定义 X 偏移的单行水平文本 -->
  <Layer>
    <Text fontFamily="Arial" fontSize="24">
      <GlyphRun font="@myFont" fontSize="24" glyphs="1,2,3,3,4" 
                y="100" xOffsets="20,50,75,90,105"/>
    </Text>
    <Fill color="#333333"/>
  </Layer>

  <!-- 示例 3：自由定位的多行文本 -->
  <Layer>
    <Text fontFamily="Arial" fontSize="24">
      <GlyphRun font="@myFont" fontSize="24" glyphs="1,2,3,3,4" 
                positions="20,150;50,150;75,150;20,180;50,180"/>
    </Text>
    <Fill color="#333333"/>
  </Layer>
</pagx>
```

**带变换的预排版示例**（路径文本场景）：

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="300" height="150">
  <Resources>
    <Font id="myFont" unitsPerEm="1000">
      <Glyph path="M 0 0 L 0 700 M 0 350 L 400 350 M 400 0 L 400 700" advance="500"/>
      <Glyph path="M 50 250 C 50 450 350 450 350 250 C 350 50 50 50 50 250 Z" advance="400"/>
      <Glyph path="M 100 0 L 100 700 L 350 700" advance="350"/>
      <Glyph path="M 200 350 C 200 550 0 550 0 350 C 0 150 200 150 200 350 Z" advance="400"/>
    </Font>
  </Resources>

  <Layer>
    <!-- 沿弧线排列的文本：每个字形有不同的位置和旋转 -->
    <Text fontFamily="Arial" fontSize="24">
      <GlyphRun font="@myFont" fontSize="24" glyphs="1,2,3,3,4"
                positions="30,100;70,80;120,70;170,80;210,100"
                rotations="-30,-15,0,15,30"/>
    </Text>
    <Fill color="#3366FF"/>
  </Layer>
</pagx>
```

**带缩放和斜切的示例**：

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="300" height="100">
  <Resources>
    <Font id="myFont" unitsPerEm="1000">
      <Glyph path="M 0 0 L 0 700 M 0 350 L 400 350 M 400 0 L 400 700" advance="500"/>
      <Glyph path="M 50 250 C 50 450 350 450 350 250 C 350 50 50 50 50 250 Z" advance="400"/>
      <Glyph path="M 100 0 L 100 700 L 350 700" advance="350"/>
      <Glyph path="M 200 350 C 200 550 0 550 0 350 C 0 150 200 150 200 350 Z" advance="400"/>
    </Font>
  </Resources>

  <Layer>
    <!-- 带缩放和斜切效果的文本 -->
    <Text fontFamily="Arial" fontSize="24">
      <GlyphRun font="@myFont" fontSize="24" glyphs="1,2,3,3,4"
                y="50" xOffsets="20,55,95,125,160"
                scales="1,1;1.2,1.2;1.5,1.5;1.2,1.2;1,1"
                skews="0,5,10,5,0"/>
    </Text>
    <Fill color="#FF6600"/>
  </Layer>
</pagx>
```

### 5.3 绘制器（Painters）

绘制器（Fill、Stroke）对**当前时刻**累积的所有几何（Path 和字形列表）进行渲染。

#### 5.3.1 填充（Fill）

填充使用指定的颜色源绘制几何的内部区域。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer>
    <!-- 纯色填充 -->
    <Rectangle center="50,50" size="80,80"/>
    <Fill color="#FF0000" alpha="0.8"/>
  </Layer>
  <Layer>
    <!-- 内联线性渐变 -->
    <Rectangle center="150,50" size="80,80"/>
    <Fill>
      <LinearGradient startPoint="110,10" endPoint="190,90">
        <ColorStop offset="0" color="#FF0000"/>
        <ColorStop offset="1" color="#0000FF"/>
      </LinearGradient>
    </Fill>
  </Layer>
  <Layer>
    <!-- 内联径向渐变 -->
    <Ellipse center="100,150" size="160,80"/>
    <Fill>
      <RadialGradient center="100,150" radius="80">
        <ColorStop offset="0" color="#FFFFFF"/>
        <ColorStop offset="1" color="#3366FF"/>
      </RadialGradient>
    </Fill>
  </Layer>
</pagx>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `color` | Color/idref | #000000 | 颜色值或颜色源引用，默认黑色 |
| `alpha` | float | 1 | 透明度 0~1 |
| `blendMode` | BlendMode | normal | 混合模式（见 4.1 节） |
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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer>
    <!-- 基础描边 -->
    <Rectangle center="50,50" size="60,60"/>
    <Stroke color="#000000" width="3" cap="round" join="round"/>
  </Layer>
  <Layer>
    <!-- 虚线描边 -->
    <Rectangle center="150,50" size="60,60"/>
    <Stroke color="#0000FF" width="2" dashes="8,4" dashOffset="0"/>
  </Layer>
  <Layer>
    <!-- 内联渐变描边 -->
    <Path data="M 20,150 Q 100,100 180,150"/>
    <Stroke width="4" cap="round">
      <LinearGradient startPoint="20,150" endPoint="180,150">
        <ColorStop offset="0" color="#FF0000"/>
        <ColorStop offset="1" color="#0000FF"/>
      </LinearGradient>
    </Stroke>
  </Layer>
</pagx>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `color` | Color/idref | #000000 | 颜色值或颜色源引用，默认黑色 |
| `width` | float | 1 | 描边宽度 |
| `alpha` | float | 1 | 透明度 0~1 |
| `blendMode` | BlendMode | normal | 混合模式（见 4.1 节） |
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
- `start > end`：反向裁剪，路径方向反转
- 支持环绕：当裁剪范围超出 [0,1] 时，自动环绕到路径另一端
- 路径总长度为 0 时，不执行任何操作

**连续模式示例**：
```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="100">
  <Layer>
    <Path data="M 20,50 L 100,50"/>
    <Path data="M 100,50 L 180,50"/>
    <TrimPath start="0.25" end="0.75" type="continuous"/>
    <Stroke color="#3366FF" width="4" cap="round"/>
  </Layer>
</pagx>
```

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
- MergePath 会**清空之前渲染的所有样式**
- 合并时应用各形状的当前变换矩阵
- 合并后的形状变换矩阵重置为单位矩阵

**示例**：

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer>
    <Rectangle center="70,70" size="100,100"/>
    <Ellipse center="130,130" size="100,100"/>
    <MergePath mode="union"/>
    <Fill color="#3366FF"/>
  </Layer>
</pagx>
```

### 5.5 文本修改器（Text Modifiers）

文本修改器对文本中的独立字形进行变换。

#### 5.5.1 文本修改器处理

遇到文本修改器时，上下文中累积的**所有字形列表**会汇总为一个统一的字形列表进行操作：

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="300" height="100">
  <Layer>
    <Group>
      <Text text="Hello " fontFamily="Arial" fontSize="24" position="20,50"/>
      <Text text="World" fontFamily="Arial" fontSize="24" position="90,50"/>
      <TextModifier position="0,-5">
        <RangeSelector start="0" end="1" shape="triangle"/>
      </TextModifier>
      <Fill color="#333333"/>
    </Group>
  </Layer>
</pagx>
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
| `skew` | float | 0 | 倾斜 |
| `skewAxis` | float | 0 | 倾斜轴 |
| `alpha` | float | 1 | 透明度 |
| `fillColor` | Color | - | 填充颜色覆盖 |
| `strokeColor` | Color | - | 描边颜色覆盖 |
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
transform = translate(-anchor × factor) 
          × scale(1 + (scale - 1) × factor)  // 缩放从 1 插值到目标值
          × skew(skew × factor, skewAxis)
          × rotate(rotation × factor)
          × translate(anchor × factor)
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
| `add` | 相加：累加选择器权重 |
| `subtract` | 相减：减去选择器权重 |
| `intersect` | 交集：取最小权重 |
| `min` | 最小：取最小值 |
| `max` | 最大：取最大值 |
| `difference` | 差值：取绝对差值 |

#### 5.5.5 文本路径（TextPath）

将文本沿指定路径排列。路径可以通过引用 Resources 中定义的 PathData，也可以内联路径数据。TextPath 使用
基线（由 baselineOrigin 和 baselineAngle 定义的直线）作为文本的参考线：字形从基线上的位置映射到路径曲线上
的对应位置，保持相对间距和偏移。当 forceAlignment 启用时，忽略原始字形位置，将字形均匀分布以填满可用路径长度。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="250" height="150">
  <Layer>
    <Text text="Hello Path" fontFamily="Arial" fontSize="20"/>
    <TextPath path="M 20,100 Q 125,20 230,100"/>
    <Fill color="#3366FF"/>
  </Layer>
</pagx>
```

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

#### 5.5.6 文本排版（TextLayout）

TextLayout 是文本排版修改器，对累积的 Text 元素应用排版，会覆盖 Text 元素的原始位置（类似 TextPath 覆盖位置的行为）。支持两种模式：

- **点文本模式**（无 width）：文本不自动换行，textAlign 控制文本相对于 (x, y) 锚点的对齐
- **段落文本模式**（有 width）：文本在指定宽度内自动换行

渲染时会由附加的文字排版模块预先排版，重新计算每个字形的位置。TextLayout 会被预排版展开，字形位置直接写入 Text。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="300" height="150">
  <!-- 点文本：居中对齐 -->
  <Layer>
    <Text text="Hello World" fontFamily="Arial" fontSize="24"/>
    <TextLayout position="150,40" textAlign="center"/>
    <Fill color="#333333"/>
  </Layer>
  <!-- 段落文本：自动换行 -->
  <Layer>
    <Text text="This is a longer text that demonstrates auto-wrap within a specified width." fontFamily="Arial" fontSize="14"/>
    <TextLayout position="20,70" width="260" textAlign="start" lineHeight="1.5"/>
    <Fill color="#666666"/>
  </Layer>
</pagx>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `position` | Point | 0,0 | 排版原点 |
| `width` | float | auto | 排版宽度（有值则自动换行） |
| `height` | float | auto | 排版高度（有值则启用垂直对齐） |
| `textAlign` | TextAlign | start | 水平对齐（见下方） |
| `verticalAlign` | VerticalAlign | top | 垂直对齐（见下方） |
| `writingMode` | WritingMode | horizontal | 排版方向（见下方） |
| `lineHeight` | float | 1.2 | 行高倍数 |

**TextAlign（水平对齐）**：

| 值 | 说明 |
|------|------|
| `start` | 起始对齐（左对齐，对于 RTL 文本为右对齐） |
| `center` | 居中对齐 |
| `end` | 结束对齐（右对齐，对于 RTL 文本为左对齐） |
| `justify` | 两端对齐（最后一行起始对齐） |

**VerticalAlign（垂直对齐）**：

| 值 | 说明 |
|------|------|
| `top` | 顶部对齐 |
| `center` | 垂直居中 |
| `bottom` | 底部对齐 |

**WritingMode（排版方向）**：

| 值 | 说明 |
|------|------|
| `horizontal` | 横排文本 |
| `vertical` | 竖排文本（列从右到左排列，传统中日文竖排） |

#### 5.5.7 富文本

富文本通过 Group 内的多个 Text 元素组合，每个 Text 可以有独立的 Fill/Stroke 样式。使用 TextLayout 进行统一排版。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="350" height="80">
  <Layer>
    <!-- 常规文本 -->
    <Group>
      <Text text="This is " fontFamily="Arial" fontSize="18"/>
      <Fill color="#333333"/>
    </Group>
    <!-- 加粗红色文本 -->
    <Group>
      <Text text="important" fontFamily="Arial" fontStyle="Bold" fontSize="18"/>
      <Fill color="#FF0000"/>
    </Group>
    <!-- 常规文本 -->
    <Group>
      <Text text=" information." fontFamily="Arial" fontSize="18"/>
      <Fill color="#333333"/>
    </Group>
    <!-- TextLayout 统一排版 -->
    <TextLayout position="20,50" width="310" textAlign="start"/>
  </Layer>
</pagx>
```

**说明**：每个 Group 内的 Text + Fill/Stroke 定义一段样式独立的文本片段，TextLayout 将所有片段作为整体进行排版，实现自动换行和对齐。

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
matrix = translate(-anchor) 
       × scale(scale^progress)      // 指数缩放
       × rotate(rotation × progress) // 线性旋转
       × translate(position × progress) // 线性位移
       × translate(anchor)
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
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="300" height="100">
  <Layer>
    <Group>
      <Rectangle center="30,50" size="40,40"/>
      <Fill color="#3366FF"/>
      <Repeater copies="5" position="50,0" startAlpha="1" endAlpha="0.2"/>
    </Group>
  </Layer>
</pagx>
```

### 5.7 容器（Group）

Group 是带变换属性的矢量元素容器。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer>
    <Group anchor="50,50" position="100,100" rotation="45" scale="1,1" alpha="0.8">
      <Rectangle center="50,50" size="80,80"/>
      <Fill color="#FF6600"/>
    </Group>
  </Layer>
</pagx>
```

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

变换按以下顺序应用（后应用的变换先计算）：

1. 平移到锚点的负方向（`translate(-anchor)`）
2. 缩放（`scale`）
3. 倾斜（`skew` 沿 `skewAxis` 方向）
4. 旋转（`rotation`）
5. 平移到位置（`translate(position)`）

**变换矩阵**：
```
M = translate(position) × rotate(rotation) × skew(skew, skewAxis) × scale(scale) × translate(-anchor)
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
- **Layer 是累积终止点**：几何向上累积直到遇到 Layer 边界，不会跨 Layer 传递

**示例 1 - 基本隔离**：
```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="100">
  <Layer>
    <Group alpha="0.5">
      <Rectangle center="50,50" size="80,80"/>
      <Fill color="#FF0000"/>
    </Group>
    <Ellipse center="150,50" size="80,80"/>
    <Fill color="#0000FF"/>
  </Layer>
</pagx>
```

**示例 2 - 子 Group 几何向上累积**：
```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="100">
  <Layer>
    <Group>
      <Rectangle center="50,50" size="80,80"/>
      <Fill color="#FF0000"/>
    </Group>
    <Group>
      <Ellipse center="150,50" size="80,80"/>
      <Fill color="#00FF00"/>
    </Group>
    <Fill color="#0000FF40"/>
  </Layer>
</pagx>
```

**示例 3 - 多个绘制器复用几何**：
```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="150" height="150">
  <Layer>
    <Rectangle center="75,75" size="100,100"/>
    <Fill color="#FF0000"/>
    <Stroke color="#000000" width="3"/>
  </Layer>
</pagx>
```

#### 多重填充与描边

由于绘制器不清空几何列表，同一几何可连续应用多个 Fill 和 Stroke。

**示例 4 - 多重填充**：
```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer>
    <Rectangle center="100,100" size="160,100" roundness="10"/>
    <Fill>
      <LinearGradient startPoint="20,50" endPoint="180,150">
        <ColorStop offset="0" color="#FFCC00"/>
        <ColorStop offset="1" color="#FF6600"/>
      </LinearGradient>
    </Fill>
    <Fill color="#FF000040"/>
  </Layer>
</pagx>
```

**示例 5 - 多重描边**：
```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="100">
  <Layer>
    <Path data="M 20,50 Q 100,10 180,50"/>
    <Stroke color="#0088FF40" width="12" cap="round" join="round"/>
    <Stroke color="#0088FF80" width="6" cap="round" join="round"/>
    <Stroke color="#0088FF" width="2" cap="round" join="round"/>
  </Layer>
</pagx>
```

**示例 6 - 混合叠加**：
```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer>
    <Ellipse center="100,100" size="160,160"/>
    <Fill>
      <RadialGradient center="100,100" radius="80">
        <ColorStop offset="0" color="#FFFFFF"/>
        <ColorStop offset="1" color="#3366FF"/>
      </RadialGradient>
    </Fill>
    <Stroke color="#1a3366" width="3"/>
  </Layer>
</pagx>
```

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
| **修改器** | `TrimPath`, `RoundCorner`, `MergePath`, `TextModifier`, `RangeSelector`, `TextPath`, `TextLayout`, `Repeater` |
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
├── TextLayout
├── Repeater
└── Group*（递归）
```

---

## 附录 B. 常见用法示例（Examples）

### B.1 完整示例

以下示例涵盖 PAGX 的所有主要节点类型，展示完整的文档结构。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="800" height="520">
  
  <!-- ==================== 图层内容 ==================== -->
  
  <!-- 背景层：深色渐变 -->
  <Layer name="Background">
    <Rectangle center="400,260" size="800,520"/>
    <Fill color="@bgGradient"/>
  </Layer>
  
  <!-- 装饰光晕 -->
  <Layer name="GlowTopLeft" blendMode="screen">
    <Ellipse center="100,80" size="300,300"/>
    <Fill color="@glowPurple"/>
  </Layer>
  <Layer name="GlowBottomRight" blendMode="screen">
    <Ellipse center="700,440" size="400,400"/>
    <Fill color="@glowBlue"/>
  </Layer>
  
  <!-- ===== 标题区 y=60 ===== -->
  <Layer name="Title">
    <Text text="PAGX" fontFamily="Arial" fontStyle="Bold" fontSize="56"/>
    <TextLayout position="400,55" textAlign="center"/>
    <Fill color="@titleGradient"/>
  </Layer>
  <Layer name="Subtitle">
    <Text text="Portable Animated Graphics XML" fontFamily="Arial" fontSize="14"/>
    <TextLayout position="400,95" textAlign="center"/>
    <Fill color="#FFFFFF60"/>
  </Layer>
  
  <!-- ===== 几何图形区 y=150 ===== -->
  <Layer name="ShapeCards" y="150">
    <!-- 卡片1：圆角矩形 -->
    <Group position="100,0">
      <Rectangle center="0,0" size="100,80" roundness="12"/>
      <Fill color="#FFFFFF08"/>
      <Stroke color="#FFFFFF15" width="1"/>
    </Group>
    <Layer x="100" y="0">
      <Rectangle center="0,0" size="50,35" roundness="6"/>
      <Fill color="@coral"/>
      <DropShadowStyle offsetX="0" offsetY="4" blurX="8" blurY="8" color="#E7524080"/>
    </Layer>
    
    <!-- 卡片2：椭圆 -->
    <Group position="230,0">
      <Rectangle center="0,0" size="100,80" roundness="12"/>
      <Fill color="#FFFFFF08"/>
      <Stroke color="#FFFFFF15" width="1"/>
    </Group>
    <Layer x="230" y="0">
      <Ellipse center="0,0" size="50,35"/>
      <Fill color="@purple"/>
      <DropShadowStyle offsetX="0" offsetY="4" blurX="8" blurY="8" color="#A855F780"/>
    </Layer>
    
    <!-- 卡片3：星形 -->
    <Group position="360,0">
      <Rectangle center="0,0" size="100,80" roundness="12"/>
      <Fill color="#FFFFFF08"/>
      <Stroke color="#FFFFFF15" width="1"/>
    </Group>
    <Layer x="360" y="0">
      <Polystar center="0,0" type="star" pointCount="5" outerRadius="22" innerRadius="10" rotation="-90"/>
      <Fill color="@amber"/>
      <DropShadowStyle offsetX="0" offsetY="4" blurX="8" blurY="8" color="#F59E0B80"/>
    </Layer>
    
    <!-- 卡片4：六边形 -->
    <Group position="490,0">
      <Rectangle center="0,0" size="100,80" roundness="12"/>
      <Fill color="#FFFFFF08"/>
      <Stroke color="#FFFFFF15" width="1"/>
    </Group>
    <Layer x="490" y="0">
      <Polystar center="0,0" type="polygon" pointCount="6" outerRadius="24"/>
      <Fill color="@teal"/>
      <DropShadowStyle offsetX="0" offsetY="4" blurX="8" blurY="8" color="#14B8A680"/>
    </Layer>
    
    <!-- 卡片5：自定义路径 -->
    <Group position="620,0">
      <Rectangle center="0,0" size="100,80" roundness="12"/>
      <Fill color="#FFFFFF08"/>
      <Stroke color="#FFFFFF15" width="1"/>
    </Group>
    <Layer x="620" y="0">
      <Path data="M -20 -15 L 0 -25 L 20 -15 L 20 15 L 0 25 L -20 15 Z"/>
      <Fill color="@orange"/>
      <DropShadowStyle offsetX="0" offsetY="4" blurX="8" blurY="8" color="#F9731680"/>
    </Layer>
  </Layer>
  
  <!-- ===== 修改器区 y=270 ===== -->
  <Layer name="Modifiers" y="270">
    <!-- TrimPath 波浪线 -->
    <Group position="120,0">
      <Path data="@wavePath"/>
      <TrimPath start="0" end="0.75"/>
      <Stroke color="@cyan" width="3" cap="round"/>
    </Group>
    
    <!-- RoundCorner -->
    <Group position="280,0">
      <Rectangle center="0,0" size="60,40"/>
      <RoundCorner radius="10"/>
      <Fill color="@emerald"/>
    </Group>
    
    <!-- MergePath -->
    <Group position="420,0">
      <Rectangle center="-10,0" size="35,35"/>
      <Ellipse center="10,0" size="35,35"/>
      <MergePath mode="xor"/>
      <Fill color="@purple"/>
    </Group>
    
    <!-- Repeater 环形 -->
    <Group position="580,0">
      <Ellipse center="25,0" size="10,10"/>
      <Fill color="@cyan"/>
      <Repeater copies="8" rotation="45" anchor="0,0" startAlpha="1" endAlpha="0.15"/>
    </Group>
    
    <!-- 遮罩示例 -->
    <Group position="700,0">
      <Rectangle center="0,0" size="50,50"/>
      <Fill color="@rainbow"/>
    </Group>
  </Layer>
  <Layer id="circleMask" visible="false">
    <Ellipse center="0,0" size="45,45"/>
    <Fill color="#FFFFFF"/>
  </Layer>
  <Layer name="MaskedLayer" x="700" y="270" mask="@circleMask" maskType="alpha">
    <Rectangle center="0,0" size="50,50"/>
    <Fill color="@rainbow"/>
  </Layer>
  
  <!-- ===== 滤镜区 y=360 ===== -->
  <Layer name="FilterCards" y="360">
    <!-- 模糊滤镜 -->
    <Layer x="130" y="0">
      <Rectangle center="0,0" size="80,60" roundness="10"/>
      <Fill color="@emerald"/>
      <BlurFilter blurX="3" blurY="3"/>
    </Layer>
    
    <!-- 投影滤镜 -->
    <Layer x="260" y="0">
      <Rectangle center="0,0" size="80,60" roundness="10"/>
      <Fill color="@cyan"/>
      <DropShadowFilter offsetX="4" offsetY="4" blurX="10" blurY="10" color="#00000080"/>
    </Layer>
    
    <!-- 颜色矩阵（灰度） -->
    <Layer x="390" y="0">
      <Ellipse center="0,0" size="55,55"/>
      <Fill color="@purple"/>
      <ColorMatrixFilter matrix="0.33,0.33,0.33,0,0,0.33,0.33,0.33,0,0,0.33,0.33,0.33,0,0,0,0,0,1,0"/>
    </Layer>
    
    <!-- 混合滤镜 -->
    <Layer x="520" y="0">
      <Rectangle center="0,0" size="80,60" roundness="10"/>
      <Fill color="@coral"/>
      <BlendFilter color="#3B82F660" blendMode="overlay"/>
    </Layer>
    
    <!-- 图片填充 + 投影样式 -->
    <Layer x="670" y="0">
      <Rectangle center="0,0" size="90,60" roundness="8"/>
      <Fill>
        <ImagePattern image="@photo" tileModeX="clamp" tileModeY="clamp"/>
      </Fill>
      <Stroke color="#FFFFFF30" width="1"/>
      <DropShadowStyle offsetX="0" offsetY="6" blurX="12" blurY="12" color="#00000060"/>
    </Layer>
  </Layer>
  
  <!-- ===== 底部区 y=450 ===== -->
  <Layer name="Footer" y="455">
    <!-- 按钮组件 -->
    <Layer x="200" y="0">
      <Rectangle center="0,0" size="120,36" roundness="18"/>
      <Fill color="@buttonGradient"/>
      <InnerShadowStyle offsetX="0" offsetY="1" blurX="2" blurY="2" color="#FFFFFF30"/>
    </Layer>
    <Group position="200,0">
      <Text text="Get Started" fontFamily="Arial" fontStyle="Bold" fontSize="13"/>
      <TextLayout position="0,0" textAlign="center"/>
      <Fill color="#FFFFFF"/>
    </Group>
    
    <!-- 预排版文本 -->
    <Group position="400,0">
      <Text fontFamily="Arial" fontSize="18">
        <GlyphRun font="@iconFont" glyphs="1,2,3" y="0" xOffsets="0,28,56"/>
      </Text>
      <Fill color="#FFFFFF60"/>
    </Group>
    
    <!-- 合成引用 -->
    <Group position="600,0">
      <Rectangle center="0,0" size="100,36" roundness="8"/>
      <Fill color="#FFFFFF10"/>
      <Stroke color="#FFFFFF20" width="1"/>
    </Group>
  </Layer>
  <Layer composition="@badgeComp" x="600" y="455"/>
  
  <!-- ==================== 资源区 ==================== -->
  <Resources>
    <!-- 图片资源（使用项目中真实存在的图片） -->
    <Image id="photo" source="resources/apitest/imageReplacement.png"/>
    <!-- 路径数据 -->
    <PathData id="wavePath" data="M 0 0 Q 30 -20 60 0 T 120 0 T 180 0"/>
    <!-- 嵌入矢量字体（图标） -->
    <Font id="iconFont">
      <Glyph advance="20" path="M 0 -8 L 8 8 L -8 8 Z"/>
      <Glyph advance="20" path="M -8 -8 L 8 -8 L 8 8 L -8 8 Z"/>
      <Glyph advance="24" path="M 0 -10 A 10 10 0 1 1 0 10 A 10 10 0 1 1 0 -10"/>
    </Font>
    <!-- 背景渐变 -->
    <LinearGradient id="bgGradient" startPoint="0,0" endPoint="800,520">
      <ColorStop offset="0" color="#0F0F1A"/>
      <ColorStop offset="0.5" color="#1A1A2E"/>
      <ColorStop offset="1" color="#0D1B2A"/>
    </LinearGradient>
    <!-- 光晕渐变 -->
    <RadialGradient id="glowPurple" center="0,0" radius="150">
      <ColorStop offset="0" color="#A855F720"/>
      <ColorStop offset="1" color="#A855F700"/>
    </RadialGradient>
    <RadialGradient id="glowBlue" center="0,0" radius="200">
      <ColorStop offset="0" color="#3B82F615"/>
      <ColorStop offset="1" color="#3B82F600"/>
    </RadialGradient>
    <!-- 标题渐变 -->
    <LinearGradient id="titleGradient" startPoint="0,0" endPoint="200,0">
      <ColorStop offset="0" color="#FFFFFF"/>
      <ColorStop offset="0.5" color="#A855F7"/>
      <ColorStop offset="1" color="#3B82F6"/>
    </LinearGradient>
    <!-- 按钮渐变 -->
    <LinearGradient id="buttonGradient" startPoint="0,0" endPoint="120,0">
      <ColorStop offset="0" color="#A855F7"/>
      <ColorStop offset="1" color="#3B82F6"/>
    </LinearGradient>
    <!-- 锥形渐变 -->
    <ConicGradient id="rainbow" center="25,25" startAngle="0" endAngle="360">
      <ColorStop offset="0" color="#F43F5E"/>
      <ColorStop offset="0.25" color="#A855F7"/>
      <ColorStop offset="0.5" color="#3B82F6"/>
      <ColorStop offset="0.75" color="#14B8A6"/>
      <ColorStop offset="1" color="#F43F5E"/>
    </ConicGradient>
    <!-- 纯色定义 -->
    <SolidColor id="coral" color="#F43F5E"/>
    <SolidColor id="purple" color="#A855F7"/>
    <SolidColor id="amber" color="#F59E0B"/>
    <SolidColor id="teal" color="#14B8A6"/>
    <SolidColor id="orange" color="#F97316"/>
    <SolidColor id="cyan" color="#06B6D4"/>
    <SolidColor id="emerald" color="#10B981"/>
    <!-- 合成组件 -->
    <Composition id="badgeComp" width="100" height="36">
      <Layer>
        <Text text="v1.0" fontFamily="Arial" fontStyle="Bold" fontSize="12"/>
        <TextLayout position="50,18" textAlign="center"/>
        <Fill color="#FFFFFF80"/>
      </Layer>
    </Composition>
  </Resources>
  
</pagx>
```

**示例说明**：

本示例展示了 PAGX 的完整功能集，采用现代深色主题设计：

| 类别 | 涵盖节点 |
|------|---------|
| **资源** | Image、PathData、Font/Glyph、SolidColor、LinearGradient、RadialGradient、ConicGradient、Composition |
| **几何元素** | Rectangle、Ellipse、Polystar（star/polygon）、Path、Text |
| **绘制器** | Fill（纯色/渐变/图片）、Stroke |
| **图层样式** | DropShadowStyle、InnerShadowStyle |
| **滤镜** | BlurFilter、DropShadowFilter、BlendFilter、ColorMatrixFilter |
| **形状修改器** | TrimPath、RoundCorner、MergePath |
| **文本修改器** | TextModifier/RangeSelector、TextPath、TextLayout、GlyphRun |
| **其他** | Repeater、Group、遮罩（mask/maskType）、合成引用 |

---

## 附录 C. 节点与属性速查（Node and Attribute Reference）

本附录列出所有节点的属性定义，省略详细说明。

**注意**：`id` 和 `data-*` 属性为通用属性，可用于任意元素（见 2.3 节），各表中不再重复列出。

### C.1 文档结构节点

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

### C.2 资源节点

#### Image

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `source` | string | (必填) |

#### PathData

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `data` | string | (必填) |

#### Font

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `unitsPerEm` | int | 1000 |

子元素：`Glyph`*

#### Glyph

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `advance` | float | (必填) |
| `path` | string | - |
| `image` | string | - |
| `offset` | Point | 0,0 |

#### SolidColor

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `color` | Color | (必填) |

#### LinearGradient

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `startPoint` | Point | (必填) |
| `endPoint` | Point | (必填) |
| `matrix` | Matrix | 单位矩阵 |

子元素：`ColorStop`+

#### RadialGradient

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `center` | Point | 0,0 |
| `radius` | float | (必填) |
| `matrix` | Matrix | 单位矩阵 |

子元素：`ColorStop`+

#### ConicGradient

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `center` | Point | 0,0 |
| `startAngle` | float | 0 |
| `endAngle` | float | 360 |
| `matrix` | Matrix | 单位矩阵 |

子元素：`ColorStop`+

#### DiamondGradient

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `center` | Point | 0,0 |
| `radius` | float | (必填) |
| `matrix` | Matrix | 单位矩阵 |

子元素：`ColorStop`+

#### ColorStop

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `offset` | float | (必填) |
| `color` | Color | (必填) |

#### ImagePattern

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `image` | idref | (必填) |
| `tileModeX` | TileMode | clamp |
| `tileModeY` | TileMode | clamp |
| `filterMode` | FilterMode | linear |
| `mipmapMode` | MipmapMode | linear |
| `matrix` | Matrix | 单位矩阵 |

### C.3 图层节点

#### Layer

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `name` | string | "" |
| `visible` | bool | true |
| `alpha` | float | 1 |
| `blendMode` | BlendMode | normal |
| `x` | float | 0 |
| `y` | float | 0 |
| `matrix` | Matrix | 单位矩阵 |
| `matrix3D` | Matrix | - |
| `preserve3D` | bool | false |
| `antiAlias` | bool | true |
| `groupOpacity` | bool | false |
| `passThroughBackground` | bool | true |
| `excludeChildEffectsInLayerStyle` | bool | false |
| `scrollRect` | Rect | - |
| `mask` | idref | - |
| `maskType` | MaskType | alpha |
| `composition` | idref | - |

子元素：`VectorElement`*、`LayerStyle`*、`LayerFilter`*、`Layer`*（按类型自动归类）

### C.4 图层样式节点

#### DropShadowStyle

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `offsetX` | float | 0 |
| `offsetY` | float | 0 |
| `blurX` | float | 0 |
| `blurY` | float | 0 |
| `color` | Color | #000000 |
| `showBehindLayer` | bool | true |
| `blendMode` | BlendMode | normal |

#### InnerShadowStyle

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `offsetX` | float | 0 |
| `offsetY` | float | 0 |
| `blurX` | float | 0 |
| `blurY` | float | 0 |
| `color` | Color | #000000 |
| `blendMode` | BlendMode | normal |

#### BackgroundBlurStyle

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `blurX` | float | 0 |
| `blurY` | float | 0 |
| `tileMode` | TileMode | mirror |
| `blendMode` | BlendMode | normal |

### C.5 滤镜节点

#### BlurFilter

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `blurX` | float | (必填) |
| `blurY` | float | (必填) |
| `tileMode` | TileMode | decal |

#### DropShadowFilter

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `offsetX` | float | 0 |
| `offsetY` | float | 0 |
| `blurX` | float | 0 |
| `blurY` | float | 0 |
| `color` | Color | #000000 |
| `shadowOnly` | bool | false |

#### InnerShadowFilter

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `offsetX` | float | 0 |
| `offsetY` | float | 0 |
| `blurX` | float | 0 |
| `blurY` | float | 0 |
| `color` | Color | #000000 |
| `shadowOnly` | bool | false |

#### BlendFilter

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `color` | Color | (必填) |
| `blendMode` | BlendMode | normal |

#### ColorMatrixFilter

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `matrix` | Matrix | (必填) |

### C.6 几何元素节点

#### Rectangle

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `center` | Point | 0,0 |
| `size` | Size | 100,100 |
| `roundness` | float | 0 |
| `reversed` | bool | false |

#### Ellipse

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `center` | Point | 0,0 |
| `size` | Size | 100,100 |
| `reversed` | bool | false |

#### Polystar

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `center` | Point | 0,0 |
| `type` | PolystarType | star |
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

#### Text

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `text` | string | "" |
| `position` | Point | 0,0 |
| `fontFamily` | string | 系统默认 |
| `fontStyle` | string | "Regular" |
| `fontSize` | float | 12 |
| `letterSpacing` | float | 0 |
| `baselineShift` | float | 0 |

子元素：`CDATA` 文本、`GlyphRun`*

#### GlyphRun

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `font` | idref | (必填) |
| `fontSize` | float | 12 |
| `glyphs` | string | (必填) |
| `x` | float | 0 |
| `y` | float | 0 |
| `xOffsets` | string | - |
| `positions` | string | - |
| `anchors` | string | - |
| `scales` | string | - |
| `rotations` | string | - |
| `skews` | string | - |

### C.7 绘制器节点

#### Fill

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `color` | Color/idref | #000000 |
| `alpha` | float | 1 |
| `blendMode` | BlendMode | normal |
| `fillRule` | FillRule | winding |
| `placement` | LayerPlacement | background |

#### Stroke

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `color` | Color/idref | #000000 |
| `width` | float | 1 |
| `alpha` | float | 1 |
| `blendMode` | BlendMode | normal |
| `cap` | LineCap | butt |
| `join` | LineJoin | miter |
| `miterLimit` | float | 4 |
| `dashes` | string | - |
| `dashOffset` | float | 0 |
| `dashAdaptive` | bool | false |
| `align` | StrokeAlign | center |
| `placement` | LayerPlacement | background |

### C.8 形状修改器节点

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

### C.9 文本修改器节点

#### TextModifier

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `anchor` | Point | 0,0 |
| `position` | Point | 0,0 |
| `rotation` | float | 0 |
| `scale` | Point | 1,1 |
| `skew` | float | 0 |
| `skewAxis` | float | 0 |
| `alpha` | float | 1 |
| `fillColor` | Color | - |
| `strokeColor` | Color | - |
| `strokeWidth` | float | - |

子元素：`RangeSelector`*

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
| `randomOrder` | bool | false |
| `randomSeed` | int | 0 |

#### TextPath

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `path` | string/idref | (必填) |
| `baselineOrigin` | Point | 0,0 |
| `baselineAngle` | float | 0 |
| `firstMargin` | float | 0 |
| `lastMargin` | float | 0 |
| `perpendicular` | bool | true |
| `reversed` | bool | false |
| `forceAlignment` | bool | false |

#### TextLayout

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `position` | Point | 0,0 |
| `width` | float | auto |
| `height` | float | auto |
| `textAlign` | TextAlign | start |
| `verticalAlign` | VerticalAlign | top |
| `writingMode` | WritingMode | horizontal |
| `lineHeight` | float | 1.2 |

### C.10 其他节点

#### Repeater

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `copies` | float | 3 |
| `offset` | float | 0 |
| `order` | RepeaterOrder | belowOriginal |
| `anchor` | Point | 0,0 |
| `position` | Point | 100,100 |
| `rotation` | float | 0 |
| `scale` | Point | 1,1 |
| `startAlpha` | float | 1 |
| `endAlpha` | float | 1 |

#### Group

| 属性 | 类型 | 默认值 |
|------|------|--------|
| `anchor` | Point | 0,0 |
| `position` | Point | 0,0 |
| `rotation` | float | 0 |
| `scale` | Point | 1,1 |
| `skew` | float | 0 |
| `skewAxis` | float | 0 |
| `alpha` | float | 1 |

子元素：`VectorElement`*（递归包含 Group）

### C.11 枚举类型

#### 图层相关

| 枚举 | 值 |
|------|------|
| **BlendMode** | `normal`, `multiply`, `screen`, `overlay`, `darken`, `lighten`, `colorDodge`, `colorBurn`, `hardLight`, `softLight`, `difference`, `exclusion`, `hue`, `saturation`, `color`, `luminosity`, `plusLighter`, `plusDarker` |
| **MaskType** | `alpha`, `luminance`, `contour` |
| **TileMode** | `clamp`, `repeat`, `mirror`, `decal` |
| **FilterMode** | `nearest`, `linear` |
| **MipmapMode** | `none`, `nearest`, `linear` |

#### 绘制器相关

| 枚举 | 值 |
|------|------|
| **FillRule** | `winding`, `evenOdd` |
| **LineCap** | `butt`, `round`, `square` |
| **LineJoin** | `miter`, `round`, `bevel` |
| **StrokeAlign** | `center`, `inside`, `outside` |
| **LayerPlacement** | `background`, `foreground` |

#### 几何元素相关

| 枚举 | 值 |
|------|------|
| **PolystarType** | `polygon`, `star` |

#### 修改器相关

| 枚举 | 值 |
|------|------|
| **TrimType** | `separate`, `continuous` |
| **MergePathOp** | `append`, `union`, `intersect`, `xor`, `difference` |
| **SelectorUnit** | `index`, `percentage` |
| **SelectorShape** | `square`, `rampUp`, `rampDown`, `triangle`, `round`, `smooth` |
| **SelectorMode** | `add`, `subtract`, `intersect`, `min`, `max`, `difference` |
| **TextAlign** | `start`, `center`, `end`, `justify` |
| **VerticalAlign** | `top`, `center`, `bottom` |
| **WritingMode** | `horizontal`, `vertical` |
| **RepeaterOrder** | `belowOriginal`, `aboveOriginal` |
