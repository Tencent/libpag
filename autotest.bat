@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Build and run PAGFullTest_<Backend> against the selected backend's baseline cache.
rem
rem Usage:
rem   autotest.bat [USE_OPENGL^|USE_OPENGL_SWIFTSHADER^|USE_D3D12^|USE_D3D12_WARP^|USE_VULKAN]
rem                [--clang-cl] [--filter ^<gtest filter^>]
rem
rem Run update_baseline.bat as a separate step first. It must not be called from here: it switches
rem git branches, and cmd.exe reads batch files from disk line by line, so the running script would
rem be corrupted or deleted mid-run. The same restriction applies to tgfx/autotest.bat.
rem
rem The script fails fast when the md5 cache for the selected backend is missing or does not match
rem test\baseline\version.json, because the tests would otherwise skip every pixel comparison.
rem

cd /d "%~dp0"
set "WORKSPACE=%CD%"
set "BACKEND="
set "USE_CLANG_CL=0"
set "FILTER_ARG="

:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="--clang-cl" goto :arg_clang_cl
if /i "%~1"=="--filter" goto :arg_filter
if /i "%~1"=="-h" goto :usage
if /i "%~1"=="--help" goto :usage
if defined BACKEND goto :too_many_backends
set "BACKEND=%~1"
shift
goto :parse_args

:arg_clang_cl
set "USE_CLANG_CL=1"
shift
goto :parse_args

:arg_filter
if "%~2"=="" (
  echo Error: --filter requires a Google Test filter value. 1>&2
  exit /b 1
)
set "FILTER_ARG=--gtest_filter=%~2"
shift
shift
goto :parse_args

:too_many_backends
echo Error: specify only one backend. 1>&2
exit /b 1

:args_done
if not defined BACKEND set "BACKEND=USE_OPENGL"

set "CMAKE_BACKEND_ARGS=-DPAG_USE_METAL=OFF -DPAG_USE_OPENGL=ON -DPAG_USE_SWIFTSHADER=OFF"
set "BACKEND_NAME=opengl"
set "TARGET_SUFFIX=OpenGL"

if /i "%BACKEND%"=="USE_OPENGL" goto :backend_ready
if /i "%BACKEND%"=="USE_OPENGL_SWIFTSHADER" goto :backend_swiftshader
if /i "%BACKEND%"=="USE_D3D12" goto :backend_d3d12
if /i "%BACKEND%"=="USE_D3D12_WARP" goto :backend_d3d12_warp
if /i "%BACKEND%"=="USE_VULKAN" goto :backend_vulkan
if /i "%BACKEND%"=="USE_METAL" goto :backend_unsupported
if /i "%BACKEND%"=="USE_WEBGL" goto :backend_unsupported
if /i "%BACKEND%"=="USE_WEBGPU" goto :backend_unsupported
echo Error: unknown backend '%BACKEND%'. 1>&2
goto :usage_error

:backend_swiftshader
set "CMAKE_BACKEND_ARGS=-DPAG_USE_METAL=OFF -DPAG_USE_OPENGL=ON -DPAG_USE_SWIFTSHADER=ON"
set "BACKEND_NAME=opengl-swiftshader"
set "TARGET_SUFFIX=OpenGL"
goto :backend_ready

:backend_d3d12
set "CMAKE_BACKEND_ARGS=-DPAG_USE_D3D12=ON -DPAG_D3D12_USE_WARP=OFF -DPAG_USE_METAL=OFF -DPAG_USE_OPENGL=OFF"
set "BACKEND_NAME=d3d12"
set "TARGET_SUFFIX=D3D12"
goto :backend_ready

:backend_d3d12_warp
set "CMAKE_BACKEND_ARGS=-DPAG_USE_D3D12=ON -DPAG_D3D12_USE_WARP=ON -DPAG_USE_METAL=OFF -DPAG_USE_OPENGL=OFF"
set "BACKEND_NAME=d3d12-warp"
set "TARGET_SUFFIX=D3D12"
goto :backend_ready

:backend_vulkan
set "CMAKE_BACKEND_ARGS=-DPAG_USE_VULKAN=ON -DPAG_USE_METAL=OFF -DPAG_USE_OPENGL=OFF"
set "BACKEND_NAME=vulkan"
set "TARGET_SUFFIX=Vulkan"
echo Warning: the Vulkan backend is not wired into libpag yet; configuration will fail until it lands.
goto :backend_ready

:backend_unsupported
echo Error: %BACKEND% is not available on Windows. Use USE_OPENGL, USE_D3D12 or USE_D3D12_WARP. 1>&2
exit /b 1

:backend_ready
set "CACHE_DIR=%WORKSPACE%\test\baseline\.cache\%BACKEND_NAME%"
set "BUILD_DIR=%WORKSPACE%\build-autotest"
set "RESULT_DIR=%WORKSPACE%\result"

where git >nul 2>nul
if errorlevel 1 (
  echo Error: git was not found.
  exit /b 1
)

rem Fail before building when the cache cannot drive pixel comparisons. A missing cache makes
rem Baseline::Compare() treat every key as a version change and skip the comparison silently.
if not exist "%CACHE_DIR%\md5.json" (
  echo Error: no baseline cache for '%BACKEND_NAME%'. Run "update_baseline.bat %BACKEND%" first.
  exit /b 1
)
git diff --no-index --quiet --ignore-cr-at-eol -- "%WORKSPACE%\test\baseline\version.json" "%CACHE_DIR%\version.json" >nul 2>nul
if errorlevel 1 (
  echo Error: the baseline cache for '%BACKEND_NAME%' is out of date. Run "update_baseline.bat %BACKEND%" first.
  exit /b 1
)

rem Use the Visual Studio developer environment so cl.exe, cmake and ninja work outside a
rem developer prompt.
set "PF86=%ProgramFiles(x86)%"
set "VSWHERE=%PF86%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_DIR=%%i"
if defined VS_DIR if exist "%VS_DIR%\VC\Auxiliary\Build\vcvars64.bat" call "%VS_DIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
if defined VS_DIR if exist "%VS_DIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" set "PATH=%VS_DIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
if defined VS_DIR if exist "%VS_DIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "PATH=%VS_DIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"

rem vcvars64 only picks up a registered Windows SDK. Add the installed SDK explicitly so rc.exe
rem and the ucrt/um import libraries are available even when VS did not register one.
set "SDK_ROOT=%PF86%\Windows Kits\10"
set "SDK_VERSION="
if defined WindowsSDKVersion set "SDK_VERSION=%WindowsSDKVersion:\=%"
if not defined SDK_VERSION if exist "%SDK_ROOT%\Include" for /d %%i in ("%SDK_ROOT%\Include\*") do set "SDK_VERSION=%%~nxi"
if defined SDK_VERSION if exist "%SDK_ROOT%\Include\%SDK_VERSION%\um" (
  if defined INCLUDE (
    set "INCLUDE=%INCLUDE%;%SDK_ROOT%\Include\%SDK_VERSION%\ucrt;%SDK_ROOT%\Include\%SDK_VERSION%\shared;%SDK_ROOT%\Include\%SDK_VERSION%\um;%SDK_ROOT%\Include\%SDK_VERSION%\winrt"
  ) else (
    set "INCLUDE=%SDK_ROOT%\Include\%SDK_VERSION%\ucrt;%SDK_ROOT%\Include\%SDK_VERSION%\shared;%SDK_ROOT%\Include\%SDK_VERSION%\um;%SDK_ROOT%\Include\%SDK_VERSION%\winrt"
  )
  if defined LIB (
    set "LIB=%LIB%;%SDK_ROOT%\Lib\%SDK_VERSION%\ucrt\x64;%SDK_ROOT%\Lib\%SDK_VERSION%\um\x64"
  ) else (
    set "LIB=%SDK_ROOT%\Lib\%SDK_VERSION%\ucrt\x64;%SDK_ROOT%\Lib\%SDK_VERSION%\um\x64"
  )
  set "PATH=%SDK_ROOT%\bin\%SDK_VERSION%\x64;%PATH%"
)

set "CMAKE_COMPILER_ARGS="
if "%USE_CLANG_CL%"=="1" (
  if defined VS_DIR if exist "%VS_DIR%\VC\Tools\Llvm\x64\bin" set "PATH=%VS_DIR%\VC\Tools\Llvm\x64\bin;%PATH%"
  where clang-cl >nul 2>nul
  if errorlevel 1 (
    echo Error: clang-cl was not found. Install the "C++ Clang tools for Windows" component.
    exit /b 1
  )
  set "CMAKE_COMPILER_ARGS=-DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl"
)

where cl >nul 2>nul
if errorlevel 1 (
  echo Error: cl.exe was not found. Install the Visual Studio C++ build tools.
  exit /b 1
)
where rc >nul 2>nul
if errorlevel 1 (
  echo Error: rc.exe was not found. Install the Windows 10/11 SDK.
  exit /b 1
)
where cmake >nul 2>nul
if errorlevel 1 (
  echo Error: cmake was not found.
  exit /b 1
)
where ninja >nul 2>nul
if errorlevel 1 (
  echo Error: ninja was not found.
  exit /b 1
)

if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
echo Building PAGFullTest_%TARGET_SUFFIX% for '%BACKEND_NAME%'...
cmake -G Ninja -S "%WORKSPACE%" -B "%BUILD_DIR%" %CMAKE_BACKEND_ARGS% %CMAKE_COMPILER_ARGS% -DPAG_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
if errorlevel 1 (
  echo Error: CMake configuration failed.
  exit /b 1
)
cmake --build "%BUILD_DIR%" --target PAGFullTest_%TARGET_SUFFIX% --parallel 12
if errorlevel 1 (
  echo Error: building PAGFullTest_%TARGET_SUFFIX% failed.
  exit /b 1
)

if exist "%RESULT_DIR%" rmdir /s /q "%RESULT_DIR%"
mkdir "%RESULT_DIR%"
pushd "%BUILD_DIR%"
PAGFullTest_%TARGET_SUFFIX%.exe --gtest_output=json:PAGFullTest.json %FILTER_ARG%
set "TEST_RESULT=%errorlevel%"
copy /y *.json "%RESULT_DIR%\" >nul 2>nul
popd
if exist "%WORKSPACE%\test\out" xcopy "%WORKSPACE%\test\out" "%RESULT_DIR%\out\" /E /I /Y /Q >nul

if not "%TEST_RESULT%"=="0" (
  echo PAGFullTest_%TARGET_SUFFIX% [%BACKEND_NAME%] failed.
  exit /b %TEST_RESULT%
)
echo PAGFullTest_%TARGET_SUFFIX% [%BACKEND_NAME%] passed.
exit /b 0

:usage
echo Usage: autotest.bat [USE_OPENGL^|USE_OPENGL_SWIFTSHADER^|USE_D3D12^|USE_D3D12_WARP^|USE_VULKAN] [--clang-cl] [--filter ^<gtest filter^>]
echo.
echo   USE_OPENGL             OpenGL (default backend).
echo   USE_OPENGL_SWIFTSHADER OpenGL on SwiftShader.
echo   USE_D3D12              D3D12 with hardware adapters.
echo   USE_D3D12_WARP         D3D12 forced onto WARP.
echo   USE_VULKAN             Reserved for the Vulkan backend.
echo.
echo   --clang-cl             Build with clang-cl so private-access tests are compiled in.
echo   --filter ^<expr^>         Forward a Google Test filter to the test binary.
echo.
echo Run update_baseline.bat as a separate step first; this script never checks out another commit.
echo Results are written to result\ (JSON report and failing screenshots).
exit /b 0

:usage_error
echo Usage: autotest.bat [USE_OPENGL^|USE_OPENGL_SWIFTSHADER^|USE_D3D12^|USE_D3D12_WARP^|USE_VULKAN] [--clang-cl] [--filter ^<gtest filter^>]
exit /b 1
