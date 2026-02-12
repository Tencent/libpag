# PAGX 文件结构优化指南

本指南提供一套系统化的排查方法，用于精简 PAGX 文件结构、消除冗余、提高可读性和可维护性。

---

## 一、调整 Resources 位置

### 原则

将 `<Resources>` 节点移到文件末尾（根元素的最后一个子节点），使图层树结构排在前面，优先阅读时能直接
看到内容结构而不需要跳过大段资源定义。

### 判断条件

`<Resources>` 出现在图层树之前。

### 优化手法

将整个 `<Resources>...</Resources>` 块剪切到根元素的闭合标签之前。

### 示例

```xml
<!-- 优化前 -->
<PAGXFile width="800" height="600">
  <Resources>
    <PathData id="arrow" data="M 0 0 L 10 0"/>
    <Composition id="card" width="100" height="80">...</Composition>
  </Resources>
  <Layer name="Main">...</Layer>
</PAGXFile>

<!-- 优化后 -->
<PAGXFile width="800" height="600">
  <Layer name="Main">...</Layer>
  <Resources>
    <PathData id="arrow" data="M 0 0 L 10 0"/>
    <Composition id="card" width="100" height="80">...</Composition>
  </Resources>
</PAGXFile>
```

### 边界条件

无。Resources 的位置不影响渲染结果，纯粹是可读性优化。

---

## 二、移除未使用的资源

### 原则

Resources 中定义但从未被引用的资源是死代码，应当删除。

### 判断条件

在 `<Resources>` 中通过 `id="xxx"` 定义的资源，在文件其余位置找不到任何 `@xxx` 引用。

### 优化手法

直接删除整个资源定义。

### 示例

```xml
<!-- 优化前：bgGradient 定义了但没有任何地方引用 @bgGradient -->
<Resources>
  <LinearGradient id="bgGradient" startPoint="0,0" endPoint="400,0">
    <ColorStop offset="0" color="#FF0000"/>
    <ColorStop offset="1" color="#0000FF"/>
  </LinearGradient>
</Resources>

<!-- 优化后：删除整个 LinearGradient -->
```

### 边界条件

- 带动画的文件中，资源可能被 keyframe 的 `value` 属性引用（如 `value="@gradA"`），搜索时不能只看
  静态属性。
- 资源之间可能存在嵌套引用（如 Composition 内部引用了 PathData），删除被间接引用的资源会导致错误。
  确保搜索覆盖整个文件。

---

## 三、移除冗余的 Group / Layer 包装

### 原则

没有携带任何属性且仅包裹单一内容组的 Group 或 Layer 是多余的中间层，可以展平。

### 判断条件

满足以下全部条件时可移除：

1. 无 `transform` / `position` / `rotation` / `scale` / `alpha` / `blendMode` / `name` / `mask` 等属性
2. 内部只有一组内容（不是多个并列子元素需要隔离的情况）

### 优化手法

将内部内容提升到上一层，删除多余的包装元素。

### 示例

```xml
<!-- 优化前：Layer 内只有一个无属性 Group -->
<Layer x="100" y="200">
  <Group>
    <Rectangle center="50,50" size="100,100"/>
    <Fill color="#FF0000"/>
  </Group>
</Layer>

<!-- 优化后：展平 -->
<Layer x="100" y="200">
  <Rectangle center="50,50" size="100,100"/>
  <Fill color="#FF0000"/>
</Layer>
```

### 边界条件

- **Layer 不可随意去除**：Layer 是承载 styles（DropShadowStyle 等）、filters（BlurFilter 等）、
  mask、composition 引用和 blendMode 的唯一容器。即使 Layer 本身看起来"多余"，如果它携带了这些
  功能，就不能移除。
- **Group 有 transform 时不可去除**：Group 的 transform 会作用于其内部所有内容，移除会改变渲染。
- **Group 的作用域隔离作用**：如果 Group 的存在是为了隔离绘制器作用域（见第四节），则不可移除。

---

## 四、合并共享相同绘制器的几何体

这是最常见、收益最高的优化方向。

### 原则

在 PAGX 中，绘制器（Fill / Stroke）会作用于**当前作用域内所有已累积的几何体**。因此，多个使用完全
相同绘制器的几何体可以放入同一作用域，共享一份绘制器声明。

### 4.1 Path 合并——多 M 子路径

**判断条件**：多条 Path 拥有完全相同的 Fill 和/或 Stroke。

**优化手法**：将多条 Path 的 data 通过多个 `M`（moveto）指令拼接为一条 Path。

```xml
<!-- 优化前：两个 Group，各自 Path + 相同 Fill -->
<Group>
  <Path data="M -15 12 L -18 55 L -8 55 L -5 12 Z"/>
  <Fill color="#2E2545"/>
</Group>
<Group>
  <Path data="M 5 12 L 8 55 L 18 55 L 15 12 Z"/>
  <Fill color="#2E2545"/>
</Group>

<!-- 优化后：一条多 M 路径 + 一个共享 Fill -->
<Group>
  <Path data="M -15 12 L -18 55 L -8 55 L -5 12 Z M 5 12 L 8 55 L 18 55 L 15 12 Z"/>
  <Fill color="#2E2545"/>
</Group>
```

**典型场景**：左右对称的角色部件、四角装饰、重复的小图标。

### 4.2 形状合并——多个 Ellipse / Rectangle 共享绘制器

**判断条件**：多个独立的 Ellipse 或 Rectangle 拥有完全相同的绘制器。

**优化手法**：将它们放入同一个 Group（或直接放在同一 Layer 中），共享一份 Fill / Stroke。

```xml
<!-- 优化前：两个 Group，各自 Ellipse + 相同 Stroke -->
<Group>
  <Ellipse center="23,23" size="46,46"/>
  <Stroke color="#3B82F6" width="1"/>
</Group>
<Group>
  <Ellipse center="69,23" size="46,46"/>
  <Stroke color="#3B82F6" width="1"/>
</Group>

<!-- 优化后：两个 Ellipse 共享一个 Stroke -->
<Group>
  <Ellipse center="23,23" size="46,46"/>
  <Ellipse center="69,23" size="46,46"/>
  <Stroke color="#3B82F6" width="1"/>
</Group>
```

### 4.3 跨 Layer 合并

**判断条件**：多个相邻 Layer 没有各自独立的 styles / filters / mask / blendMode / alpha / name，
且内部几何体使用完全相同的绘制器。

**优化手法**：将多个 Layer 合并为一个，内部几何体共享绘制器。

```xml
<!-- 优化前：3 个 Layer，各自画一个圆 + 相同 Stroke -->
<Layer><Ellipse center="100,100" size="200,200"/><Stroke color="#00FF0040" width="1"/></Layer>
<Layer><Ellipse center="100,100" size="140,140"/><Stroke color="#00FF0040" width="1"/></Layer>
<Layer><Ellipse center="100,100" size="80,80"/><Stroke color="#00FF0040" width="1"/></Layer>

<!-- 优化后：1 个 Layer，3 个 Ellipse 共享 Stroke -->
<Layer>
  <Ellipse center="100,100" size="200,200"/>
  <Ellipse center="100,100" size="140,140"/>
  <Ellipse center="100,100" size="80,80"/>
  <Stroke color="#00FF0040" width="1"/>
</Layer>
```

### 边界条件——绘制器作用域隔离

**这是最容易出错的地方。** 合并后，如果不同几何体需要不同的绘制器，必须用 Group 隔离：

```xml
<!-- 错误！Fill 和 Stroke 都会作用于所有几何体 -->
<Layer>
  <Path data="M 0 0 L 50 50"/>       <!-- 只想要 Stroke -->
  <Ellipse center="25,25" size="10,10"/> <!-- 只想要 Fill -->
  <Stroke color="#FFFFFF" width="1"/>
  <Fill color="#06B6D4"/>
</Layer>

<!-- 正确：用 Group 隔离不同绘制器的几何体 -->
<Layer>
  <Group>
    <Path data="M 0 0 L 50 50"/>
    <Stroke color="#FFFFFF" width="1"/>
  </Group>
  <Group>
    <Ellipse center="25,25" size="10,10"/>
    <Fill color="#06B6D4"/>
  </Group>
</Layer>
```

**判断规则**：合并前逐个检查每个几何体原本拥有的绘制器组合（仅 Fill、仅 Stroke、Fill + Stroke）。
只有绘制器组合完全一致的几何体才能放入同一作用域。

---

## 五、合并同一几何体上的多个绘制器

### 原则

当两个 Group 使用**完全相同的几何体**但施加不同的绘制器（如一个 Fill、一个 Stroke）时，可以合并为
一个 Group，同时拥有两个绘制器。绘制器不会清除几何体列表，后续绘制器可以继续渲染相同几何体。

### 判断条件

两个相邻 Group 内的几何体元素（Path data / Rectangle / Ellipse 参数）完全一致，仅绘制器不同。

### 示例

```xml
<!-- 优化前：重复几何体 -->
<Group>
  <Path data="M 46 -30 L 48 -78 L 44 -78 Z"/>
  <Fill color="#E2E8F0"/>
</Group>
<Group>
  <Path data="M 46 -30 L 48 -78 L 44 -78 Z"/>
  <Stroke color="#FFFFFF40" width="1"/>
</Group>

<!-- 优化后：一份几何体，两个绘制器 -->
<Group>
  <Path data="M 46 -30 L 48 -78 L 44 -78 Z"/>
  <Fill color="#E2E8F0"/>
  <Stroke color="#FFFFFF40" width="1"/>
</Group>
```

### 边界条件

- 仅适用于几何体**完全相同**的情况。如果路径数据有任何差异，不能合并。
- 绘制器的渲染顺序按文档顺序——先声明的在下层。合并时需保持原有的视觉层次。

---

## 六、Composition 资源复用

### 原则

当多个图层组合的内部结构完全相同（仅位置不同）时，应提取为 `<Composition>` 资源，通过
`<Layer composition="@id"/>` 实例化。Composition 不支持参数化，因此只有内容完全一致的部分才能
提取。

### 判断条件

2 个及以上的图层子树满足：
1. 内部结构（几何体、绘制器、子层层次）完全相同
2. 差异仅在于父 Layer 的 `x` / `y` 位置

### 优化手法

1. 在 Resources 中定义 Composition，设置合适的 `width` 和 `height`
2. 将内部坐标从画布绝对坐标转换为 Composition 内部的相对坐标
3. 在原位置用 `<Layer composition="@id" x="..." y="..."/>` 替换

### 坐标转换规则

Composition 有自己的坐标系，原点在左上角。引用时 Layer 的 `x` / `y` 定位的是 Composition 左上角
在父坐标系中的位置。

**转换步骤**：假设原始 Layer 定位于 `(layerX, layerY)`，内部几何体使用 `center="cx,cy"`：

```
Composition width  = 几何体 width
Composition height = 几何体 height
内部 center = (width/2, height/2)
引用 Layer x = layerX - width/2 + cx   （当原始 cx=0 时简化为 layerX - width/2）
引用 Layer y = layerY - height/2 + cy  （当原始 cy=0 时简化为 layerY - height/2）
```

### 示例

```xml
<!-- 优化前：5 个相同的卡片背景，仅 x 不同 -->
<Layer x="160" y="195">
  <Rectangle center="0,0" size="100,80" roundness="12"/>
  <Fill color="#1E293B"/>
  <Stroke color="#334155" width="1"/>
</Layer>
<Layer x="280" y="195">
  <Rectangle center="0,0" size="100,80" roundness="12"/>
  <Fill color="#1E293B"/>
  <Stroke color="#334155" width="1"/>
</Layer>
<!-- ... 共 5 处 -->

<!-- 优化后 -->
<Composition id="cardBg" width="100" height="80">
  <Layer>
    <Rectangle center="50,40" size="100,80" roundness="12"/>
    <Fill color="#1E293B"/>
    <Stroke color="#334155" width="1"/>
  </Layer>
</Composition>

<Layer composition="@cardBg" x="110" y="155"/>
<Layer composition="@cardBg" x="230" y="155"/>
<Layer composition="@cardBg" x="350" y="155"/>
<Layer composition="@cardBg" x="470" y="155"/>
<Layer composition="@cardBg" x="590" y="155"/>
```

### 渐变坐标的转换

当 Composition 内部的几何体使用渐变时，需要将绝对画布坐标转换为基于几何体局部坐标系的相对坐标。
PAGX 规范规定：除纯色外，所有颜色源的坐标都**相对于几何体局部坐标系的原点**。

```xml
<!-- 优化前：按钮使用画布绝对坐标的渐变 -->
<Layer x="217" y="522">
  <Ellipse center="23,23" size="46,46"/>
  <Fill>
    <LinearGradient startPoint="217,545" endPoint="263,545">...</LinearGradient>
  </Fill>
</Layer>

<!-- 优化后：Composition 内使用相对坐标 -->
<Composition id="navButton" width="46" height="46">
  <Layer>
    <Ellipse center="23,23" size="46,46"/>
    <Fill>
      <!-- startPoint/endPoint 相对于 Ellipse 局部坐标系原点 -->
      <LinearGradient startPoint="0,23" endPoint="46,23">...</LinearGradient>
    </Fill>
  </Layer>
</Composition>

<Layer composition="@navButton" x="217" y="522"/>
```

### 边界条件

- **DropShadowStyle 作用范围变化**：DropShadowStyle 基于整个 Layer 的不透明内容计算阴影。如果
  父 Layer 上有 DropShadowStyle，将其部分子内容提取为 Composition 后，阴影的计算范围不会改变
  （因为 Composition 实例化后仍然是该 Layer 的子内容）。但如果被提取的内容本身的 Layer 上有
  DropShadowStyle，提取后需确保 Composition 内部的 Layer 仍然携带该 style。
- **Composition 不支持参数化**：如果多个实例之间除位置外还有颜色、大小等差异，不能提取为同一个
  Composition。只能提取完全相同的子集部分。
- **Composition 内部是独立的图层树**：内部的 Layer 是独立渲染单元，几何体不会向 Composition
  外部传播。

---

## 七、PathData 资源复用

### 原则

当同一段 Path data 字符串在文件中出现 2 次及以上时，应提取为 `<PathData>` 资源，消除重复并提高
可维护性。

### 判断条件

在文件中搜索所有 `<Path data="..."/>` 的 data 值，找出完全相同的字符串。

### 优化手法

1. 在 Resources 中添加 `<PathData id="..." data="..."/>`
2. 将所有相同的内联 data 替换为 `<Path data="@id"/>`

### 示例

```xml
<!-- 优化前：子弹图标路径出现 3 次 -->
<Path data="M 0 0 L 4 0 L 6 4 L 4 12 L 0 12 L -2 4 Z"/>
<!-- ... 另外 2 处相同 ... -->

<!-- 优化后 -->
<PathData id="bullet" data="M 0 0 L 4 0 L 6 4 L 4 12 L 0 12 L -2 4 Z"/>
<!-- 引用处 -->
<Path data="@bullet"/>
```

### 边界条件

- **reversed 属性**：`<Path>` 元素有 `reversed` 属性可以反转路径方向。如果两处引用中一处需要
  反转、一处不需要，仍然可以提取为同一个 PathData，在引用处分别设置 `reversed`。
- **仅 2 次出现时权衡**：如果路径很短（如 `M 0 0 L 10 0`），提取后加上 Resources 定义反而增加
  总行数。此时优化的收益主要在可维护性而非行数减少——修改路径时只需改一处。

---

## 八、Color Source 资源共享

### 原则

当完全相同的渐变定义在多处内联出现时，应在 Resources 中定义一次，各处通过 `@id` 引用。

### 判断条件

多个 Fill / Stroke 内嵌了参数完全相同的 LinearGradient / RadialGradient / ConicGradient /
DiamondGradient。

### 示例

```xml
<!-- 优化前：相同渐变内联定义了 3 次 -->
<Fill>
  <LinearGradient startPoint="0,0" endPoint="100,0">
    <ColorStop offset="0" color="#FF0000"/>
    <ColorStop offset="1" color="#0000FF"/>
  </LinearGradient>
</Fill>

<!-- 优化后：Resources 中定义一次 -->
<LinearGradient id="mainGrad" startPoint="0,0" endPoint="100,0">
  <ColorStop offset="0" color="#FF0000"/>
  <ColorStop offset="1" color="#0000FF"/>
</LinearGradient>

<Fill color="@mainGrad"/>
```

### 边界条件

- **坐标系是相对于几何体的**：渐变的 startPoint / endPoint / center 等坐标是相对于几何体局部
  坐标系原点的。两个不同位置、不同大小的几何体引用同一个渐变，渲染效果会不同（渐变坐标相对于各自
  几何体解释）。只有当几何体形状和大小相同时，共享渐变才能保证视觉一致。
- **单次使用不提取**：PAGX 规范支持两种模式——多处引用用共享定义，单次使用用内联定义。不必把所有
  渐变都提取到 Resources。

---

## 附录：核心概念速查

### 绘制器作用域

绘制器（Fill / Stroke）渲染**当前作用域内截至该绘制器位置时已累积的所有几何体**。绘制器不会清除
几何体列表，后续绘制器继续渲染相同几何体。

```xml
<Layer>
  <Rectangle .../>  <!-- 几何体 A -->
  <Ellipse .../>    <!-- 几何体 B -->
  <Fill color="red"/>   <!-- 渲染 A + B -->
  <Stroke color="black" width="1"/>  <!-- 也渲染 A + B -->
</Layer>
```

### Group 的隔离作用

Group 为几何体累积创建**隔离作用域**：
- 内部几何体仅在 Group 内累积
- 内部绘制器仅渲染 Group 内的几何体
- Group 结束后，其几何体**向上传播**到父作用域（但已被 Group 内绘制器渲染过的和未被渲染过的都会
  传播）

### Layer 与 Group 的区别

| 特性 | Layer | Group |
|---|---|---|
| 几何体传播 | 不传播（Layer 是累积边界） | 向上传播到父作用域 |
| Styles | 支持（DropShadow / InnerShadow / BackgroundBlur） | 不支持 |
| Filters | 支持（Blur / DropShadow / DisplacementMap 等） | 不支持 |
| Mask | 支持 | 不支持 |
| Composition 引用 | 支持 | 不支持 |
| BlendMode | 支持 | 不支持 |
| Transform | 支持（通过 matrix 属性） | 支持（position / rotation / scale） |

**选择原则**：需要 styles / filters / mask / composition / blendMode 时必须用 Layer，否则优先
用 Group（更轻量）。

### DropShadowStyle 的作用层级

DropShadowStyle 是**图层级别**的样式，基于整个 Layer 的不透明内容计算阴影形状。它作用于 Layer 的
全部渲染结果（包括所有子层）。这意味着：

- 合并多个 Layer 为一个时，如果原来各自有不同参数的 DropShadowStyle，合并后只能保留一个。
- 将 Layer 的部分内容提取为子 Composition 不会改变阴影范围（Composition 实例仍是该 Layer 的
  子内容）。

### 颜色源坐标系

除纯色外，所有颜色源（渐变、图案）的坐标**相对于几何体局部坐标系的原点**：
- 外部 transform（Group transform、Layer matrix）同时作用于几何体和颜色源，两者一起缩放/旋转
- 修改几何体属性（如 Rectangle 的 size）不影响颜色源坐标
