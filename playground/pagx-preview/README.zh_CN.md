# PAGX Preview

本地实时预览服务器，用于 [PAGX](https://pag.io/pagx/latest/) 动画文件的渲染和调试。支持文件变更自动刷新、MCP 协议接入 AI 编码助手。

## 功能

- 一行命令启动，浏览器自动打开预览
- 文件修改自动检测、实时重新渲染（SSE 推送）
- 后台常驻守护进程，多文件复用同一服务
- 支持拖放 `.pagx` 文件到浏览器窗口预览
- 作为 MCP Server 接入 AI 编码助手（CodeBuddy / Claude Desktop / VS Code Copilot）
- 字体自动下载（NotoSansSC + NotoColorEmoji）

## 依赖

- Node.js >= 16.7
- 已构建的 `pagx-viewer`（多线程或单线程版本）

## 快速开始

```bash
# 全局安装
npm install -g ./libpag-pagx-preview-<version>.tgz

# 预览文件
pagx-preview /path/to/animation.pagx

# 预览另一个文件（复用已有服务）
pagx-preview /path/to/other.pagx

# 停止后台服务
pagx-preview stop
```

## CLI 参数

| 参数 | 说明 |
|------|------|
| `--port <n>` | 指定端口（默认：系统分配） |
| `--host <addr>` | 绑定地址（默认：127.0.0.1） |
| `--fonts <dir>` | 指定字体目录 |
| `--no-open` | 不自动打开浏览器 |
| `--foreground` | 前台运行（不后台化） |
| `--mcp` | 作为 MCP stdio 服务器运行（见下方 MCP 章节） |
| `--log` | 查看服务日志 |
| `stop` | 停止后台服务 |

## MCP Server 模式

`pagx-preview` 可作为 MCP (Model Context Protocol) 服务器运行，让 AI 编码助手在对话中直接预览 `.pagx` 文件。

### 工作原理

使用 `--mcp` 启动时，进程通过 stdin/stdout 通信 MCP 协议，同时自动启动本地 HTTP 服务器用于 WASM 渲染。用户无需手动启动任何服务——MCP 客户端管理进程生命周期。

### 提供的工具

| 工具 | 说明 |
|------|------|
| `preview_pagx` | 加载 .pagx 文件预览，返回内联 widget + 浏览器 URL |
| `reload_file` | 强制重新加载文件 |
| `get_document` | 获取文档信息（尺寸、时长等） |

### 各平台配置

#### CodeBuddy IDE

在 `~/.codebuddy/mcp.json` 中添加：

```json
{
  "mcpServers": {
    "pagx-preview": {
      "command": "pagx-preview",
      "args": ["--mcp"]
    }
  }
}
```

#### Claude Desktop

macOS: `~/Library/Application Support/Claude/claude_desktop_config.json`
Windows: `%APPDATA%\Claude\claude_desktop_config.json`

```json
{
  "mcpServers": {
    "pagx-preview": {
      "command": "pagx-preview",
      "args": ["--mcp"]
    }
  }
}
```

#### VS Code GitHub Copilot

在项目中创建 `.vscode/mcp.json`：

```json
{
  "servers": {
    "pagx-preview": {
      "type": "http",
      "url": "http://127.0.0.1:<端口>/mcp"
    }
  }
}
```

注意：Copilot 使用 HTTP 传输，需先手动启动服务（`pagx-preview --port <端口> <文件>`）。

### 已知兼容性问题

内联 MCP Apps widget 已在官方 [ext-apps basic-host](https://github.com/modelcontextprotocol/ext-apps/tree/main/examples/basic-host) 参考宿主中验证通过。但各平台宿主的支持情况：

| 宿主 | 状态 | 替代方案 |
|------|------|----------|
| Claude Desktop | widget 可能不渲染（已知 host 侧 bug） | 使用返回的浏览器 URL |
| CodeBuddy IDE | 暂不支持 MCP Apps widget | 使用返回的浏览器 URL |
| VS Code Copilot | 在 Simple Browser 中打开 | 功能等价 |

所有情况下，`preview_pagx` 返回的浏览器 URL（`http://127.0.0.1:<端口>/session/<id>/`）都能正常预览。

## 开发

### 首次设置

```bash
# 1. 构建 pagx-viewer
cd ../pagx-viewer
npm run build:debug:st       # 单线程版（推荐用于 IDE 嵌入）

# 2. 构建 pagx-player
cd ../pagx-player
npm install && npm run build

# 3. 安装依赖并预构建
cd ../pagx-preview
npm install
npm run prebuild             # 复制 viewer/player 产物到 static/
```

`prebuild` 会自动：
- 检测 viewer 构建变体（MT/ST），复制 wasm + glue 到 `static/viewer/`
- 复制 pagx-player ESM bundle 到 `static/player/`
- 复制 ext-apps SDK bundle 到 `static/ext/`
- 用 esbuild 打包 MCP widget bundle 到 `static/mcp-widget.bundle.js`

### 从源码运行

```bash
node src/cli.js /path/to/file.pagx              # 后台
node src/cli.js --foreground /path/to/file.pagx  # 前台
node src/cli.js --mcp                            # MCP 模式
```

### 全局链接测试

```bash
npm link
pagx-preview /path/to/file.pagx
npm unlink -g @libpag/pagx-preview
```

## 目录结构

```
pagx-preview/
├── src/
│   ├── cli.js          # CLI 入口，参数解析
│   ├── daemon.js       # 后台进程管理、stdio MCP 启动
│   ├── server/
│   │   ├── index.js    # Express HTTP 服务器（SSE、静态资源、MCP 挂载）
│   │   ├── session.js  # 文件监听 session（chokidar）
│   │   ├── fonts.js    # 字体查找
│   │   ├── font-cache.js # 字体懒下载
│   │   └── lock.js     # 进程锁
│   └── mcp/
│       ├── server.js   # MCP Server 构建（stdio + HTTP）
│       └── tools.js    # MCP 工具 + 资源定义
├── static/
│   ├── index.js / index.html / index.css  # 浏览器客户端
│   ├── viewer/         # pagx-viewer WASM + glue（prebuild 生成）
│   ├── player/         # pagx-player ESM bundle（prebuild 生成）
│   ├── ext/            # ext-apps SDK bundle（prebuild 生成）
│   ├── icons/          # 播放控制图标
│   ├── mcp-widget.js   # MCP Apps widget 源码
│   ├── mcp-widget.html # MCP Apps widget HTML 壳
│   └── mcp-widget.bundle.js  # 打包后的 widget（prebuild 生成）
├── scripts/
│   ├── prebuild.js     # 产物复制 + bundle 构建
│   └── check-artifacts.js # 发布前检查
└── package.json
```

## 字体

首次运行时自动下载 NotoSansSC-Regular.otf + NotoColorEmoji.ttf 到 `~/.pagx/fonts/`。
优先级：`--fonts` 参数 > `PAGX_FONTS_DIR` 环境变量 > `~/.pagx/fonts/` > libpag 仓库内 `resources/font/`。

设置 `PAGX_FONTS_NO_AUTO_DOWNLOAD=1` 禁用自动下载。

## 许可证

Apache-2.0
