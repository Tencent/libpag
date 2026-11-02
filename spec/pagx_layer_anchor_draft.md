# PAGX Layer 锚点约定（草案 v0.1）

> 状态：讨论稿，待与 3D matrix 方案一起定稿
> 涉及：PAGX 规范、libpag（Importer / Exporter / Runtime）、Ardot 导入器

## 0. 背景

SVG 的 `rotate` 允许带旋转中心：

```xml
<animateTransform type="rotate" from="0 100 100" to="360 100 100" dur="4s"/>
```

后两个参数是旋转轴心。PAGX 里 `Group` / `TextBox` 有 `anchor` 可以表达这个语义，但 `Layer` 没有；
而带 `filter` / `mask` / `alpha` 或**子 Layer**的容器只能落成 `Layer`，于是轴心只能被烘焙进
`matrix` 的 `tx/ty`：

```xml
<!-- 现状：绕 (100,100) 旋转 120° 的关键帧 -->
<Key time="80" value="-0.5,0.866025,-0.866025,-0.5,236.603,63.3975"/>
```

这带来两个问题：

1. `matrix` 里出现"伪位移"，下游容易误读成真实平移（Ardot 拆成 `rotation + position` 后尤其明显）；
2. 下游若要还原轴心，必须解算矩阵不动点，跨语言重复实现且容易出错。

因此给 `Layer` 增加 `anchor`，让轴心有一个显式、可逆的表达位置。

## 1. 属性定义

```xml
<Layer anchor="x,y" matrix="a,b,c,d,tx,ty"/>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `anchor` | Point | `0,0` | 锚点，图层本地坐标，绝对像素；不随 `size` 缩放 |

- 与 `Group.anchor` 同名同义：本地坐标中的旋转 / 缩放支点；
- 缺省 `0,0` 时行为与当前版本完全一致（向后兼容）。

## 2. 变换公式

```text
M = T(tx, ty) · T(anchor) · L · T(−anchor)
```

- `L`：`matrix` 的 2×2 线性部分（旋转 / 缩放 / 斜切）；
- `(tx, ty)`：`matrix` 的平移，表示"绕锚点变换之后再平移"。

分步语义：

| 步骤 | 含义 |
|------|------|
| `T(−anchor)` | 把锚点移到原点 |
| `L` | 绕锚点做旋转 / 缩放 / 斜切 |
| `T(anchor)` | 锚点回到原位 |
| `T(tx, ty)` | 之后的平移，**不受锚点影响** |

落点关系：

```text
M · anchor = anchor + (tx, ty)
```

## 3. 与 matrix 的分工（核心约束）

```text
anchor = 0   → matrix 是完整变换（现状，含轴心补偿，完全兼容）
anchor ≠ 0   → matrix 的线性部分绕 anchor 作用，tx/ty 只表示纯平移
```

**同一份数据里不允许同时存在「烘焙进 tx/ty 的轴心补偿」和「anchor 字段」**——两者是同一语义的
两种表达，叠加会导致整体偏移。

正确写法（绕 (100,100) 旋转）：

```xml
<Layer anchor="100,100" matrix="cos,sin,-sin,cos,0,0"/>
```

`tx/ty` 恒为 0，角度变化只改线性部分。

## 4. 动画通道

`Layer` 没有 `rotation` / `scale` / `position` 属性，因此**不新增变换通道**，仍使用 `matrix` 通道，
只改变关键帧的书写规范：

```xml
<Channel name="matrix" type="matrix">
  <Key time="0"   value="1,0,0,1,0,0"/>
  <Key time="80"  value="-0.5,0.866025,-0.866025,-0.5,0,0"/>
  <Key time="160" value="-0.5,-0.866025,0.866025,-0.5,0,0"/>
  <Key time="240" value="1,0,0,1,0,0"/>
</Channel>
```

- 关键帧中不再出现 `236.603 / 63.3975` 这类补偿值；
- 运行时按第 2 节公式在锚点处求值，效果等价于绕 `(100,100)` 旋转；
- 线性部分的变化与锚点无关，插值仍按原有矩阵分解口径进行。

`anchor` 自身是否可动画（对应 Group 的 `anchor.x` / `anchor.y`）不在 v0.1 范围内。

## 5. 与布局属性的关系

锚点不参与布局，保持现有层级：

```text
最终渲染矩阵 = T(renderPosition) · M
M            = T(t) · T(anchor) · L · T(−anchor)
```

- `x` / `y`、`left` / `top`、auto-layout 语义不变；
- `anchor` 只作用于图层自身的 `matrix`，不影响子层布局计算；
- `anchor` 使用绝对像素，不随 `width` / `height` 变化而变化（与 `Group.anchor` 一致）。

## 6. 3D 预留

等 3D matrix 方案落地时，沿用同一规则扩展到三维：

```text
M3 = T(t3) · T(anchor3) · L3 · T(−anchor3)
```

建议：

- 保持 `anchor` 为 2D `Point`，3D 版本另加 `anchorZ`（默认 0），避免把 `anchor` 的解析改成三值；
- 2D 与 3D 共用"线性部分绕锚点、平移后置"的规则。

## 7. 映射关系

| 方向 | 规则 |
|------|------|
| SVG → PAGX | `rotate(θ, cx, cy)` → `anchor="cx,cy"`，矩阵关键帧只写旋转，`tx = ty = 0` |
| PAGX → SVG | 由矩阵线性部分求旋转角，不动点 `(I−L)⁻¹·t` 作为 `(cx, cy)`，输出 `rotate(θ, cx, cy)` |
| PAGX → Ardot | `transformOrigin = anchor / size`；`translation = matrix.t`，不再做轴心补偿换算 |

Ardot 侧合成公式与本约定同构：

```text
M = T(translation + anchor) · L · T(−anchor)
```

因此映射是 1:1：`transformOrigin` 恒落在 `[0,1]` 内，无需再出现越界锚点。

## 8. 兼容与校验

| 规则 | 说明 |
|------|------|
| 老文件 | `anchor` 缺省 `0,0`，矩阵原样使用，行为不变 |
| 新文件 | `anchor ≠ 0` 时，矩阵关键帧不得包含轴心补偿 |
| 校验建议 | 若 `anchor ≠ 0` 且平移量随帧变化、且近似满足 `t = a − L·a`，说明生产者仍在烘焙补偿，导入器应输出 warning |

## 9. 待确认项

1. `anchorZ` 字段命名与默认值；
2. 3D 下 `matrix3D` 与 `anchor` 的优先级（是否允许同时出现）；
3. `anchor` 是否允许被动画驱动（后续是否与 Group 的 `anchor.x` / `anchor.y` 对齐）；
4. `matrix` 与 `matrix3D` 同时存在时，`anchor` 的归属。

## 10. 评审检查项

- [ ] `anchor ≠ 0` 的数据里，矩阵关键帧 `tx/ty` 是否为纯平移（不随旋转变化）；
- [ ] 迁移旧数据时，是否先把烘焙的轴心补偿折算成 `anchor + 纯旋转`，再写入新字段；
- [ ] 导出到 SVG / Ardot 时，锚点换算是否只做一次（不重复补偿）。
