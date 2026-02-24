# PAGX 微信小程序预览工具

基于 TGFX 和 WebAssembly 的 PAGX 文件预览工具微信小程序版本。

## 快速开始

### 1. 构建项目

在 `pagx` 目录下执行：

```bash
# 快速构建（使用现有 WASM 文件）
npm run build:wechat:quick

# 或完整构建（重新编译 WASM）
npm run build:wechat
```

### 2. 打开微信开发者工具

1. 打开「微信开发者工具」
2. 选择「导入项目」
3. 项目目录选择：`pagx/wechat`
4. AppID 设置为 `touristappid`（测试）或你的 AppID

### 3. 编译运行

点击「编译」按钮，查看预览效果。

## 项目结构

```
wechat/
├── app.json              # 小程序配置
├── app.js                # 小程序入口
├── app.wxss              # 全局样式
├── project.config.json   # 工具配置
├── pages/
│   └── viewer/           # 预览页面
│       ├── viewer.js     # 页面逻辑
│       ├── viewer.wxml   # 页面结构
│       ├── viewer.wxss   # 页面样式
│       └── viewer.json   # 页面配置
└── wasm/                 # WASM 文件（构建生成）
    ├── pagx.wasm         # WASM 模块
    └── pagx.js           # WASM 加载器（已适配微信环境）
```

## 功能说明

### 当前功能（MVP）

- ✅ 从 CDN 加载 PAGX 文件
- ✅ 在 WebGL Canvas 上渲染
- ✅ 触摸拖动手势
- ✅ 重置按钮

### 后续计划

- [ ] 多个示例文件切换
- [ ] 缩放手势
- [ ] 播放控制（暂停/播放/进度条）
- [ ] 截图分享

## 配置说明

### 修改示例文件 URL

编辑 `pages/viewer/viewer.js`：

```javascript
// 修改这个 URL 为你的 CDN 地址
const SAMPLE_URL = 'https://your-cdn.com/samples/your-file.pagx';
```

### 配置合法域名

如果使用自己的 CDN，需要在微信小程序后台配置：

1. 登录 [微信小程序后台](https://mp.weixin.qq.com/)
2. 进入「开发」-「开发管理」-「开发设置」
3. 在「服务器域名」中添加 `downloadFile` 合法域名
4. 示例：`https://your-cdn.com`

## 常见问题

### 1. 报错：WebAssembly is not defined

**原因**：微信小程序使用 `WXWebAssembly` 而非标准 `WebAssembly`。

**解决**：确保已运行 `npm run build:wechat:quick` 或 `npm run build:wechat`，这些命令会自动适配微信环境。

### 2. 报错：Failed to load WASM module

**可能原因**：
- WASM 文件不存在或路径错误
- 使用了多线程版本的 WASM（小程序不支持）

**解决**：
- 检查 `wasm/` 目录下是否有 `pagx.wasm` 和 `pagx.js`
- 确保使用单线程版本构建（`-a wasm`）

### 3. 下载文件失败

**可能原因**：
- CDN URL 不可访问
- 未配置合法域名
- CDN 未启用 HTTPS

**解决**：
- 检查 CDN URL 是否正确
- 在小程序后台配置 `downloadFile` 合法域名
- 确保 CDN 支持 HTTPS 和 CORS

### 4. 渲染黑屏

**可能原因**：
- WebGL 初始化失败
- PAGX 文件格式错误
- Canvas 尺寸问题

**解决**：
- 检查基础库版本（需要 2.15.0+）
- 验证 PAGX 文件是否正常
- 查看控制台错误日志

## 技术限制

### 微信小程序限制

1. **不支持多线程 WASM**：必须使用单线程版本（`-a wasm`）
2. **包体积限制**：单个分包不超过 2MB，总包不超过 20MB
3. **域名白名单**：需要在后台配置合法域名
4. **基础库版本**：最低 2.15.0（支持 `WXWebAssembly`）

### 性能限制

1. **内存限制**：iOS 约 300MB，Android 约 500MB
2. **渲染性能**：取决于设备性能
3. **文件大小**：建议 PAGX 文件不超过 10MB

## 开发调试

### 启用调试信息

在 `pages/viewer/viewer.js` 中，已包含 `console.log` 输出：

```javascript
console.log('Starting initialization...');
console.log('WASM module loaded');
console.log('PAGXView created successfully');
// ...
```

在微信开发者工具的「控制台」中查看日志。

### 性能监控

使用微信开发者工具的「性能」面板：
1. 点击「性能」标签
2. 选择「渲染性能」
3. 查看 FPS、内存使用情况

## 参考资料

- [微信小程序官方文档](https://developers.weixin.qq.com/miniprogram/dev/framework/)
- [WXWebAssembly API](https://developers.weixin.qq.com/miniprogram/dev/framework/performance/wasm.html)
- [TGFX 项目](https://github.com/Tencent/tgfx)
- [PAG 官网](https://pag.io)

## 许可证

BSD 3-Clause License

Copyright (C) 2026 Tencent. All rights reserved.
