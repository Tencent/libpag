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
- 比较机制：对比 `test/baseline/version.json`（仓库）与 `test/baseline/.cache/version.json`（本地）中同一 key 的版本号
    - 两边 key 都存在且版本号不同：跳过比较并返回成功（用于接受截图变更）
    - 其他情况：正常比较基准图，基准图不存在或不匹配则测试失败

**!! IMPORTANT - 截图基准变更限制**：
- **NEVER** 自动接受截图基准变更，包括禁止自动运行 `UpdateBaseline` target、禁止修改或覆盖 `version.json` 文件
- 必须经过用户确认后运行 `accept_baseline.sh` 脚本来接受变更

## 使用本地 tgfx 源码调试

当怀疑 bug 出在渲染引擎 tgfx 而非 libpag 本身时，可以链接本地 tgfx 源码进行源码级调试。

通过 `-DTGFX_DIR` 参数指定本地 tgfx 仓库路径，CMake 会以 `add_subdirectory` 方式将 tgfx 源码直接加入构建（而非使用预编译缓存），从而支持修改 tgfx 代码后增量编译和断点调试。

本地 tgfx 仓库通常位于 libpag 同级目录 `../tgfx`，但可以指向任意路径。使用独立的构建目录以避免与常规构建冲突：

```bash
cmake -G Ninja -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -DTGFX_DIR=../tgfx -B cmake-build-debuglocal
cmake --build cmake-build-debuglocal --target PAGFullTest
```

- 修改 tgfx 代码后重新执行 build 命令即可增量编译，无需重新 configure
- 如果本地 `../tgfx` 目录存在，且怀疑问题出在渲染层，优先使用此方式调试
