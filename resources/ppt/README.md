# PPT 视觉回归测试

`PPTTest` 验证 PAGX 导出的 PPTX 在 LibreOffice 中实际打开后的视觉结果：

```text
pagx render → reference PNG
pagx export --format pptx → LibreOffice PDF → 96 DPI PNG → compare
```

它与 `PAGXPPTTest` 互补：后者验证导出器 API、OOXML 结构和边界条件；本测试验证完整应用链路的视觉保真度。

## 运行

```bash
cmake --build cmake-build-debug --target PPTTest

# 快速验证
test/run_ppt_eval.sh smoke

# 通过 CMake 只跑 smoke
PPT_EVAL_CORPORA=smoke \
  cmake --build cmake-build-debug --target PPTTest

# 筛选用例、调整并发
PPT_EVAL_REQUIRE_BASELINE=0 PPT_EVAL_ONLY=text CONCURRENCY=1 test/run_ppt_eval.sh features
```

前置条件：

- macOS 或 Linux；原生 Windows 暂不支持，Windows 上请在 WSL 中使用 Linux 版工具链；
- 已构建 `pagx` CLI；
- Node.js；
- LibreOffice (`soffice`)；
- `pdftocairo` 或 `pdftoppm`（Poppler），用于把每页 PDF 固定以 96 DPI 光栅化。
- 已执行 `git lfs pull`；入口会检查语料目录、声明的外部资源和 `resources/font`，发现 pointer 文件立即失败。

开发机暂时没有 Poppler 时，可用 `PPT_EVAL_REQUIRE_BASELINE=0 PPT_EVAL_ALLOW_PNG_FALLBACK=1 test/run_ppt_eval.sh smoke` 生成单页 PNG 报告。该兼容路径仅支持 `--scale 1`，不适合多页或严格 CI，也不能用于更新 baseline。

## 语料

语料清单在 [`corpora.json`](corpora.json)：

- `features`：递归收集 `resources/pagx_to_html/**/*.pagx`，包括 `unit/` 下的父子阴影等 18 个用例；外部图片 `resources/apitest/test_timestretch.png` 纳入缓存键
- `layout`：`resources/layout/*.pagx`
- `text`：`resources/text/*.pagx`
- `cli`：`resources/cli/*.pagx` 中可正常渲染的用例；排除预期失败和导入冲突样例
- `spec`：静态 `spec/samples/*.pagx`
- `smoke`：PPT 专属小型单页语料
- `decks`：多页 PPTX 语料

不要复制已有 PAGX 文件到本目录。新增通用 PAGX 语料应进入原有资源目录；只有 PPT 专属回归用例才放到 `cases/`。

## 输出与失败条件

输出位于 `tools/ppt-eval/out/`，包含每套语料的 `report.csv`、`report.md`、`report.json`、`index.html` 和总览 `summary.html`。逐例目录包含：

- `reference-N.png`
- `export.pptx`
- `export.pdf`
- `actual-N.png`
- `diff-N.png`

以下情况直接失败：导出/渲染失败、PPT 页数不符、普通尺寸范围内的页面尺寸异常，或已建立 baseline 后的语料均值退化。

指标包括整页 SSIM、像素差、RGB 差，以及以 PAGX 非白内容包围盒计算的 ROI SSIM/RGB 差。

## Baseline

Baseline 与操作系统、架构、LibreOffice 和 PDF 光栅化器版本绑定。CMake `PPTTest` 强制设置 `PPT_EVAL_REQUIRE_BASELINE=1`，shell 入口也默认开启，缺失或环境不匹配直接 `FAIL`。
需要在其他环境生成报告时，显式设置 `PPT_EVAL_REQUIRE_BASELINE=0` 调用 shell 入口；此时不匹配的条目显示 `SKIP`，空 baseline 或全 SKIP 会打印醒目警告。
同一 corpus 可以在 `baseline.json` 中保存多套环境基准，更新某个环境不会覆盖其他环境。

首份 `smoke` 基准由本地真实渲染生成，并人工核对两页输出：macOS 15.7.9（arm64）、LibreOffice 26.2.2.2（`1f77d10d6938fd34972958f64b2bcfa54f8b1ba5`）、pdftocairo 26.09.0，默认导出参数、scale 1。原生文字在 LibreOffice 中存在字宽差异；该基准记录当前表现，用于检测后续退化，不代表像素完全一致。

其他语料和 CI 环境尚未 seed，严格模式会失败。首次启用 CI 视觉门禁时，先固定操作系统、字体、LibreOffice 和 Poppler 版本，执行 LFS 检查并完整运行 `smoke`，人工核对参考图、实际图和 diff 后提交对应环境的基准；其他 corpus 按相同步骤逐套纳入，禁止用自动更新覆盖回归。

在固定、可信环境完成所选语料的全量运行后更新：

```bash
PPT_EVAL_UPDATE_BASELINE=1 test/run_ppt_eval.sh smoke
# 再独立运行一次严格门禁
test/run_ppt_eval.sh smoke
```

更新模式在报告中标记为 `UPDATED`，不与刚写入的数据比较，也不输出 `PASS`。筛选或不完整的运行不能更新基准。

不要手工修改均值。语料数量变化也会触发门禁失败，确保新增或删除用例需要显式重新基线。
除语料均值外，baseline 还记录逐页指标；单页明显退化无法被其他高分用例的均值掩盖。
`tolerance` 和 `pageTolerance` 可以局部调整字段，按“默认值 → corpus → environment”的优先级合并。
