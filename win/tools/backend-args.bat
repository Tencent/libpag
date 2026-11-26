@echo off
rem Resolves a USE_* backend argument into the CMake flags, cache name and target suffix shared by
rem update_baseline.bat and autotest.bat. Sets PAG_BACKEND_SUPPORTED=1 on success.

set "PAG_BACKEND_SUPPORTED=0"
set "PAG_BACKEND_NAME="
set "PAG_TARGET_SUFFIX="
set "PAG_CMAKE_ARGS="

if /i "%~1"=="USE_OPENGL" goto :opengl
if /i "%~1"=="USE_OPENGL_SWIFTSHADER" goto :swiftshader
if /i "%~1"=="USE_D3D12" goto :d3d12
if /i "%~1"=="USE_D3D12_WARP" goto :d3d12_warp
if /i "%~1"=="USE_VULKAN" goto :vulkan
echo Error: backend '%~1' is not available on Windows. Use USE_OPENGL, USE_D3D12 or USE_D3D12_WARP. 1>&2
exit /b 1

:opengl
set "PAG_CMAKE_ARGS=-DPAG_USE_METAL=OFF -DPAG_USE_OPENGL=ON -DPAG_USE_SWIFTSHADER=OFF"
set "PAG_BACKEND_NAME=opengl"
set "PAG_TARGET_SUFFIX=OpenGL"
goto :supported

:swiftshader
set "PAG_CMAKE_ARGS=-DPAG_USE_METAL=OFF -DPAG_USE_OPENGL=ON -DPAG_USE_SWIFTSHADER=ON"
set "PAG_BACKEND_NAME=opengl-swiftshader"
set "PAG_TARGET_SUFFIX=OpenGL"
goto :supported

:d3d12
set "PAG_CMAKE_ARGS=-DPAG_USE_D3D12=ON -DPAG_D3D12_USE_WARP=OFF -DPAG_USE_METAL=OFF -DPAG_USE_OPENGL=OFF"
set "PAG_BACKEND_NAME=d3d12"
set "PAG_TARGET_SUFFIX=D3D12"
goto :supported

:d3d12_warp
set "PAG_CMAKE_ARGS=-DPAG_USE_D3D12=ON -DPAG_D3D12_USE_WARP=ON -DPAG_USE_METAL=OFF -DPAG_USE_OPENGL=OFF"
set "PAG_BACKEND_NAME=d3d12-warp"
set "PAG_TARGET_SUFFIX=D3D12"
goto :supported

:vulkan
set "PAG_CMAKE_ARGS=-DPAG_USE_VULKAN=ON -DPAG_USE_METAL=OFF -DPAG_USE_OPENGL=OFF"
set "PAG_BACKEND_NAME=vulkan"
set "PAG_TARGET_SUFFIX=Vulkan"
echo Warning: the Vulkan backend is not wired into libpag yet; configuration will fail until it lands.
goto :supported

:supported
set "PAG_BACKEND_SUPPORTED=1"
exit /b 0
