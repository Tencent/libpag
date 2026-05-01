[English](./README.md) | [中文](./README.zh_CN.md)

# PAGX Viewer 微信小程序版

用于在微信小程序中渲染 PAGX 文件的运行时。C++ 渲染核心通过 Emscripten
编译为 WebAssembly，TypeScript 绑定代码打包为单一 ESM 模块，供小程序直接
引入使用。

## 目录结构

```
pagx/wechat/
├── CMakeLists.txt       # 构建 wasm 模块的 CMake 入口
├── src/                 # 绑定到 JS 的 C++ 源码（PAGXView、GridBackground、binding.cpp）
├── ts/                  # TypeScript 源码（pagx.ts、pagx-view.ts、gesture-manager.ts 等）
├── script/              # 构建脚本（CMake / Rollup / emsdk 环境准备 / 产物拷贝）
├── wasm/                # CMake 原始产物（pagx-viewer.wasm、pagx-viewer.js）
├── lib/                 # 打包后的 JS 与压缩 wasm（.br）
├── wx_demo/             # 使用 viewer 的小程序示例
├── pagx-viewer-miniprogram/  # 独立的小程序分包
└── package.json
```

## 环境要求

- **Node.js** 16 及以上，含 `npm`
- **CMake** 3.13+ 与 **Ninja**（供 CMake 包装脚本使用）
- **Brotli** 命令行工具需在 `PATH` 中（用于压缩最终的 wasm）
  - macOS：`brew install brotli`
  - Debian/Ubuntu：`sudo apt-get install brotli`
- 主机上需具备可用的 C/C++ 工具链（供 CMake 配置阶段使用）

**无需手动安装 Emscripten**。首次运行构建脚本时会自动将 `emsdk 3.1.20`
克隆并激活到 `third_party/emsdk`。

构建前请先在 libpag 仓库根目录执行依赖同步脚本：

```bash
cd /path/to/libpag
./sync_deps.sh
```

## 一键构建

在当前目录（`pagx/wechat/`）执行：

```bash
npm install
npm run build:wechat
```

这条命令会依次执行完整流水线：

1. `node script/cmake.wx.js -a wasm` — 通过 Emscripten + CMake 构建
   `pagx-viewer.wasm` / `pagx-viewer.js`，产物输出到 `wasm/` 并拷贝到
   `lib/`。
2. `brotli -f ./lib/pagx-viewer.wasm` — 生成 `lib/pagx-viewer.wasm.br`。
3. `rollup -c ./script/rollup.wx.js` — 将 TypeScript 源码（`ts/pagx.ts`、
   `ts/gesture-manager.ts`）打包为 `lib/pagx-viewer.js` 和
   `wx_demo/utils/gesture-manager.js`。
4. `node script/copy-files.js` — 把 `lib/pagx-viewer.js` 与
   `lib/pagx-viewer.wasm.br` 拷贝到 `wx_demo/utils/`，让示例小程序用上
   最新产物。
5. `tsc -p ./tsconfig.type.json` — 输出 `.d.ts` 类型声明。
6. `node script/copy-to-cocraft.js` — 把产物拷贝到一个外部小程序工程。
   **此步骤指向写死的本地路径，仅限内部使用**，详见下文
   [可选：外部拷贝步骤](#可选外部拷贝步骤)。

## 分步构建

只重新编译 wasm：

```bash
node script/cmake.wx.js -a wasm
```

只重新打包 JS（不编译 wasm）：

```bash
npm run build:wechat:js
```

清理 wasm 构建缓存（强制下次完整重编）：

```bash
npm run clean
```

## 产物说明

构建成功后，可分发的文件如下：

| 文件 | 说明 |
|------|------|
| `lib/pagx-viewer.js` | 打包后的 ESM 胶水层 + TypeScript 绑定 |
| `lib/pagx-viewer.wasm` | 未压缩的 WebAssembly 模块 |
| `lib/pagx-viewer.wasm.br` | 随小程序分发的 Brotli 压缩版 wasm |
| `wx_demo/utils/pagx-viewer.js` | 示例小程序副本，由 `copy-files.js` 自动同步 |
| `wx_demo/utils/pagx-viewer.wasm.br` | 示例小程序副本，由 `copy-files.js` 自动同步 |

## 运行示例

`wx_demo/` 是使用该 viewer 的最小示例小程序。用微信开发者工具打开该
目录并点击 **运行** 即可。每次执行 `npm run build:wechat` 都会自动刷新
demo 的 `utils/` 目录。

## 可选：外部拷贝步骤

`script/copy-to-cocraft.js` 中写死了一个指向内部消费者工程
（`cocraft-wechat`）的绝对路径。如果你不是该工程的使用者：

- 可修改 `script/copy-to-cocraft.js` 中的 `targetDir` 为你自己的路径；
- 或跳过这一步，手动执行前几个子任务：
  ```bash
  node script/cmake.wx.js -a wasm
  brotli -f ./lib/pagx-viewer.wasm
  npm run build:wechat:js
  ```

## 常见问题

- **`emsdk` 克隆或更新失败**：脚本需要访问
  `github.com/emscripten-core/emsdk`。如处于受限网络环境，可先手动将
  `emsdk` 克隆到 `third_party/emsdk`，脚本会识别并激活 3.1.20 版本。
- **`brotli: command not found`**：请按上文 [环境要求](#环境要求) 安装
  Brotli 命令行工具。
- **wasm 似乎没有重新编译**：删除 `.pagx-viewer.wasm.md5` 以及 `wasm/`
  下的构建目录（或执行 `npm run clean`）可强制重新编译。
- **找不到 `tgfx`**：确保已在 libpag 根目录执行过 `./sync_deps.sh`，使
  `third_party/tgfx` 存在。也可直接调用 CMake 时通过
  `-DTGFX_DIR=/path/to/tgfx` 指定位置。
