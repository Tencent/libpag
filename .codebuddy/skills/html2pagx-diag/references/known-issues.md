# 已知问题与实战经验（html2pagx 保真度）

本文件沉淀自真实排查（`930测试集/04-image-heavy-gallery.html`、`06-deep-nesting-stacking.html`）。
按现象查表 → 定位 → 确认归属。

---

## 〇、设计红线：导入器只做「翻译」，不做「绘制算法」

`src/pagx/html`（importer）与 `tools/html-snapshot` 是**翻译器**，不是渲染器。
仓库里 PAGX 的格式转换**没有任何"复刻浏览器绘制算法"的代码**，新增改动也必须避免。

- ✅ **允许（把 CSS 声明映射到 PAGX 已有图元）**：渐变 → `LinearGradient` ColorSource；
  per-corner `border-radius` → `Path` 几何（因 `Rectangle.roundness` 只能表达单一半径）；
  `box-shadow` → `DropShadowStyle`；`dashed/dotted` → `Stroke.dashes`。
- ❌ **禁止（复刻渲染器的画法）**：自行推导 bevel 高光/阴影系数；把某个 `border-style`
  分解成计算出来的中间几何（如 double 边框的同心环、outline 的外扩环）；拟合椭圆弧等。

判定一句话：**这段计算是不是在替渲染器决定"像素长什么样"？**是 → 就不该放在导入器。
PAGX 没有对应图元时，二选一：**带诊断地丢弃**，或**在 PAGX schema / 渲染器新增图元**
让 importer 只做声明式表达。

> 实例：2026-10-11 按此原则撤回了 E25（double/groove 明暗+双线）、E27（椭圆弧拟合）、
> E31（outline 环层）三处复刻实现，各自退回"不支持 → solid / Drop + warning"；
> 保留了 E26（`border-image: linear-gradient` → 渐变 `Stroke`，纯翻译）。commit `00824fc7f`。

---

## 一、层叠 / z-index 类（最容易误判）

所有渲染器都按**图层树顺序**绘制，PAGX 没有跨层 z-index。因此「谁盖住谁」完全取决于
`tools/html-snapshot` 把节点排成什么顺序。相关逻辑在 `lib/browser-snapshot.ts`：
`paintBand` / `paintOrder` / `resolveChildStacking`（同一父节点内按 band→z→DOM 序排序）。

### 1. 物化伪元素被作者选择器污染（已修，commit ac2bb60d0）
- 现象：`.hero:after` 的 linear-gradient 渐变**盖在标题之上**，白字变灰。
- 根因：装饰伪元素被物化成真实 `<div>` 放进 DOM，又命中作者规则
  `.hero div{position:relative;z-index:1}`，于是排序读到被污染的 z=1，与标题同级、
  按 DOM 序（伪元素在末尾）盖住标题。
- 修法：`resolveChildStacking` 对带 `data-snapshot-pseudo` 的替身，改读
  `getComputedStyle(host, '::after')` 的**伪元素自身** stacking。

### 2. 跨子树 z-index：静态「纯层叠包裹层」吞掉子元素层级（已修，commit 6eb113732）
- 现象：`.over`（图片堆叠，子元素 `position:relative; z-index:3..10`）与后面的
  `.pull`（`position:relative; z-index:auto` 白卡，`margin-top:-50px` 上移）——浏览器里
  **图片盖住白卡顶部**（带 z 的子元素高于 z:auto 的兄弟）；PAGX 里白卡盖住图片。
- 根因：`renderChildrenInto` 只按**直接子节点**排序。`.over` 是静态容器（按流内位置排到
  `.pull` 之前），其子元素的 z-index 无法参与父级排序。
- 修法：新增 `resolveEscapingWrapperZIndex`——识别「纯层叠包裹层」（静态、自身不绘制任何
  东西：无背景/边框/阴影/描边/transform/filter/mask/clip、不裁剪、无直接文本、且**所有子
  元素都是可层叠元素**且无负 z），按其子元素**最高 z-index** 参与父级排序。
- 注意：只对**纯**包裹层生效（自身有背景的容器不能整块抬高，否则背景会跟着错位）。

### 3. 负 z-index 逃逸（**未修**，较难）
- 现象：`.stage{position:relative}`（**不是**层叠上下文）里的 `z-index:-1` 卡片，浏览器中
  逃逸到根层叠上下文、被 `.stage` 白底遮住 → **不可见**；PAGX 作为 stage 子层 → **可见**。
- 根因：负 z 后代需画在**父背景之下**，嵌套的 PAGX 图层做不到（子层永远在父填充之上）。
- 修法方向：需把父背景拆成独立图层，或把负 z 后代提升到父背景之上一层再排。风险高。

---

## 二、图片映射类

### object-fit / background-size 的 contain / none / scale-down
- **务必实测再判**：曾仅凭旧产物误判「contain 失效」，定量测量后发现 contain 正确，
  只有 `object-fit:none` 被**有意降级为 contain**（`HTMLSubsetPropertyTable.cpp` 有
  warning；`ScaleMode` 枚举无 none/scale-down 对应值）。
- 映射：cover→`zoom`、fill→`stretch`、contain→`letterBox`（缺省）；none/scale-down 降级。

### object-position（丢失）
- snapshot 的 `imageInnerStyle()` 只转发 `object-fit`，**丢弃 `object-position`**；
  C++ 侧也无解析。多个不同 position 的格子渲染结果相同。

### 多重背景 `url(A),url(B)`（损坏）
- 只生成 **1 个** `<Image>`，且 `source` 里塞进**原始 CSS 文本**
  （`data:...&apos;), url(&apos;data:...`，含 `&apos;`），非法 data URI → 该格空白/纯底色。
- 属序列化 bug（疑似 importer 侧），优先查。

### repeating-conic-gradient 棋盘格 → 纯灰块
- `repeating-conic-gradient(#e5e5e5 0 25%,#fff 0 50%) 0 0/20px 20px` 被近似成**单个
  ConicGradient**（4 段风车），既不表达 `20px` 平铺也做不出棋盘格。
- 正解：把 20×20 tile 栅格化成图片，再用 `ImagePattern tileMode=repeat`。

### SVG（data URI）图片不渲染（旧 importer）
- `<img src="data:image/svg+xml;base64,...">` 原样存为资源；libpag 不把 SVG 当图片栅格化
  → 整格消失。新版本可能已展开成矢量；用本地重跑确认。

---

## 三、几何 / 图层顺序类

### 头像白色描边被 Fill 覆盖（HEAD 已修，commit 6c5c8cb68）
- 早期节点顺序 `<Ellipse> → <Stroke> → <Fill>`，Fill 盖住 Stroke；HEAD 已改为先 Fill 后 Stroke。

### 纵向累积漂移（HEAD 已修）
- 页面下半部累积漂移 13~18px，源于各区段 `round`/`ceil` 取整叠加。HEAD 本地重跑高度与基准一致、漂移为 0。

---

## 四、文字引擎类（单独归类，通常另案）

- `font-weight:800` → `fontStyle="ExtraBold"`，Helvetica Neue 无该字面 → 回退 Regular（字变细）。
- `h1` 带 `letter-spacing:-2px` 时，PAGX 墨迹宽比浏览器**窄**（曾测 503 vs 519，逐字 ~1px 累积）。
- 判断口径：字号（cap height）一致、字重/字距/字形微差 → 文字引擎问题，不要当结构问题修。

---

## 五、交付产物 / 视口 类（流程性）

- **交付的 PAGX 常出自旧版 importer**（AI 小助手后端 vendored 副本旧）。
  → 必须用**当前 HEAD 本地重跑**再对比；本地重跑恢复而给定产物不恢复 = 产物过期，无需改代码。
- **视口宽度**：PAGX 根节点 `width` 就是转换时用的视口宽度（例：1920）。基准图必须用同一宽度。
  页面无固定宽度时按该值自适应。
- **`pagx render` 一定用 `--scale 1`**，否则插值模糊、对比不公平。

---

## 六、排查手法备忘

- **对齐偏移**：`compare.py --align` 输出每个 y 带的最佳 (dx,dy)，漂移常暴露取整/坐标问题。
- **最小 PAGX 复现**：手写小 `.pagx`（几十行）隔离一个属性，`pagx render` 看渲染器真实行为
  （曾用它验证 `Ellipse 100%` 是正圆、`Stroke` 顺序、SVG 不渲染）。
- **像素测量**：直接采样颜色/亮度、算包围盒/段宽，比目视可靠（例：判断文字是否纯白、
  头像是否正圆、contain 留白宽度）。
- **改动验证要求**：修复后重跑管线，确认目标行区间差异消失，且**其它行区间逐行不变**
  （说明改动是外科手术式的、无回归）。
