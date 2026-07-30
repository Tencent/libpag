# PAGX Preview

本地实时预览服务器，用于 [PAGX](https://pag.io/pagx/latest/) 动画文件的渲染和调试。支持文件变更自动刷新、MCP 协议接入 AI 编码助手。

它作为 [`@libpag/pagx`](https://www.npmjs.com/package/@libpag/pagx) 的 `pagx preview` 子命令对外提供。终端用户通过 `@libpag/pagx` CLI 以 `pagx preview` 调用；本包也可作为独立命令 `pagx-preview` 运行，该形式用于本地开发和维护者测试（见[开发](#开发)）。

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
# 全局安装主 CLI（内置 preview 子命令）
npm install -g @libpag/pagx

# 预览文件
pagx preview /path/to/animation.pagx

# 预览另一个文件（复用已有服务）
pagx preview /path/to/other.pagx

# 停止后台服务
pagx preview stop
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

`pagx preview` 可作为 MCP (Model Context Protocol) 服务器运行，让 AI 编码助手在对话中直接预览 `.pagx` 文件。

### 工作原理

使用 `--mcp` 启动时，进程通过 stdin/stdout 通信 MCP 协议，同时自动启动本地 HTTP 服务器用于 WASM 渲染。用户无需手动启动任何服务——MCP 客户端管理进程生命周期。

### 提供的工具

| 工具 | 说明 |
|------|------|
| `preview_pagx` | **默认预览。** 加载文件并返回 session URL，供在 IDE webview 面板或浏览器中打开；不渲染内联 widget。 |
| `preview_pagx_widget` | **内联小窗预览。** 直接在对话中渲染动画（对话内小窗）。仅当用户明确要求内联 /「小窗」预览时使用。 |
| `reload_file` | 强制从磁盘重新加载文件（文件变更会自动重载，这是手动触发）。 |
| `get_document` | 获取文档信息（尺寸、时长等）。 |

`preview_pagx` 作为默认工具，因为内联 widget 在各桌面端宿主中的渲染并不可靠（见
[已知兼容性问题](#已知兼容性问题)）。只有 `preview_pagx_widget` 才携带 MCP Apps UI 资源
（`ui://pagx-preview/main`），让支持的宿主挂载内联 iframe；`preview_pagx` 刻意不带它，因此永远
不会触发有问题的 widget。

### 各平台配置

（请注意 开发调试和发布版本的mcp配置有所区别，详情请看 开发-本地打包测试-4）

#### CodeBuddy IDE

在 `~/.codebuddy/mcp.json` 中添加：

```json
{
  "mcpServers": {
    "pagx-preview": {
      "command": "pagx",
      "args": ["preview", "--mcp"]
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
      "command": "pagx",
      "args": ["preview", "--mcp"]
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

注意：Copilot 使用 HTTP 传输，需先手动启动服务（`pagx preview --port <端口> <文件>`）。

### 已知兼容性问题

内联 widget（`preview_pagx_widget`）已在官方 [ext-apps basic-host](https://github.com/modelcontextprotocol/ext-apps/tree/main/examples/basic-host) 参考宿主中验证通过，但各桌面端宿主对内联 MCP Apps 的支持情况不一：

| 宿主 | 内联 widget 状态 |
|------|------|
| Claude Desktop | widget iframe 可能不被宿主挂载（已知 host 侧 bug） |
| CodeBuddy IDE | 暂不支持 MCP Apps 内联 widget |
| VS Code Copilot | 在 Simple Browser（编辑器标签）中打开，而非内联 widget |

因此 `preview_pagx`（在 IDE webview 面板 / 浏览器中打开）为默认工具，`preview_pagx_widget` 为
可选。所有情况下，session URL（`http://127.0.0.1:<端口>/session/<id>/`）都能提供带实时重载的完整
预览。

## 开发

> 本节命令针对**独立**的 `pagx-preview` 命令（通过 `npm link` 或 `node src/cli.js` 运行），这是本包
> 单独开发和测试的方式。终端用户则通过 `@libpag/pagx` CLI 以 `pagx preview` 使用同一工具。

### 首次设置

```bash
cd ../pagx-preview
npm install
npm run build                # 一键构建 pagx-viewer(单线程) + pagx-player，并拷贝产物
```

`npm run build` 会串联完成全部步骤：先构建上游的 `pagx-viewer`（单线程 `st` 变体）和
`pagx-player`，再把产物拷贝到 `static/`。

构建命令：

| 命令 | viewer 变体 | 适用场景 |
| --- | --- | --- |
| `npm run build` | 单线程 (st) | 默认。全场景可用——MCP widget **和** 浏览器预览。 |
| `npm run build:release` | 单线程 (st)，release | 同上，优化构建。 |
| `npm run build:mt` | 多线程 (mt) | 仅浏览器预览；渲染更快。 |
| `npm run build:release:mt` | 多线程 (mt)，release | 同上，优化构建。 |

默认用单线程（`st`）变体：MCP widget 运行在没有跨源隔离的沙箱 iframe 中，多线程版依赖的
`SharedArrayBuffer` 在那里不可用。`mt` 变体渲染更快，但只在纯浏览器预览（`pagx-preview
file.pagx`，服务器会发送所需的 COOP/COEP 头）下可用；在 MCP 宿主中 widget 会回退到打开浏览器
URL。

如果上游产物已经构建好（或克隆的仓库已自带产物），可以只运行 `npm run prebuild` 仅拷贝而不
重新构建。`prebuild` 会自动：
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

### 用户如何获取

用户无需构建或打包。包发布后，编译产物（`static/viewer/*.wasm`、`static/player/*.js` 等）已经
随 npm tarball 一起发出，直接安装即可拿到开箱即用的命令——和 `@libpag/pagx` CLI 一样：

```bash
npm install -g @libpag/pagx-preview   # 下载预编译产物，无需编译
```

构建（`npm run build`）是维护者在发布前重新生成产物的步骤。下面的本地打包测试仅在包尚未发布到
npm registry 前用于模拟真实安装。

### 本地打包测试

用本地 tarball 复现用户从 `npm install` 得到的完整效果。

```bash
# 1. 构建产物（发布版）并打包 tarball
cd ../pagx-preview
npm run build:release
npm pack --dry-run           # 检查将要发布的文件列表
npm pack                     # 生成 libpag-pagx-preview-<version>.tgz

# 2. 全局安装 tarball，模拟已发布状态
npm unlink -g @libpag/pagx-preview   # 先移除已有的开发链接
npm install -g ./libpag-pagx-preview-<version>.tgz

# 3. 验证 CLI
pagx-preview --help
pagx-preview /path/to/file.pagx      # 打开浏览器预览

# 4. 验证 MCP 服务
#    注意：本节针对的是**独立** pagx-preview 命令，其 MCP json 与终端用户（发布形态）不同：
#      - 独立开发/测试： { "command": "pagx-preview", "args": ["--mcp"] }
#      - 终端用户（@libpag/pagx）： { "command": "pagx", "args": ["preview", "--mcp"] }（见上文「各平台配置」）
#    在 MCP 客户端（CodeBuddy / Claude Desktop）中指向已安装的独立命令：
#      { "mcpServers": { "pagx-preview": { "command": "pagx-preview", "args": ["--mcp"] } } }
#    然后让助手预览一个 .pagx 文件。也可直接冒烟测试 stdio 启动：
pagx-preview --mcp < /dev/null       # 应正常启动、stdout 无输出并挂起等待

# 5. 清理
npm uninstall -g @libpag/pagx-preview
rm libpag-pagx-preview-<version>.tgz
rm -rf ~/.pagx                       # 清除缓存的字体、日志和进程锁（还原为全新用户状态）
```

### 发布到 npm


> 当前分发方式：本工具随 `@libpag/pagx` 以 `pagx preview` 子命令提供，终端用户无需单独安装本包。

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
