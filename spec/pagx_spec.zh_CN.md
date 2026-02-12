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
<!-- Demonstrates basic PAGX document structure -->
<pagx version="1.0" width="400" height="400">
  <!-- Main content card with modern gradient -->
  <Layer name="content">
    <Rectangle center="200,200" size="240,240" roundness="40"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="240,240">
        <ColorStop offset="0" color="#6366F1"/>
        <ColorStop offset="1" color="#8B5CF6"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="16" blurX="48" blurY="48" color="#6366F160"/>
  </Layer>
</pagx>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `version` | string | (必填) | 格式版本 |
| `width` | float | (必填) | 画布宽度 |
| `height` | float | (必填) | 画布高度 |

**图层渲染顺序**：图层按文档顺序依次渲染，文档中靠前的图层先渲染（位于下方），靠后的图层后渲染（位于上方）。
> 📄 [示例](samples/3.2_document_structure.pagx) | [预览](https://pag.io/pagx/?file=./samples/3.2_document_structure.pagx)

### 3.3 资源区（Resources）

`<Resources>` 定义可复用的资源，包括图片、路径数据、颜色源和合成。资源通过 `id` 属性标识，在文档其他位置通过 `@id` 形式引用。

**元素位置**：Resources 元素可放置在根元素内的任意位置，对位置没有限制。解析器必须支持元素引用在文档后面定义的资源或图层（即前向引用）。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates resource definitions and references -->
<pagx version="1.0" width="400" height="400">
  <!-- Main shape using gradient resource reference -->
  <Layer>
    <Rectangle center="200,200" size="320,320" roundness="32"/>
    <Fill color="@oceanGradient"/>
    <DropShadowStyle offsetY="12" blurX="40" blurY="40" color="#06B6D450"/>
  </Layer>
  <!-- Shape using solid color resource reference -->
  <Layer>
    <Ellipse center="200,200" size="120,120"/>
    <Fill color="@coral"/>
  </Layer>
  <Resources>
    <SolidColor id="coral" color="#F43F5E"/>
    <LinearGradient id="oceanGradient" startPoint="0,0" endPoint="320,320">
      <ColorStop offset="0" color="#06B6D4"/>
      <ColorStop offset="1" color="#3B82F6"/>
    </LinearGradient>
  </Resources>
</pagx>
```
> 📄 [示例](samples/3.3_resources.pagx) | [预览](https://pag.io/pagx/?file=./samples/3.3_resources.pagx)

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
<!-- Demonstrates linear gradient -->
<pagx version="1.0" width="400" height="400">
  <!-- Linear gradient rectangle -->
  <Layer>
    <Rectangle center="200,200" size="320,320" roundness="32"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="320,320">
        <ColorStop offset="0" color="#6366F1"/>
        <ColorStop offset="0.5" color="#8B5CF6"/>
        <ColorStop offset="1" color="#EC4899"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="12" blurX="40" blurY="40" color="#8B5CF650"/>
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
<!-- Demonstrates radial gradient -->
<pagx version="1.0" width="400" height="400">
  <!-- Radial gradient circle -->
  <Layer>
    <Ellipse center="200,200" size="320,320"/>
    <Fill>
      <RadialGradient center="140,140" radius="260">
        <ColorStop offset="0" color="#FFF"/>
        <ColorStop offset="0.3" color="#06B6D4"/>
        <ColorStop offset="0.7" color="#3B82F6"/>
        <ColorStop offset="1" color="#1E293B"/>
      </RadialGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="32" blurY="32" color="#06B6D450"/>
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
<!-- Demonstrates conic (sweep) gradient -->
<pagx version="1.0" width="400" height="400">
  <!-- Conic gradient circle -->
  <Layer>
    <Ellipse center="200,200" size="320,320"/>
    <Fill>
      <ConicGradient center="200,200">
        <ColorStop offset="0" color="#F43F5E"/>
        <ColorStop offset="0.2" color="#F59E0B"/>
        <ColorStop offset="0.4" color="#10B981"/>
        <ColorStop offset="0.6" color="#06B6D4"/>
        <ColorStop offset="0.8" color="#8B5CF6"/>
        <ColorStop offset="1" color="#F43F5E"/>
      </ConicGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="32" blurY="32" color="#8B5CF650"/>
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
<!-- Demonstrates diamond gradient -->
<pagx version="1.0" width="400" height="400">
  <!-- Diamond gradient shape -->
  <!-- Rectangle size=320x320, half=160. Diamond radius=160 so corners touch rect edges exactly -->
  <Layer>
    <Rectangle center="200,200" size="320,320" roundness="24"/>
    <Fill>
      <DiamondGradient center="200,200" radius="160">
        <ColorStop offset="0" color="#FBBF24"/>
        <ColorStop offset="0.4" color="#F59E0B"/>
        <ColorStop offset="0.7" color="#D97706"/>
        <ColorStop offset="1" color="#1E293B"/>
      </DiamondGradient>
    </Fill>
    <DropShadowStyle offsetY="12" blurX="40" blurY="40" color="#F59E0B50"/>
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
<!-- Demonstrates ImagePattern fill with different tile modes -->
<pagx version="1.0" width="400" height="400">
  
  <!-- Clamp mode: original size, centered in 140x140 rect -->
  <!-- Rectangle center=110,110 size=140,140: left-top=(40,40), right-bottom=(180,180) -->
  <!-- Image 256x256, scale 0.5 = 128x128 -->
  <!-- Center in rect: image left-top = 40 + (140-128)/2 = 46 -->
  <Layer name="ClampFill">
    <Rectangle center="110,110" size="140,140" roundness="24"/>
    <Fill>
      <ImagePattern image="@logo" tileModeX="clamp" tileModeY="clamp" matrix="0.5,0,0,0.5,46,46"/>
    </Fill>
    <Stroke color="#FFF" width="2"/>
  </Layer>
  
  <!-- Repeat mode: tiled pattern -->
  <Layer name="RepeatFill">
    <Rectangle center="290,110" size="140,140" roundness="24"/>
    <Fill>
      <ImagePattern image="@logo" tileModeX="repeat" tileModeY="repeat" matrix="0.24,0,0,0.24,0,0"/>
    </Fill>
    <Stroke color="#FFF" width="2"/>
  </Layer>
  
  <!-- Mirror mode: mirrored tiles -->
  <Layer name="MirrorFill">
    <Rectangle center="110,290" size="140,140" roundness="24"/>
    <Fill>
      <ImagePattern image="@logo" tileModeX="mirror" tileModeY="mirror" matrix="0.24,0,0,0.24,0,0"/>
    </Fill>
    <Stroke color="#FFF" width="2"/>
  </Layer>
  
  <!-- Repeat mode: scaled pattern -->
  <Layer name="RepeatScaled">
    <Rectangle center="290,290" size="140,140" roundness="24"/>
    <Fill>
      <ImagePattern image="@logo" tileModeX="repeat" tileModeY="repeat" matrix="0.35,0,0,0.35,0,0"/>
    </Fill>
    <Stroke color="#FFF" width="2"/>
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
<!-- Demonstrates that color source coordinates are relative to geometry origin -->
<pagx version="1.0" width="400" height="400">
  <!-- Gradient showing coordinate system -->
  <Layer>
    <Rectangle center="200,200" size="300,300" roundness="24"/>
    <Fill color="@grad"/>
    <DropShadowStyle offsetY="12" blurX="40" blurY="40" color="#EC489950"/>
  </Layer>
  <Resources>
    <LinearGradient id="grad" startPoint="0,0" endPoint="300,300">
      <ColorStop offset="0" color="#EC4899"/>
      <ColorStop offset="0.5" color="#8B5CF6"/>
      <ColorStop offset="1" color="#6366F1"/>
    </LinearGradient>
  </Resources>
</pagx>
```

- 对该图层应用 `scale(2, 2)` 变换：矩形变为 200×200，渐变也随之放大，视觉效果保持一致
- 直接将 Rectangle 的 size 改为 200,200：矩形变为 200×200，但渐变坐标不变，只覆盖矩形的左半部分

> 📄 Samples: [LinearGradient](samples/3.3.3_linear_gradient.pagx) | [RadialGradient](samples/3.3.3_radial_gradient.pagx) | [ConicGradient](samples/3.3.3_conic_gradient.pagx) | [DiamondGradient](samples/3.3.3_diamond_gradient.pagx) | [ImagePattern](samples/3.3.3_image_pattern.pagx) | [Coordinates](samples/3.3.3_color_source_coordinates.pagx)
> 🔗 Preview: [Linear](https://pag.io/pagx/?file=./samples/3.3.3_linear_gradient.pagx) | [Radial](https://pag.io/pagx/?file=./samples/3.3.3_radial_gradient.pagx) | [Conic](https://pag.io/pagx/?file=./samples/3.3.3_conic_gradient.pagx) | [Diamond](https://pag.io/pagx/?file=./samples/3.3.3_diamond_gradient.pagx) | [Pattern](https://pag.io/pagx/?file=./samples/3.3.3_image_pattern.pagx) | [Coords](https://pag.io/pagx/?file=./samples/3.3.3_color_source_coordinates.pagx)

#### 3.3.4 合成（Composition）

合成用于内容复用（类似 After Effects 的 Pre-comp）。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates composition (reusable component) -->
<pagx version="1.0" width="400" height="400">
  <!-- Multiple instances of the same composition to demonstrate reusability -->
  <Layer composition="@buttonComp" x="100" y="60"/>
  <Layer composition="@buttonComp" x="100" y="170"/>
  <Layer composition="@buttonComp" x="100" y="280"/>
  <Resources>
    <Composition id="buttonComp" width="200" height="60">
      <Layer name="button">
        <Rectangle center="100,30" size="200,60" roundness="30"/>
        <Fill>
          <LinearGradient startPoint="0,0" endPoint="200,0">
            <ColorStop offset="0" color="#6366F1"/>
            <ColorStop offset="1" color="#8B5CF6"/>
          </LinearGradient>
        </Fill>
        <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#6366F180"/>
      </Layer>
      <Layer name="label">
        <Text text="Button" fontFamily="Arial" fontStyle="Bold" fontSize="22"/>
        <TextLayout position="100,36" textAlign="center"/>
        <Fill color="#FFF"/>
      </Layer>
    </Composition>
  </Resources>
</pagx>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `width` | float | (必填) | 合成宽度 |
| `height` | float | (必填) | 合成高度 |

> 📄 [示例](samples/3.3.4_composition.pagx) | [预览](https://pag.io/pagx/?file=./samples/3.3.4_composition.pagx)

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
<!-- Demonstrates layer properties and nesting -->
<pagx version="1.0" width="400" height="400">
  <!-- Parent layer with nested child -->
  <Layer name="Parent" x="50" y="50">
    <Rectangle center="150,150" size="260,260" roundness="32"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="260,260">
        <ColorStop offset="0" color="#F43F5E"/>
        <ColorStop offset="1" color="#EC4899"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="16" blurX="40" blurY="40" color="#F43F5E60"/>
    <!-- Nested child layer -->
    <Layer name="Child">
      <Ellipse center="150,150" size="120,120"/>
      <Fill color="#FFF"/>
      <DropShadowStyle offsetY="4" blurX="16" blurY="16" color="#00000030"/>
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

> 📄 [示例](samples/4.2_layer.pagx) | [预览](https://pag.io/pagx/?file=./samples/4.2_layer.pagx)

### 4.3 图层样式（Layer Styles）

图层样式在图层内容的上方或下方添加视觉效果，不会替换原有内容。

**图层样式的输入源**：

所有图层样式都基于**图层内容**计算效果。在图层样式中，图层内容会自动转换为 **不透明模式**：使用正常的填充方式渲染几何形状，然后将所有半透明像素转换为完全不透明（完全透明的像素保留）。这意味着半透明填充产生的阴影效果与完全不透明填充相同。

部分图层样式还会额外使用**图层轮廓**或**图层背景**作为输入（详见各样式说明）。图层轮廓和图层背景的定义参见 4.1 节。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates layer styles: DropShadow and InnerShadow -->
<pagx version="1.0" width="400" height="400">
  <!-- Shape with both drop shadow and inner shadow -->
  <Layer x="70" y="70">
    <Rectangle center="130,130" size="260,260" roundness="40"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="260,260">
        <ColorStop offset="0" color="#06B6D4"/>
        <ColorStop offset="1" color="#0891B2"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="16" blurX="48" blurY="48" color="#06B6D480"/>
    <InnerShadowStyle offsetX="8" offsetY="8" blurX="24" blurY="24" color="#00000040"/>
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

> 📄 [示例](samples/4.3_layer_styles.pagx) | [预览](https://pag.io/pagx/?file=./samples/4.3_layer_styles.pagx)

### 4.4 图层滤镜（Layer Filters）

图层滤镜是图层渲染的最后一个环节，所有之前按顺序渲染的结果（包含图层样式）累积起来作为滤镜的输入。滤镜按文档顺序链式应用，每个滤镜的输出作为下一个滤镜的输入。

与图层样式（4.3 节）的关键区别：图层样式在图层内容的上方或下方**独立渲染**视觉效果，而滤镜**修改**图层的整体渲染输出。图层样式先于滤镜应用。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates layer filters: Blur and DropShadow -->
<pagx version="1.0" width="400" height="400">
  <!-- Shape with blur and drop shadow filters -->
  <Layer x="70" y="70">
    <Rectangle center="130,130" size="260,260" roundness="40"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="260,260">
        <ColorStop offset="0" color="#8B5CF6"/>
        <ColorStop offset="1" color="#6366F1"/>
      </LinearGradient>
    </Fill>
    <BlurFilter blurX="4" blurY="4"/>
    <DropShadowFilter offsetX="0" offsetY="16" blurX="48" blurY="48" color="#8B5CF680"/>
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

> 📄 [示例](samples/4.4_layer_filters.pagx) | [预览](https://pag.io/pagx/?file=./samples/4.4_layer_filters.pagx)

### 4.5 裁剪与遮罩（Clipping and Masking）

#### 4.5.1 scrollRect（滚动裁剪）

`scrollRect` 属性定义图层的可视区域，超出该区域的内容会被裁剪。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates scrollRect clipping -->
<pagx version="1.0" width="400" height="400">
  <!-- Clipped content showing scrollRect effect -->
  <Layer x="100" y="100" scrollRect="100,100,200,200">
    <Ellipse center="200,200" size="320,320"/>
    <Fill>
      <RadialGradient center="200,200" radius="160">
        <ColorStop offset="0" color="#F43F5E"/>
        <ColorStop offset="0.6" color="#EC4899"/>
        <ColorStop offset="1" color="#8B5CF6"/>
      </RadialGradient>
    </Fill>
  </Layer>
  <!-- Visible clip region indicator (dashed border) -->
  <Layer>
    <Rectangle center="200,200" size="200,200" roundness="16"/>
    <Stroke color="#94A3B840" width="2" dashes="4,4"/>
  </Layer>
</pagx>
```

> 📄 [示例](samples/4.5.1_scroll_rect.pagx) | [预览](https://pag.io/pagx/?file=./samples/4.5.1_scroll_rect.pagx)

#### 4.5.2 遮罩（Masking）

通过 `mask` 属性引用另一个图层作为遮罩。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates alpha masking -->
<pagx version="1.0" width="400" height="400">
  <!-- Mask shape (invisible but used for masking) -->
  <Layer id="maskShape" visible="false">
    <Polystar center="200,200" type="star" pointCount="5" outerRadius="150" innerRadius="70" rotation="-90"/>
    <Fill color="#FFF"/>
  </Layer>
  <!-- Masked content with gradient - the rectangle will be clipped to star shape -->
  <Layer mask="@maskShape">
    <Rectangle center="200,200" size="360,360"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="360,360">
        <ColorStop offset="0" color="#6366F1"/>
        <ColorStop offset="0.33" color="#8B5CF6"/>
        <ColorStop offset="0.66" color="#EC4899"/>
        <ColorStop offset="1" color="#F43F5E"/>
      </LinearGradient>
    </Fill>
  </Layer>
</pagx>
```

**遮罩规则**：
- 遮罩图层自身不渲染（`visible` 属性被忽略）
- 遮罩图层的变换不影响被遮罩图层

> 📄 [示例](samples/4.5.2_masking.pagx) | [预览](https://pag.io/pagx/?file=./samples/4.5.2_masking.pagx)

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

**示例**:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates Rectangle shape with various roundness values -->
<pagx version="1.0" width="400" height="400">
  <!-- Sharp rectangle -->
  <Layer>
    <Rectangle center="110,110" size="140,140"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="140,140">
        <ColorStop offset="0" color="#F43F5E"/>
        <ColorStop offset="1" color="#E11D48"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#F43F5E50"/>
  </Layer>
  <!-- Medium rounded rectangle -->
  <Layer>
    <Rectangle center="290,110" size="140,140" roundness="24"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="140,140">
        <ColorStop offset="0" color="#8B5CF6"/>
        <ColorStop offset="1" color="#7C3AED"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#8B5CF650"/>
  </Layer>
  <!-- Fully rounded rectangle (pill shape) -->
  <Layer>
    <Rectangle center="110,290" size="140,140" roundness="70"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="140,140">
        <ColorStop offset="0" color="#06B6D4"/>
        <ColorStop offset="1" color="#0891B2"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#06B6D450"/>
  </Layer>
  <!-- Wide rectangle -->
  <Layer>
    <Rectangle center="290,290" size="140,100" roundness="16"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="140,100">
        <ColorStop offset="0" color="#10B981"/>
        <ColorStop offset="1" color="#059669"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#10B98150"/>
  </Layer>
</pagx>
```

> 📄 [示例](samples/5.2.1_rectangle.pagx) | [预览](https://pag.io/pagx/?file=./samples/5.2.1_rectangle.pagx)

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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates Ellipse shape -->
<pagx version="1.0" width="400" height="400">
  <!-- Perfect circle -->
  <Layer>
    <Ellipse center="120,120" size="140,140"/>
    <Fill>
      <RadialGradient center="50,50" radius="100">
        <ColorStop offset="0" color="#FBBF24"/>
        <ColorStop offset="1" color="#F59E0B"/>
      </RadialGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#F59E0B60"/>
  </Layer>
  <!-- Horizontal ellipse -->
  <Layer>
    <Ellipse center="290,110" size="160,100"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="160,100">
        <ColorStop offset="0" color="#EC4899"/>
        <ColorStop offset="1" color="#DB2777"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#EC489950"/>
  </Layer>
  <!-- Vertical ellipse -->
  <Layer>
    <Ellipse center="110,290" size="100,160"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="100,160">
        <ColorStop offset="0" color="#06B6D4"/>
        <ColorStop offset="1" color="#0891B2"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#06B6D450"/>
  </Layer>
  <!-- Large ellipse -->
  <Layer>
    <Ellipse center="290,280" size="150,150"/>
    <Fill>
      <RadialGradient center="55,55" radius="100">
        <ColorStop offset="0" color="#A78BFA"/>
        <ColorStop offset="1" color="#8B5CF6"/>
      </RadialGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#8B5CF650"/>
  </Layer>
</pagx>
```

> 📄 [示例](samples/5.2.2_ellipse.pagx) | [预览](https://pag.io/pagx/?file=./samples/5.2.2_ellipse.pagx)

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

**示例**:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates Polystar shape (star and polygon) -->
<pagx version="1.0" width="400" height="400">
  <!-- 5-pointed star -->
  <Layer>
    <Polystar center="110,110" type="star" pointCount="5" outerRadius="60" innerRadius="28" rotation="-90"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="120,120">
        <ColorStop offset="0" color="#FBBF24"/>
        <ColorStop offset="1" color="#F59E0B"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#F59E0B60"/>
  </Layer>
  <!-- 6-pointed star -->
  <Layer>
    <Polystar center="290,110" type="star" pointCount="6" outerRadius="60" innerRadius="32" rotation="-90"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="120,120">
        <ColorStop offset="0" color="#F43F5E"/>
        <ColorStop offset="1" color="#E11D48"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#F43F5E50"/>
  </Layer>
  <!-- Hexagon (polygon) -->
  <Layer>
    <Polystar center="110,290" type="polygon" pointCount="6" outerRadius="64"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="128,128">
        <ColorStop offset="0" color="#06B6D4"/>
        <ColorStop offset="1" color="#0891B2"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#06B6D450"/>
  </Layer>
  <!-- Pentagon (polygon) -->
  <Layer>
    <Polystar center="290,290" type="polygon" pointCount="5" outerRadius="64" rotation="-90"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="128,128">
        <ColorStop offset="0" color="#8B5CF6"/>
        <ColorStop offset="1" color="#7C3AED"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#8B5CF650"/>
  </Layer>
</pagx>
```

> 📄 [示例](samples/5.2.3_polystar.pagx) | [预览](https://pag.io/pagx/?file=./samples/5.2.3_polystar.pagx)

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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates Path shape with custom bezier curves -->
<pagx version="1.0" width="400" height="400">
  
  <!-- Heart shape (top-left quadrant: 0-200 x 0-200, center at 100,100) -->
  <Layer>
    <Path data="M 100,80 C 100,60 120,50 140,50 C 160,50 170,60 170,80 C 170,100 140,140 100,170 C 60,140 30,100 30,80 C 30,60 40,50 60,50 C 80,50 100,60 100,80 Z"/>
    <Fill>
      <LinearGradient startPoint="30,50" endPoint="170,170">
        <ColorStop offset="0" color="#F43F5E"/>
        <ColorStop offset="1" color="#EC4899"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="4" blurX="16" blurY="16" color="#F43F5E60"/>
  </Layer>
  
  <!-- Lightning bolt (top-right quadrant: 200-400 x 0-200, center at 300,100) -->
  <Layer>
    <Path data="M 310,45 L 275,110 L 305,110 L 270,195 L 340,100 L 310,100 L 340,45 Z"/>
    <Fill>
      <LinearGradient startPoint="270,45" endPoint="340,195">
        <ColorStop offset="0" color="#FBBF24"/>
        <ColorStop offset="1" color="#F59E0B"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#F59E0B60"/>
  </Layer>
  
  <!-- Arrow (bottom-left quadrant: 0-200 x 200-400, center at 100,300) -->
  <Layer>
    <Path data="M 30,290 L 130,290 L 130,260 L 180,300 L 130,340 L 130,310 L 30,310 Z"/>
    <Fill color="#06B6D4"/>
    <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#06B6D460"/>
  </Layer>
  
  <!-- Star (bottom-right quadrant: 200-400 x 200-400, center at 300,300) -->
  <Layer>
    <Path data="M 300,220 L 318,270 L 370,270 L 328,300 L 343,350 L 300,322 L 257,350 L 272,300 L 230,270 L 282,270 Z"/>
    <Fill color="#8B5CF6"/>
    <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#8B5CF660"/>
  </Layer>
</pagx>
```

> 📄 [示例](samples/5.2.4_path.pagx) | [预览](https://pag.io/pagx/?file=./samples/5.2.4_path.pagx)

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
<pagx version="1.0" width="400" height="400">
  <Layer>
    <Text text="GlyphRun" fontFamily="Arial" fontStyle="Bold" fontSize="72">
      <GlyphRun font="@font1" fontSize="72" glyphs="14,15,16,17,18,19,20,21" x="34" y="145"/>
    </Text>
    <TextLayout position="200,145" textAlign="center"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="280,0">
        <ColorStop offset="0" color="#6366F1"/>
        <ColorStop offset="0.5" color="#8B5CF6"/>
        <ColorStop offset="1" color="#EC4899"/>
      </LinearGradient>
    </Fill>
  </Layer>
  <Layer>
    <Text text="Embedded Font" fontFamily="Arial" fontSize="36">
      <GlyphRun font="@font1" fontSize="36" glyphs="6,7,8,9,10,10,9,10,22,11,12,21,13" y="230" xOffsets="66,87,120,142,162,184,206,226,248,256,276,298,320"/>
    </Text>
    <TextLayout position="200,230" textAlign="center"/>
    <Fill color="#334155"/>
  </Layer>
  <Layer>
    <Text text="Pre-shaped Glyphs" fontFamily="Arial" fontSize="24">
      <GlyphRun font="@font1" fontSize="24" glyphs="1,2,9,3,4,18,5,17,9,10,22,14,15,16,17,18,4" y="310" xOffsets="94.5,109.5,118.5,131.5,139.5,150.5,165.5,179.5,194.5,207.5,222.5,227.5,244.5,251.5,264.5,279.5,294.5"/>
    </Text>
    <TextLayout position="200,310" textAlign="center"/>
    <Fill color="#475569"/>
  </Layer>
  <Resources>
    <Font id="font1">
      <Glyph path="M100 0L193 0L193 -299L314 -299C475 -299 584 -372 584 -531C584 -693 474 -750 310 -750L100 -750L100 0ZM193 -375L193 -674L298 -674C426 -674 492 -639 492 -531C492 -423 431 -375 301 -375L193 -375Z" advance="625"/>
      <Glyph path="M92 0L183 0L183 -337C219 -427 275 -458 320 -458C342 -458 355 -456 372 -450L390 -542C372 -538 355 -542 331 -542C271 -542 215 -497 178 -429L174 -429L167 -542L92 -542L92 0Z" advance="375"/>
      <Glyph path="M46 -250L301 -250L301 -320L46 -320L46 -250Z" advance="333.333"/>
      <Glyph path="M234 0C362 0 432 -72 432 -155C432 -251 344 -281 266 -309C204 -330 148 -348 148 -396C148 -436 181 -469 250 -469C298 -469 336 -451 372 -424L417 -480C375 -513 316 -542 249 -542C131 -542 61 -473 61 -393C61 -307 144 -272 219 -246C280 -225 344 -202 344 -150C344 -106 309 -71 237 -71C172 -71 122 -95 76 -130L31 -74C83 -32 157 0 234 0Z" advance="458.333"/>
      <Glyph path="M217 0C284 0 344 -35 396 -77L400 -77L408 0L482 0L482 -323C482 -453 426 -542 295 -542C208 -542 131 -503 81 -470L117 -410C160 -438 217 -465 280 -465C368 -465 392 -400 392 -333C161 -307 58 -251 58 -136C58 -56 126 0 217 0ZM243 -73C189 -73 146 -96 146 -154C146 -219 209 -261 392 -282L392 -140C339 -96 296 -73 243 -73Z" advance="583.333"/>
      <Glyph path="M100 0L534 0L534 -79L193 -79L193 -338L471 -338L471 -417L193 -417L193 -643L523 -643L523 -722L100 -722L100 0Z" advance="583.333"/>
      <Glyph path="M92 0L183 0L183 -392C233 -448 279 -475 320 -475C389 -475 421 -432 421 -331L421 0L512 0L512 -392C563 -448 607 -475 649 -475C718 -475 750 -432 750 -331L750 0L841 0L841 -343C841 -481 788 -556 676 -556C610 -556 553 -512 497 -451C475 -515 430 -556 347 -556C282 -556 225 -514 178 -462L175 -462L167 -556L92 -556L92 0Z" advance="916.667"/>
      <Glyph path="M331 0C456 0 567 -106 567 -286C567 -447 491 -556 350 -556C290 -556 229 -521 180 -478L183 -576L183 -794L92 -794L92 0L165 0L173 -69L177 -69C224 -26 281 0 331 0ZM316 -76C280 -76 231 -90 183 -131L183 -406C235 -453 283 -478 329 -478C432 -478 472 -400 472 -284C472 -154 406 -76 316 -76Z" advance="611.111"/>
      <Glyph path="M311 0C385 0 443 -25 491 -56L458 -113C418 -88 375 -73 322 -73C219 -73 148 -142 142 -250L508 -250C510 -263 512 -282 512 -302C512 -456 434 -556 296 -556C170 -556 51 -446 51 -271C51 -102 167 0 311 0ZM141 -316C152 -421 220 -482 297 -482C382 -482 432 -424 432 -316L141 -316Z" advance="555.556"/>
      <Glyph path="M277 0C342 0 399 -35 442 -77L445 -77L453 0L527 0L527 -794L437 -794L437 -586L441 -491C393 -531 352 -556 288 -556C164 -556 53 -445 53 -270C53 -102 141 0 277 0ZM297 -76C201 -76 147 -151 147 -278C147 -397 216 -478 304 -478C349 -478 390 -463 437 -423L437 -148C391 -100 347 -76 297 -76Z" advance="611.111"/>
      <Glyph path="M100 0L193 0L193 -333L473 -333L473 -411L193 -411L193 -643L523 -643L523 -722L100 -722L100 0Z" advance="555.556"/>
      <Glyph path="M303 0C436 0 555 -103 555 -276C555 -451 436 -556 303 -556C170 -556 51 -451 51 -276C51 -103 170 0 303 0ZM303 -76C209 -76 146 -156 146 -276C146 -397 209 -479 303 -479C397 -479 461 -397 461 -276C461 -156 397 -76 303 -76Z" advance="611.111"/>
      <Glyph path="M263 0C296 0 332 -10 363 -20L345 -88C327 -81 302 -74 283 -74C220 -74 199 -112 199 -179L199 -481L346 -481L346 -556L199 -556L199 -708L123 -708L112 -556L27 -550L27 -481L108 -481L108 -181C108 -73 147 0 263 0Z" advance="388.889"/>
      <Glyph path="M388 14C487 14 568 -22 615 -71L615 -382L375 -382L375 -306L530 -306L530 -111C501 -83 450 -67 398 -67C240 -67 153 -185 153 -372C153 -555 249 -669 396 -669C471 -669 518 -638 555 -599L605 -659C563 -704 496 -750 394 -750C200 -750 58 -606 58 -368C58 -128 196 14 388 14Z" advance="694.444"/>
      <Glyph path="M188 14C213 14 228 11 241 6L228 -64C218 -62 214 -62 209 -62C195 -62 183 -73 183 -101L183 -795L92 -795L92 -107C92 -30 120 14 188 14Z" advance="291.667"/>
      <Glyph path="M101 236C209 236 266 154 303 47L508 -542L419 -542L322 -239C307 -191 291 -136 276 -87L271 -87C254 -136 235 -192 219 -239L108 -542L13 -542L231 3L219 44C197 111 158 161 96 161C82 161 66 156 55 152L37 225C54 232 75 236 101 236Z" advance="527.778"/>
      <Glyph path="M92 222L183 222L183 45L181 -49C230 -9 282 14 331 14C456 14 567 -93 567 -279C567 -446 491 -556 351 -556C288 -556 227 -520 178 -479L175 -479L167 -542L92 -542L92 222ZM316 -62C280 -62 232 -77 183 -119L183 -403C236 -452 283 -479 329 -479C432 -479 472 -398 472 -278C472 -143 406 -62 316 -62Z" advance="625"/>
      <Glyph path="M92 0L183 0L183 -393C238 -447 276 -475 332 -475C404 -475 435 -433 435 -331L435 0L526 0L526 -343C526 -481 474 -556 360 -556C286 -556 230 -515 180 -464L183 -576L183 -794L92 -794L92 0Z" advance="611.111"/>
      <Glyph path="M100 0L193 0L193 -314L325 -314L503 0L608 0L420 -324C520 -348 586 -416 586 -529C586 -682 479 -736 330 -736L100 -736L100 0ZM193 -389L193 -660L316 -660C431 -660 494 -626 494 -529C494 -435 431 -389 316 -389L193 -389Z" advance="638.889"/>
      <Glyph path="M250 14C325 14 379 -25 430 -84L433 -84L440 0L516 0L516 -542L425 -542L425 -157C373 -92 334 -65 278 -65C206 -65 176 -108 176 -209L176 -542L85 -542L85 -198C85 -60 136 14 250 14Z" advance="611.111"/>
      <Glyph path="M92 0L183 0L183 -393C238 -447 276 -475 332 -475C404 -475 435 -433 435 -331L435 0L526 0L526 -343C526 -481 474 -556 360 -556C286 -556 230 -515 178 -464L175 -464L167 -542L92 -542L92 0Z" advance="611.111"/>
      <Glyph advance="208.333"/>
    </Font>
  </Resources>
</pagx>
```

> 📄 Samples: [文本排版](samples/5.2.5_text.pagx) | [嵌入字体与 GlyphRun](samples/5.2.5_glyph_run.pagx)
> 🔗 Preview: [Text](https://pag.io/pagx/?file=./samples/5.2.5_text.pagx) | [GlyphRun](https://pag.io/pagx/?file=./samples/5.2.5_glyph_run.pagx)

### 5.3 绘制器（Painters）

绘制器（Fill、Stroke）对**当前时刻**累积的所有几何（Path 和字形列表）进行渲染。

#### 5.3.1 填充（Fill）

填充使用指定的颜色源绘制几何的内部区域。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates different fill types -->
<pagx version="1.0" width="400" height="400">
  <!-- Solid color fill -->
  <Layer>
    <Rectangle center="110,110" size="140,140" roundness="24"/>
    <Fill color="#F43F5E"/>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#F43F5E50"/>
  </Layer>
  <!-- Linear gradient fill -->
  <Layer>
    <Rectangle center="290,110" size="140,140" roundness="24"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="140,140">
        <ColorStop offset="0" color="#6366F1"/>
        <ColorStop offset="1" color="#8B5CF6"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#8B5CF650"/>
  </Layer>
  <!-- Radial gradient fill -->
  <Layer>
    <Ellipse center="200,300" size="320,140"/>
    <Fill>
      <RadialGradient center="160,70" radius="170">
        <ColorStop offset="0" color="#FFF"/>
        <ColorStop offset="0.5" color="#06B6D4"/>
        <ColorStop offset="1" color="#0891B2"/>
      </RadialGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#06B6D450"/>
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

> 📄 [示例](samples/5.3.1_fill.pagx) | [预览](https://pag.io/pagx/?file=./samples/5.3.1_fill.pagx)

#### 5.3.2 描边（Stroke）

描边沿几何边界绘制线条。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates different stroke styles -->
<pagx version="1.0" width="400" height="400">
  <!-- Basic solid stroke -->
  <Layer>
    <Rectangle center="110,110" size="130,130" roundness="20"/>
    <Stroke color="#06B6D4" width="8" cap="round" join="round"/>
  </Layer>
  <!-- Dashed stroke -->
  <Layer>
    <Rectangle center="290,110" size="130,130" roundness="20"/>
    <Stroke color="#8B5CF6" width="6" dashes="12,8" cap="round"/>
  </Layer>
  <!-- Gradient stroke on curve -->
  <Layer>
    <Path data="M 50,300 Q 200,180 350,300"/>
    <Stroke width="10" cap="round">
      <LinearGradient startPoint="0,0" endPoint="300,0">
        <ColorStop offset="0" color="#F43F5E"/>
        <ColorStop offset="0.5" color="#EC4899"/>
        <ColorStop offset="1" color="#8B5CF6"/>
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

> 📄 [示例](samples/5.3.2_stroke.pagx) | [预览](https://pag.io/pagx/?file=./samples/5.3.2_stroke.pagx)

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

**示例**（separate 与 continuous 模式对比）：
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates TrimPath: separate vs continuous mode comparison -->
<pagx version="1.0" width="400" height="400">
  <!-- === Top row: Separate mode === -->
  <!-- Track -->
  <Layer>
    <Ellipse center="120,95" size="100,100"/>
    <Ellipse center="280,95" size="100,100"/>
    <Stroke color="#E2E8F0" width="8"/>
  </Layer>
  <!-- Trimmed: each ellipse trimmed independently to 0.2~0.9 -->
  <Layer>
    <Ellipse center="120,95" size="100,100"/>
    <Ellipse center="280,95" size="100,100"/>
    <TrimPath start="0.2" end="0.9"/>
    <Stroke width="10" cap="round">
      <LinearGradient startPoint="70,40" endPoint="330,150">
        <ColorStop offset="0" color="#06B6D4"/>
        <ColorStop offset="1" color="#8B5CF6"/>
      </LinearGradient>
    </Stroke>
  </Layer>
  <!-- Label -->
  <Layer>
    <Text text="Separate (0.2–0.9)" fontFamily="Arial" fontStyle="Bold" fontSize="18"/>
    <TextLayout position="200,187" textAlign="center"/>
    <Fill color="#475569"/>
  </Layer>

  <!-- === Bottom row: Continuous mode === -->
  <!-- Track -->
  <Layer>
    <Ellipse center="120,265" size="100,100"/>
    <Ellipse center="280,265" size="100,100"/>
    <Stroke color="#E2E8F0" width="8"/>
  </Layer>
  <!-- Trimmed: both ellipses treated as one path, 0.2~0.9 of total length -->
  <Layer>
    <Ellipse center="120,265" size="100,100"/>
    <Ellipse center="280,265" size="100,100"/>
    <TrimPath start="0.2" end="0.9" type="continuous"/>
    <Stroke width="10" cap="round">
      <LinearGradient startPoint="70,210" endPoint="330,320">
        <ColorStop offset="0" color="#F59E0B"/>
        <ColorStop offset="1" color="#F43F5E"/>
      </LinearGradient>
    </Stroke>
  </Layer>
  <!-- Label -->
  <Layer>
    <Text text="Continuous (0.2–0.9)" fontFamily="Arial" fontStyle="Bold" fontSize="18"/>
    <TextLayout position="200,357" textAlign="center"/>
    <Fill color="#475569"/>
  </Layer>
</pagx>
```

> 📄 [示例](samples/5.4.1_trim_path.pagx) | [预览](https://pag.io/pagx/?file=./samples/5.4.1_trim_path.pagx)

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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates RoundCorner modifier -->
<pagx version="1.0" width="400" height="400">
  <!-- Original sharp rectangle (reference) -->
  <Layer>
    <Rectangle center="120,120" size="140,140"/>
    <Stroke color="#94A3B840" width="2" dashes="4,4"/>
  </Layer>
  <!-- RoundCorner applied to sharp rectangle -->
  <Layer>
    <Rectangle center="120,120" size="140,140"/>
    <RoundCorner radius="30"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="140,140">
        <ColorStop offset="0" color="#10B981"/>
        <ColorStop offset="1" color="#059669"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#10B98160"/>
  </Layer>
  <!-- Original star (reference) -->
  <Layer>
    <Polystar center="290,120" type="star" pointCount="5" outerRadius="70" innerRadius="32" rotation="-90"/>
    <Stroke color="#94A3B840" width="2" dashes="4,4"/>
  </Layer>
  <!-- RoundCorner applied to star -->
  <Layer>
    <Polystar center="290,120" type="star" pointCount="5" outerRadius="70" innerRadius="32" rotation="-90"/>
    <RoundCorner radius="12"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="140,140">
        <ColorStop offset="0" color="#F59E0B"/>
        <ColorStop offset="1" color="#D97706"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#F59E0B60"/>
  </Layer>
  <!-- Large rounded hexagon -->
  <Layer>
    <Polystar center="200,300" type="polygon" pointCount="6" outerRadius="80"/>
    <RoundCorner radius="20"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="160,160">
        <ColorStop offset="0" color="#8B5CF6"/>
        <ColorStop offset="1" color="#7C3AED"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#8B5CF660"/>
  </Layer>
</pagx>
```

> 📄 [示例](samples/5.4.2_round_corner.pagx) | [预览](https://pag.io/pagx/?file=./samples/5.4.2_round_corner.pagx)

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
<!-- Demonstrates MergePath modifier with different modes -->
<pagx version="1.0" width="400" height="400">
  <!-- Union mode: combining shapes -->
  <Layer>
    <Rectangle center="150,150" size="180,180" roundness="24"/>
    <Ellipse center="250,250" size="180,180"/>
    <MergePath mode="union"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="280,280">
        <ColorStop offset="0" color="#6366F1"/>
        <ColorStop offset="0.5" color="#8B5CF6"/>
        <ColorStop offset="1" color="#EC4899"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="12" blurX="40" blurY="40" color="#8B5CF660"/>
  </Layer>
</pagx>
```

> 📄 [示例](samples/5.4.3_merge_path.pagx) | [预览](https://pag.io/pagx/?file=./samples/5.4.3_merge_path.pagx)

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

> 完整可运行示例：[TextModifier 效果](samples/5.5.3_text_modifier.pagx)

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
<!-- Demonstrates TextPath for arranging text along a curve -->
<pagx version="1.0" width="400" height="400">
  <!-- Text along an arc path with force alignment -->
  <Layer>
    <Text text="PAGX" fontFamily="Arial" fontStyle="Bold" fontSize="56"/>
    <TextPath path="M 60,230 Q 200,30 340,230" forceAlignment="true"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="300,0">
        <ColorStop offset="0" color="#06B6D4"/>
        <ColorStop offset="0.5" color="#8B5CF6"/>
        <ColorStop offset="1" color="#EC4899"/>
      </LinearGradient>
    </Fill>
  </Layer>
  <!-- Guide path (dashed) to visualize the curve -->
  <Layer>
    <Path data="M 60,230 Q 200,30 340,230"/>
    <Stroke color="#64748B" width="1" dashes="6,4"/>
  </Layer>
  <!-- Text along a lower curve -->
  <Layer>
    <Text text="TextPath" fontFamily="Arial" fontStyle="Bold" fontSize="28"/>
    <TextPath path="M 80,320 Q 200,250 320,320" forceAlignment="true"/>
    <Fill color="#475569"/>
  </Layer>
  <!-- Guide path for lower curve -->
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

> 📄 [示例](samples/5.5.5_text_path.pagx) | [预览](https://pag.io/pagx/?file=./samples/5.5.5_text_path.pagx)

#### 5.5.6 文本排版（TextLayout）

TextLayout 是文本排版修改器，对累积的 Text 元素应用排版，会覆盖 Text 元素的原始位置（类似 TextPath 覆盖位置的行为）。支持两种模式：

- **点文本模式**（无 width）：文本不自动换行，textAlign 控制文本相对于 (x, y) 锚点的对齐
- **段落文本模式**（有 width）：文本在指定宽度内自动换行

渲染时会由附加的文字排版模块预先排版，重新计算每个字形的位置。TextLayout 会被预排版展开，字形位置直接写入 Text。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates TextLayout for text positioning and alignment -->
<pagx version="1.0" width="400" height="400">
  <!-- Title: point text, center aligned -->
  <Layer>
    <Text text="Text Layout" fontFamily="Arial" fontStyle="Bold" fontSize="40"/>
    <TextLayout position="200,74" textAlign="center"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="280,0">
        <ColorStop offset="0" color="#6366F1"/>
        <ColorStop offset="1" color="#8B5CF6"/>
      </LinearGradient>
    </Fill>
  </Layer>
  <!-- Vertical line to show the anchor at x=200 -->
  <Layer>
    <Path data="M 200,110 L 200,358"/>
    <Stroke color="#64748B" width="1" dashes="6,4"/>
  </Layer>
  <!-- Start aligned: text starts at anchor point -->
  <Layer>
    <Text text="Start" fontFamily="Arial" fontStyle="Bold" fontSize="36"/>
    <TextLayout position="200,139" textAlign="start"/>
    <Fill color="#F43F5E"/>
  </Layer>
  <Layer>
    <Text text="text begins at anchor" fontFamily="Arial" fontSize="18"/>
    <TextLayout position="200,164" textAlign="start"/>
    <Fill color="#475569"/>
  </Layer>
  <!-- Center aligned: text centered on anchor point -->
  <Layer>
    <Text text="Center" fontFamily="Arial" fontStyle="Bold" fontSize="36"/>
    <TextLayout position="200,234" textAlign="center"/>
    <Fill color="#10B981"/>
  </Layer>
  <Layer>
    <Text text="text centered on anchor" fontFamily="Arial" fontSize="18"/>
    <TextLayout position="200,259" textAlign="center"/>
    <Fill color="#475569"/>
  </Layer>
  <!-- End aligned: text ends at anchor point -->
  <Layer>
    <Text text="End" fontFamily="Arial" fontStyle="Bold" fontSize="36"/>
    <TextLayout position="200,329" textAlign="end"/>
    <Fill color="#06B6D4"/>
  </Layer>
  <Layer>
    <Text text="text ends at anchor" fontFamily="Arial" fontSize="18"/>
    <TextLayout position="200,354" textAlign="end"/>
    <Fill color="#475569"/>
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

> 📄 [示例](samples/5.5.6_text_layout.pagx) | [预览](https://pag.io/pagx/?file=./samples/5.5.6_text_layout.pagx)

#### 5.5.7 富文本

富文本通过 Group 内的多个 Text 元素组合，每个 Text 可以有独立的 Fill/Stroke 样式。使用 TextLayout 进行统一排版。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates rich text with mixed styles via shared TextLayout -->
<pagx version="1.0" width="400" height="400">

  <!-- Title -->
  <Layer>
    <Text text="Rich Text" fontFamily="Arial" fontStyle="Bold" fontSize="42"/>
    <TextLayout position="200,105" textAlign="center"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="200,0">
        <ColorStop offset="0" color="#8B5CF6"/>
        <ColorStop offset="1" color="#EC4899"/>
      </LinearGradient>
    </Fill>
  </Layer>

  <!-- Rich text paragraph: multiple Groups with shared TextLayout -->
  <Layer>
    <Group>
      <Text text="Supports " fontFamily="Arial" fontSize="20"/>
      <Fill color="#475569"/>
    </Group>
    <Group>
      <Text text="bold" fontFamily="Arial" fontStyle="Bold" fontSize="20"/>
      <Fill color="#F43F5E"/>
    </Group>
    <Group>
      <Text text=", " fontFamily="Arial" fontSize="20"/>
      <Fill color="#475569"/>
    </Group>
    <Group>
      <Text text="italic" fontFamily="Arial" fontStyle="Italic" fontSize="20"/>
      <Fill color="#10B981"/>
    </Group>
    <Group>
      <Text text=" and " fontFamily="Arial" fontSize="20"/>
      <Fill color="#475569"/>
    </Group>
    <Group>
      <Text text="colored" fontFamily="Arial" fontStyle="Bold" fontSize="20"/>
      <Fill color="#06B6D4"/>
    </Group>
    <Group>
      <Text text=" text." fontFamily="Arial" fontSize="20"/>
      <Fill color="#475569"/>
    </Group>
    <TextLayout position="30,165" textAlign="start"/>
  </Layer>

  <!-- Second paragraph: mixed sizes -->
  <Layer>
    <Group>
      <Text text="Mix " fontFamily="Arial" fontSize="16"/>
      <Fill color="#475569"/>
    </Group>
    <Group>
      <Text text="different" fontFamily="Arial" fontStyle="Bold" fontSize="24"/>
      <Fill color="#F59E0B"/>
    </Group>
    <Group>
      <Text text=" sizes " fontFamily="Arial" fontSize="16"/>
      <Fill color="#475569"/>
    </Group>
    <Group>
      <Text text="freely" fontFamily="Arial" fontStyle="Bold Italic" fontSize="24"/>
      <Fill color="#8B5CF6"/>
    </Group>
    <Group>
      <Text text=" within one line." fontFamily="Arial" fontSize="16"/>
      <Fill color="#475569"/>
    </Group>
    <TextLayout position="30,215" textAlign="start"/>
  </Layer>

  <!-- Center aligned rich text -->
  <Layer>
    <Group>
      <Text text="Center " fontFamily="Arial" fontSize="20"/>
      <Fill color="#475569"/>
    </Group>
    <Group>
      <Text text="aligned" fontFamily="Arial" fontStyle="Bold" fontSize="20"/>
      <Fill color="#EC4899"/>
    </Group>
    <Group>
      <Text text=" rich text." fontFamily="Arial" fontSize="20"/>
      <Fill color="#475569"/>
    </Group>
    <TextLayout position="200,265" textAlign="center"/>
  </Layer>

  <!-- Gradient fill text -->
  <Layer>
    <Text text="Gradient Fill" fontFamily="Arial" fontStyle="Bold" fontSize="28"/>
    <TextLayout position="200,310" textAlign="center"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="240,0">
        <ColorStop offset="0" color="#F43F5E"/>
        <ColorStop offset="0.5" color="#F59E0B"/>
        <ColorStop offset="1" color="#10B981"/>
      </LinearGradient>
    </Fill>
  </Layer>
</pagx>
```

> 📄 [示例](samples/5.5.7_rich_text.pagx) | [预览](https://pag.io/pagx/?file=./samples/5.5.7_rich_text.pagx)

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
<!-- Demonstrates Repeater modifier with position and alpha fade -->
<pagx version="1.0" width="400" height="400">
  <!-- Repeated rectangles with fade -->
  <Layer>
    <Rectangle center="60,200" size="56,56" roundness="12"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="56,56">
        <ColorStop offset="0" color="#6366F1"/>
        <ColorStop offset="1" color="#8B5CF6"/>
      </LinearGradient>
    </Fill>
    <Repeater copies="5" position="70,0" endAlpha="0.15"/>
  </Layer>
</pagx>
```

> 📄 [示例](samples/5.6_repeater.pagx) | [预览](https://pag.io/pagx/?file=./samples/5.6_repeater.pagx)

### 5.7 容器（Group）

Group 是带变换属性的矢量元素容器。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates Group with transform properties -->
<pagx version="1.0" width="400" height="400">
  <!-- Transformed group -->
  <Layer>
    <Group anchor="110,110" position="200,200" rotation="12">
      <Rectangle center="110,110" size="220,220" roundness="32"/>
      <Fill>
        <LinearGradient startPoint="0,0" endPoint="220,220">
          <ColorStop offset="0" color="#F43F5E"/>
          <ColorStop offset="0.5" color="#EC4899"/>
          <ColorStop offset="1" color="#8B5CF6"/>
        </LinearGradient>
      </Fill>
    </Group>
    <DropShadowStyle offsetY="12" blurX="40" blurY="40" color="#EC489960"/>
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
<!-- Demonstrates Group isolation: alpha applied to group, not individual shapes -->
<pagx version="1.0" width="400" height="400">
  <!-- Group with alpha isolation -->
  <Layer>
    <Group alpha="0.7">
      <Rectangle center="140,200" size="200,200" roundness="24"/>
      <Fill color="#F43F5E"/>
    </Group>
    <Ellipse center="260,200" size="200,200"/>
    <Fill color="#06B6D4"/>
  </Layer>
</pagx>
```

**示例 2 - 子 Group 几何向上累积**：
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates style propagation: Fill outside Groups applies to all shapes -->
<pagx version="1.0" width="400" height="400">
  <!-- Groups with shared overlay fill -->
  <Layer>
    <Group>
      <Rectangle center="140,200" size="160,160" roundness="24"/>
      <Fill color="#F43F5E"/>
    </Group>
    <Group>
      <Ellipse center="260,200" size="160,160"/>
      <Fill color="#06B6D4"/>
    </Group>
    <!-- Semi-transparent overlay applied to both shapes -->
    <Fill color="#8B5CF630"/>
  </Layer>
</pagx>
```

**示例 3 - 多个绘制器复用几何**：
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates Fill and Stroke combined on same shape -->
<pagx version="1.0" width="400" height="400">
  <!-- Rectangle with fill and stroke -->
  <Layer>
    <Rectangle center="200,200" size="260,260" roundness="32"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="260,260">
        <ColorStop offset="0" color="#F43F5E"/>
        <ColorStop offset="1" color="#E11D48"/>
      </LinearGradient>
    </Fill>
    <Stroke color="#BE123C" width="8"/>
    <DropShadowStyle offsetY="12" blurX="40" blurY="40" color="#F43F5E60"/>
  </Layer>
</pagx>
```

#### 多重填充与描边

由于绘制器不清空几何列表，同一几何可连续应用多个 Fill 和 Stroke。

**示例 4 - 多重填充**：
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates multiple Fill layers on same shape -->
<pagx version="1.0" width="400" height="400">
  <!-- Shape with multiple fills -->
  <Layer>
    <Rectangle center="200,200" size="300,200" roundness="32"/>
    <!-- Base gradient fill -->
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="300,200">
        <ColorStop offset="0" color="#F59E0B"/>
        <ColorStop offset="1" color="#F43F5E"/>
      </LinearGradient>
    </Fill>
    <!-- Semi-transparent overlay -->
    <Fill color="#8B5CF640"/>
    <DropShadowStyle offsetY="12" blurX="40" blurY="40" color="#F43F5E50"/>
  </Layer>
</pagx>
```

**示例 5 - 多重描边**：
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates multiple Stroke styles creating glow/outline effect -->
<pagx version="1.0" width="400" height="400">
  <!-- Path with multiple glow strokes -->
  <Layer>
    <Path data="M 60,200 Q 200,60 340,200 Q 200,340 60,200"/>
    <!-- Outer glow -->
    <Stroke color="#8B5CF615" width="48" cap="round" join="round"/>
    <!-- Middle glow -->
    <Stroke color="#8B5CF640" width="28" cap="round" join="round"/>
    <!-- Core stroke -->
    <Stroke width="8" cap="round" join="round">
      <LinearGradient startPoint="0,0" endPoint="280,0">
        <ColorStop offset="0" color="#6366F1"/>
        <ColorStop offset="0.5" color="#8B5CF6"/>
        <ColorStop offset="1" color="#EC4899"/>
      </LinearGradient>
    </Stroke>
  </Layer>
</pagx>
```

**示例 6 - 混合叠加**：
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates mixed Fill and Stroke on same shape -->
<pagx version="1.0" width="400" height="400">
  <!-- Circle with fill and stroke -->
  <Layer>
    <Ellipse center="200,200" size="280,280"/>
    <Fill>
      <!-- RadialGradient uses canvas coordinates. Ellipse center is at 200,200, 
           highlight offset should be relative to that. 140,140 creates top-left highlight -->
      <RadialGradient center="140,140" radius="200">
        <ColorStop offset="0" color="#FFF"/>
        <ColorStop offset="0.4" color="#06B6D4"/>
        <ColorStop offset="1" color="#0891B2"/>
      </RadialGradient>
    </Fill>
    <Stroke color="#0E7490" width="8"/>
    <DropShadowStyle offsetX="0" offsetY="8" blurX="32" blurY="32" color="#06B6D460"/>
  </Layer>
</pagx>
```

**渲染顺序**：多个绘制器按文档顺序渲染，先出现的位于下方。

> 📄 Samples: [Group](samples/5.7_group.pagx) | [Isolation](samples/5.7_group_isolation.pagx) | [Propagation](samples/5.7_group_propagation.pagx) | [Multiple Painters](samples/5.7_multiple_painters.pagx) | [Multiple Fills](samples/5.7_multiple_fills.pagx) | [Multiple Strokes](samples/5.7_multiple_strokes.pagx) | [Mixed Overlay](samples/5.7_mixed_overlay.pagx)
> 🔗 Preview: [Group](https://pag.io/pagx/?file=./samples/5.7_group.pagx) | [Isolation](https://pag.io/pagx/?file=./samples/5.7_group_isolation.pagx) | [Propagation](https://pag.io/pagx/?file=./samples/5.7_group_propagation.pagx) | [Painters](https://pag.io/pagx/?file=./samples/5.7_multiple_painters.pagx) | [Fills](https://pag.io/pagx/?file=./samples/5.7_multiple_fills.pagx) | [Strokes](https://pag.io/pagx/?file=./samples/5.7_multiple_strokes.pagx) | [Mixed](https://pag.io/pagx/?file=./samples/5.7_mixed_overlay.pagx)

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
  
  <!-- ==================== Layer Content ==================== -->
  <!-- Background with subtle gradient -->
  <Layer name="background">
    <Rectangle center="400,260" size="800,520"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="800,520">
        <ColorStop offset="0" color="#0F172A"/>
        <ColorStop offset="1" color="#1E293B"/>
      </LinearGradient>
    </Fill>
  </Layer>
  <!-- Decorative glow effects -->
  <Layer name="GlowTopLeft" blendMode="screen">
    <Ellipse center="80,60" size="320,320"/>
    <Fill color="@glowPurple"/>
    <BlurFilter blurX="30" blurY="30"/>
  </Layer>
  <Layer name="GlowBottomRight" blendMode="screen">
    <Ellipse center="720,460" size="400,400"/>
    <Fill color="@glowCyan"/>
    <BlurFilter blurX="40" blurY="40"/>
  </Layer>
  
  <!-- ===== Title Area y=60 ===== -->
  <Layer name="Title">
    <Text text="PAGX" fontFamily="Arial" fontStyle="Bold" fontSize="56"/>
    <TextLayout position="400,55" textAlign="center"/>
    <Fill color="@titleGradient"/>
  </Layer>
  <Layer name="Subtitle">
    <Text text="Portable Animated Graphics XML" fontFamily="Arial" fontSize="14"/>
    <TextPath path="M 220,135 Q 400,65 580,135" forceAlignment="true"/>
    <Fill color="#64748B"/>
  </Layer>
  
  <!-- ===== Shape Area y=195 ===== -->
  <!-- Card 1: Rounded Rectangle -->
  <Layer composition="@cardBg" x="110" y="155"/>
  <Layer y="195" x="160">
    <Rectangle center="0,0" size="50,35" roundness="8"/>
    <Fill color="@coral"/>
    <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#F43F5E80"/>
  </Layer>
  
  <!-- Card 2: Ellipse -->
  <Layer composition="@cardBg" x="230" y="155"/>
  <Layer y="195" x="280">
    <Ellipse center="0,0" size="50,35"/>
    <Fill color="@purple"/>
    <InnerShadowStyle offsetX="2" offsetY="2" blurX="6" blurY="6" color="#00000080"/>
  </Layer>
  
  <!-- Card 3: Star -->
  <Layer composition="@cardBg" x="350" y="155"/>
  <Layer y="195" x="400">
    <Polystar center="0,0" type="star" pointCount="5" outerRadius="24" innerRadius="11" rotation="-90"/>
    <Fill color="@amber"/>
    <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#F59E0B80"/>
  </Layer>
  
  <!-- Card 4: Hexagon -->
  <Layer composition="@cardBg" x="470" y="155"/>
  <Layer y="195" x="520">
    <Polystar center="0,0" type="polygon" pointCount="6" outerRadius="26"/>
    <Fill color="@diamond"/>
    <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#14B8A680"/>
  </Layer>
  
  <!-- Card 5: Custom Path -->
  <Layer composition="@cardBg" x="590" y="155"/>
  <Layer y="195" x="640">
    <Path data="M -20 -15 L 0 -25 L 20 -15 L 20 15 L 0 25 L -20 15 Z"/>
    <Fill color="@orange"/>
    <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#F9731680"/>
  </Layer>
  
  <!-- ===== Modifier Area y=325 (Get Started + 5 modifiers, centered) ===== -->
  <!-- Get Started Button -->
  <Layer y="325" x="148">
    <Rectangle center="0,0" size="120,36" roundness="18"/>
    <Fill color="@buttonGradient"/>
    <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#6366F180"/>
  </Layer>
  <Layer y="325" x="148">
    <Text text="Get Started" fontFamily="Arial" fontStyle="Bold" fontSize="13"/>
    <TextLayout position="0,4" textAlign="center"/>
    <Fill color="#FFF"/>
  </Layer>
  <Layer name="Modifiers" y="325">
    <!-- TrimPath Wave -->
    <Group position="258,0">
      <Path data="@wavePath"/>
      <TrimPath end="0.75"/>
      <Stroke color="@cyan" width="3" cap="round"/>
    </Group>
    
    <!-- RoundCorner -->
    <Group position="368,0">
      <Rectangle center="0,0" size="60,40"/>
      <RoundCorner radius="10"/>
      <Fill color="@emerald"/>
    </Group>
    
    <!-- MergePath -->
    <Group position="478,0">
      <Rectangle center="-10,0" size="35,35"/>
      <Ellipse center="10,0" size="35,35"/>
      <MergePath mode="xor"/>
      <Fill color="@purple"/>
    </Group>
    
    <!-- Repeater -->
    <Group position="588,0">
      <Ellipse center="22,0" size="12,12"/>
      <Fill color="@cyan"/>
      <Repeater copies="5" rotation="72" position="0,0"/>
    </Group>
    
    <!-- Mask Example (shape without mask) -->
    <Group position="688,0">
      <Rectangle center="0,0" size="50,50"/>
      <Fill color="@rainbow"/>
    </Group>
  </Layer>
  <Layer id="circleMask" visible="false">
    <Ellipse center="0,0" size="45,45"/>
    <Fill color="#FFF"/>
  </Layer>
  <Layer name="MaskedLayer" x="688" y="325" mask="@circleMask">
    <Rectangle center="0,0" size="50,50"/>
    <Fill color="@rainbow"/>
  </Layer>
  
  <!-- ===== Filter Area y=430 ===== -->
  <!-- Blur Filter -->
  <Layer y="430" x="130">
    <Rectangle center="0,0" size="80,60" roundness="10"/>
    <Fill color="@emerald"/>
    <BlurFilter blurX="3" blurY="3"/>
  </Layer>
  
  <!-- Drop Shadow Filter -->
  <Layer y="430" x="260">
    <Rectangle center="0,0" size="80,60" roundness="10"/>
    <Fill color="@cyan"/>
    <DropShadowFilter offsetX="4" offsetY="4" blurX="12" blurY="12" color="#00000080"/>
  </Layer>
  
  <!-- Color Matrix (Grayscale) -->
  <Layer y="430" x="390">
    <Ellipse center="0,0" size="55,55"/>
    <Fill color="@purple"/>
    <ColorMatrixFilter matrix="0.33,0.33,0.33,0,0,0.33,0.33,0.33,0,0,0.33,0.33,0.33,0,0,0,0,0,1,0"/>
  </Layer>
  
  <!-- Blend Filter -->
  <Layer y="430" x="520">
    <Rectangle center="0,0" size="80,60" roundness="10"/>
    <Fill color="@coral"/>
    <BlendFilter color="#6366F160" blendMode="overlay"/>
  </Layer>
  
  <!-- Image Fill + Drop Shadow Style -->
  <Layer y="430" x="670">
    <Rectangle center="0,0" size="90,60" roundness="8"/>
    <Fill>
      <ImagePattern image="@photo" tileModeX="clamp" tileModeY="clamp" matrix="0.23,0,0,0.23,-30,-30"/>
    </Fill>
    <Stroke color="#FFFFFF20" width="1"/>
    <DropShadowStyle offsetY="6" blurX="16" blurY="16" color="#00000060"/>
  </Layer>
  
  <!-- ===== Footer Area y=500 ===== -->
  <!-- Pre-layout Icon Text -->
  <Layer y="500" x="400">
    <Text fontFamily="Arial" fontSize="18">
      <GlyphRun font="@iconFont" glyphs="1,2,3" y="0" xOffsets="0,28,56"/>
    </Text>
    <Fill color="#64748B"/>
  </Layer>
  
  <!-- ==================== Resources ==================== -->
  <Resources>
    <!-- Image Resource (using a real image path from project) -->
    <Image id="photo" source="pag_logo.png"/>
    <!-- Path Data -->
    <PathData id="wavePath" data="M 0 0 Q 30 -20 60 0 T 120 0 T 180 0"/>
    <!-- Embedded Vector Font (Icons) -->
    <Font id="iconFont">
      <Glyph advance="20" path="M 0 -8 L 8 8 L -8 8 Z"/>
      <Glyph advance="20" path="M -8 -8 L 8 -8 L 8 8 L -8 8 Z"/>
      <Glyph advance="24" path="M 0 -10 A 10 10 0 1 1 0 10 A 10 10 0 1 1 0 -10"/>
    </Font>
    <!-- Glow Gradients (center relative to ellipse geometry) -->
    <RadialGradient id="glowPurple" center="160,160" radius="160">
      <ColorStop offset="0" color="#8B5CF660"/>
      <ColorStop offset="1" color="#8B5CF600"/>
    </RadialGradient>
    <RadialGradient id="glowCyan" center="200,200" radius="200">
      <ColorStop offset="0" color="#06B6D450"/>
      <ColorStop offset="1" color="#06B6D400"/>
    </RadialGradient>
    <!-- Title Gradient -->
    <LinearGradient id="titleGradient" startPoint="0,0" endPoint="200,0">
      <ColorStop offset="0" color="#FFF"/>
      <ColorStop offset="0.5" color="#8B5CF6"/>
      <ColorStop offset="1" color="#06B6D4"/>
    </LinearGradient>
    <!-- Button Gradient -->
    <LinearGradient id="buttonGradient" startPoint="0,0" endPoint="120,0">
      <ColorStop offset="0" color="#6366F1"/>
      <ColorStop offset="1" color="#8B5CF6"/>
    </LinearGradient>
    <!-- Conic Gradient (center relative to rectangle geometry: size 50x50, center at 0,0, so geometry origin is -25,-25) -->
    <ConicGradient id="rainbow" center="25,25">
      <ColorStop offset="0" color="#F43F5E"/>
      <ColorStop offset="0.25" color="#8B5CF6"/>
      <ColorStop offset="0.5" color="#06B6D4"/>
      <ColorStop offset="0.75" color="#10B981"/>
      <ColorStop offset="1" color="#F43F5E"/>
    </ConicGradient>
    <!-- Solid Colors -->
    <SolidColor id="coral" color="#F43F5E"/>
    <SolidColor id="purple" color="#8B5CF6"/>
    <SolidColor id="amber" color="#F59E0B"/>
    <SolidColor id="orange" color="#F97316"/>
    <SolidColor id="cyan" color="#06B6D4"/>
    <SolidColor id="emerald" color="#10B981"/>
    <!-- Diamond Gradient -->
    <DiamondGradient id="diamond" center="26,26" radius="26">
      <ColorStop offset="0" color="#14B8A6"/>
      <ColorStop offset="1" color="#0F766E"/>
    </DiamondGradient>
    <!-- Card Background -->
    <Composition id="cardBg" width="100" height="80">
      <Layer>
        <Rectangle center="50,40" size="100,80" roundness="12"/>
        <Fill color="#1E293B"/>
        <Stroke color="#334155" width="1"/>
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

### B.2 RPG 角色面板

一个奇幻 RPG 风格的角色状态面板，展示了复杂的 UI 组合，包含嵌套图层、渐变和装饰元素。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="800" height="600">

  <!-- ==================== BACKGROUND ==================== -->
  <Layer name="Background">
    <Rectangle center="400,300" size="800,600"/>
    <Fill color="@bgGrad"/>
  </Layer>

  <!-- Ambient glows -->
  <Layer name="GlowGold" blendMode="screen">
    <Ellipse center="400,300" size="800,800"/>
    <Fill color="@glowGold"/>
  </Layer>
  <Layer name="GlowBlue" blendMode="screen">
    <Ellipse center="200,500" size="600,600"/>
    <Fill color="@glowPurple"/>
  </Layer>

  <!-- Subtle star dust decoration -->
  <Layer name="StarDust" alpha="0.15">
    <Group>
      <Path data="@diamondPath"/>
      <Fill color="#FFD700"/>
      <Repeater copies="12" position="65,0" endAlpha="0.3"/>
    </Group>
  </Layer>

  <!-- ==================== TOP BAR ==================== -->
  <Layer name="TopBar">
    <!-- Bar background (Simplified) -->
    <Layer>
      <Rectangle center="400,32" size="800,64"/>
      <Fill color="#0E0B1880"/>
    </Layer>
    <!-- Bottom border line -->
    <Layer>
      <Path data="M 0 64 L 800 64"/>
      <Stroke color="#5C451080" width="1"/>
    </Layer>

    <!-- Avatar area -->
    <Layer name="Avatar" x="40" y="32">
      <!-- Ring glow -->
      <Layer>
        <Ellipse size="52,52"/>
        <Fill color="@avatarRing"/>
      </Layer>
      <!-- Avatar inner -->
      <Layer>
        <Ellipse size="46,46"/>
        <Fill color="#1A1428"/>
      </Layer>
      <!-- Avatar silhouette -->
      <Layer>
        <Path data="@helmetIcon"/>
        <Fill color="#A89878"/>
      </Layer>
    </Layer>

    <!-- Player name & level -->
    <Layer x="80" y="28">
      <Text text="Aethon" fontFamily="Arial" fontStyle="Bold" fontSize="20"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#F8FAFC"/>
    </Layer>
    <Layer x="80" y="48">
      <Text text="Lv. 72  Paladin" fontFamily="Arial" fontSize="12"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#A89878"/>
    </Layer>

    <!-- HP bar -->
    <Layer name="HPBar" x="190" y="16">
      <Layer>
        <Text text="HP" fontFamily="Arial" fontStyle="Bold" fontSize="11"/>
        <TextLayout position="-10,9" textAlign="start"/>
        <Fill color="#FF2D2D"/>
      </Layer>
      <Layer>
        <Rectangle center="105,5" size="190,14" roundness="7"/>
        <Fill color="#0E0B18"/>
        <Stroke color="#5C451080" width="1"/>
      </Layer>
      <Layer>
        <Rectangle center="83,5" size="142,10" roundness="5"/>
        <Fill color="@hpFill"/>
        <InnerShadowStyle offsetY="-1" blurX="2" blurY="2" color="#FFFFFF40"/>
      </Layer>
      <Layer>
        <Text text="2,847 / 3,800" fontFamily="Arial" fontSize="9"/>
        <TextLayout position="105,8" textAlign="center"/>
        <Fill color="#FFFFFFD0"/>
      </Layer>
    </Layer>

    <!-- MP bar -->
    <Layer name="MPBar" x="190" y="38">
      <Layer>
        <Text text="MP" fontFamily="Arial" fontStyle="Bold" fontSize="11"/>
        <TextLayout position="-10,9" textAlign="start"/>
        <Fill color="#4488FF"/>
      </Layer>
      <Layer>
        <Rectangle center="105,5" size="190,14" roundness="7"/>
        <Fill color="#0E0B18"/>
        <Stroke color="#5C451080" width="1"/>
      </Layer>
      <Layer>
        <Rectangle center="74,5" size="124,10" roundness="5"/>
        <Fill color="@mpFill"/>
        <InnerShadowStyle offsetY="-1" blurX="2" blurY="2" color="#FFFFFF40"/>
      </Layer>
      <Layer>
        <Text text="1,250 / 2,000" fontFamily="Arial" fontSize="9"/>
        <TextLayout position="105,8" textAlign="center"/>
        <Fill color="#FFFFFFD0"/>
      </Layer>
    </Layer>

    <!-- Gold currency -->
    <Layer name="Gold" x="640" y="16">
      <Layer>
        <Rectangle center="70,16" size="160,32" roundness="16"/>
        <Fill color="#00000040"/>
        <Stroke color="#FFD70060" width="1"/>
      </Layer>
      <!-- Coin icon -->
      <Layer x="10" y="16">
        <Ellipse size="22,22"/>
        <Fill color="#FFD700"/>
        <InnerShadowStyle offsetX="-2" offsetY="-2" blurX="3" blurY="3" color="#FFFFFF60"/>
      </Layer>
      <Layer x="10" y="16">
        <Text text="G" fontFamily="Arial" fontStyle="Bold" fontSize="12"/>
        <TextLayout position="0,4" textAlign="center"/>
        <Fill color="#78350F"/>
      </Layer>
      <Layer>
        <Text text="128,450" fontFamily="Arial" fontStyle="Bold" fontSize="16"/>
        <TextLayout position="70,22" textAlign="center"/>
        <Fill color="#FFE880"/>
      </Layer>
    </Layer>
  </Layer>

  <!-- ==================== LEFT PANEL: CHARACTER ==================== -->
  <Layer name="CharPanel" x="30" y="80">
    <!-- Panel background -->
    <Layer>
      <Rectangle center="130,237" size="260,470" roundness="16"/>
      <Fill color="@panelBg"/>
      <Stroke color="@panelBorder" width="1"/>
      <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#00000080"/>
    </Layer>

    <!-- Panel title -->
    <Layer>
      <Path data="M 20 40 L 240 40"/>
      <Stroke color="#5C451080" width="1"/>
    </Layer>
    <Layer>
      <Text text="CHARACTER" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
      <TextLayout position="130,30" textAlign="center"/>
      <Fill color="@titleGrad"/>
    </Layer>

    <!-- Character portrait area -->
    <Layer x="130" y="155">
      <!-- Portrait frame -->
      <Layer>
        <Rectangle center="0,0" size="200,200" roundness="12"/>
        <Fill color="#0E0B1880"/>
        <Stroke color="#5C451080" width="1"/>
      </Layer>
      <!-- Character glow behind -->
      <Layer>
        <Ellipse size="140,140"/>
        <Fill>
          <RadialGradient radius="70">
            <ColorStop offset="0" color="#FFD70010"/>
            <ColorStop offset="1" color="#FFD70000"/>
          </RadialGradient>
        </Fill>
      </Layer>
      <!-- Character silhouette -->
      <Layer y="20">
        <!-- Cape (behind body) -->
        <Group>
          <Path data="M -22 -35 Q -28 15 -18 65 L 18 65 Q 28 15 22 -35"/>
          <Fill color="#7C3AED25"/>
          <Stroke color="#7C3AED40" width="1"/>
        </Group>
        <!-- Body armor -->
        <Group>
          <Path data="M 0 -65 Q 28 -65 32 -38 L 36 10 L 18 10 L 14 -18 Q 10 -28 0 -28 Q -10 -28 -14 -18 L -18 10 L -36 10 L -32 -38 Q -28 -65 0 -65 Z"/>
          <Fill>
            <LinearGradient startPoint="0,-65" endPoint="0,10">
              <ColorStop offset="0" color="#3D3555"/>
              <ColorStop offset="1" color="#2A2040"/>
            </LinearGradient>
          </Fill>
          <Stroke color="#B8860B30" width="1"/>
        </Group>
        <!-- Armor plate lines -->
        <Group>
          <Path data="M -12 -15 L 12 -15 M -14 -5 L 14 -5"/>
          <Stroke color="#B8860B20" width="1"/>
        </Group>
        <!-- Chest emblem -->
        <Group>
          <Path data="M 0 -50 L 6 -40 L -6 -40 Z"/>
          <Fill color="#FFD70060"/>
        </Group>
        <!-- Belt -->
        <Group>
          <Path data="M -20 5 L 20 5 L 20 12 L -20 12 Z"/>
          <Fill color="#92400E"/>
          <Stroke color="#78350F" width="1"/>
        </Group>
        <Group>
          <Rectangle center="0,8" size="10,10" roundness="1"/>
          <Rectangle center="0,8" size="5,5" roundness="1"/>
          <Fill color="#FFD700"/>
        </Group>
        <!-- Legs -->
        <Group>
          <Path data="M -15 12 L -18 55 L -8 55 L -5 12 Z M 5 12 L 8 55 L 18 55 L 15 12 Z"/>
          <Fill color="#2E2545"/>
        </Group>
        <!-- Knee guards -->
        <Group>
          <Ellipse center="-12,32" size="10,8"/>
          <Ellipse center="12,32" size="10,8"/>
          <Fill color="#3D3555"/>
          <Stroke color="#B8860B30" width="1"/>
        </Group>
        <!-- Boots -->
        <Group>
          <Path data="M -20 50 L -6 50 L -5 62 L -22 62 Z M 6 50 L 20 50 L 22 62 L 5 62 Z"/>
          <Fill color="#4B3D5A"/>
          <Stroke color="#B8860B20" width="1"/>
        </Group>
        <!-- Boot cuffs -->
        <Group>
          <Path data="M -20 50 L -6 50 L -6 53 L -20 53 Z M 6 50 L 20 50 L 20 53 L 6 53 Z"/>
          <Fill color="#3D3555"/>
        </Group>
        <!-- Head -->
        <Group>
          <Ellipse center="0,-76" size="32,36"/>
          <Fill color="#64748B"/>
          <Stroke color="#B8860B20" width="1"/>
        </Group>
        <!-- Helmet visor -->
        <Group>
          <Path data="M -10 -78 L 10 -78 L 8 -70 L -8 -70 Z"/>
          <Fill color="#1A1428"/>
        </Group>
        <!-- Visor glow -->
        <Group>
          <Path data="M -6 -76 L 6 -76 L 4 -72 L -4 -72 Z"/>
          <Fill color="#60A5FA30"/>
        </Group>
        <!-- Helmet crest -->
        <Group>
          <Path data="M -4 -95 Q 0 -112 4 -95"/>
          <Stroke color="#FFD700" width="3" cap="round"/>
        </Group>
        <Group>
          <Path data="M -2 -96 Q 0 -104 2 -96"/>
          <Stroke color="#FFD700" width="2" cap="round"/>
        </Group>
        <!-- Shoulders -->
        <Group>
          <Ellipse center="-32,-42" size="24,18"/>
          <Ellipse center="32,-42" size="24,18"/>
          <Fill color="#3D3555"/>
          <Stroke color="#B8860B50" width="1"/>
        </Group>
        <Group>
          <Ellipse center="-32,-42" size="12,8"/>
          <Ellipse center="32,-42" size="12,8"/>
          <Fill color="#B8860B20"/>
        </Group>
        <!-- Arms -->
        <Group>
          <Path data="M -36 -32 L -40 -10 L -34 -10 L -30 -32 Z M 30 -32 L 34 -10 L 40 -10 L 36 -32 Z"/>
          <Fill color="#2E2545"/>
        </Group>
        <!-- Shield (left arm) -->
        <Group>
          <Path data="M -50 -18 L -36 -26 L -36 -6 Q -36 6 -44 10 Q -50 6 -50 -6 Z"/>
          <Fill>
            <LinearGradient startPoint="-50,-26" endPoint="-36,10">
              <ColorStop offset="0" color="#3B82F6"/>
              <ColorStop offset="1" color="#2563EB"/>
            </LinearGradient>
          </Fill>
          <Stroke color="#60A5FA" width="1"/>
        </Group>
        <!-- Shield cross -->
        <Group>
          <Path data="M -43 -14 L -43 0 M -48 -7 L -38 -7"/>
          <Stroke color="#FFFFFF60" width="1"/>
        </Group>
        <!-- Sword (right hand) -->
        <Group>
          <Path data="M 46 -30 L 48 -78 L 44 -78 Z"/>
          <Fill color="#E2E8F0"/>
          <Stroke color="#FFFFFF40" width="1"/>
        </Group>
        <!-- Sword guard -->
        <Group>
          <Path data="M 40 -28 L 52 -28 L 52 -24 L 40 -24 Z"/>
          <Fill color="#92400E"/>
        </Group>
        <!-- Sword grip -->
        <Group>
          <Path data="M 44 -24 L 48 -24 L 48 -16 L 44 -16 Z"/>
          <Fill color="#78350F"/>
        </Group>
        <!-- Sword pommel -->
        <Group>
          <Ellipse center="46,-15" size="6,6"/>
          <Fill color="#FFD700"/>
        </Group>
      </Layer>

    </Layer>

    <!-- Rarity stars (below portrait) -->
    <Layer x="148" y="276">
      <Group>
        <Polystar center="0,0" type="star" pointCount="5" outerRadius="7" innerRadius="3"/>
        <Fill color="@starGrad"/>
        <Repeater copies="5" position="18,0"/>
      </Group>
    </Layer>

    <!-- Stats section -->
    <Layer x="30" y="305">
      <!-- ATK -->
      <Layer>
        <Polystar center="10,0" type="star" pointCount="4" outerRadius="7" innerRadius="2" rotation="45"/>
        <Fill color="#FF4444"/>
      </Layer>
      <Layer>
        <Text text="ATK" fontFamily="Arial" fontSize="12"/>
        <TextLayout position="28,4" textAlign="start"/>
        <Fill color="#A89878"/>
      </Layer>
      <Layer>
        <Text text="4,256" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
        <TextLayout position="198,5" textAlign="end"/>
        <Fill color="#FF4444"/>
      </Layer>

      <!-- DEF -->
      <Layer y="28">
        <Ellipse center="10,0" size="14,14"/>
        <Fill color="#4488FF"/>
      </Layer>
      <Layer y="28">
        <Text text="DEF" fontFamily="Arial" fontSize="12"/>
        <TextLayout position="28,4" textAlign="start"/>
        <Fill color="#A89878"/>
      </Layer>
      <Layer y="28">
        <Text text="3,180" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
        <TextLayout position="198,5" textAlign="end"/>
        <Fill color="#4488FF"/>
      </Layer>

      <!-- SPD -->
      <Layer y="56">
        <Polystar center="10,0" type="star" pointCount="4" outerRadius="8" innerRadius="3"/>
        <Fill color="#44DD88"/>
      </Layer>
      <Layer y="56">
        <Text text="SPD" fontFamily="Arial" fontSize="12"/>
        <TextLayout position="28,4" textAlign="start"/>
        <Fill color="#A89878"/>
      </Layer>
      <Layer y="56">
        <Text text="186" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
        <TextLayout position="198,5" textAlign="end"/>
        <Fill color="#44DD88"/>
      </Layer>

      <!-- CRT -->
      <Layer y="84">
        <Path data="M 10 -7 L 16 0 L 10 7 L 4 0 Z"/>
        <Fill color="#FFD700"/>
      </Layer>
      <Layer y="84">
        <Text text="CRT" fontFamily="Arial" fontSize="12"/>
        <TextLayout position="28,4" textAlign="start"/>
        <Fill color="#A89878"/>
      </Layer>
      <Layer y="84">
        <Text text="42.5%" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
        <TextLayout position="198,5" textAlign="end"/>
        <Fill color="#FFD700"/>
      </Layer>
    </Layer>

    <!-- Separator -->
    <Layer>
      <Path data="M 30 413 L 230 413"/>
      <Stroke color="#5C451080" width="1"/>
    </Layer>

    <!-- Power number -->
    <Layer>
      <Text text="52,847" fontFamily="Arial" fontStyle="Bold" fontSize="28"/>
      <TextLayout position="130,450" textAlign="center"/>
      <Fill color="@titleGrad"/>
      <DropShadowStyle offsetY="2" blurX="8" blurY="8" color="#FFD70040"/>
    </Layer>
  </Layer>

  <!-- ==================== RIGHT AREA: EQUIPMENT GRID ==================== -->
  <Layer name="EquipArea" x="310" y="95">

    <!-- Section title -->
    <Layer>
      <Text text="EQUIPMENT" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
      <TextLayout position="0,15" textAlign="start"/>
      <Fill color="@titleGrad"/>
    </Layer>
    <Layer>
      <Path data="M 0 25 L 460 25"/>
      <Stroke color="#5C451080" width="1"/>
    </Layer>

    <!-- Equipment slot row 1 -->
    <!-- Slot 1: Weapon -->
    <Layer name="Slot1" x="10" y="45">
      <Layer>
        <Rectangle center="65,65" size="130,130" roundness="12"/>
        <Fill color="@panelBg"/>
        <Stroke color="#FF8C0080" width="1.5"/>
        <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#00000060"/>
      </Layer>
      <!-- Item glow -->
      <Layer x="65" y="50">
        <Ellipse size="60,60"/>
        <Fill>
          <RadialGradient radius="30">
            <ColorStop offset="0" color="#FF8C0030"/>
            <ColorStop offset="1" color="#FF8C0000"/>
          </RadialGradient>
        </Fill>
      </Layer>
      <!-- Item icon -->
      <Layer x="65" y="50">
        <Path data="@swordBig"/>
        <Fill color="#FF8C00"/>
      </Layer>
      <!-- Item name -->
      <Layer>
        <Text text="Excalibur" fontFamily="Arial" fontStyle="Bold" fontSize="11"/>
        <TextLayout position="65,108" textAlign="center"/>
        <Fill color="#F8FAFC"/>
      </Layer>
      <!-- Rarity indicator -->
      <Layer>
        <Rectangle center="65,120" size="50,3" roundness="2"/>
        <Fill color="#FF8C00"/>
      </Layer>
    </Layer>

    <!-- Slot 2: Shield -->
    <Layer name="Slot2" x="165" y="45">
      <Layer>
        <Rectangle center="65,65" size="130,130" roundness="12"/>
        <Fill color="@panelBg"/>
        <Stroke color="#A855F780" width="1.5"/>
        <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#00000060"/>
      </Layer>
      <Layer x="65" y="50">
        <Ellipse size="60,60"/>
        <Fill>
          <RadialGradient radius="30">
            <ColorStop offset="0" color="#A855F730"/>
            <ColorStop offset="1" color="#A855F700"/>
          </RadialGradient>
        </Fill>
      </Layer>
      <Layer x="65" y="50">
        <Path data="@shieldBig"/>
        <Fill color="#A855F7"/>
      </Layer>
      <Layer>
        <Text text="Aegis" fontFamily="Arial" fontStyle="Bold" fontSize="11"/>
        <TextLayout position="65,108" textAlign="center"/>
        <Fill color="#F8FAFC"/>
      </Layer>
      <Layer>
        <Rectangle center="65,120" size="50,3" roundness="2"/>
        <Fill color="#A855F7"/>
      </Layer>
    </Layer>

    <!-- Slot 3: Helmet -->
    <Layer name="Slot3" x="320" y="45">
      <Layer>
        <Rectangle center="65,65" size="130,130" roundness="12"/>
        <Fill color="@panelBg"/>
        <Stroke color="#3B82F680" width="1.5"/>
        <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#00000060"/>
      </Layer>
      <Layer x="65" y="50">
        <Ellipse size="60,60"/>
        <Fill>
          <RadialGradient radius="30">
            <ColorStop offset="0" color="#3B82F630"/>
            <ColorStop offset="1" color="#3B82F600"/>
          </RadialGradient>
        </Fill>
      </Layer>
      <Layer x="65" y="50">
        <Path data="@helmetBig"/>
        <Fill color="#3B82F6"/>
      </Layer>
      <Layer>
        <Text text="Crown Helm" fontFamily="Arial" fontStyle="Bold" fontSize="11"/>
        <TextLayout position="65,108" textAlign="center"/>
        <Fill color="#F8FAFC"/>
      </Layer>
      <Layer>
        <Rectangle center="65,120" size="50,3" roundness="2"/>
        <Fill color="#3B82F6"/>
      </Layer>
    </Layer>

    <!-- Equipment slot row 2 -->
    <!-- Slot 4: Potion -->
    <Layer name="Slot4" x="10" y="195">
      <Layer>
        <Rectangle center="65,65" size="130,130" roundness="12"/>
        <Fill color="@panelBg"/>
        <Stroke color="#22C55E80" width="1.5"/>
        <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#00000060"/>
      </Layer>
      <Layer x="65" y="50">
        <Ellipse size="60,60"/>
        <Fill>
          <RadialGradient radius="30">
            <ColorStop offset="0" color="#22C55E30"/>
            <ColorStop offset="1" color="#22C55E00"/>
          </RadialGradient>
        </Fill>
      </Layer>
      <Layer x="65" y="50">
        <Path data="@potionBig"/>
        <Fill color="#22C55E"/>
      </Layer>
      <Layer>
        <Text text="Elixir" fontFamily="Arial" fontStyle="Bold" fontSize="11"/>
        <TextLayout position="65,108" textAlign="center"/>
        <Fill color="#F8FAFC"/>
      </Layer>
      <Layer>
        <Rectangle center="65,120" size="50,3" roundness="2"/>
        <Fill color="#22C55E"/>
      </Layer>
    </Layer>

    <!-- Slot 5: Empty -->
    <Layer name="Slot5" composition="@emptySlot" x="165" y="195"/>

    <!-- Slot 6: Empty -->
    <Layer name="Slot6" composition="@emptySlot" x="320" y="195"/>

    <!-- Action buttons -->
    <Layer name="Actions" x="0" y="355">
      <!-- Upgrade button -->
      <Layer>
        <Rectangle center="110,22" size="200,44" roundness="22"/>
        <Fill color="@btnGold"/>
        <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#DAA52060"/>
        <InnerShadowStyle offsetY="-1" blurX="2" blurY="2" color="#FFFFFF30"/>
      </Layer>
      <Layer>
        <Text text="UPGRADE" fontFamily="Arial" fontStyle="Bold" fontSize="16"/>
        <TextLayout position="110,28" textAlign="center"/>
        <Fill color="#3D2200"/>
      </Layer>

      <!-- Battle button -->
      <Layer x="240">
        <Rectangle center="110,22" size="200,44" roundness="22"/>
        <Fill color="@btnRed"/>
        <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#CC000060"/>
        <InnerShadowStyle offsetY="-1" blurX="2" blurY="2" color="#FFFFFF40"/>
      </Layer>
      <Layer x="240">
        <Text text="BATTLE" fontFamily="Arial" fontStyle="Bold" fontSize="16"/>
        <TextLayout position="110,28" textAlign="center"/>
        <Fill color="#FFF"/>
      </Layer>
    </Layer>
  </Layer>

  <!-- ==================== BOTTOM EXP BAR ==================== -->
  <Layer name="ExpBar" y="568">
    <!-- Bar background -->
    <Layer>
      <Rectangle center="400,10" size="740,16" roundness="8"/>
      <Fill color="#0E0B18"/>
      <Stroke color="#5C451080" width="1"/>
    </Layer>
    <!-- Fill (65%) -->
    <Layer>
      <Rectangle center="282,10" size="500,12" roundness="6"/>
      <Fill color="@expFill"/>
      <InnerShadowStyle offsetY="-1" blurX="3" blurY="3" color="#FFFFFF30"/>
    </Layer>
    <!-- Label -->
    <Layer>
      <Text text="EXP  12,500 / 18,000" fontFamily="Arial" fontSize="9"/>
      <TextLayout position="400,13" textAlign="center"/>
      <Fill color="#FFFFFFC0"/>
    </Layer>
  </Layer>

  <!-- ==================== NOTIFICATION TOAST ==================== -->
  <Layer name="Toast" x="310" y="516">
    <Layer>
      <Rectangle center="230,18" size="460,36" roundness="8"/>
      <Fill color="#8B6914E0"/>
      <DropShadowStyle offsetY="4" blurX="10" blurY="10" color="#00000040"/>
    </Layer>
    <Layer>
      <Polystar center="24,18" type="star" pointCount="4" outerRadius="8" innerRadius="3"/>
      <Fill color="#FFF"/>
    </Layer>
    <Layer>
      <Text text="New legendary item discovered in the Ancient Ruins!" fontFamily="Arial" fontStyle="Bold" fontSize="13"/>
      <TextLayout position="230,23" textAlign="center"/>
      <Fill color="#FFF"/>
    </Layer>
  </Layer>

  <!-- ==================== RESOURCES ==================== -->
  <Resources>
    <!-- Background: Dark Fantasy -->
    <LinearGradient id="bgGrad" startPoint="0,0" endPoint="800,600">
      <ColorStop offset="0" color="#0A0A12"/>
      <ColorStop offset="0.6" color="#12101E"/>
      <ColorStop offset="1" color="#1A1428"/>
    </LinearGradient>

    <!-- Ambient glow -->
    <RadialGradient id="glowGold" center="400,300" radius="400">
      <ColorStop offset="0" color="#FFD70020"/>
      <ColorStop offset="1" color="#FFD70000"/>
    </RadialGradient>
    <RadialGradient id="glowPurple" center="200,500" radius="300">
      <ColorStop offset="0" color="#7C3AED0A"/>
      <ColorStop offset="1" color="#7C3AED00"/>
    </RadialGradient>

    <!-- Panel background: Dark Fantasy -->
    <LinearGradient id="panelBg" startPoint="0,0" endPoint="0,400">
      <ColorStop offset="0" color="#1A1428E8"/>
      <ColorStop offset="1" color="#0E0B18E8"/>
    </LinearGradient>
    
    <!-- Panel Border: Gold Metallic -->
    <LinearGradient id="panelBorder" startPoint="0,0" endPoint="0,400">
      <ColorStop offset="0" color="#8B6914"/>
      <ColorStop offset="0.5" color="#5C4510"/>
      <ColorStop offset="1" color="#3D2E08"/>
    </LinearGradient>

    <!-- HP bar: Blood Red -->
    <LinearGradient id="hpFill" startPoint="0,0" endPoint="200,0">
      <ColorStop offset="0" color="#FF2D2D"/>
      <ColorStop offset="1" color="#CC0000"/>
    </LinearGradient>

    <!-- MP bar: Arcane Blue -->
    <LinearGradient id="mpFill" startPoint="0,0" endPoint="200,0">
      <ColorStop offset="0" color="#4488FF"/>
      <ColorStop offset="1" color="#2255CC"/>
    </LinearGradient>

    <!-- EXP bar: Golden -->
    <LinearGradient id="expFill" startPoint="0,0" endPoint="500,0">
      <ColorStop offset="0" color="#FFD700"/>
      <ColorStop offset="1" color="#FF8C00"/>
    </LinearGradient>

    <!-- Button gradients -->
    <LinearGradient id="btnGold" startPoint="0,0" endPoint="0,44">
      <ColorStop offset="0" color="#FFD700"/>
      <ColorStop offset="0.5" color="#DAA520"/>
      <ColorStop offset="1" color="#B8860B"/>
    </LinearGradient>
    <LinearGradient id="btnRed" startPoint="0,0" endPoint="0,44">
      <ColorStop offset="0" color="#FF4444"/>
      <ColorStop offset="0.5" color="#CC0000"/>
      <ColorStop offset="1" color="#990000"/>
    </LinearGradient>

    <!-- Star / rank gradient -->
    <LinearGradient id="starGrad" startPoint="0,-12" endPoint="0,12">
      <ColorStop offset="0" color="#FFE880"/>
      <ColorStop offset="1" color="#FFD700"/>
    </LinearGradient>

    <!-- Title gradient: Bright Gold -->
    <LinearGradient id="titleGrad" startPoint="0,0" endPoint="250,0">
      <ColorStop offset="0" color="#FFE880"/>
      <ColorStop offset="0.5" color="#FFD700"/>
      <ColorStop offset="1" color="#CC8800"/>
    </LinearGradient>

    <!-- Avatar ring gradient -->
    <ConicGradient id="avatarRing">
      <ColorStop offset="0" color="#FFD700"/>
      <ColorStop offset="0.25" color="#B8860B"/>
      <ColorStop offset="0.5" color="#FFD700"/>
      <ColorStop offset="0.75" color="#B8860B"/>
      <ColorStop offset="1" color="#FFD700"/>
    </ConicGradient>

    <!-- Decorative diamond path -->
    <PathData id="diamondPath" data="M 0 -8 L 6 0 L 0 8 L -6 0 Z"/>

    <!-- Small Icons -->
    <PathData id="helmetIcon" data="M -12 4 L -12 -2 Q -12 -16 0 -16 Q 12 -16 12 -2 L 12 4 L 8 4 L 8 -2 Q 8 -10 0 -10 Q -8 -10 -8 -2 L -8 4 Z M -14 4 L 14 4 L 14 10 Q 14 14 10 14 L -10 14 Q -14 14 -14 10 Z"/>
    
    <!-- Large Icons -->
    <PathData id="swordBig" data="M 0 -40 L 5 -36 L 5 0 L 10 0 L 26 16 L 24 19 L 20 22 L 17 20 L 5 8 L 5 16 L 3 20 L 0 32 L -3 20 L -5 16 L -5 8 L -17 20 L -20 22 L -24 19 L -26 16 L -10 0 L -5 0 L -5 -36 Z M -4 -32 L 4 -32 L 4 -28 L -4 -28 Z M -3 -28 L 3 -28 L 3 -2 L -3 -2 Z"/>
    <PathData id="shieldBig" data="M 0 -36 L 32 -24 L 32 4 Q 32 20 24 28 Q 14 36 0 40 Q -14 36 -24 28 Q -32 20 -32 4 L -32 -24 Z M 0 -28 L -24 -18 L -24 2 Q -24 16 -18 22 Q -10 28 0 32 Q 10 28 18 22 Q 24 16 24 2 L 24 -18 Z M -2 -16 L -2 -6 L -14 -6 L -14 6 L -2 6 L -2 22 L 2 22 L 2 6 L 14 6 L 14 -6 L 2 -6 L 2 -16 Z"/>
    <PathData id="helmetBig" data="M 0 -38 Q -32 -38 -32 -8 L -32 4 L -22 4 L -22 -8 Q -22 -28 0 -28 Q 22 -28 22 -8 L 22 4 L 32 4 L 32 -8 Q 32 -38 0 -38 Z M -34 8 L 34 8 L 34 16 L 26 16 L 20 24 L 14 24 L 14 16 L -14 16 L -14 24 L -20 24 L -26 16 L -34 16 Z M -30 28 L 30 28 Q 30 38 20 38 L -20 38 Q -30 38 -30 28 Z M -10 -20 L -4 -20 L -4 -10 L -10 -10 Z M 4 -20 L 10 -20 L 10 -10 L 4 -10 Z"/>
    <PathData id="potionBig" data="M -5 -38 L 5 -38 L 5 -36 L 7 -36 L 7 -34 L -7 -34 L -7 -36 L -5 -36 Z M -9 -34 L 9 -34 L 9 -30 L -9 -30 Z M -11 -30 L 11 -30 L 11 -26 L -11 -26 Z M -6 -26 L 6 -26 L 6 -20 L 16 -6 Q 24 10 22 22 Q 18 34 8 38 Q 4 40 0 40 Q -4 40 -8 38 Q -18 34 -22 22 Q -24 10 -16 -6 L -6 -20 Z M -3 -26 L 3 -26 L 3 -20 L -3 -20 Z M -14 8 Q -14 20 -6 24 Q 0 28 6 24 Q 14 20 14 8 Q 14 -2 6 -4 Q 0 -8 -6 -4 Q -14 -2 -14 8 Z M -10 10 Q -10 18 0 18 Q 10 18 10 10 Q 10 4 0 4 Q -10 4 -10 10 Z M -6 12 Q -6 16 0 16 Q 6 16 6 12 Q 6 8 0 8 Q -6 8 -6 12 Z"/>
    <!-- Empty Equipment Slot -->
    <Composition id="emptySlot" width="130" height="130">
      <Layer>
        <Rectangle center="65,65" size="130,130" roundness="12"/>
        <Fill color="#1A142880"/>
        <Stroke color="#5C451040" width="1" dashes="8,4"/>
      </Layer>
      <Layer x="65" y="50">
        <Text text="+" fontFamily="Arial" fontSize="36"/>
        <TextLayout position="0,0" textAlign="center"/>
        <Fill color="#8B691440"/>
      </Layer>
      <Layer>
        <Text text="Empty" fontFamily="Arial" fontSize="11"/>
        <TextLayout position="65,108" textAlign="center"/>
        <Fill color="#5C451060"/>
      </Layer>
    </Composition>
  </Resources>

</pagx>
```

> 📄 [示例](samples/B.2_rpg_character_panel.pagx) | [预览](https://pag.io/pagx/?file=./samples/B.2_rpg_character_panel.pagx)

### B.3 星云学员

一个太空主题的学员资料卡片，展示了星云效果、星空背景和现代 UI 设计模式。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="800" height="600">
  <!-- ==================== Background ==================== -->
  <Layer name="Background">
    <Rectangle center="400,300" size="800,600"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="0,600">
        <ColorStop offset="0" color="#0B1026"/>
        <ColorStop offset="0.6" color="#0F1C3D"/>
        <ColorStop offset="1" color="#1B1140"/>
      </LinearGradient>
    </Fill>
  </Layer>

  <Layer name="NebulaLeft">
    <Ellipse center="160,220" size="360,300"/>
    <Fill>
      <RadialGradient center="160,220" radius="220">
        <ColorStop offset="0" color="#1D4ED880"/>
        <ColorStop offset="1" color="#1D4ED800"/>
      </RadialGradient>
    </Fill>
  </Layer>

  <Layer name="NebulaRight">
    <Ellipse center="640,180" size="360,280"/>
    <Fill>
      <RadialGradient center="640,180" radius="220">
        <ColorStop offset="0" color="#C026D380"/>
        <ColorStop offset="1" color="#C026D300"/>
      </RadialGradient>
    </Fill>
  </Layer>

  <Layer name="NebulaBottom" alpha="0.9">
    <Ellipse center="400,505" size="600,220"/>
    <Fill>
      <RadialGradient center="400,505" radius="260">
        <ColorStop offset="0" color="#22D3EE60"/>
        <ColorStop offset="1" color="#22D3EE00"/>
      </RadialGradient>
    </Fill>
  </Layer>

  <Layer name="Stars" alpha="0.9">
    <Group>
      <Ellipse center="120,80" size="2,2"/>
      <Ellipse center="180,110" size="3,3"/>
      <Ellipse center="260,90" size="2,2"/>
      <Ellipse center="340,70" size="2,2"/>
      <Ellipse center="420,110" size="3,3"/>
      <Ellipse center="520,80" size="2,2"/>
      <Ellipse center="610,120" size="2,2"/>
      <Ellipse center="700,90" size="2,2"/>
      <Ellipse center="740,150" size="3,3"/>
      <Ellipse center="90,160" size="2,2"/>
      <Fill color="#FFFFFFCC"/>
    </Group>
    <Group>
      <Ellipse center="150,200" size="2,2"/>
      <Ellipse center="240,170" size="2,2"/>
      <Ellipse center="360,190" size="2,2"/>
      <Ellipse center="470,160" size="3,3"/>
      <Ellipse center="560,210" size="2,2"/>
      <Ellipse center="660,180" size="2,2"/>
      <Ellipse center="740,230" size="2,2"/>
      <Ellipse center="210,240" size="2,2"/>
      <Ellipse center="540,240" size="2,2"/>
      <Fill color="#7DD3FC88"/>
    </Group>
  </Layer>

  <Layer name="StarBursts">
    <Layer>
      <Polystar center="220,130" type="star" pointCount="4" outerRadius="8" innerRadius="3" rotation="45"/>
      <Fill color="#FFFFFFEE"/>
      <DropShadowStyle blurX="6" blurY="6" color="#7DD3FC80"/>
    </Layer>
    <Layer>
      <Polystar center="620,210" type="star" pointCount="4" outerRadius="7" innerRadius="3" rotation="45"/>
      <Fill color="#FFFFFFDD"/>
      <DropShadowStyle blurX="6" blurY="6" color="#C084FC80"/>
    </Layer>
    <Layer>
      <Polystar center="700,420" type="star" pointCount="4" outerRadius="6" innerRadius="2" rotation="45"/>
      <Fill color="#FFFFFFCC"/>
      <DropShadowStyle blurX="5" blurY="5" color="#22D3EE80"/>
    </Layer>
  </Layer>

  <!-- ==================== HUD Rings ==================== -->
  <Layer name="HudRings" x="400" y="310" alpha="0.7">
    <Layer>
      <Ellipse size="480,480"/>
      <Stroke color="#1E3A8A" width="2" dashes="10,12"/>
    </Layer>
    <Layer>
      <Ellipse size="420,420"/>
      <TrimPath start="0.1" end="0.4" offset="40"/>
      <Stroke width="4" cap="round">
        <ConicGradient>
          <ColorStop offset="0" color="#38BDF8"/>
          <ColorStop offset="0.5" color="#22D3EE"/>
          <ColorStop offset="1" color="#7C3AED"/>
        </ConicGradient>
      </Stroke>
    </Layer>
    <Layer>
      <Ellipse size="360,360"/>
      <TrimPath start="0.55" end="0.85" offset="-30"/>
      <Stroke width="3" cap="round" color="#38BDF8" alpha="0.8"/>
    </Layer>
  </Layer>

  <!-- ==================== Main Panel ==================== -->
  <Layer name="MainPanel">
    <Rectangle center="400,310" size="680,360" roundness="28"/>
    <Fill>
      <LinearGradient startPoint="60,310" endPoint="740,310">
        <ColorStop offset="0" color="#0F1C3DCC"/>
        <ColorStop offset="0.5" color="#12244ACC"/>
        <ColorStop offset="1" color="#0C1B33CC"/>
      </LinearGradient>
    </Fill>
    <Stroke width="1.5">
      <LinearGradient startPoint="60,310" endPoint="740,310">
        <ColorStop offset="0" color="#3B82F6AA"/>
        <ColorStop offset="0.5" color="#22D3EE88"/>
        <ColorStop offset="1" color="#A78BFAAA"/>
      </LinearGradient>
    </Stroke>
    <BackgroundBlurStyle blurX="24" blurY="24" tileMode="mirror"/>
    <DropShadowStyle offsetY="12" blurX="20" blurY="20" color="#00000066"/>
  </Layer>

  <!-- ==================== Top Bar ==================== -->
  <Layer name="TopBar">
    <Rectangle center="400,65" size="760,90" roundness="22"/>
    <Fill>
      <LinearGradient startPoint="20,65" endPoint="780,65">
        <ColorStop offset="0" color="#0C1B33EE"/>
        <ColorStop offset="1" color="#10284CEE"/>
      </LinearGradient>
    </Fill>
    <Stroke width="1">
      <LinearGradient startPoint="20,65" endPoint="780,65">
        <ColorStop offset="0" color="#1D4ED8AA"/>
        <ColorStop offset="1" color="#38BDF8AA"/>
      </LinearGradient>
    </Stroke>
    <DropShadowStyle offsetY="8" blurX="16" blurY="16" color="#00000055"/>
  </Layer>

  <Layer name="Avatar">
    <Group>
      <Polystar center="70,65" type="polygon" pointCount="6" outerRadius="36" rotation="30"/>
      <Stroke width="2">
        <LinearGradient startPoint="34,29" endPoint="106,101">
          <ColorStop offset="0" color="#38BDF8"/>
          <ColorStop offset="1" color="#A78BFA"/>
        </LinearGradient>
      </Stroke>
    </Group>
    <Group>
      <Ellipse center="70,65" size="56,56"/>
      <Fill color="#0B1225"/>
    </Group>
    <Group>
      <Ellipse center="70,65" size="48,48"/>
      <Fill>
        <RadialGradient center="70,65" radius="30">
          <ColorStop offset="0" color="#38BDF8AA"/>
          <ColorStop offset="1" color="#0B122500"/>
        </RadialGradient>
      </Fill>
    </Group>
    <DropShadowStyle blurX="10" blurY="10" color="#1D4ED880"/>
  </Layer>

  <Layer name="PlayerName">
    <Text text="NEBULA CADET" fontFamily="Arial" fontStyle="Bold" fontSize="16"/>
    <TextLayout position="120,45" textAlign="start"/>
    <Fill color="#E2E8F0"/>
  </Layer>

  <Layer name="PlayerIdRow">
    <Layer name="PlayerId">
      <Text text="#A17X9" fontFamily="Arial" fontSize="11"/>
      <TextLayout position="120,63" textAlign="start"/>
      <Fill color="#7DD3FC"/>
    </Layer>
    <Layer name="LevelBadge">
      <Rectangle center="195,60" size="48,14" roundness="7"/>
      <Fill>
        <LinearGradient startPoint="171,60" endPoint="219,60">
          <ColorStop offset="0" color="#0EA5E9"/>
          <ColorStop offset="1" color="#22D3EE"/>
        </LinearGradient>
      </Fill>
      <Stroke color="#7DD3FC" width="1"/>
      <Layer>
        <Text text="LV 27" fontFamily="Arial" fontStyle="Bold" fontSize="9"/>
        <TextLayout position="195,64" textAlign="center"/>
        <Fill color="#E0F2FE"/>
      </Layer>
    </Layer>
  </Layer>

  <Layer name="ExpBar">
    <Group>
      <Rectangle center="260,79" size="280,10" roundness="5"/>
      <Fill color="#1E293B"/>
    </Group>
    <Group>
      <Rectangle center="215,79" size="190,6" roundness="3"/>
      <Fill>
        <LinearGradient startPoint="120,79" endPoint="310,79">
          <ColorStop offset="0" color="#22D3EE"/>
          <ColorStop offset="1" color="#3B82F6"/>
        </LinearGradient>
      </Fill>
    </Group>
  </Layer>

  <Layer name="EnergyBar">
    <Group>
      <Rectangle center="260,95" size="280,10" roundness="5"/>
      <Fill color="#172437"/>
    </Group>
    <Group>
      <Rectangle center="200,95" size="160,6" roundness="3"/>
      <Fill>
        <LinearGradient startPoint="120,95" endPoint="280,95">
          <ColorStop offset="0" color="#34D399"/>
          <ColorStop offset="1" color="#10B981"/>
        </LinearGradient>
      </Fill>
    </Group>
  </Layer>

  <Layer name="CoinChip">
    <Rectangle center="691,48" size="150,28" roundness="14"/>
    <Fill>
      <LinearGradient startPoint="616,48" endPoint="766,48">
        <ColorStop offset="0" color="#0F1A33CC"/>
        <ColorStop offset="1" color="#14244ACC"/>
      </LinearGradient>
    </Fill>
    <Stroke color="#334155" width="1"/>
    <Group>
      <Ellipse center="636,48" size="16,16"/>
      <Fill>
        <LinearGradient startPoint="628,48" endPoint="644,48">
          <ColorStop offset="0" color="#FFE29A"/>
          <ColorStop offset="1" color="#F59E0B"/>
        </LinearGradient>
      </Fill>
    </Group>
    <Group>
      <Ellipse center="636,48" size="8,8"/>
      <Stroke color="#F8D477" width="1"/>
    </Group>
    <Layer>
      <Text text="12,450" fontFamily="Arial" fontStyle="Bold" fontSize="13"/>
      <TextLayout position="656,52" textAlign="start"/>
      <Fill color="#F8FAFC"/>
    </Layer>
  </Layer>

  <Layer name="DiamondChip">
    <Rectangle center="691,82" size="150,28" roundness="14"/>
    <Fill>
      <LinearGradient startPoint="616,82" endPoint="766,82">
        <ColorStop offset="0" color="#0F1A33CC"/>
        <ColorStop offset="1" color="#14244ACC"/>
      </LinearGradient>
    </Fill>
    <Stroke color="#334155" width="1"/>
    <Layer x="628" y="74">
      <Path data="@IconDiamond"/>
      <Fill>
        <LinearGradient startPoint="0,8" endPoint="16,8">
          <ColorStop offset="0" color="#7DD3FC"/>
          <ColorStop offset="1" color="#A78BFA"/>
        </LinearGradient>
      </Fill>
    </Layer>
    <Layer>
      <Text text="860" fontFamily="Arial" fontStyle="Bold" fontSize="13"/>
      <TextLayout position="656,86" textAlign="start"/>
      <Fill color="#F8FAFC"/>
    </Layer>
  </Layer>

  <!-- ==================== Buttons ==================== -->
  <Layer name="StartButton">
    <Rectangle center="400,218" size="340,78" roundness="28"/>
    <Fill>
      <LinearGradient startPoint="230,218" endPoint="570,218">
        <ColorStop offset="0" color="#00FFC6"/>
        <ColorStop offset="0.5" color="#00C2FF"/>
        <ColorStop offset="1" color="#00FFA6"/>
      </LinearGradient>
    </Fill>
    <Stroke color="#5AFADF" width="2"/>
    <Layer>
      <Rectangle center="400,208" size="310,34" roundness="18"/>
      <Fill color="#FFFFFF22"/>
    </Layer>
    <Layer x="280" y="206">
      <Path data="@IconPlay"/>
      <Fill color="#ECFEFF"/>
    </Layer>
    <Layer>
      <Text text="START" fontFamily="Arial" fontStyle="Bold" fontSize="24"/>
      <TextLayout position="400,227" textAlign="center"/>
      <Fill color="#F0FFFD"/>
      <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#0F172A80"/>
    </Layer>
    <DropShadowStyle offsetY="12" blurX="18" blurY="18" color="#00FFC066"/>
  </Layer>

  <Layer name="ShopButton">
    <Rectangle center="400,310" size="340,78" roundness="28"/>
    <Fill>
      <LinearGradient startPoint="230,310" endPoint="570,310">
        <ColorStop offset="0" color="#FFD166"/>
        <ColorStop offset="1" color="#FF8C42"/>
      </LinearGradient>
    </Fill>
    <Stroke color="#FFD166" width="1.5"/>
    <Layer x="280" y="298">
      <Path data="@IconCart"/>
      <Fill color="#FFF7ED"/>
    </Layer>
    <Layer>
      <Text text="SHOP" fontFamily="Arial" fontStyle="Bold" fontSize="22"/>
      <TextLayout position="400,319" textAlign="center"/>
      <Fill color="#FFF7ED"/>
    </Layer>
    <DropShadowStyle offsetY="10" blurX="16" blurY="16" color="#F59E0B66"/>
  </Layer>

  <Layer name="RankButton">
    <Rectangle center="400,402" size="340,78" roundness="28"/>
    <Fill>
      <LinearGradient startPoint="230,402" endPoint="570,402">
        <ColorStop offset="0" color="#7C3AED"/>
        <ColorStop offset="1" color="#EC4899"/>
      </LinearGradient>
    </Fill>
    <Stroke color="#C084FC" width="1.5"/>
    <Layer x="280" y="390">
      <Path data="@IconCrown"/>
      <Fill color="#FDF2F8"/>
    </Layer>
    <Layer>
      <Text text="RANK" fontFamily="Arial" fontStyle="Bold" fontSize="22"/>
      <TextLayout position="400,411" textAlign="center"/>
      <Fill color="#FDF2F8"/>
    </Layer>
    <DropShadowStyle offsetY="10" blurX="16" blurY="16" color="#C084FC66"/>
  </Layer>

  <!-- ==================== Bottom Bar ==================== -->
  <Layer name="BottomBar">
    <Rectangle center="400,545" size="760,70" roundness="20"/>
    <Fill>
      <LinearGradient startPoint="20,545" endPoint="780,545">
        <ColorStop offset="0" color="#0B1A33CC"/>
        <ColorStop offset="1" color="#10284ECC"/>
      </LinearGradient>
    </Fill>
    <Stroke color="#1D4ED8AA" width="1"/>
  </Layer>

  <Layer name="BottomButtonSettings">
    <Layer composition="@navButton" x="217" y="522"/>
    <Layer x="228" y="533">
      <Group>
        <Polystar center="12,12" type="star" pointCount="8" outerRadius="10" innerRadius="6"/>
        <Stroke color="#93C5FD" width="2"/>
      </Group>
      <Group>
        <Ellipse center="12,12" size="6,6"/>
        <Fill color="#93C5FD"/>
      </Group>
    </Layer>
  </Layer>

  <Layer name="BottomButtonHelp">
    <Layer composition="@navButton" x="377" y="522"/>
    <Layer x="388" y="533">
      <Path data="@IconQuestion"/>
      <Fill color="#93C5FD"/>
    </Layer>
  </Layer>

  <Layer name="BottomButtonShare">
    <Layer composition="@navButton" x="537" y="522"/>
    <Layer x="548" y="533">
      <Path data="@IconShare"/>
      <Fill color="#93C5FD"/>
    </Layer>
  </Layer>

  <!-- ==================== Decorations ==================== -->
  <Layer name="TechLines" alpha="0.6">
    <Group>
      <Path data="M 80 130 L 160 130 L 200 170"/>
      <Stroke color="#38BDF8" width="1"/>
    </Group>
    <Group>
      <Path data="M 720 130 L 640 130 L 600 170"/>
      <Stroke color="#A78BFA" width="1"/>
    </Group>
    <Group>
      <Path data="M 90 460 L 170 460 L 210 420"/>
      <Stroke color="#22D3EE" width="1"/>
    </Group>
    <Group>
      <Path data="M 710 460 L 630 460 L 590 420"/>
      <Stroke color="#C084FC" width="1"/>
    </Group>
  </Layer>

  <Layer name="HexDecor" alpha="0.6">
    <Group>
      <Polystar center="120,200" type="polygon" pointCount="6" outerRadius="14" rotation="30"/>
      <Stroke color="#1D4ED8" width="1"/>
    </Group>
    <Group>
      <Polystar center="680,240" type="polygon" pointCount="6" outerRadius="12" rotation="30"/>
      <Stroke color="#38BDF8" width="1"/>
    </Group>
    <Group>
      <Polystar center="180,420" type="polygon" pointCount="6" outerRadius="10" rotation="30"/>
      <Stroke color="#22D3EE" width="1"/>
    </Group>
    <Group>
      <Polystar center="640,420" type="polygon" pointCount="6" outerRadius="12" rotation="30"/>
      <Stroke color="#A78BFA" width="1"/>
    </Group>
  </Layer>

  <!-- ==================== Resources ==================== -->
  <Resources>
    <PathData id="IconPlay" data="M 0 0 L 24 12 L 0 24 Z"/>
    <PathData id="IconCart" data="M 5 6 L 20 6 L 18 14 L 7 14 Z M 9 18 A 2 2 0 1 0 9 22 A 2 2 0 1 0 9 18 Z M 16 18 A 2 2 0 1 0 16 22 A 2 2 0 1 0 16 18 Z"/>
    <PathData id="IconCrown" data="M 2 16 L 6 6 L 12 12 L 18 6 L 22 16 Z M 4 16 L 4 20 L 20 20 L 20 16 Z"/>
    <PathData id="IconDiamond" data="M 8 0 L 16 4 L 16 12 L 8 16 L 0 12 L 0 4 Z"/>
    <PathData id="IconQuestion" data="M 12 4 C 8 4 6 6 6 9 L 9 9 C 9 7.5 10 6.5 12 6.5 C 14 6.5 15 7.5 15 9 C 15 10.5 13.5 11 12 12 C 10.5 13 10 14 10 16 L 14 16 C 14 14.5 14.5 13.5 16 12.5 C 18 11.5 19 10 19 8 C 19 5 16 4 12 4 Z M 12 18 A 2 2 0 1 0 12 22 A 2 2 0 1 0 12 18 Z"/>
    <PathData id="IconShare" data="M 14 4 L 22 12 L 14 20 L 14 15 L 4 15 L 4 9 L 14 9 Z"/>
    <!-- Navigation Button Shell -->
    <Composition id="navButton" width="46" height="46">
      <Layer>
        <Ellipse center="23,23" size="46,46"/>
        <Fill>
          <LinearGradient startPoint="0,23" endPoint="46,23">
            <ColorStop offset="0" color="#0D2038CC"/>
            <ColorStop offset="1" color="#162B4DCC"/>
          </LinearGradient>
        </Fill>
        <Stroke color="#3B82F6" width="1"/>
      </Layer>
    </Composition>
  </Resources>
</pagx>
```

> 📄 [示例](samples/B.3_nebula_cadet.pagx) | [预览](https://pag.io/pagx/?file=./samples/B.3_nebula_cadet.pagx)

### B.4 游戏 HUD

一个游戏平视显示器（HUD），展示了血条、分数显示和游戏界面元素。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="800" height="600">
  <Resources>
    <PathData id="bullet" data="M 0 0 L 4 0 L 6 4 L 4 12 L 0 12 L -2 4 Z"/>
    <PathData id="healthArc" data="M -180 100 A 200 200 0 0 0 -180 -100"/>
    <PathData id="energyArc" data="M 180 -100 A 200 200 0 0 0 180 100"/>
  </Resources>

  <!-- Mask Definitions -->
  <Layer id="triangleMask" visible="false">
    <Polystar type="polygon" pointCount="3" outerRadius="80" rotation="90"/>
    <Fill color="#FFF"/>
  </Layer>

  <!-- Background Layer -->
  <Layer name="Background">
    <!-- Base Gradient -->
    <Layer>
      <Rectangle center="400,300" size="800,600"/>
      <Fill>
        <RadialGradient center="400,300" radius="500">
          <ColorStop offset="0" color="#001122"/>
          <ColorStop offset="0.7" color="#000811"/>
          <ColorStop offset="1" color="#000"/>
        </RadialGradient>
      </Fill>
    </Layer>

    <!-- Hex Grid Pattern (3 sets of parallel lines forming hexagonal grid) -->
    <Layer alpha="0.15" scrollRect="0,0,800,600">
      <!-- Horizontal lines -->
      <Rectangle center="400,0" size="800,1"/>
      <Repeater copies="35" position="0,17"/>
      <!-- 60° diagonal lines -->
      <Group position="400,300" rotation="60">
        <Rectangle center="0,-510" size="1200,1"/>
        <Repeater copies="60" position="0,17"/>
      </Group>
      <!-- -60° diagonal lines -->
      <Group position="400,300" rotation="-60">
        <Rectangle center="0,-510" size="1200,1"/>
        <Repeater copies="60" position="0,17"/>
      </Group>
      <Fill color="#0066AA"/>
    </Layer>

    <!-- Vignette -->
    <Layer>
      <Rectangle center="400,300" size="800,600"/>
      <Fill>
        <RadialGradient center="400,300" radius="600">
          <ColorStop offset="0.6" color="#00000000"/>
          <ColorStop offset="1" color="#00000080"/>
        </RadialGradient>
      </Fill>
    </Layer>
  </Layer>

  <!-- Central Reticle Complex -->
  <Layer name="ReticleComplex" x="400" y="300">
    <!-- Outer Scale Ring (Tick Marks) -->
    <Layer>
      <Rectangle center="0,-220" size="2,15"/>
      <Fill color="#00CCFF" alpha="0.6"/>
      <Repeater copies="60" rotation="6"/>
    </Layer>

    <!-- Secondary Scale (Smaller Ticks, offset to skip major tick positions) -->
    <Layer>
      <Rectangle center="0,-215" size="1,8"/>
      <Fill color="#00CCFF" alpha="0.3"/>
      <Repeater copies="60" rotation="6" offset="0.5"/>
    </Layer>

    <!-- Rotating Tech Ring 1 -->
    <Layer rotation="15">
      <Ellipse size="380,380"/>
      <Stroke width="1" color="#0FF" dashes="60,40" alpha="0.4"/>
    </Layer>

    <!-- Rotating Tech Ring 2 (Counter-rotate) -->
    <Layer rotation="-20">
      <Ellipse size="360,360"/>
      <Stroke width="2" color="#0088FF" dashes="10,80" alpha="0.5"/>
    </Layer>

    <!-- Main HUD Ring -->
    <Layer>
      <Ellipse size="300,300"/>
      <Stroke width="3" color="#00CCFF"/>
      <DropShadowStyle blurX="8" blurY="8" color="#00CCFF80"/>
    </Layer>

    <!-- Center Triangle Target -->
    <Layer>
      <Polystar type="polygon" pointCount="3" outerRadius="80" rotation="90"/>
      <Stroke width="4" color="#FFAA00"/>
      <Fill>
        <RadialGradient radius="80">
          <ColorStop offset="0" color="#FFAA0040"/>
          <ColorStop offset="1" color="#FFAA0010"/>
        </RadialGradient>
      </Fill>
      <DropShadowStyle blurX="25" blurY="25" color="#FFAA00"/>
    </Layer>

    <!-- Inner scanning lines inside triangle (Masked) -->
    <Layer mask="@triangleMask">
      <Rectangle center="0,0" size="100,100"/>
      <Fill>
        <LinearGradient startPoint="-50,0" endPoint="50,0">
          <ColorStop offset="0" color="#FFAA0000"/>
          <ColorStop offset="0.5" color="#FFAA0040"/>
          <ColorStop offset="1" color="#FFAA0000"/>
        </LinearGradient>
      </Fill>
    </Layer>

    <!-- Center Dot with crosshair lines -->
    <Layer>
      <Ellipse size="6,6"/>
      <Fill color="#FFF"/>
      <Path data="M -20 0 L 20 0 M 0 -20 L 0 20"/>
      <Stroke width="1" color="#FFF" alpha="0.5"/>
    </Layer>
  </Layer>

  <!-- Left Side: Health & Systems -->
  <Layer name="LeftSide" x="400" y="300">
    <!-- Curved Health Bar Track -->
    <Layer>
      <Path data="@healthArc"/>
      <Stroke width="24" color="#002233" cap="butt"/>
    </Layer>

    <!-- Segmented Health Bar -->
    <Layer>
      <Path data="@healthArc"/>
      <Stroke width="20" cap="butt" dashes="4,2">
        <LinearGradient startPoint="-180,100" endPoint="-180,-100">
          <ColorStop offset="0" color="#F00"/>
          <ColorStop offset="0.5" color="#FF0"/>
          <ColorStop offset="1" color="#0F0"/>
        </LinearGradient>
      </Stroke>
      <TrimPath end="0.85"/>
      <DropShadowStyle blurX="5" blurY="5" color="#FF000040"/>
    </Layer>

    <!-- Health Text -->
    <Layer>
      <Text text="100%" fontFamily="Arial" fontSize="24" fontStyle="Bold" position="-230,0"/>
      <Fill color="#FFF"/>
      <TextLayout position="-240,10" textAlign="end"/>
    </Layer>
    <Layer>
      <Text text="STRUCTURAL INTEGRITY" fontFamily="Arial" fontSize="10" position="-230,20"/>
      <Fill color="#0088FF"/>
      <TextLayout position="-240,25" textAlign="end"/>
    </Layer>
  </Layer>

  <!-- System Stats (Moved to Top Left Corner) -->
  <Layer name="SystemStats" x="50" y="60">
    <!-- Individual Data Lines -->
    <Layer>
      <Text text="PWR: 88%" fontFamily="Arial" fontSize="12"/>
      <Fill color="#00CCFF" alpha="0.7"/>
      <TextLayout textAlign="start"/>
    </Layer>
    <Layer>
      <Text text="RADAR: ACT" fontFamily="Arial" fontSize="12"/>
      <Fill color="#00CCFF" alpha="0.7"/>
      <TextLayout position="0,18" textAlign="start"/>
    </Layer>
    <Layer>
      <Text text="SHIELD: OK" fontFamily="Arial" fontSize="12"/>
      <Fill color="#00CCFF" alpha="0.7"/>
      <TextLayout position="0,36" textAlign="start"/>
    </Layer>
  </Layer>

  <!-- Right Side: Energy & Ammo -->
  <Layer name="RightSide" x="400" y="300">
    <!-- Curved Energy Bar Track -->
    <Layer>
      <Path data="@energyArc"/>
      <Stroke width="24" color="#002233" cap="butt"/>
    </Layer>

    <!-- Segmented Energy Bar -->
    <Layer>
      <Path data="@energyArc"/>
      <Stroke width="20" cap="butt" dashes="10,2">
        <LinearGradient startPoint="180,-100" endPoint="180,100">
          <ColorStop offset="0" color="#0FF"/>
          <ColorStop offset="1" color="#0044FF"/>
        </LinearGradient>
      </Stroke>
      <TrimPath end="0.6"/>
      <DropShadowStyle blurX="5" blurY="5" color="#00FFFF40"/>
    </Layer>

    <!-- Energy Text -->
    <Layer>
      <Text text="60%" fontFamily="Arial" fontSize="24" fontStyle="Bold" position="230,0"/>
      <Fill color="#FFF"/>
      <TextLayout position="240,10" textAlign="start"/>
    </Layer>
    <Layer>
      <Text text="PLASMA RESERVES" fontFamily="Arial" fontSize="10" position="230,20"/>
      <Fill color="#0088FF"/>
      <TextLayout position="240,25" textAlign="start"/>
    </Layer>
  </Layer>

  <!-- Bottom: Weapon & Radar -->
  <Layer name="Bottom" x="400" y="520">
    <!-- Complex Frame -->
    <Layer>
      <Path data="M -200 0 L -180 -40 L 180 -40 L 200 0 L 180 20 L -180 20 Z"/>
      <Fill color="#001122" alpha="0.95"/>
      <Stroke color="#0066AA" width="2"/>
      <DropShadowStyle blurX="15" blurY="15" color="#000"/>
    </Layer>

    <!-- Weapon Name -->
    <Layer>
      <Text text="MK-IV RAILGUN" fontFamily="Arial" fontSize="20" fontStyle="Bold"/>
      <Fill color="#FFF"/>
      <TextLayout position="0,-15" textAlign="center"/>
    </Layer>

    <!-- Fire Mode -->
    <Layer>
      <Text text="[ BURST MODE ]" fontFamily="Arial" fontSize="12"/>
      <Fill color="#FFAA00"/>
      <TextLayout position="0,5" textAlign="center"/>
    </Layer>

    <!-- Decorative Tech Bits under frame -->
    <Layer y="30">
      <Layer>
        <Rectangle center="0,0" size="40,4"/>
        <Fill color="#004488"/>
      </Layer>
      <Layer>
        <Rectangle center="-30,0" size="10,4"/>
        <Rectangle center="30,0" size="10,4"/>
        <Fill color="#002244"/>
      </Layer>
    </Layer>
  </Layer>

  <!-- Minimap / Radar (Bottom Left) -->
  <Layer name="Radar" x="100" y="500">
    <!-- Background -->
    <Layer>
      <Ellipse size="140,140"/>
      <Fill color="#001122" alpha="0.9"/>
      <Stroke color="#0066AA" width="2"/>
    </Layer>
    <!-- Grid Circles & Crosshairs -->
    <Layer>
      <Ellipse size="100,100"/>
      <Ellipse size="50,50"/>
      <Path data="M -70 0 L 70 0 M 0 -70 L 0 70"/>
      <Stroke color="#004488" width="1"/>
    </Layer>
    <!-- Sweep (Conic Gradient) -->
    <Layer rotation="-45">
      <Ellipse size="130,130"/>
      <Fill>
        <ConicGradient startAngle="0" endAngle="90">
          <ColorStop offset="0" color="#00FF0000"/>
          <ColorStop offset="1" color="#00FF0080"/>
        </ConicGradient>
      </Fill>
    </Layer>
    <!-- Enemy Blips -->
    <Layer>
      <Ellipse center="30,-20" size="6,6"/>
      <Ellipse center="-40,10" size="6,6"/>
      <Fill color="#F00"/>
      <DropShadowStyle blurX="4" blurY="4" color="#F00"/>
    </Layer>
    <!-- Player Blip -->
    <Layer>
      <Ellipse size="4,4"/>
      <Fill color="#FFF"/>
    </Layer>
  </Layer>

  <!-- Objective Markers (Top Center) -->
  <Layer name="Objectives" x="400" y="50">
    <Text fontFamily="Arial" fontSize="12" fontStyle="Bold">
<![CDATA[MISSION OBJECTIVES:
[ ] INFILTRATE BASE
[ ] HACK TERMINAL
[x] SECURE PERIMETER]]>
    </Text>
    <Fill color="#FFF"/>
    <TextLayout textAlign="center" lineHeight="1.6"/>
    <DropShadowStyle blurX="2" blurY="2" color="#000"/>
  </Layer>

  <!-- Yellow Status Indicators (Restored in Top Right) -->
  <Layer name="StatusIcons" x="706" y="70">
    <Rectangle size="8,8"/>
    <Stroke color="#FFAA00" width="1"/>
    <Repeater copies="3" position="15,0"/>
    <DropShadowStyle blurX="5" blurY="5" color="#FFAA00"/>
  </Layer>

  <!-- Ammo Display (Moved to Bottom Right) -->
  <Layer name="AmmoDisplay" x="630" y="496">
    <Layer>
      <!-- Bullet Icon -->
      <Path data="@bullet"/>
      <Fill color="#FFAA00"/>
      <Stroke color="#000" width="1"/>
      <Repeater copies="10" position="10,0"/>
    </Layer>
    <!-- Row 2 -->
    <Layer y="15">
      <Layer>
        <!-- Bullet Icon -->
        <Path data="@bullet"/>
        <Fill color="#FFAA00"/>
        <Stroke color="#000" width="1"/>
        <Repeater copies="5" position="10,0"/>
      </Layer>
      <Layer>
        <!-- Depleted Bullets -->
        <Path data="@bullet"/>
        <Fill color="#331100"/>
        <Stroke color="#442200" width="1"/>
        <Repeater copies="5" position="10,0" offset="5"/>
      </Layer>
    </Layer>
  </Layer>

  <!-- Corners -->
  <Layer name="Corners">
    <Path data="M 40 100 L 40 40 L 100 40 M 760 100 L 760 40 L 700 40 M 40 500 L 40 560 L 100 560 M 760 500 L 760 560 L 700 560"/>
    <Stroke width="4" color="#00CCFF"/>
  </Layer>

</pagx>
```

> 📄 [示例](samples/B.4_game_hud.pagx) | [预览](https://pag.io/pagx/?file=./samples/B.4_game_hud.pagx)

### B.5 PAGX 特性概览

PAGX 格式能力的综合展示，包括渐变、效果、文本样式和矢量图形。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="1600" height="1200">

  <!-- Background -->
  <Layer name="Background">
    <Rectangle center="800,600" size="1600,1200"/>
    <Fill color="@bgGradient"/>
  </Layer>

  <!-- Background aurora glow (2 compact blobs, cool contrast) -->
  <Layer name="GlowB" alpha="0.22">
    <Ellipse center="1560,300" size="700,550"/>
    <Fill color="#38BDF8"/>
    <BlurFilter blurX="220" blurY="200"/>
  </Layer>
  <Layer name="GlowC" alpha="0.18">
    <Ellipse center="350,1050" size="500,350"/>
    <Fill color="#10B981"/>
    <BlurFilter blurX="180" blurY="130"/>
  </Layer>


  <!-- Subtle orbit ring decoration -->
  <Layer name="OrbitRing" x="800" y="530" alpha="0.18">
    <Ellipse size="760,760"/>
    <Stroke color="#334155" width="1"/>
  </Layer>


  <!-- Background grid pattern -->
  <Layer name="Grid" alpha="0.05">
    <Group position="60,60">
      <Path data="M -4 0 L 4 0 M 0 -4 L 0 4"/>
      <Stroke color="#475569" width="1" cap="round"/>
      <Repeater copies="13" position="120,0"/>
    </Group>
    <Repeater copies="10" position="0,120"/>
  </Layer>

  <!-- Core -->
  <Layer name="Core" x="800" y="530">
    <!-- Glow halo -->
    <Layer alpha="0.2">
      <Ellipse size="320,320"/>
      <Fill color="#3B82F6"/>
      <BlurFilter blurX="80" blurY="80"/>
    </Layer>
    <!-- Main ring -->
    <Layer>
      <Ellipse size="260,260"/>
      <Stroke color="#475569" width="2"/>
    </Layer>
    <!-- Gradient arc decoration -->
    <Layer alpha="0.7">
      <Ellipse size="310,310"/>
      <TrimPath end="0.77"/>
      <Stroke width="2.5" cap="round">
        <ConicGradient>
          <ColorStop offset="0" color="#06B6D400"/>
          <ColorStop offset="0.77" color="#06B6D4"/>
          <ColorStop offset="1" color="#06B6D400"/>
        </ConicGradient>
      </Stroke>
    </Layer>
    <!-- Small tick marks at cardinal directions -->
    <Layer alpha="0.4">
      <Path data="M 0 -155 L 0 -163 M 155 0 L 163 0 M 0 155 L 0 163 M -155 0 L -163 0"/>
      <Stroke color="#94A3B8" width="1.5" cap="round"/>
    </Layer>
    <!-- PAGX title -->
    <Layer x="0" y="-10">
      <Text text="PAGX" fontFamily="Arial" fontStyle="Bold" fontSize="52"/>
      <TextLayout position="0,10" textAlign="center"/>
      <Fill color="#FFF"/>
      <DropShadowStyle offsetY="4" blurX="20" blurY="20" color="#06B6D440"/>
    </Layer>
    <!-- Subtitle -->
    <Layer x="0" y="42">
      <Text text="Portable Animated Graphics XML" fontFamily="Arial" fontSize="14"/>
      <TextLayout position="0,0" textAlign="center"/>
      <Fill color="#94A3B8"/>
    </Layer>
    <!-- Curved definition caption -->
    <Layer x="0" y="0">
      <Text text="An XML-based markup language for describing animated vector graphics." fontFamily="Arial" fontSize="12"/>
      <TextPath path="M -140 0 A 140 140 0 0 0 140 0" forceAlignment="true"/>
      <Fill color="#64748B60"/>
    </Layer>
  </Layer>

  <!-- Connector endpoint dots -->
  <Layer name="ConnectorDots" alpha="0.7">
    <Ellipse center="800,375" size="6,6"/>
    <Ellipse center="954,511" size="6,6"/>
    <Ellipse center="903,646" size="6,6"/>
    <Ellipse center="697,646" size="6,6"/>
    <Ellipse center="646,511" size="6,6"/>
    <Fill color="#06B6D4"/>
  </Layer>

  <!-- Connectors -->
  <Layer name="Connectors" alpha="0.35">
    <Path data="M 800 375 L 800 240 M 954 511 L 1119 466 M 903 646 L 989 790 M 697 646 L 611 790 M 646 511 L 481 466"/>
    <Stroke color="@accentLine" width="2" dashes="8,14"/>
  </Layer>

  <!-- Card 1: Readable (top center) -->
  <Layer name="CardReadable" x="800" y="160">
    <Rectangle center="0,0" size="400,160" roundness="20"/>
    <Fill color="#1E293B80"/>
    <Stroke color="#33415580" width="1"/>
    <DropShadowStyle offsetY="4" blurX="24" blurY="24" color="#00000060"/>
    <Layer alpha="0.08">
      <Path data="M -180 -80 L 180 -80"/>
      <Stroke width="1">
        <LinearGradient startPoint="-180,-80" endPoint="180,-80">
          <ColorStop offset="0" color="#F43F5E00"/>
          <ColorStop offset="0.5" color="#F43F5E"/>
          <ColorStop offset="1" color="#F43F5E00"/>
        </LinearGradient>
      </Stroke>
    </Layer>
    <Layer x="-120" y="0">
      <!-- File outline with folded corner -->
      <Group>
        <Path data="M -22 -32 L 6 -32 L 22 -16 L 22 36 L -22 36 Z"/>
        <Stroke color="#F43F5E" width="2" join="round"/>
        <Fill color="#F43F5E10"/>
      </Group>
      <Group>
        <Path data="M 6 -32 L 6 -16 L 22 -16"/>
        <Stroke color="#F43F5E" width="1.5" join="round"/>
      </Group>
      <!-- </> symbol -->
      <Group>
        <Path data="M -10 -4 L -16 4 L -10 12 M 10 -4 L 16 4 L 10 12"/>
        <Stroke color="#F43F5E" width="2" cap="round" join="round"/>
      </Group>
      <Group>
        <Path data="M 3 -8 L -3 16"/>
        <Stroke color="#F43F5E" width="1.5" cap="round"/>
      </Group>
    </Layer>
    <Layer x="-50" y="-20">
      <Text text="Readable" fontFamily="Arial" fontStyle="Bold" fontSize="24"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#F43F5E"/>
    </Layer>
    <Layer x="-50" y="10">
      <Text text="Plain-text XML, easy to" fontFamily="Arial" fontSize="16"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#CBD5E1"/>
    </Layer>
    <Layer x="-50" y="32">
      <Text text="diff, debug, and AI-generate." fontFamily="Arial" fontSize="16"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#CBD5E1"/>
    </Layer>
  </Layer>

  <!-- Card 2: Comprehensive (right-upper) -->
  <Layer name="CardComprehensive" x="1319" y="466">
    <Rectangle center="0,0" size="400,160" roundness="20"/>
    <Fill color="#1E293B80"/>
    <Stroke color="#33415580" width="1"/>
    <DropShadowStyle offsetY="4" blurX="24" blurY="24" color="#00000060"/>
    <Layer alpha="0.08">
      <Path data="M -180 -80 L 180 -80"/>
      <Stroke width="1">
        <LinearGradient startPoint="-180,-80" endPoint="180,-80">
          <ColorStop offset="0" color="#38BDF800"/>
          <ColorStop offset="0.5" color="#38BDF8"/>
          <ColorStop offset="1" color="#38BDF800"/>
        </LinearGradient>
      </Stroke>
    </Layer>
    <Layer x="-120" y="-2">
      <Group>
        <Rectangle center="-5,-18" size="52,32" roundness="6"/>
        <Stroke color="#38BDF8" width="2"/>
        <Fill color="#38BDF820"/>
      </Group>
      <Group>
        <Rectangle center="5,2" size="52,32" roundness="6"/>
        <Stroke color="#38BDF8" width="2"/>
        <Fill color="#38BDF815"/>
      </Group>
      <Group>
        <Rectangle center="15,22" size="52,32" roundness="6"/>
        <Stroke color="#38BDF8" width="2"/>
        <Fill color="#38BDF810"/>
      </Group>
    </Layer>
    <Layer x="-50" y="-20">
      <Text text="Comprehensive" fontFamily="Arial" fontStyle="Bold" fontSize="24"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#38BDF8"/>
    </Layer>
    <Layer x="-50" y="10">
      <Text text="Covers vectors, images," fontFamily="Arial" fontSize="16"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#CBD5E1"/>
    </Layer>
    <Layer x="-50" y="32">
      <Text text="text, effects, and masks." fontFamily="Arial" fontSize="16"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#CBD5E1"/>
    </Layer>
  </Layer>

  <!-- Card 3: Expressive (left-upper) -->
  <Layer name="CardExpressive" x="281" y="466">
    <Rectangle center="0,0" size="400,160" roundness="20"/>
    <Fill color="#1E293B80"/>
    <Stroke color="#33415580" width="1"/>
    <DropShadowStyle offsetY="4" blurX="24" blurY="24" color="#00000060"/>
    <Layer alpha="0.08">
      <Path data="M -180 -80 L 180 -80"/>
      <Stroke width="1">
        <LinearGradient startPoint="-180,-80" endPoint="180,-80">
          <ColorStop offset="0" color="#A855F700"/>
          <ColorStop offset="0.5" color="#A855F7"/>
          <ColorStop offset="1" color="#A855F700"/>
        </LinearGradient>
      </Stroke>
    </Layer>
    <Layer x="-110" y="-2">
      <!-- Small code block on left -->
      <Group>
        <Rectangle center="-22,0" size="32,72" roundness="5"/>
        <Stroke color="#A855F7" width="2"/>
        <Fill color="#A855F720"/>
      </Group>
      <Group>
        <Path data="M -34 -16 L -10 -16 M -34 0 L -16 0 M -34 16 L -10 16"/>
        <Stroke color="#A855F7" width="1.5" cap="round"/>
      </Group>
      <!-- Expansion arrow -->
      <Group>
        <Path data="M 2 0 L 12 0 M 8 -5 L 12 0 L 8 5"/>
        <Stroke color="#A855F7" width="2" cap="round" join="round"/>
      </Group>
      <!-- Rich output shapes on right -->
      <Group>
        <Ellipse center="22,-28" size="18,18"/>
        <Stroke color="#A855F7" width="1.5"/>
      </Group>
      <Group>
        <Path data="M 15 0 L 22 -7 L 29 0 L 22 7 Z"/>
        <Fill color="#A855F7"/>
      </Group>
      <Group>
        <Path data="M 22 18 L 15 36 L 29 36 Z"/>
        <Stroke color="#A855F7" width="1.5" join="round"/>
        <Fill color="#A855F715"/>
      </Group>
    </Layer>
    <Layer x="-50" y="-20">
      <Text text="Expressive" fontFamily="Arial" fontStyle="Bold" fontSize="24"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#A855F7"/>
    </Layer>
    <Layer x="-50" y="10">
      <Text text="Compact structure for both" fontFamily="Arial" fontSize="16"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#CBD5E1"/>
    </Layer>
    <Layer x="-50" y="32">
      <Text text="static and animated content." fontFamily="Arial" fontSize="16"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#CBD5E1"/>
    </Layer>
  </Layer>

  <!-- Card 4: Interoperable (left-lower) -->
  <Layer name="CardInteroperable" x="499" y="870">
    <Rectangle center="0,0" size="400,160" roundness="20"/>
    <Fill color="#1E293B80"/>
    <Stroke color="#33415580" width="1"/>
    <DropShadowStyle offsetY="4" blurX="24" blurY="24" color="#00000060"/>
    <Layer alpha="0.08">
      <Path data="M -180 -80 L 180 -80"/>
      <Stroke width="1">
        <LinearGradient startPoint="-180,-80" endPoint="180,-80">
          <ColorStop offset="0" color="#10B98100"/>
          <ColorStop offset="0.5" color="#10B981"/>
          <ColorStop offset="1" color="#10B98100"/>
        </LinearGradient>
      </Stroke>
    </Layer>
    <Layer x="-110" y="0">
      <Group>
        <Ellipse center="-18,0" size="40,40"/>
        <Ellipse center="18,0" size="40,40"/>
        <Stroke color="#10B981" width="2"/>
      </Group>
      <Group>
        <Path data="M -2 0 L 2 0"/>
        <Stroke color="#10B981" width="2" cap="round"/>
      </Group>
      <Group>
        <Path data="M -35 -25 L -48 -38 M 35 25 L 48 38"/>
        <Stroke color="#10B981" width="1" dashes="3,3"/>
      </Group>
    </Layer>
    <Layer x="-50" y="-20">
      <Text text="Interoperable" fontFamily="Arial" fontStyle="Bold" fontSize="24"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#10B981"/>
    </Layer>
    <Layer x="-50" y="10">
      <Text text="Bridges AE, Figma, SVG," fontFamily="Arial" fontSize="16"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#CBD5E1"/>
    </Layer>
    <Layer x="-50" y="32">
      <Text text="PDF, and more seamlessly." fontFamily="Arial" fontSize="16"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#CBD5E1"/>
    </Layer>
  </Layer>

  <!-- Card 5: Deployable (right-lower) -->
  <Layer name="CardDeployable" x="1101" y="870">
    <Rectangle center="0,0" size="400,160" roundness="20"/>
    <Fill color="#1E293B80"/>
    <Stroke color="#33415580" width="1"/>
    <DropShadowStyle offsetY="4" blurX="24" blurY="24" color="#00000060"/>
    <Layer alpha="0.08">
      <Path data="M -180 -80 L 180 -80"/>
      <Stroke width="1">
        <LinearGradient startPoint="-180,-80" endPoint="180,-80">
          <ColorStop offset="0" color="#FBBF2400"/>
          <ColorStop offset="0.5" color="#FBBF24"/>
          <ColorStop offset="1" color="#FBBF2400"/>
        </LinearGradient>
      </Stroke>
    </Layer>
    <Layer x="-120" y="-2">
      <!-- Rocket body -->
      <Group>
        <Path data="M 0 -40 C 10 -35 18 -20 18 0 L 18 22 L -18 22 L -18 0 C -18 -20 -10 -35 0 -40 Z"/>
        <Stroke color="#FBBF24" width="2" join="round"/>
        <Fill color="#FBBF2420"/>
      </Group>
      <!-- Fins -->
      <Group>
        <Path data="M -18 12 L -30 30 L -18 25 Z M 18 12 L 30 30 L 18 25 Z"/>
        <Stroke color="#FBBF24" width="2" join="round"/>
        <Fill color="#FBBF2415"/>
      </Group>
      <!-- Window -->
      <Group>
        <Ellipse center="0,-10" size="12,12"/>
        <Stroke color="#FBBF24" width="2"/>
      </Group>
      <!-- Flame -->
      <Group>
        <Path data="M -10 22 L -5 38 L 0 30 L 5 38 L 10 22"/>
        <Stroke color="#FB923C" width="2" cap="round" join="round"/>
      </Group>
    </Layer>
    <Layer x="-50" y="-20">
      <Text text="Deployable" fontFamily="Arial" fontStyle="Bold" fontSize="24"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#FBBF24"/>
    </Layer>
    <Layer x="-50" y="10">
      <Text text="One-click export to binary" fontFamily="Arial" fontSize="16"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#CBD5E1"/>
    </Layer>
    <Layer x="-50" y="32">
      <Text text="PAG with high performance." fontFamily="Arial" fontSize="16"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#CBD5E1"/>
    </Layer>
  </Layer>

  <!-- Pipeline Section -->
  <Layer name="Pipeline" x="800" y="1070">
    <!-- Separator line -->
    <Layer alpha="0.18">
      <Path data="M -400 -40 L 400 -40"/>
      <Stroke width="1">
        <LinearGradient startPoint="-400,0" endPoint="400,0">
          <ColorStop offset="0" color="#6366F100"/>
          <ColorStop offset="0.2" color="#6366F1"/>
          <ColorStop offset="0.5" color="#8B5CF6"/>
          <ColorStop offset="0.8" color="#EC4899"/>
          <ColorStop offset="1" color="#EC489900"/>
        </LinearGradient>
      </Stroke>
    </Layer>
    <!-- .pagx badge -->
    <Layer x="-180" y="0">
      <Rectangle center="0,0" size="120,40" roundness="20"/>
      <Fill color="#6366F120"/>
      <Stroke color="#6366F160" width="1"/>
      <Layer y="5">
        <Text text=".pagx" fontFamily="Arial" fontStyle="Bold" fontSize="16"/>
        <TextLayout position="0,0" textAlign="center"/>
        <Fill color="#A78BFA"/>
      </Layer>
    </Layer>
    <!-- Arrow 1 -->
    <Layer x="-80" y="0">
      <Path data="@arrowRight"/>
      <Stroke color="#64748B" width="2" cap="round" join="round"/>
    </Layer>
    <!-- .pag badge -->
    <Layer x="0" y="0">
      <Rectangle center="0,0" size="100,40" roundness="20"/>
      <Fill color="#EC489920"/>
      <Stroke color="#EC489960" width="1"/>
      <Layer y="5">
        <Text text=".pag" fontFamily="Arial" fontStyle="Bold" fontSize="16"/>
        <TextLayout position="0,0" textAlign="center"/>
        <Fill color="#F472B6"/>
      </Layer>
    </Layer>
    <!-- Arrow 2 -->
    <Layer x="90" y="0">
      <Path data="@arrowRight"/>
      <Stroke color="#64748B" width="2" cap="round" join="round"/>
    </Layer>
    <!-- Production badge -->
    <Layer x="190" y="0">
      <Rectangle center="0,0" size="140,40" roundness="20"/>
      <Fill color="#34D39920"/>
      <Stroke color="#34D39960" width="1"/>
      <Layer y="5">
        <Text text="Production" fontFamily="Arial" fontStyle="Bold" fontSize="16"/>
        <TextLayout position="0,0" textAlign="center"/>
        <Fill color="#34D399"/>
      </Layer>
    </Layer>
  </Layer>

  <!-- Bottom tagline -->
  <Layer name="BottomTagline" x="800" y="1130">
    <Text text="Design  -  Develop  -  Deploy" fontFamily="Arial" fontSize="16"/>
    <TextLayout position="0,0" textAlign="center"/>
    <Fill color="#64748B"/>
  </Layer>

  <!-- Corner decorative brackets -->
  <Layer name="Corners" alpha="0.2">
    <Group>
      <Path data="M 50 110 L 50 50 L 110 50 M 1490 50 L 1550 50 L 1550 110 M 50 1090 L 50 1150 L 110 1150 M 1490 1150 L 1550 1150 L 1550 1090"/>
      <Stroke color="#64748B" width="1" cap="round" join="round"/>
    </Group>
    <Group>
      <Ellipse center="50,50" size="4,4"/>
      <Ellipse center="1550,50" size="4,4"/>
      <Ellipse center="50,1150" size="4,4"/>
      <Ellipse center="1550,1150" size="4,4"/>
      <Fill color="#64748B"/>
    </Group>
  </Layer>

  <Resources>
    <LinearGradient id="bgGradient" startPoint="0,0" endPoint="0,1200">
      <ColorStop offset="0" color="#0F172A"/>
      <ColorStop offset="0.4" color="#0B1228"/>
      <ColorStop offset="1" color="#0F172A"/>
    </LinearGradient>
    <LinearGradient id="accentLine" startPoint="-120,0" endPoint="120,0">
      <ColorStop offset="0" color="#06B6D4"/>
      <ColorStop offset="1" color="#3B82F6"/>
    </LinearGradient>
    <PathData id="arrowRight" data="M -15 0 L 10 0 M 4 -6 L 12 0 L 4 6"/>
  </Resources>
</pagx>
```

> 📄 [示例](samples/B.5_pagx_features.pagx) | [预览](https://pag.io/pagx/?file=./samples/B.5_pagx_features.pagx)

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
