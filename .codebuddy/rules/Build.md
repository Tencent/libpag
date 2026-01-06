---
description: PAG 项目编译与测试相关配置
alwaysApply: true
---

## 编译验证

修改代码后，使用以下命令验证编译。必须传递 `-DPAG_BUILD_TESTS=ON` 以启用所有模块触发编译。

```bash
cmake -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
cmake --build cmake-build-debug --target PAGFullTest -- -j 12
```

## 测试框架
- 测试用例位于 `test/src/`，基于 Google Test 框架
- 测试代码可通过编译参数访问所有 private 成员，无需 friend class
- 运行测试：按上述编译验证步骤构建并执行 `PAGFullTest`

## 截图测试
- 使用 `Baseline::Compare(pixels, key)` 比较截图，key 格式为 `{folder}/{name}`，例如 `PAGSurfaceTest/Mask`
- 截图输出到 `test/out/{folder}/{name}.webp`，基准图为同目录下 `{name}_base.webp`
- 比较机制：对比 `test/baseline/version.json`（仓库）与 `.cache/version.json`（本地）中同一 key 的版本号，一致时才进行基准图对比，不一致则跳过返回成功，以此接受截图变更

## 更新基准

### 接受截图变更
将 `test/out/version.json` 覆盖到 `test/baseline/` 以接受截图变更，需同时满足：
1. 使用完整运行 `PAGFullTest` 所有用例后输出的 `version.json`，禁止使用部分用例的输出
2. 用户明确确认接受变更

### 同步本地缓存
`UpdateBaseline` 或 `update_baseline.sh` 用于同步 version.json 到 `.cache/` 并生成本地缓存。
CMake 会在两者不一致时提醒手动运行，**禁止自动执行此命令**。
