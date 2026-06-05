# PAGX Bind 生成指南（AI 视角）

> 工程细节参考 `PAGX-Bind-Design.md`。本文档聚焦 AI 如何生成绑定三层节点。

## 三层模型

绑定关系拆成三个独立概念，互不耦合：

```
第一层 <Param>      数据源：一个可调参数 + 控件元数据（slider 范围、当前值）
第二层 <Bind>       连线：把一个 Param 连到一个 target，并指定 mapping
第三层 <Converter>  变换：可选，复杂映射才用；常见 set/scale/offset 走 Bind 的 mapping 内置算子
```

```
PAGXDocument 上的数据：
  ├── keyframes            ← AI 生成，始终是原始值（引擎运行时改 Property，不回流给 AI）
  ├── <Param>              ← AI 生成定义参数；data-default 由前端写回，记录 slider 当前位置
  ├── <Bind>               ← AI 生成定义 Param→target 连线
  └── <Converter>          ← AI 生成（可选），定义复杂变换

一值多目标：多条 <Bind> 共享同一个 param id（如 blurX/blurY 同步）
```

## 职责边界

| 角色 | 职责 |
|------|------|
| **AI** | 生成原始 keyframes + `<Param>` + `<Bind>`（+ 可选 `<Converter>`）；`data-default` 直接读已写回的 slider 位置 |
| **引擎** | `setBindValue(paramId, value)` — 找所有 `param=paramId` 的 Bind，各自按 mapping/converter 变换并写入 target |
| **前端** | 解析 `<Param>` 的 `data-` 属性渲染控件；用户操作时调 `setBindValue`；将 slider 位置写回对应 Param 的 `data-default` |

---

## 第一层：`<Param>` 参数

声明一个可调参数。引擎只认 `id`，控件相关参数统一用 `data-` 前缀，通过 Node 的 customData 访问。

```xml
<Param id="fade_duration"
       data-control="slider"
       data-min="0.3" data-max="3.0" data-default="1.0" data-step="0.1"
       data-label="时长" data-group="淡入淡出"/>
```

### 引擎属性

| 属性 | 类型 | 必需 | 说明 |
|------|------|------|------|
| `id` | string | 是 | 全局唯一，`setBindValue(id, value)` 的 key |

### 前端属性（`data-` 前缀，通过 customData 访问）

| 属性 | 类型 | 必需 | 说明 |
|------|------|------|------|
| `data-control` | string | 是 | `"slider"` / `"color"` |
| `data-min` | number | slider 必需 | slider 最小值 |
| `data-max` | number | slider 必需 | slider 最大值 |
| `data-default` | number / string | 是 | 初始值；前端拖动后写回当前 slider 位置 |
| `data-step` | number | 否 | slider 步进 |
| `data-label` | string | 否 | 前端显示标签 |
| `data-group` | string | 否 | 分组名 |

---

## 第二层：`<Bind>` 连线

把一个 Param 连到一个 target，并指定如何映射。

```xml
<Bind param="fade_duration" target="fade_anim" mapping="scale"/>
<Bind param="peak_opacity"  target="title_alpha" mapping="set"/>
```

### 引擎属性

| 属性 | 类型 | 必需 | 说明 |
|------|------|------|------|
| `param` | string | 是 | 引用 `<Param>` 的 id（数据源） |
| `target` | string | 是 | 被绑定节点的 id：`Property.id`（改 keyframe value）或 `Animation.id`（改 duration） |
| `mapping` | string | 否 | `"set"` / `"scale"` / `"offset"`，默认 `"set"` |
| `keyframe` | string | 否 | 仅 target 为 Property 时有效：`"all"`（默认）/ `"first"` / `"last"` / 整数索引。限定作用到哪些 keyframe |
| `converter` | string | 否 | 引用 `<Converter>` 的 id；与 `mapping` 二选一，用于复杂变换 |

### target 类型与作用字段

引擎按 target 节点类型自动决定改什么：

| target 类型 | 作用对象 | 说明 |
|------------|---------|------|
| `Property` | 该 Property 的 keyframe value | 受 `keyframe` 属性限定范围 |
| `Animation` | `Animation.duration` 字段 | 改总时长（截断点），不影响 keyframe time |

### mapping 组合

mapping 基于**原始值**计算。keyframes 永远存 AI 生成的原始值，引擎每次 `setBindValue` 都基于原始值重算，无累积漂移。

| mapping | 公式 | slider 含义 |
|---------|------|------------|
| `"set"` | `新值 = slider值` | 直接替换 |
| `"scale"` | `新值 = 原始值 × slider值` | 倍率 |
| `"offset"` | `新值 = 原始值 + slider值` | 增量 |

### 引擎查找流程

```
setBindValue("blur_strength", 2.0)
  → 找到所有 param="blur_strength" 的 Bind（可能多条）
  → Bind: target="blur_x" (Property) → 每个 keyframe value = 原始value × 2.0
  → Bind: target="blur_y" (Property) → 每个 keyframe value = 原始value × 2.0
  → blurX / blurY 自动同步
```

---

## 第三层：`<Converter>` 变换（可选）

仅当 set/scale/offset 不够用时（如区间映射、公式、串联），才定义独立 Converter，被 Bind 的 `converter` 属性引用。多个 Bind 可共享同一个 Converter。

```xml
<Converter id="ease_map" type="range"
           in-min="0" in-max="1" out-min="0.1" out-max="1.0"/>

<Bind param="opacity" target="title_alpha" converter="ease_map"/>
```

基础场景不需要 Converter，本文档案例均用 mapping 内置算子。Converter 的完整类型（range / formula / operation / group 串联）见工程文档。

---

## data-default 与 slider 位置

`data-default` 既是 slider 初始值，也承载用户拖动后的当前位置：前端拖动 slider 时把最新值写回对应 Param 的 `data-default`，AI 下一轮直接读回。

### AI 读取规则

AI 生成 Param 时，`data-default` 的取值优先级：

| 优先 | 来源 | 值 |
|------|------|-----|
| 最高 | 已有同 id Param 的 data-default | 直接用 |
| 其次 | 关联 Bind 的 mapping="scale" | `1.0` |
| 其次 | 关联 Bind 的 mapping="offset" | `0` |
| 其次 | mapping="set" / color | 从当前 keyframes 取实际值 |

---

# 用户案例

以下展示典型用户场景，AI 如何自主决定 keyframes、Param 和 Bind。

---

## 案例 1：淡入淡出

用户：**"给这个文字加个淡入淡出"**

### AI 决策

- 动画效果：alpha 从 0 → 1 → 1 → 0，3 秒
- 暴露 2 个 slider：
  - **时长**：scale Animation 的 duration，default=1.0（倍率）
  - **透明度**：set alpha keyframes 的 value，default=1.0（当前峰值）

### 生成的 PAGXDocument

```xml
<PAGXDoc width="400" height="200">
  <Layers>
    <Layer id="title" x="100" y="80" width="200" height="40">
      <Text id="titleText" text="Hello PAGX" fontSize="24"
            position="100,20"/>
    </Layer>
  </Layers>
  <Animations>
    <Animation id="fade_anim" duration="180" frameRate="60">
      <AnimationObject target="title">
        <Property id="title_alpha" channel="alpha">
          <Keyframe time="0" value="0" interpolation="Bezier"/>
          <Keyframe time="60" value="1" interpolation="Bezier"/>
          <Keyframe time="120" value="1" interpolation="Hold"/>
          <Keyframe time="180" value="0" interpolation="Bezier"/>
        </Property>
      </AnimationObject>
    </Animation>
  </Animations>
  <Bindings>
    <Param id="duration"
           data-control="slider"
           data-min="0.3" data-max="3.0" data-default="1.0" data-step="0.1"
           data-label="时长" data-group="淡入淡出"/>
    <Param id="opacity"
           data-control="slider"
           data-min="0.1" data-max="1.0" data-default="1.0" data-step="0.01"
           data-label="透明度" data-group="淡入淡出"/>
    <Bind param="duration" target="fade_anim" mapping="scale"/>
    <Bind param="opacity"  target="title_alpha" mapping="set"/>
  </Bindings>
</PAGXDoc>
```

### 用户操作效果

| 操作 | 效果 |
|------|------|
| 时长拖到 0.5 | Animation.duration = 180×0.5 = 90，播到第 90 帧停（1.5 秒） |
| 时长拖到 2.0 | Animation.duration = 360，总时长 6 秒 |
| 透明度拖到 0.3 | alpha keyframes 峰值替换为 0.3 |

---

## 案例 2：滑入

用户：**"让这个卡片从右边滑进来"**

### AI 决策

- 动画效果：x 从 400 → 100，opacity 从 0 → 1，1 秒
- 暴露 2 个 slider：
  - **时长**：scale Animation duration
  - **滑动距离**：scale x keyframes 的 value，default=1.0

### 生成的 PAGXDocument

```xml
<PAGXDoc width="400" height="300">
  <Layers>
    <Layer id="card" x="400" y="50" width="300" height="200">
      <Rectangle size="300,200" roundness="12">
        <Fill><SolidColor color="#FFFFFF"/></Fill>
      </Rectangle>
    </Layer>
  </Layers>
  <Animations>
    <Animation id="slide_anim" duration="60" frameRate="60">
      <AnimationObject target="card">
        <Property id="card_x" channel="x">
          <Keyframe time="0" value="400" interpolation="Bezier"/>
          <Keyframe time="60" value="100" interpolation="Bezier"/>
        </Property>
        <Property id="card_alpha" channel="alpha">
          <Keyframe time="0" value="0" interpolation="Bezier"/>
          <Keyframe time="60" value="1" interpolation="Bezier"/>
        </Property>
      </AnimationObject>
    </Animation>
  </Animations>
  <Bindings>
    <Param id="duration"
           data-control="slider"
           data-min="0.3" data-max="3.0" data-default="1.0" data-step="0.1"
           data-label="时长" data-group="滑入效果"/>
    <Param id="distance"
           data-control="slider"
           data-min="0.5" data-max="2.0" data-default="1.0" data-step="0.05"
           data-label="滑动距离" data-group="滑入效果"/>
    <Bind param="duration" target="slide_anim" mapping="scale"/>
    <Bind param="distance" target="card_x" mapping="scale"/>
  </Bindings>
</PAGXDoc>
```

### 用户操作效果

| 操作 | 效果 |
|------|------|
| 时长拖到 0.5 | 0.5 秒滑入 |
| 距离拖到 1.5 | x keyframes ×1.5，从更远处滑入 |
| 距离拖到 0.3 | x keyframes ×0.3，几乎原地出现 |

---

## 案例 3：弹跳

用户：**"让这个图标弹跳两下"**

### AI 决策

- 动画效果：y 上下弹跳，关键帧 time=[0,15,30,45,60]，value=[0,-80,0,-40,0]
- 暴露 2 个 slider：
  - **时长**：scale Animation duration
  - **弹跳高度**：scale y value，default=1.0

### 生成的 PAGXDocument

```xml
<PAGXDoc width="200" height="300">
  <Layers>
    <Layer id="icon" x="60" y="50" width="80" height="80">
      <Ellipse size="80,80">
        <Fill><SolidColor color="#FF6B6B"/></Fill>
      </Ellipse>
    </Layer>
  </Layers>
  <Animations>
    <Animation id="bounce_anim" duration="60" frameRate="60">
      <AnimationObject target="icon">
        <Property id="icon_y" channel="y">
          <Keyframe time="0" value="0" interpolation="Bezier"/>
          <Keyframe time="15" value="-80" interpolation="Bezier"/>
          <Keyframe time="30" value="0" interpolation="Bezier"/>
          <Keyframe time="45" value="-40" interpolation="Bezier"/>
          <Keyframe time="60" value="0" interpolation="Bezier"/>
        </Property>
      </AnimationObject>
    </Animation>
  </Animations>
  <Bindings>
    <Param id="duration"
           data-control="slider"
           data-min="0.3" data-max="3.0" data-default="1.0" data-step="0.1"
           data-label="时长" data-group="弹跳"/>
    <Param id="height"
           data-control="slider"
           data-min="0.2" data-max="3.0" data-default="1.0" data-step="0.05"
           data-label="弹跳高度" data-group="弹跳"/>
    <Bind param="duration" target="bounce_anim" mapping="scale"/>
    <Bind param="height"   target="icon_y" mapping="scale"/>
  </Bindings>
</PAGXDoc>
```

### 用户操作效果

| 操作 | 效果 |
|------|------|
| 时长拖到 2.0 | duration ×2，弹跳变慢 |
| 高度拖到 2.0 | y keyframes ×2，最高点 -160px（基于原始 -80） |
| 高度拖到 0.3 | y keyframes ×0.3，弹跳很轻 |

---

## 案例 4：模糊过渡（一值多目标）

用户：**"这个背景从不模糊到模糊"**

### AI 决策

- 动画效果：blurX/blurY 从 0 → 20，持续 2 秒
- blurX、blurY 是两个独立 Property，**一个 slider 同时控两者**：两条 Bind 共享同一个 param

### 生成的 PAGXDocument

```xml
<PAGXDoc width="400" height="300">
  <Layers>
    <Layer id="bg" x="0" y="0" width="400" height="300">
      <BlurFilter id="blurFx" blurX="0" blurY="0"/>
      <Rectangle size="400,300">
        <Fill><SolidColor id="bgColor" color="#4488FF"/></Fill>
      </Rectangle>
    </Layer>
    <Layer id="content" x="100" y="100" width="200" height="100">
      <Text id="contentText" text="毛玻璃效果" fontSize="20"/>
    </Layer>
  </Layers>
  <Animations>
    <Animation id="blur_anim" duration="120" frameRate="60">
      <AnimationObject target="blurFx">
        <Property id="blur_x" channel="blurX">
          <Keyframe time="0" value="0" interpolation="Bezier"/>
          <Keyframe time="120" value="20" interpolation="Bezier"/>
        </Property>
        <Property id="blur_y" channel="blurY">
          <Keyframe time="0" value="0" interpolation="Bezier"/>
          <Keyframe time="120" value="20" interpolation="Bezier"/>
        </Property>
      </AnimationObject>
    </Animation>
  </Animations>
  <Bindings>
    <Param id="duration"
           data-control="slider"
           data-min="0.3" data-max="3.0" data-default="1.0" data-step="0.1"
           data-label="时长" data-group="模糊"/>
    <Param id="blur_strength"
           data-control="slider"
           data-min="0" data-max="40" data-default="1.0" data-step="0.1"
           data-label="模糊强度" data-group="模糊"/>
    <Bind param="duration"      target="blur_anim" mapping="scale"/>
    <Bind param="blur_strength" target="blur_x" mapping="scale"/>
    <Bind param="blur_strength" target="blur_y" mapping="scale"/>
  </Bindings>
</PAGXDoc>
```

### 用户操作效果

| 操作 | 效果 |
|------|------|
| 时长拖到 0.5 | duration ×0.5，模糊过渡加速 |
| 强度拖到 2.0 | blurX、blurY 同步 ×2，最大 40px（X/Y 始终一致） |
| 强度拖到 0.5 | blurX、blurY 同步 ×0.5 |

---

## 案例 5：颜色变换（keyframe 限定）

用户：**"让这个矩形的颜色从红色变到蓝色"**

### AI 决策

- 动画效果：color keyframes 从 red(#FF0000) 到 blue(#0000FF)
- 暴露 2 个控件：
  - **时长** slider：scale Animation duration
  - **目标颜色** color picker：set color Property 的**最后一个** keyframe（用 `keyframe="last"` 限定，避免改到起点红色）

### 生成的 PAGXDocument

```xml
<PAGXDoc width="400" height="300">
  <Layers>
    <Layer id="shape" x="100" y="100" width="200" height="100">
      <Rectangle size="200,100" roundness="8">
        <Fill><SolidColor id="rectColor" color="#FF0000"/></Fill>
      </Rectangle>
    </Layer>
  </Layers>
  <Animations>
    <Animation id="color_anim" duration="120" frameRate="60">
      <AnimationObject target="rectColor">
        <Property id="rect_color" channel="color">
          <Keyframe time="0" value="#FF0000" interpolation="Bezier"/>
          <Keyframe time="120" value="#0000FF" interpolation="Bezier"/>
        </Property>
      </AnimationObject>
    </Animation>
  </Animations>
  <Bindings>
    <Param id="duration"
           data-control="slider"
           data-min="0.3" data-max="5.0" data-default="1.0" data-step="0.1"
           data-label="时长" data-group="颜色变换"/>
    <Param id="target_color"
           data-control="color"
           data-default="#0000FF"
           data-label="目标颜色" data-group="颜色变换"/>
    <Bind param="duration"     target="color_anim" mapping="scale"/>
    <Bind param="target_color" target="rect_color" mapping="set" keyframe="last"/>
  </Bindings>
</PAGXDoc>
```

### 用户操作效果

| 操作 | 效果 |
|------|------|
| 时长拖到 0.5 | duration ×0.5，颜色过渡加速 |
| 颜色选绿色 #00FF00 | 最后一个 keyframe 替换为绿色，从红渐变到绿 |

---

## 案例 6：多个对象同时滑入（一值多目标）

用户：**"让这 3 张卡片从左到右依次滑进来"**

### AI 决策

- 3 张卡片各有独立 x Property，依次到位
- 共享 2 个 slider：
  - **时长**：scale Animation duration
  - **卡间距**：offset card2、card3 的 x value（两条 Bind 共享 param）

### 生成的 PAGXDocument

```xml
<PAGXDoc width="800" height="300">
  <Layers>
    <Layer id="card1" x="-300" y="80" width="180" height="140">
      <Rectangle size="180,140" roundness="8">
        <Fill><SolidColor color="#FF6B6B"/></Fill>
      </Rectangle>
    </Layer>
    <Layer id="card2" x="-300" y="80" width="180" height="140">
      <Rectangle size="180,140" roundness="8">
        <Fill><SolidColor color="#4ECDC4"/></Fill>
      </Rectangle>
    </Layer>
    <Layer id="card3" x="-300" y="80" width="180" height="140">
      <Rectangle size="180,140" roundness="8">
        <Fill><SolidColor color="#45B7D1"/></Fill>
      </Rectangle>
    </Layer>
  </Layers>
  <Animations>
    <Animation id="slide_anim" duration="60" frameRate="60">
      <AnimationObject target="card1">
        <Property id="card1_x" channel="x">
          <Keyframe time="0" value="-300" interpolation="Bezier"/>
          <Keyframe time="60" value="10" interpolation="Bezier"/>
        </Property>
      </AnimationObject>
      <AnimationObject target="card2">
        <Property id="card2_x" channel="x">
          <Keyframe time="0" value="-300" interpolation="Bezier"/>
          <Keyframe time="60" value="210" interpolation="Bezier"/>
        </Property>
      </AnimationObject>
      <AnimationObject target="card3">
        <Property id="card3_x" channel="x">
          <Keyframe time="0" value="-300" interpolation="Bezier"/>
          <Keyframe time="60" value="410" interpolation="Bezier"/>
        </Property>
      </AnimationObject>
    </Animation>
  </Animations>
  <Bindings>
    <Param id="duration"
           data-control="slider"
           data-min="0.3" data-max="3.0" data-default="1.0" data-step="0.1"
           data-label="时长" data-group="滑入效果"/>
    <Param id="spacing"
           data-control="slider"
           data-min="-50" data-max="100" data-default="0" data-step="5"
           data-label="卡间距" data-group="滑入效果"/>
    <Bind param="duration" target="slide_anim" mapping="scale"/>
    <Bind param="spacing"  target="card2_x" mapping="offset"/>
    <Bind param="spacing"  target="card3_x" mapping="offset"/>
  </Bindings>
</PAGXDoc>
```

### 用户操作效果

| 操作 | 效果 |
|------|------|
| 时长拖到 0.5 | 3 张卡 0.5 秒滑入 |
| 间距拖到 30 | card2、card3 的 x 各 +30px，卡片间距变大 |

---

## 案例 7：旋转 + 缩放

用户：**"让这个图标边旋转边变大"**

### AI 决策

- 动画效果：rotation 从 0 → 360，scale 从 0.5 → 1，1 秒
- 暴露 2 个 slider：
  - **时长**：scale Animation duration
  - **旋转圈数**：scale rotation value，default=1.0

### 生成的 PAGXDocument

```xml
<PAGXDoc width="300" height="300">
  <Layers>
    <Layer id="icon" x="110" y="110" width="80" height="80">
      <Ellipse size="80,80">
        <Fill><SolidColor color="#FF6B6B"/></Fill>
      </Ellipse>
    </Layer>
  </Layers>
  <Animations>
    <Animation id="spin_anim" duration="60" frameRate="60">
      <AnimationObject target="icon">
        <Property id="icon_rotation" channel="rotation">
          <Keyframe time="0" value="0" interpolation="Bezier"/>
          <Keyframe time="60" value="360" interpolation="Bezier"/>
        </Property>
        <Property id="icon_scale" channel="scale">
          <Keyframe time="0" value="0.5" interpolation="Bezier"/>
          <Keyframe time="60" value="1.0" interpolation="Bezier"/>
        </Property>
      </AnimationObject>
    </Animation>
  </Animations>
  <Bindings>
    <Param id="duration"
           data-control="slider"
           data-min="0.3" data-max="3.0" data-default="1.0" data-step="0.1"
           data-label="时长" data-group="旋转"/>
    <Param id="spins"
           data-control="slider"
           data-min="0.5" data-max="3.0" data-default="1.0" data-step="0.1"
           data-label="圈数" data-group="旋转"/>
    <Bind param="duration" target="spin_anim" mapping="scale"/>
    <Bind param="spins"    target="icon_rotation" mapping="scale"/>
  </Bindings>
</PAGXDoc>
```

### 用户操作效果

| 操作 | 效果 |
|------|------|
| 时长拖到 2.0 | duration ×2，旋转一圈要 2 秒 |
| 圈数拖到 2.0 | rotation keyframes ×2，旋转 2 圈（720°） |
| 圈数拖到 0.5 | 只转半圈（180°） |

---

# 多轮对话示例

用户持续提需求，AI 每次从当前 keyframes（原始值）+ Param 的 data-default 出发，重新生成完整 PAGXDocument。

### 第 1 轮

用户：**"给 layer1 加个淡入淡出"**

```xml
<PAGXDoc width="400" height="300">
  <Layers>
    <Layer id="layer1" x="0" y="0" width="400" height="300">
      <Rectangle size="400,300">
        <Fill><SolidColor color="#4488FF"/></Fill>
      </Rectangle>
    </Layer>
  </Layers>
  <Animations>
    <Animation id="fade_anim" duration="180" frameRate="60">
      <AnimationObject target="layer1">
        <Property id="layer1_alpha" channel="alpha">
          <Keyframe time="0" value="0" interpolation="Bezier"/>
          <Keyframe time="60" value="1" interpolation="Bezier"/>
          <Keyframe time="120" value="1" interpolation="Hold"/>
          <Keyframe time="180" value="0" interpolation="Bezier"/>
        </Property>
      </AnimationObject>
    </Animation>
  </Animations>
  <Bindings>
    <Param id="fade_duration"
           data-control="slider"
           data-min="0.3" data-max="3.0" data-default="1.0" data-step="0.1"
           data-label="时长" data-group="淡入淡出"/>
    <Param id="peak_opacity"
           data-control="slider"
           data-min="0.1" data-max="1.0" data-default="1.0" data-step="0.01"
           data-label="峰值透明度" data-group="淡入淡出"/>
    <Bind param="fade_duration" target="fade_anim" mapping="scale"/>
    <Bind param="peak_opacity"  target="layer1_alpha" mapping="set"/>
  </Bindings>
</PAGXDoc>
```

用户拖时长到 **0.6**：
- 前端调 `setBindValue("fade_duration", 0.6)`
- 引擎：Bind target=fade_anim (Animation) → `duration = 180 × 0.6 = 108`，播到第 108 帧停
- keyframes time 不变，仍是原始 `[0,60,120,180]`
- 前端把 fade_duration 的 `data-default` 写回 `0.6`

---

### 第 2 轮

用户：**"消失的时候往左边飞出去"**

AI 读到：keyframes 原始值不变，fade_duration 的 data-default=0.6

AI 增加 x 位移动画，去掉 peak_opacity，换成 exit_offset。data-default 从上一轮 Param 读回。

```xml
<PAGXDoc width="400" height="300">
  <Layers>
    <Layer id="layer1" x="0" y="0" width="400" height="300">
      <Rectangle size="400,300">
        <Fill><SolidColor color="#4488FF"/></Fill>
      </Rectangle>
    </Layer>
  </Layers>
  <Animations>
    <Animation id="fade_fly_anim" duration="180" frameRate="60">
      <AnimationObject target="layer1">
        <Property id="layer1_alpha" channel="alpha">
          <Keyframe time="0" value="0" interpolation="Bezier"/>
          <Keyframe time="60" value="1" interpolation="Bezier"/>
          <Keyframe time="120" value="1" interpolation="Hold"/>
          <Keyframe time="180" value="0" interpolation="Bezier"/>
        </Property>
        <Property id="layer1_x" channel="x">
          <Keyframe time="0" value="0" interpolation="Hold"/>
          <Keyframe time="60" value="0" interpolation="Hold"/>
          <Keyframe time="120" value="0" interpolation="Bezier"/>
          <Keyframe time="180" value="-200" interpolation="Bezier"/>
        </Property>
      </AnimationObject>
    </Animation>
  </Animations>
  <Bindings>
    <Param id="fade_duration"
           data-control="slider"
           data-min="0.3" data-max="3.0" data-default="0.6" data-step="0.1"
           data-label="时长" data-group="出场效果"/>
    <Param id="exit_offset"
           data-control="slider"
           data-min="0.5" data-max="3.0" data-default="1.0" data-step="0.1"
           data-label="飞出距离" data-group="出场效果"/>
    <Bind param="fade_duration" target="fade_fly_anim" mapping="scale"/>
    <Bind param="exit_offset"   target="layer1_x" mapping="scale"/>
  </Bindings>
</PAGXDoc>
```

用户拖 exit_offset 到 **1.5**：
- 前端调 `setBindValue("exit_offset", 1.5)`
- 引擎：Bind target=layer1_x (Property) → x keyframes 基于原始值 ×1.5 → 末帧 `-200 × 1.5 = -300`（运行时，不回流）
- 前端把 exit_offset 的 `data-default` 写回 `1.5`

---

### 第 3 轮

用户：**"背景同时模糊一下"**

AI 读到：keyframes 原始值不变，fade_duration data-default=0.6，exit_offset data-default=1.5

AI 增加 BlurFilter + blur 动画，blurX/blurY 共享一个 blur_amount param。

```xml
<PAGXDoc width="400" height="300">
  <Layers>
    <Layer id="layer1" x="0" y="0" width="400" height="300">
      <Rectangle size="400,300">
        <Fill><SolidColor color="#4488FF"/></Fill>
      </Rectangle>
      <BlurFilter id="blurFx" blurX="0" blurY="0"/>
    </Layer>
  </Layers>
  <Animations>
    <Animation id="fade_fly_blur_anim" duration="180" frameRate="60">
      <AnimationObject target="layer1">
        <Property id="layer1_alpha" channel="alpha">
          <Keyframe time="0" value="0" interpolation="Bezier"/>
          <Keyframe time="60" value="1" interpolation="Bezier"/>
          <Keyframe time="120" value="1" interpolation="Hold"/>
          <Keyframe time="180" value="0" interpolation="Bezier"/>
        </Property>
        <Property id="layer1_x" channel="x">
          <Keyframe time="0" value="0" interpolation="Hold"/>
          <Keyframe time="60" value="0" interpolation="Hold"/>
          <Keyframe time="120" value="0" interpolation="Bezier"/>
          <Keyframe time="180" value="-200" interpolation="Bezier"/>
        </Property>
      </AnimationObject>
      <AnimationObject target="blurFx">
        <Property id="blur_x" channel="blurX">
          <Keyframe time="0" value="0" interpolation="Hold"/>
          <Keyframe time="60" value="0" interpolation="Hold"/>
          <Keyframe time="120" value="0" interpolation="Bezier"/>
          <Keyframe time="180" value="20" interpolation="Bezier"/>
        </Property>
        <Property id="blur_y" channel="blurY">
          <Keyframe time="0" value="0" interpolation="Hold"/>
          <Keyframe time="60" value="0" interpolation="Hold"/>
          <Keyframe time="120" value="0" interpolation="Bezier"/>
          <Keyframe time="180" value="20" interpolation="Bezier"/>
        </Property>
      </AnimationObject>
    </Animation>
  </Animations>
  <Bindings>
    <Param id="fade_duration"
           data-control="slider"
           data-min="0.3" data-max="3.0" data-default="0.6" data-step="0.1"
           data-label="时长" data-group="出场效果"/>
    <Param id="exit_offset"
           data-control="slider"
           data-min="0.5" data-max="3.0" data-default="1.5" data-step="0.1"
           data-label="飞出距离" data-group="出场效果"/>
    <Param id="blur_amount"
           data-control="slider"
           data-min="0" data-max="40" data-default="1.0" data-step="0.1"
           data-label="模糊强度" data-group="出场效果"/>
    <Bind param="fade_duration" target="fade_fly_blur_anim" mapping="scale"/>
    <Bind param="exit_offset"   target="layer1_x" mapping="scale"/>
    <Bind param="blur_amount"   target="blur_x" mapping="scale"/>
    <Bind param="blur_amount"   target="blur_y" mapping="scale"/>
  </Bindings>
</PAGXDoc>
```

3 个 slider 分别在 0.6、1.5、1.0，完美延续之前所有调整，blurX/blurY 由同一 param 同步。

---

## 总结

| 数据 | 谁写 | 谁读 |
|------|------|------|
| keyframes（原始值） | AI | 引擎（基于原始值算）/ AI（下一轮） |
| `<Param>` 引擎属性 | AI | 引擎 |
| `<Param>` data- 属性 | AI（data-default 前端写回当前位置） | 前端 |
| `<Bind>` | AI | 引擎 |
| `<Converter>` | AI（可选） | 引擎 |

## AI 生成原则

1. **用户不说，AI 自己决定哪些参数值得暴露**，只暴露最有意义的 1-3 个
2. **keyframes 始终是原始值**，引擎运行时改 Property 不回流，mapping 基于原始值算，无漂移
3. **每个被绑定的 Property 要有 id**，Bind 用 `target` 指向它；时长类 Bind 指向 `Animation.id` 改 duration
4. **一值多目标**：多条 Bind 共享同一个 param id（如 blurX/blurY 同步、card2/card3 同移）
5. `data-default` 优先读已有同 id Param 的 data-default，其次按 mapping 规则（scale→1.0, offset→0），其次取当前值
6. 多轮迭代时从当前 keyframes（原始值）+ data-default 出发做增量修改
7. 基础变换用 Bind 的 `mapping`（set/scale/offset）；复杂变换才定义独立 `<Converter>`
