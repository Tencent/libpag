# 快速上手指南

## 问题修复说明

如果你遇到了以下错误：
```
Initialization failed: ReferenceError: WebAssembly is not defined
```

这是因为微信小程序使用 `WXWebAssembly` 而非标准 `WebAssembly`。

**解决方案**：已通过适配器自动处理，只需重新构建即可。

## 快速构建（推荐）

在 `pagx` 目录下执行：

```bash
npm run build:wechat:quick
```

这个命令会：
1. 复用现有的 WASM 文件
2. 自动适配微信小程序环境
3. 生成到 `wechat/wasm/` 目录

**执行时间**：约 5 秒

## 验证构建

```bash
npm run verify:wechat
```

如果看到 `✅ Verification PASSED`，说明构建成功。

## 在微信开发者工具中测试

### 1. 打开项目

1. 启动「微信开发者工具」
2. 点击「导入项目」
3. 项目目录：`pagx/wechat`
4. AppID：`touristappid`（测试用）

### 2. 编译运行

点击工具栏的「编译」按钮。

### 3. 验证成功

如果看到以下日志（在「控制台」中），说明初始化成功：

```
Starting initialization...
Loading WASM module...
WASM module loaded
PAGXView created successfully
Loading sample from CDN: https://...
Downloaded file size: ... bytes
Sample loaded successfully
Initialization completed successfully!
Rendering started
```

## 常见问题

### Q: 看到 "WebAssembly is not defined" 错误

**A**: 重新运行构建命令：
```bash
cd pagx
npm run build:wechat:quick
```

### Q: 下载 PAGX 文件失败

**A**: 检查网络连接，或修改 `pages/viewer/viewer.js` 中的 `SAMPLE_URL`：
```javascript
const SAMPLE_URL = 'https://your-cdn.com/your-file.pagx';
```

### Q: 黑屏或无内容显示

**A**: 
1. 检查控制台是否有错误
2. 确认基础库版本 ≥ 2.15.0（在「详情」-「本地设置」中查看）
3. 确认 PAGX 文件格式正确

### Q: 如何修改示例文件？

**A**: 编辑 `pages/viewer/viewer.js`：
```javascript
// 修改第 7 行
const SAMPLE_URL = 'https://your-new-url.pagx';
```

然后重新「编译」。

## 技术说明

### 为什么需要适配？

微信小程序的 WebAssembly API 叫 `WXWebAssembly`，而标准浏览器使用 `WebAssembly`。构建脚本会自动：

1. 在 `pagx.js` 开头注入 Polyfill
2. 替换所有 API 调用为兼容版本
3. 处理不支持的 API（如 `instantiateStreaming`）

### 关键配置

- **WASM 版本**：单线程（`-a wasm`）
- **基础库**：最低 2.15.0
- **域名配置**：如使用自定义 CDN，需在后台配置

## 下一步

### 自定义 CDN

1. 上传 PAGX 文件到你的 CDN
2. 修改 `SAMPLE_URL`
3. 在小程序后台配置域名：
   - 登录 [微信小程序后台](https://mp.weixin.qq.com/)
   - 「开发」-「开发管理」-「开发设置」
   - 添加 `downloadFile` 合法域名

### 添加更多功能

参考 `README.md` 了解如何添加：
- 多个示例切换
- 缩放手势
- 播放控制

## 获取帮助

- 查看完整文档：`README.md`
- 技术细节：`../../../.codebuddy/designs/wechat_webassembly_fix.md`
- TGFX 项目：https://github.com/Tencent/tgfx

---

**提示**：如果一切正常，你应该能看到一个可以拖动的 PAGX 动画预览界面！
