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

call "win\tools\backend-args.bat" "%BACKEND%"
if not "%PAG_BACKEND_SUPPORTED%"=="1" exit /b 1
set "CMAKE_BACKEND_ARGS=%PAG_CMAKE_ARGS%"
set "BACKEND_NAME=%PAG_BACKEND_NAME%"
set "TARGET_SUFFIX=%PAG_TARGET_SUFFIX%"

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

rem Configure MSVC, CMake, Ninja and the Windows SDK in this cmd process.
call "win\tools\vs-env.bat"
if errorlevel 1 (
  echo Error: failed to configure the Visual Studio C++ toolchain, CMake, Ninja or the Windows SDK.
  exit /b 1
)

set "CMAKE_COMPILER_ARGS="
if "%USE_CLANG_CL%"=="1" (
  if defined VS_DIR if exist "%VS_DIR%\VC\Tools\Llvm\x64\bin" set "PATH=%VS_DIR%\VC\Tools\Llvm\x64\bin;%PATH%"
  where clang-cl >nul 2>nul
  if errorlevel 1 (
    echo Error: clang-cl was not found. Install the C++ Clang tools for Windows component.
    exit /b 1
  )
  set "CMAKE_COMPILER_ARGS=-DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_C_COMPILER_LAUNCHER=python;%WORKSPACE%\win\tools\filter_clang.py -DCMAKE_CXX_COMPILER_LAUNCHER=python;%WORKSPACE%\win\tools\filter_clang.py"
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
