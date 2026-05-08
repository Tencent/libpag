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

### 安装依赖

```bash
cd playground/pagx-playground
npm install
```

### 构建

```bash
# 构建 TypeScript 源码
npm run build

# 构建 Release 版本（压缩）
npm run build:release

# 清理构建产物
npm run clean
```

## 开发

### 启动开发服务器

启动本地开发服务器，支持热更新：

```bash
npm run server
```

服务器将自动在浏览器中打开 http://localhost:8081。

> 注意：开发服务器需要 SharedArrayBuffer 来支持 WASM 多线程。
> 如果遇到问题，请确保浏览器支持所需的 COOP/COEP 头。

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

## 与 PAGX Viewer 配合使用

Playground 依赖 [pagx-viewer](../pagx-viewer) 的构建产物。
构建 playground 前请先构建 pagx-viewer：

```bash
# 在 playground/pagx-viewer 中
npm run build:release

# 然后在 playground/pagx-playground 中
npm run build:release
```

## 浏览器要求

- Chrome 69+
- Firefox 79+
- Safari 15+
- Edge 79+

需要浏览器支持 WebGL2 和 WebAssembly。

## 开源协议

Apache-2.0
