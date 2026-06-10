# 微信小程序构建指南

## 🚀 快速开始

### 一键构建

```bash
cd pagx/wechat
npm run build:wechat
```

这个命令会：
1. 构建 TGFX 库（`npm run build:tgfx`）
2. 编译单线程 WASM（`node script/cmake.wx.js`）
3. 打包到 `wechat/wasm/`（`node script/rollup.wx.js`）

---

## ⚠️ 重要说明

### 为什么需要单独构建？

微信小程序**不支持 SharedArrayBuffer**，必须使用**单线程 WASM**。

| 特性 | Web 版本 | 微信版本 |
|------|---------|---------|
| 多线程 | ✅ | ❌ |
| SharedArrayBuffer | ✅ | ❌ |
| 构建命令 | `npm run build` | `npm run build:wechat` |
| 输出目录 | `wasm-mt/` | `wechat/wasm/` |

---

## 📦 构建流程

### Step 1: 配置环境

```bash
# 确保 Emscripten 已安装
source $EMSDK/emsdk_env.sh

# 验证版本
emcc --version
```

### Step 2: 运行构建

```bash
cd pagx/wechat
npm run build:wechat
```

### Step 3: 验证输出

```bash
ls -lh wechat/wasm/
```

应该看到：
- `pagx.wasm` (~1.8 MB)
- `pagx.js` (~150-200 KB)

---

## 🎯 成功标志

```
✅ Build artifacts:
   - pagx.wasm: 1.85 MB
   - pagx.js: 168 KB
   - Location: build-wechat

✅ WeChat Miniprogram package completed!
```

---

## 🔧 故障排查

### 问题 1: 编译错误

```
WebTypeface.cpp:119:35: error
```

**原因：** TGFX 源码错误  
**解决：** 修复源码后重新构建

---

### 问题 2: 找不到 WASM

```
❌ pagx.wasm not found
```

**检查：**
```bash
# 查看构建目录
ls build-wechat/

# 如果不存在，重新构建
rm -rf build-wechat
npm run build:wechat
```

---

### 问题 3: 微信小程序运行失败

```
SharedArrayBuffer is not defined
```

**原因：** 使用了多线程版本  
**解决：** 确保运行 `npm run build:wechat`

---

## 🧪 测试验证

### 1. 导入微信开发者工具

- 项目目录：`pagx/wechat`
- AppID：`touristappid`

### 2. 检查文件加载

控制台应显示：
```
✓ WASM loaded successfully
✓ PAGXViewer initialized
```

### 3. 测试功能

- [ ] PAGX 文件加载成功
- [ ] Canvas 显示内容（非黑屏）
- [ ] 拖动移动功能正常
- [ ] 重置功能正常
- [ ] 无控制台错误

---

## 📚 相关文档

- **完整设计方案：** `.codebuddy/designs/pagx_wechat_design.md`
- **构建原理：** `.codebuddy/designs/pagx_wechat_build.md`
- **快速启动：** `QUICKSTART.md`

---

## 🎉 下一步

构建成功后：

1. 打开微信开发者工具
2. 导入项目：`pagx/wechat/wx_demo`
3. 编译运行
4. 测试功能

Good luck! 🚀
