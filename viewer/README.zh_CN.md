[English](./README.md) | 简体中文

# PAGViewer

PAGViewer 是一款基于 Qt 6 和 libpag 渲染引擎的跨平台桌面应用，用于预览和编辑 PAG 动画文件。

## 功能特性

- PAG 动画文件实时预览
- 图层树检视与属性编辑
- 文本图层内容编辑
- PAG 文件中的图片替换
- 性能分析与诊断
- 自动更新支持（macOS 使用 Sparkle，Windows 使用 WinSparkle）

## 环境要求

- **Qt 6.2.0+**（推荐 Qt 6.8.1）
- **CLion** IDE
- **macOS 10.15+** 或 **Windows 10+**（64 位）
- 已成功编译 libpag 库（参见[主 README](../README.zh_CN.md)）

## 编译指南

### macOS

1. 请先参考[编译指南](../README.zh_CN.md)成功编译出 libpag 库文件。

2. 用 CLion 打开 `viewer` 文件夹。

3. 首次 CMake 刷新会自动生成 `QTCMAKE.cfg` 文件，请修改其中的 Qt 路径为本地安装路径：
   ```cmake
   set(CMAKE_PREFIX_PATH /Users/[username]/Qt/6.6.1/macos/lib/cmake)
   ```

4. 编译并运行 `PAGViewer` 目标。

### Windows

1. 请参考[编译指南](../README.zh_CN.md)安装好 VS2019 必要模块，并确保 CLion 的 ToolChain 配置为 **Visual Studio**，架构选择 **amd64**。

2. 用 CLion 打开 `viewer` 文件夹。

3. 修改自动生成的 `QTCMAKE.cfg` 文件，填入本地 Qt 路径：
   ```cmake
   set(CMAKE_PREFIX_PATH C:/Qt/Qt6.2.0/6.2.0/msvc2019_64/lib/cmake)
   ```

4. 在 CLion 中打开 `PAGViewer` 目标的配置面板，在 **Environment Variables** 中添加 Qt DLL 路径：
   ```
   PATH=C:\Qt\Qt6.2.0\6.2.0\msvc2019_64\bin
   ```

5. 编译并运行 `PAGViewer` 目标。

## 命令行参数

```bash
PAGViewer [选项] [文件]

选项:
  -cpu    强制使用软件渲染（禁用 GPU 加速）

参数:
  file    启动时打开的 PAG 文件路径
```

## 项目结构

```
viewer/
├── src/                    # 源代码
│   ├── audio/              # 音频播放
│   ├── editing/            # 图层与属性编辑
│   ├── maintenance/        # 更新与插件管理
│   ├── platform/           # 平台相关代码 (mac/win)
│   ├── profiling/          # 性能分析
│   ├── rendering/          # PAG 渲染集成
│   ├── task/               # 后台任务管理
│   ├── utils/              # 工具函数
│   └── video/              # 视频导出
├── assets/                 # 资源文件
│   ├── qml/                # QML 界面文件
│   ├── images/             # 图标与图片
│   └── translation/        # 国际化文件
├── package/                # 构建与打包脚本
└── vendor/                 # 第三方依赖
```

## 许可证

本项目采用 Apache License 2.0 许可证，详见 [LICENSE](../LICENSE.txt) 文件。
