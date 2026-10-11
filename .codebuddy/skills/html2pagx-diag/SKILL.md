---
name: html2pagx-diag
description: >-
  HTML → PAGX 转换保真度排查（libpag 仓库内）。给定 HTML 源文件与「转换后的 PAGX」
  （可选：问题处截图），渲染浏览器基准图与 `pagx render` 输出并逐区域对比，定位差异、
  判定归属模块（tools/html-snapshot 快照 / src/pagx/html importer / 渲染器），并区分
  「本仓库问题」「产物过期（旧版 importer）」「本仓库外导入端问题」。当用户排查
  html2pagx 转换效果与 HTML 不一致、PAGX 渲染异常、导出保真度问题时使用。
  触发词：html2pagx 排查、PAGX 与 HTML 不一致、转换效果不对、PAGX 渲染问题、导出差异。
disable-model-invocation: true
---

# html2pagx 排查 Skill

排查「HTML 经 html2pagx 转换出的 PAGX 效果与 HTML 不一致」的问题。全部工作在 **libpag 仓库根目录**执行。

> 相关但不同的 skill：用户级 `pagx-debugger` 处理的是 **CoCraft 设计稿 → PAGX 导出**；本 skill 处理 **HTML → PAGX 转换**。Rendering 原理可参考 `pagx-debugger`。

## 输入（阶段 0：收集，缺就向用户索取）

| 输入 | 必需 | 说明 |
|---|---|---|
| HTML 源文件路径 | ✅ | 转换的输入，浏览器渲染它作为**真值** |
| 转换后的 PAGX 路径 | ✅ | 要排查的产物（通常来自 AI 小助手后端链路） |
| 问题处截图 | ⭕ | 快速聚焦用户关心的区域（给 `compare.py --crop` 定位用） |
| 视口宽度/高度 | ⭕ | 缺省从 PAGX 根节点 `width` 读；高度默认 900 |

## 转换链路与「归属」概念

```
HTML ──snapshot.js(TS)──▶ 子集HTML ──pagx import(C++)──▶ PAGX ──pagx render(C++)──▶ PNG
        tools/html-snapshot              src/pagx/html              渲染器
        （阶段A：快照）                   （阶段B：导入）             （阶段C：渲染）
```

- **阶段 A 归属**：`tools/html-snapshot`（TS，`lib/browser-snapshot.ts` 等）。
- **阶段 B 归属**：`src/pagx/html`（C++ importer）。
- **阶段 C 归属**：PAGX 渲染器（C++ `src/pagx`）。
- 交付的 PAGX 有可能**出自旧版 importer**（后端链路版本旧）→ 必须做「本地重跑」排除。

## 核心判定规则（**必须先做**）

渲染 HTML 基准图 + 给定 PAGX，对比：

- **一致**（尺寸相同、平均像素差极小、无显著行差异带）⇒ 本仓库三个阶段转换无误。
  问题在**本仓库之外的「导入端 / 消费端」**（如后端服务、客户端集成）。
  → **只归类记录，不在本仓库修复**，停止并告知用户这是导入端问题。
- **不一致** ⇒ 本仓库保真度问题，继续，按模块归因。

`★ 判定阈值经验值：平均像素差 < ~1 且无 row-diff > 25 的带 ⇒ 可视为一致；有 25~250 的行差异带 ⇒ 必有问题。`

## 工作流

### 1. 建工作目录、读视口宽度

```bash
cd <repo>
mkdir -p diag-<tag>          # tag 例：06 / 04-fix
grep -oE '<pagx width="[0-9]+"' "<pagx>"   # 得到 W（高度默认 900）
cp "<html>" diag-<tag>/source.html
```

### 2. 渲染浏览器基准图（真值）

```bash
node tools/html-snapshot/eval/baseline.js "<html>" -o diag-<tag>/baseline.png \
  --viewport-width <W> --viewport-height 900 --browser-engine playwright
```

- 输出为**整页**高度（与 PAGX 高度应基本一致；差很多 → 先怀疑视口不同或产物异常）。
- 若页面用 JS/React 驱动或含动画，参考 `--scroll-reveal` / 管线选项（见下）。

### 3. 渲染给定 PAGX

```bash
./cmake-build-debug/pagx render "<pagx>" -o diag-<tag>/pagx.png --scale 1
```

> ⚠️ **一定用 `--scale 1`**。用 `0.5` 会模糊、对比不公平。
> 其他有用参数：`--crop x,y,w,h`（只看某区域）、`--id`/`--xpath`（只渲某层）、`--time`（动画某一帧）。

### 4. 对比（核心判定）

```bash
python3 .codebuddy/skills/html2pagx-diag/scripts/compare.py \
  diag-<tag>/baseline.png diag-<tag>/pagx.png --outdir diag-<tag>
```

输出：尺寸、平均像素差、**行差异带列表**（每个问题的 y 区间），并写 `diag-<tag>/diffmap.png` 热力图。

- 无差异带 → 一致 → 记为**导入端问题**，停止。
- 有差异带 → 不一致 → 继续。

逐段细看（并排裁剪 + 放大）：

```bash
python3 .../compare.py baseline.png pagx.png --crop 0,3800,1920,4200 --zoom 1.5 \
  --save diag-<tag>/z_F.png
```

### 5. 排除「产物过期」（**关键，别跳过**）

用**当前 HEAD** 本地重跑整条管线，再渲染对比：

```bash
cd <repo>/tools/html-snapshot
./html2pagx "<html>" -o <repo>/diag-<tag>/local --output-name local \
  --pagx-bin <repo>/cmake-build-debug/pagx --pagx-images embed \
  --browser-engine playwright --viewport-width <W> --viewport-height 900 --no-render
cd <repo>
./cmake-build-debug/pagx render diag-<tag>/local/local.pagx -o diag-<tag>/local.png --scale 1
python3 .codebuddy/skills/html2pagx-diag/scripts/compare.py \
  diag-<tag>/baseline.png diag-<tag>/local.png --outdir diag-<tag> --label local
```

- 本地重跑与基准**一致**、而给定 PAGX 不一致 → 交付产物**过期**（旧版 importer 生成），HEAD 已修复。
  → 记录「产物过期 / 后端版本旧」，**无需改代码**；提醒用户同步后端 vendored 副本并重新部署。
- 本地重跑**仍不一致** → 真问题，进入定位。

### 6. 定位与归属

1. 按差异带 y 区间裁剪对比（`--crop`），目视确认现象。
2. 回到 HTML/CSS 找对应 DOM 与样式。
3. 看**子集 HTML**（`diag-<tag>/local/local.subset.html` 或交付 PAGX 对应的 `*.subset.html`）里该结构/样式被表达成什么 → 判断是阶段 A 还是 B 丢了东西。
4. 必要时**像素测量**（颜色/亮度/包围盒/几何）或**最小 PAGX 复现**（手写小 `.pagx`，`pagx render` 看渲染器行为）。
5. 按归属表归类。

### 7. 产出问题清单（等用户确认后再改）

按严重度列出，每条含：**现象 + 像素证据 + 根因(`file:line`) + 归属模块 + 修复难度**。
遵循仓库习惯：**先给清单、用户确认后再动手**（不要未经确认就改代码）。

### 8. 修复与验证（用户确认后）

- TS 改动（`tools/html-snapshot`）：`npm run build && npm run build:browser && npx jest`
- C++ 改动：按仓库构建 `cmake-build-debug`，跑相应测试（如 `PAGFullTest`）、`codeformat.sh`。
- 端到端复验：重跑第 2~4 步，确认目标行区间差异消失，并确认**其它行区间逐行不变**（改动外科手术式、无回归）。
- 提交用 `/commit`（本仓库 `feature/*` 分支直接提交，勿推 main；`dist/` 为 gitignored）。

## 归属分类表

| 现象 | 归属 |
|---|---|
| 子集 HTML 里结构/CSS 就已经丢了或错了 | `tools/html-snapshot`（阶段 A，TS） |
| 子集 HTML 正确，但 PAGX 属性缺失/非法/错误 | `src/pagx/html` importer（阶段 B，C++） |
| PAGX 属性正确，渲染结果仍错 | 渲染器（阶段 C，C++） |
| 字体字距 / 字重 / 字形微差 | **文字引擎**（单列，通常另案处理） |

## 输出与记录约定

- 差异证据落在 `diag-<tag>/`（**草稿目录，不入库**；`.gitignore` 未覆盖时勿提交）。
- 「一致 ⇒ 导入端问题」的记录方式：在结论里明确写
  `归类：导入端（本仓库外/消费端）— 本仓库三段转换与 HTML 一致，不在本仓库修复`。
- 除非用户要求，**不新建文档文件**（结论直接对话输出）。

## 命令速查

```bash
# 基准图
node tools/html-snapshot/eval/baseline.js "<html>" -o <out>/baseline.png \
  --viewport-width <W> --viewport-height 900 --browser-engine playwright

# PAGX 渲染
./cmake-build-debug/pagx render "<pagx>" -o <out>/pagx.png --scale 1

# 本地重跑管线（当前 HEAD）
tools/html-snapshot/html2pagx "<html>" -o <out>/local --output-name local \
  --pagx-bin <repo>/cmake-build-debug/pagx --pagx-images embed \
  --browser-engine playwright --viewport-width <W> --viewport-height 900 --no-render

# 对比
python3 .codebuddy/skills/html2pagx-diag/scripts/compare.py <base.png> <pagx.png> \
  --outdir <out> [--crop x0,y0,x1,y1 --zoom Z --save f.png] [--align] [--label NAME]
```

## 详细经验与已知陷阱

见 [references/known-issues.md](references/known-issues.md)（层叠/z-index、object-fit、棋盘格、字体、旧产物等实战结论）。
