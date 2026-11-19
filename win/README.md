# Win32Demo

English | [简体中文](./README.zh_CN.md)

The Windows demo links the static `libpag.lib`, so the GPU backend is selected when libpag is
built, not by the demo project. Build libpag for the backend you want, then build the demo.

## Build

Run every command below from the repository root; the `build_pag` and `ms-build` paths are
relative to it.

Install the dependency sync tool once and fetch the prebuilt dependencies:

```powershell
npm install -g depsync
depsync
```

OpenGL (default backend):

```powershell
node build_pag pag -a x64 -i -DPAG_BUILD_SHARED=OFF
node third_party/vendor_tools/ms-build -a x64 win/Win32Demo.sln /p:Configuration=Release /p:Platform=x64
```

D3D12 with hardware adapters (falls back to WARP when no adapter is available):

```powershell
node build_pag pag -a x64 -i -DPAG_BUILD_SHARED=OFF -DPAG_USE_D3D12=ON -DPAG_USE_OPENGL=OFF
node third_party/vendor_tools/ms-build -a x64 win/Win32Demo.sln /p:Configuration=Release /p:Platform=x64 /p:PreBuildEventUseInBuild=false
```

D3D12 forced onto WARP:

```powershell
node build_pag pag -a x64 -i -DPAG_BUILD_SHARED=OFF -DPAG_USE_D3D12=ON -DPAG_D3D12_USE_WARP=ON -DPAG_USE_OPENGL=OFF
node third_party/vendor_tools/ms-build -a x64 win/Win32Demo.sln /p:Configuration=Release /p:Platform=x64 /p:PreBuildEventUseInBuild=false
```

The demo reads `libpag.lib` from `out/release/win/x64` for every x64 configuration, so rebuild
libpag before switching backends and keep the CRT/configuration consistent with the demo.

Notes on the `ms-build` commands:

- `PreBuildEventUseInBuild=false` is required for the D3D12 and WARP commands: the project's
  pre-build step runs `build_pag` without backend arguments, which would replace the D3D12
  `libpag.lib` with a default OpenGL build. The OpenGL command already matches that default and
  does not need the switch.
- The project pins the `v142` toolset. If only VS2022 is installed, append
  `/p:PlatformToolset=v143` to every `ms-build` command, otherwise the build stops with `MSB8020`.

## Run

The build writes `win\x64\Release\WinPAGDemo.exe`. The demo looks for `assets/test2.pag` and the
fallback fonts next to the executable first and falls back to the working directory, so it can be
started from any directory:

```powershell
.\win\x64\Release\WinPAGDemo.exe
```

The demo is a GUI application, so it writes its log to `win\x64\Release\WinPAGDemo.log` next to the
executable instead of printing to the console. The log contains the backend line, the resolved asset
path, and an explicit error when a resource is missing. A missing `assets/test2.pag` leaves the
window black because the demo has nothing to draw.

## Runtime

`PAGEngineImpl::InitOnscreenRender()` creates a `GPUDrawable` from the window handle. In a D3D12
build libpag logs `GPUDrawable: the window surface uses the D3D12 backend.` once per drawable so
the active backend is visible in the console or debugger output. Window resize and minimize
restore use the D3D12 swap chain's client-area size.

## D3D12 limitations

- `PAGSurface::waitUntilCompleted()` blocks the calling thread until libpag's D3D12 queue drains.
  There is no fence-based `BackendSemaphore` ABI yet.
- External resources are typed 2D textures only: typeless formats, texture arrays, multisampling,
  multi-plane YUV and compressed formats are rejected. Callers hand the texture over in
  `D3D12_RESOURCE_STATE_COMMON` and must keep the resource alive until completion.
- `HardwareBuffer` on Windows wraps an `ID3D11Texture2D`. Imported D3D11 textures can be sampled
  but cannot be used as render targets.
- The demo has no D3D12 debug-layer configuration; use a D3D12-enabled debugger or the D3D12
  validation layer when diagnosing barrier errors.
