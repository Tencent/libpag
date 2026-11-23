# Win32Demo

[English](./README.md) | 简体中文

Windows 示例工程链接的是静态库 `libpag.lib`，因此 GPU 后端由 libpag 的构建配置决定，而不是由示例工程决定。先按目标后端构建 libpag，再构建示例。

## 构建

下面的命令都要在仓库根目录执行，`build_pag` 和 `ms-build` 的路径是相对仓库根目录的。

首次使用需安装依赖同步工具并拉取预编译依赖：

```powershell
npm install -g depsync@1.4.6
depsync
```

OpenGL（默认后端）：

```powershell
node build_pag pag -a x64 -i -DPAG_BUILD_SHARED=OFF
node third_party/vendor_tools/ms-build -a x64 win/Win32Demo.sln /p:Configuration=Release /p:Platform=x64
```

D3D12，使用硬件适配器（没有可用适配器时回退到 WARP）：

```powershell
node build_pag pag -a x64 -i -DPAG_BUILD_SHARED=OFF -DPAG_USE_D3D12=ON -DPAG_USE_OPENGL=OFF
node third_party/vendor_tools/ms-build -a x64 win/Win32Demo.sln /p:Configuration=Release /p:Platform=x64 /p:PagBackend=D3D12
```

强制 D3D12 使用 WARP：

```powershell
node build_pag pag -a x64 -i -DPAG_BUILD_SHARED=OFF -DPAG_USE_D3D12=ON -DPAG_D3D12_USE_WARP=ON -DPAG_USE_OPENGL=OFF
node third_party/vendor_tools/ms-build -a x64 win/Win32Demo.sln /p:Configuration=Release /p:Platform=x64 /p:PagBackend=D3D12WARP
```

示例工程在所有 x64 配置下都从 `out/release/win/x64` 读取 `libpag.lib`，因此切换后端前需要重新构建 libpag，并保证 CRT 和构建配置与示例一致。

`ms-build` 命令说明：

- `PagBackend=D3D12` 或 `PagBackend=D3D12WARP` 会让工程的预构建步骤给 `build_pag` 传同样的后端参数，
  默认值是 `OpenGL`。不加这个属性时，预构建会用默认的 OpenGL 构建覆盖 D3D12 版 `libpag.lib`。
- 如果 libpag 已经自己构建过，或者不想执行预构建，可以改用 `/p:PreBuildEventUseInBuild=false`。
- 工程固定使用 `v142` 工具集。如果只安装了 VS2022，需要在每条 `ms-build` 命令后追加
  `/p:PlatformToolset=v143`，否则构建会以 `MSB8020` 失败。

## 运行

构建产物是 `win\x64\Release\WinPAGDemo.exe`。示例会先从可执行文件向上回溯到仓库根目录查找 `assets/test2.pag` 和兜底字体，
找不到时再按当前工作目录查找，因此可以从任意目录启动：

```powershell
.\win\x64\Release\WinPAGDemo.exe
```

示例是 GUI 程序，日志写到可执行文件旁边的 `win\x64\Release\WinPAGDemo.log`，不会输出到控制台。
日志里包含当前后端、实际解析到的资源路径，以及资源缺失时的明确报错；`assets/test2.pag` 缺失时示例没有内容可画，窗口会保持黑屏。

## 运行时

`PAGEngineImpl::InitOnscreenRender()` 使用窗口句柄创建 `GPUDrawable`。在 D3D12 构建下，libpag 会为每个 drawable 输出一次 `GPUDrawable: the window surface uses the D3D12 backend.`，因此可以在控制台或调试器输出中确认当前生效的后端。窗口缩放和最小化恢复使用 D3D12 交换链的客户区尺寸。

## D3D12 限制

- `PAGSurface::waitUntilCompleted()` 会阻塞调用线程，直到 libpag 的 D3D12 队列排空；目前还没有基于 fence 的 `BackendSemaphore` ABI。
- 外部资源仅支持 typed 2D 纹理：typeless 格式、纹理数组、多重采样、多平面 YUV 和压缩格式都会被拒绝。调用方需要以 `D3D12_RESOURCE_STATE_COMMON` 状态传入纹理，并保证资源在渲染完成前不被释放。
- Windows 上的 `HardwareBuffer` 包装的是 `ID3D11Texture2D`。导入的 D3D11 纹理可以被采样，但不能作为渲染目标使用。
- 示例工程没有配置 D3D12 调试层；排查 barrier 错误时请使用开启 D3D12 的调试器或 D3D12 校验层。
