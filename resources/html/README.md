# HTML 测试集（浏览器保真度评测）

本目录收录用于回归 PAGX HTML 管线的 HTML 测试语料，配套 `HTMLTest` 目标对它们做
统一的**浏览器保真度评测**：

```
snapshot.js (Chromium)  ->  pagx import  ->  pagx render  ->  与 Chromium 基准图比对
```

即复用 [`tools/html-snapshot/eval`](../../tools/html-snapshot/eval/README.md) 这条评测管线。
每套语料跑完会生成 `report.md` / `report.csv` / `index.html`，供人工查看 SSIM、像素差、
平均 RGB 偏差等指标。**逐例不设门禁**（浏览器保真度本质是“语料级均值”指标，逐例卡阈值
意义不大），但**在语料级均值上设了回归门禁**：每套语料的平均 SSIM / 像素差 / RGB 偏差
会与仓库内的 [`baseline.json`](baseline.json) 比对，均值退化超过容忍值时 `HTMLTest` 失败。
详见下方 [基准门禁](#基准门禁语料级均值)。

## 语料清单

| 目录 | 来源 | 规模 | 说明 |
|------|------|------|------|
| `cases/` | 单特性 importer 语料 | 187 个 `.html` | 每个文件隔离一个 HTML/CSS 特性，便于把回归定位到具体构造。含 `manifest.csv`、`coverage.md`、`README.md`（详见 [cases/README.md](cases/README.md)）。 |
| `corpus/cli/` | pagx CLI / 导出往返语料 | 276 个 `.html` + 必要图片/字体 | 按 `cli/ layout/ pagx_to_html/ spec/ text/` 分类，多为 `HTMLExporter` 产物，用于往返保真。 |
| `corpus/websites/` | 真实网页手写快照 | 26 个 `.html` | 移动端页面的静态化版本，图片/字体一律运行时按需从 CDN 拉取，不入库。 |
| `corpus/generated/` | 多模型生成的演示/技术页 | 162 个 `.html` | 按模型分子目录（`case/`、`glm5.1/`、`ima-*`、`tech-*`），多使用 Tailwind CDN + web 字体。 |

### 语料源码与必要资源

本目录收录 `.html` 源文件、`cases/` 元数据，以及少量 HTML 在浏览器中正确渲染所必需的本地
图片和字体资源。入库的二进制资源仅限 `corpus/cli/` 往返语料所引用的 18 个 PNG（由 Git LFS
管理）和 3 个小型 WOFF2 字体（作为普通 Git 文件提交，单文件均为 KB 级）；`corpus/websites`、
`corpus/generated` 的图片和字体一律在评测时按需从 CDN 拉取，不入库。克隆后需确保 Git LFS
已安装并完成资源拉取：

```bash
git lfs pull
```

驱动脚本会在安装 Node 依赖前检查 PNG 是否仍为 Git LFS 指针；资源未拉取时直接失败，避免
用损坏图片生成看似有效的保真度报告。

评测派生物一律不入库，包括 `*.subset.html`、评测生成的 `baseline.png` / `subset.png` /
`diff.png`、`*.pagx`、`*.pptx` 以及 `tools/html-snapshot/eval/out/`。不要删除 `corpus/cli/`
下 HTML 引用的图片和 `fonts/` 目录，否则浏览器基准图和 PAGX 渲染都会缺少内容。

评测所需的**逐例基准图（ground truth）由 eval 在运行时用 Chromium 从原始 `.html` 现场
渲染**，无需提交（因此逐例像素不具确定性、不适合做逐例回归门禁）。入库的门禁基准是
[`baseline.json`](baseline.json) 里的**语料级均值**，与逐例基准图是两回事。`corpus/websites`、`corpus/generated` 引用的 CDN 样式、web 字体、外链图片，
由 eval 在运行时按需下载（见下方开关），因此这两套语料评测时**需要联网**。

## 前置条件

- 已构建 `pagx` 二进制（默认 `cmake-build-debug/pagx`，或用环境变量 `PAGX_BIN` 指定）。
- Node.js。驱动脚本 `test/run_html_eval.sh` 每次运行前都会在 `tools/html-snapshot` 执行
  `npm install` 与 `npm run build`；使用默认 Playwright 引擎时还会执行
  `npx --no-install playwright install chromium`，确保全新环境具备匹配的 Chromium。
- 评测 `websites` / `generated` 需要网络访问（拉取 CDN CSS、字体、图片）。

## 运行方式

### 方式一：CMake 目标（推荐）

先启用 CLI/测试并构建 `pagx`，再运行 `HTMLTest`：

```bash
cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
cmake --build cmake-build-debug --target pagx      # 产出 cmake-build-debug/pagx
cmake --build cmake-build-debug --target HTMLTest  # 对四套语料整链评测并出报告
```

`HTMLTest` 会依赖并先构建 `pagx-cli`，然后调用 [`test/run_html_eval.sh`](../../test/run_html_eval.sh)。

### 方式二：直接跑驱动脚本

```bash
# 全部四套语料
test/run_html_eval.sh

# 只跑指定语料（cases / cli / websites / generated 任意组合）
test/run_html_eval.sh cases cli

# 指定 pagx 路径 / 透传额外 run.sh 参数
PAGX_BIN=/path/to/pagx test/run_html_eval.sh
EVAL_EXTRA_ARGS="--only bilibili --skip-existing" test/run_html_eval.sh websites

# 调整并发与浏览器引擎（默认 concurrency=16、browser-engine=playwright）
CONCURRENCY=8 BROWSER_ENGINE=puppeteer test/run_html_eval.sh
```

驱动脚本默认以 `--concurrency 16 --browser-engine playwright` 调用底层 eval，可用
环境变量 `CONCURRENCY` / `BROWSER_ENGINE` 覆盖。

逐例低 SSIM 和逐例转换错误只记录在报告中，不构成逐例阈值门禁；但**语料级均值退化**
（见下方基准门禁）、浏览器无法启动、参数错误、语料目录缺失等会使脚本最终返回非零。
若部分语料运行失败，脚本仍会汇总本次成功完成的语料，并明确打印失败列表；旧报告不会
被当成本次结果复用。

## 基准门禁（语料级均值）

`HTMLTest` 在跑完所有语料后，会把每套语料本次的**平均 SSIM / 平均像素差 / 平均 RGB
偏差**与仓库内的 [`baseline.json`](baseline.json) 逐套比对：

- 平均 SSIM 下降超过容忍值 → 该语料 **FAIL**（SSIM 越高越好）
- 平均像素差 / 平均 RGB 偏差上升超过容忍值 → 该语料 **FAIL**（越低越好）
- 均值持平或改善 → PASS
- 任一语料 FAIL，脚本整体返回非零

`baseline.json` 结构为 `corpora.<label>`（`label` 即 `html-cases` / `html-cli` /
`html-websites` / `html-generated`），每套记录 `ssimMean` / `pdMean` / `rgbMean` 三个均值
和一个可选的 `tolerance`。`websites` / `generated` 联网拉取 CDN 资源、天然更抖，默认给了
更宽松的容忍值（`ssim 0.05 / pd 0.05 / rgb 5.0`），`cases` / `cli` 为 `ssim 0.02 / pd 0.02
/ rgb 2.0`。若某套语料的均值为 `null`（尚未 seed），则该套**只出报告、不参与门禁**。

### 更新基准

**NEVER** 手改 `baseline.json` 里的均值。当保真度是有意提升（或首次 seed）时，通过一次
可信的完整运行重新生成基准：

```bash
HTML_EVAL_UPDATE_BASELINE=1 cmake --build cmake-build-debug --target HTMLTest
# 或直接跑驱动脚本：
HTML_EVAL_UPDATE_BASELINE=1 test/run_html_eval.sh
```

`--update-baseline` 只在**本次所有语料都成功**时才会改写 `baseline.json`（部分语料失败时
拒绝写入，避免丢掉缺失语料的基准）；它会保留你手工调过的 `tolerance`，只覆盖三个均值。
更新后请把 `baseline.json` 的变更一并提交，并在 PR/commit 说明保真度变化的原因。

也可用 `HTML_BASELINE=/path/to/baseline.json` 指向其他基准文件。

### 方式三：直接用底层 eval（单套语料、灵活调参）

驱动脚本对每套语料就是以下形式调用（仅语料目录不同），`pagx` 路径通过 `PAGX_BIN`
环境变量传入：

```bash
cd tools/html-snapshot/eval
PAGX_BIN=../../../cmake-build-debug/pagx \
./run.sh --recursive --corpus ../../../resources/html/cases --label html-cases \
         --concurrency 16 --browser-engine playwright
open out/html-cases/index.html
```

## 常用开关（透传给 `eval/run.sh`）

| 开关 | 作用 |
|------|------|
| `--recursive`, `-r` | 递归子目录，case id 用相对路径拼接（驱动脚本已默认开启） |
| `--label <name>` | 输出子目录名 `out/<label>/`（驱动脚本按语料名设为 `html-<corpus>`，即 `html-cases`/`html-cli`/`html-websites`/`html-generated`） |
| `--concurrency <N>`, `-j` | 并行处理的用例数（驱动脚本默认 16，可用 `CONCURRENCY` 覆盖） |
| `--browser-engine <e>` | 浏览器引擎 `playwright` / `puppeteer`（驱动脚本默认 playwright，可用 `BROWSER_ENGINE` 覆盖） |
| `--download-fonts` | 下载页面 web 字体并嵌入/回退（真实网页/CDN 页可按需加，经 `EVAL_EXTRA_ARGS` 传入） |
| `--download-images` | 下载外链图片并按文件路径引用（同上，按需经 `EVAL_EXTRA_ARGS` 传入） |
| `--only <substr>` | 只跑文件名含该子串的用例，便于快速迭代 |
| `--skip-existing` | 复用已生成的 PNG，仅重算指标 |

> `pagx` 二进制通过 `PAGX_BIN` 环境变量传入（驱动脚本会导出；默认 `cmake-build-debug/pagx`），
> 因此每套语料的调用与裸 `eval/run.sh` 完全对齐，仅语料目录不同。若真实网页/生成页需要
> 联网拉取字体与图片，用 `EVAL_EXTRA_ARGS="--download-fonts --download-images"` 传入。

## 输出与指标

输出位于 `tools/html-snapshot/eval/out/html-<corpus>/`（`<corpus>` 为 `cases`/`cli`/`websites`/`generated`）：

- `report.md` —— 顶部是语料级均值汇总，下面是逐例明细表。
- `report.csv` —— 机器可读的逐例数据。
- `index.html` —— `基准 / subset / diff` 三联对照查看器，浏览器打开即可。
- `<case>/` —— 每个用例的 `baseline.png`、`subset.png`、`diff.png`、`subset.html`、
  `subset.pagx` 及各步骤 stderr。

跑完所有语料后，驱动脚本还会调用 `tools/html-snapshot/eval/summary.js` 做**跨语料汇总**：
把每套语料与总体的 `cases/ok/err、SSIM 均值与中位数、pixelDiff、rgbΔ、SSIM 分桶、告警数`
**打印到控制台**，并写出总汇总页 `tools/html-snapshot/eval/out/summary.html`（表格内每套
语料链接到各自的 `index.html`）；随后对照 `baseline.json` 做[基准门禁](#基准门禁语料级均值)。
也可单独运行（`--baseline` 触发门禁，`--update-baseline` 重新 seed）：

```bash
node tools/html-snapshot/eval/summary.js --out tools/html-snapshot/eval/out \
     --baseline resources/html/baseline.json \
     html-cases html-cli html-websites html-generated
```

指标含义（`ssim` / `pixelDiffRatio` / `meanRgbDelta` / `flex*` / `importerWarnings` 等）详见
[`tools/html-snapshot/eval/README.md`](../../tools/html-snapshot/eval/README.md)。**逐例**不做
回归门禁，验收以“语料级均值随迭代改善”为准，并由 `baseline.json` 守住均值不退化。

## 补充：cases 的直接 importer 校验（可选，无需浏览器）

`cases/` 语料另附一份 subset 层的纯 importer 校验脚本 `cases/validate.sh`（`import` +
`verify` + 告警断言，不经浏览器）。它**不接入** `HTMLTest`，仅供需要时手动运行：

```bash
resources/html/cases/validate.sh    # 默认使用 cmake-build-debug/pagx，可传入自定义 pagx 路径
```
