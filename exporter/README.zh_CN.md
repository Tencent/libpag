[English](./README.md) | 简体中文

# PAGExporter

PAGExporter 是一款 Adobe After Effects 插件，能够一键将设计师在 AE 中制作的动效内容导出成 PAG 动画文件。

## 功能特性

- **纯矢量导出**：将 AE 合成导出为纯矢量文件，体积小且支持无限缩放
- **混合导出**：BMP 预合成结合矢量的混合导出，支持所有 AE 特性同时保持运行时可编辑性
- **实时预览**：导出前可直接在 After Effects 内预览 PAG 效果
- **批量导出**：通过面板界面一次导出多个合成
- **导出验证**：自动检测不支持的特性并给出详细警告
- **PAGViewer 集成**：一键安装 PAGViewer 桌面预览工具

## 菜单命令

安装后，After Effects 中将提供以下命令：

| 菜单 | 命令 | 说明 |
|------|---------|-------------|
| 编辑 > 首选项 | PAG Config... | 配置导出设置和偏好 |
| 文件 > 导出 | PAG File... | 将当前合成导出为 PAG 文件 |
| 文件 > 导出 | PAG File (Panel)... | 打开导出面板进行批量操作 |
| 文件 > 导出 | PAG Preview... | 预览当前合成的 PAG 效果 |

## 环境要求

- **Adobe After Effects 2020+**（推荐 CC 2023）
- **Qt 6.2.0+**（推荐 Qt 6.6.1）
- **CLion** IDE
- **macOS 10.15+** 或 **Windows 10+**（64 位）
- **After Effects SDK**（推荐 2023 Windows 版本）
- 已成功编译 libpag 库（参见[主 README](../README.zh_CN.md)）

## 编译指南

### 环境准备

1. **下载 After Effects SDK**

   下载地址：https://developer.adobe.com/console/3505738/servicesandapis

   建议下载 2023 Windows 版本。解压后在 exporter 目录创建 `AESDK.cfg` 文件：
   ```cmake
   set(AE_SDK_PATH /path/to/AfterEffectsSDK/Examples)
   ```

2. **配置 Qt 路径**

   在 exporter 目录创建 `QTCMAKE.cfg` 文件：
   ```cmake
   set(CMAKE_PREFIX_PATH /Users/[username]/Qt/6.6.1/macos/lib/cmake)
   ```

3. **先编译 libpag**

   请先确保 libpag 能正常编译，再编译 PAGExporter。参见[编译指南](../README.zh_CN.md)。

### macOS

1. 用 CLion 打开 `exporter` 文件夹。
2. 编译 `PAGExporter` 目标。
3. 插件将生成为 `PAGExporter.plugin`。

### Windows

1. 确保 VS2019 安装了**使用 C++ 的桌面开发**和**通用 Windows 平台开发**组件。

2. 在 CLion 中，打开 **File > Settings > Build, Execution, Deployment > Toolchains**，设置：
   - Toolchain：**Visual Studio**
   - Architecture：**amd64**（推荐）或 **x86**

3. 用 CLion 打开 `exporter` 文件夹。

4. 编译 `PAGExporter` 目标。

5. 插件将生成为 `PAGExporter.aex`。

## 安装方法

### macOS
将 `PAGExporter.plugin` 复制到：
```
/Applications/Adobe After Effects [版本]/Plug-ins/
```

### Windows
将 `PAGExporter.aex` 及其依赖文件复制到：
```
C:\Program Files\Adobe\Adobe After Effects [版本]\Support Files\Plug-ins\
```

## 项目结构

```
exporter/
├── src/
│   ├── aecommand/          # AE 菜单命令处理
│   ├── config/             # 导出配置管理
│   ├── export/             # 核心导出逻辑
│   ├── platform/           # 平台相关代码 (mac/win)
│   ├── ui/                 # Qt 窗口和模型类
│   └── utils/              # AE 数据转换工具
├── assets/
│   ├── qml/                # QML 界面文件
│   ├── images/             # 图标与图片
│   └── translation/        # 国际化文件
├── scripts/                # 构建和部署脚本
├── templates/              # 版本文件模板
└── vendor/                 # 第三方依赖
```

## 常见问题

- **CMake 编译报错**：更新 CMake 到最新版本后重新编译。
- **插件未出现在 AE 菜单**：确认插件已放置在正确的 Plug-ins 目录，并重启 After Effects。
- **导出失败**：查看 AE 控制台获取详细错误信息。

## 许可证

本项目采用 Apache License 2.0 许可证，详见 [LICENSE](../LICENSE.txt) 文件。
