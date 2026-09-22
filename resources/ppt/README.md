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
PPT_EVAL_ALLOW_PNG_FALLBACK=1 test/run_ppt_eval.sh smoke

# 通过 CMake 只跑 smoke
PPT_EVAL_CORPORA=smoke PPT_EVAL_ALLOW_PNG_FALLBACK=1 \
  cmake --build cmake-build-debug --target PPTTest

# 筛选用例、调整并发
PPT_EVAL_ONLY=text CONCURRENCY=1 test/run_ppt_eval.sh features
```

前置条件：

- 已构建 `pagx` CLI；
- Node.js；
- LibreOffice (`soffice`)；
- `pdftocairo` 或 `pdftoppm`（Poppler），用于把每页 PDF 固定以 96 DPI 光栅化。

开发机暂时没有 Poppler 时，可用 `PPT_EVAL_ALLOW_PNG_FALLBACK=1` 允许 LibreOffice 直接导出单页 PNG。该兼容路径不适合多页或严格 CI，也不能用于更新 baseline。

## 语料

语料清单在 [`corpora.json`](corpora.json)：

- `features`：`resources/pagx_to_html/*.pagx`
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

Baseline 与操作系统、架构、LibreOffice 和 PDF 光栅化器版本绑定。环境不匹配或没有对应条目时只生成报告，并明确显示 `SKIP`，不会错误套用另一环境的数值。
CI 应设置 `PPT_EVAL_REQUIRE_BASELINE=1`，使缺失或环境不匹配的 baseline 直接失败。
同一 corpus 可以在 `baseline.json` 中保存多套环境基准，更新某个环境不会覆盖其他环境。

在固定、可信环境完成全量运行后更新：

```bash
PPT_EVAL_UPDATE_BASELINE=1 test/run_ppt_eval.sh
```

不要手工修改均值。语料数量变化也会触发门禁失败，确保新增或删除用例需要显式重新基线。
除语料均值外，baseline 还记录逐页指标；单页明显退化无法被其他高分用例的均值掩盖。
