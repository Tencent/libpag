# PAGX Playground

[English](./README.md) | 简体中文

一个用于在浏览器中查看和测试 PAGX 文件的交互式 Web 示例。

## 简介

PAGX Playground 是一个基于 Web 的交互式演示项目，使用 [pagx-viewer](./pagx-viewer) SDK 在浏览器中渲染和预览 PAGX 文件。
它提供了可视化界面，用于加载 PAGX 文件、浏览示例文件，以及查看 PAGX 规范文档。

## 功能特性

- 在浏览器中直接加载和预览 PAGX 文件
- 预置 PAGX 示例文件浏览器
- 交互式渲染控制
- PAGX 规范文档查看器
- Skill 文档集成

## 目录结构

```
pagx-playground/
├── static/          # 静态资源（HTML、CSS、图片）
├── pages/           # 文档页面的 HTML 模板
├── scripts/         # 构建和发布脚本
├── src/             # TypeScript 源码
└── wasm-mt/         # WebAssembly 构建产物（构建时生成）
```

> 注意：开发时字体文件直接从 `libpag/resources/font` 提供，发布时会复制到输出目录。
> 字体文件不纳入 git 版本管理。

## 构建

### 前置依赖

首先，请确保已安装项目根目录 [README.md](../../README.md#Development) 中列出的所有工具和依赖，
包括 Emscripten。

> Playground 本身不编译任何 WebAssembly——WASM 来自 [pagx-viewer](../pagx-viewer)，播放器 ESM
> 包来自 [pagx-player](../pagx-player)。你不再需要手动构建它们：下面的 `build` 命令会在打包
> playground 前自动编译这两个上游包。多线程（MT）/ 单线程（ST）的选择通过命令变体决定
> （`:st` = 单线程）。

### 安装依赖

```bash
cd playground/pagx-playground
npm install
```

### 构建与运行

每个 `build` 命令都会先编译上游的 `pagx-viewer`（MT 或 ST）和 `pagx-player`，再打包 playground。
选择所需变体，然后启动开发服务器：

```bash
# 多线程 viewer（默认，需要 SharedArrayBuffer / COOP+COEP）
npm run build
npm run build:release

# 单线程 viewer（无需 SharedArrayBuffer）
npm run build:st
npm run build:release:st

# 然后启动服务
npm run server
```

所以常用的单线程开发流程就是：

```bash
npm run build:release:st && npm run server
```

`clean` 清理构建产物：

```bash
npm run clean
```

如果上游产物已构建好（或克隆的仓库已自带产物），想跳过重新编译，可改用仅拷贝的 `bundle` 命令
——它们只暂存已有产物并打包，不重新构建 pagx-viewer / pagx-player：

```bash
npm run bundle        # 多线程，仅拷贝
npm run bundle:st     # 单线程，仅拷贝
```

`:st` 变体选择单线程 viewer 版本；真正的 MT/ST WASM 编译发生在 pagx-viewer 中（由 `build`
命令自动触发）。多线程构建依赖 `SharedArrayBuffer`，需要 `Cross-Origin-Opener-Policy` 和
`Cross-Origin-Embedder-Policy` 头。开发服务器会自动检测构建类型，仅在多线程构建时发送这些头，
因此在两种构建之间切换时无需手动配置。

## 开发

### 启动开发服务器

启动本地开发服务器：

```bash
npm run server
```

服务器会自动在默认浏览器中打开 playground，并根据检测到的构建类型应用对应的 COOP/COEP 头
（参见[构建](#构建)）。

### 局域网分享

默认情况下服务器仅绑定 `localhost`。如需与同一网络下的其他设备共享（例如在手机上测试），
可通过 `PAGX_LAN` 环境变量启用局域网绑定：

```bash
PAGX_LAN=1 npm run server
```

启用后控制台会打印 `LAN access` 地址。请仅在可信网络中启用，因为它会将本地文件服务器
暴露给局域网内的任意设备。

## 发布

将 playground 发布到输出目录：

```bash
npm run publish
```

这将构建 Release 版本并复制所有必要的文件到输出目录，包括：
- 静态资源（HTML、CSS、图片）
- WASM 文件
- 字体文件
- 示例 PAGX 文件
- 规范文档
- Skill 文档

### 发布选项

```bash
# 发布到自定义输出目录
npm run publish -- -o /path/to/output

# 跳过构建步骤（使用预构建文件）
npm run publish -- --skip-build
```

## 浏览器要求

- Chrome 69+
- Firefox 79+
- Safari 15+
- Edge 79+

需要浏览器支持 WebGL2 和 WebAssembly。

## 开源协议

Apache-2.0
