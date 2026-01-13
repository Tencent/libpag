---
description: 项目编译与测试相关配置
alwaysApply: true
---

## 编译验证

修改代码后，使用以下命令验证编译。必须传递 `-DPAG_BUILD_TESTS=ON` 以启用所有模块触发编译。

```bash
cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -B cmake-build-debug
cmake --build cmake-build-debug --target PAGFullTest
```

## 测试框架

- 测试用例位于 `test/src/`，基于 Google Test 框架
- 测试代码可通过编译参数访问所有 private 成员，无需 friend class
- 运行测试：按上述编译验证步骤构建并执行 `PAGFullTest`
- 测试命令返回非零退出码表示测试失败，这是正常行为，不要重复执行同一命令
- 测试用例构造时，所有字号、坐标、矩阵等数值尽可能使用整数，避免小数点，以确保清晰度

## 截图测试

- 使用 `Baseline::Compare(pixels, key)` 比较截图，key 格式为 `{folder}/{name}`，例如 `PAGSurfaceTest/Mask`
- 截图输出到 `test/out/{folder}/{name}.webp`，基准图为同目录下 `{name}_base.webp`
- 比较机制：对比 `test/baseline/version.json`（仓库）与 `test/baseline/.cache/version.json`（本地）中同一 key 的版本号，一致时才进行基准图对比，不一致则跳过返回成功，以此接受截图变更

## 接受截图基准变更

**禁止自动接受基准变更**，必须经过用户主动确认后，按以下步骤操作：
1. 编译并完整运行 `PAGFullTest` 所有用例，禁止使用部分用例的输出
2. 将前一步用例输出的 `test/out/version.json` 覆盖到 `test/baseline/`
3. 运行 `UpdateBaseline` target 更新本地基准图缓存

