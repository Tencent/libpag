@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Refresh the selected backend's local baseline cache on Windows.
rem
rem Usage:
rem   update_baseline.bat [USE_OPENGL^|USE_OPENGL_SWIFTSHADER^|USE_D3D12^|USE_D3D12_WARP^|USE_VULKAN]
rem                       [--skip-images] [--no-checkout]
rem
rem By default the script mirrors update_baseline.sh: it stashes local changes, checks out the
rem origin/main commit, builds and runs UpdateBaseline_<Backend>, then restores the original branch
rem and stash. Pass --no-checkout to build the current working tree instead, which is required for
rem backends that are not merged into main yet.
rem
rem Screenshot outputs and version metadata are shared, so run one backend at a time.
rem
rem The cache is written to test\baseline\.cache\<backend>, where <backend> is opengl,
rem opengl-swiftshader, d3d12, d3d12-warp or vulkan. Running the script regenerates the local md5
rem cache; it never commits anything.

cd /d "%~dp0"
set "WORKSPACE=%CD%"
set "STATUS=0"
set "RESTORE_NEEDED=0"
set "STASH_CREATED=0"
set "BACKEND="
set "SKIP_IMAGES=0"
set "NO_CHECKOUT=0"

:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="--skip-images" goto :arg_skip_images
if /i "%~1"=="--no-checkout" goto :arg_no_checkout
if /i "%~1"=="-h" goto :usage
if /i "%~1"=="--help" goto :usage
if defined BACKEND goto :too_many_backends
set "BACKEND=%~1"
shift
goto :parse_args

:arg_skip_images
set "SKIP_IMAGES=1"
shift
goto :parse_args

:arg_no_checkout
set "NO_CHECKOUT=1"
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
echo Warning: the Vulkan baseline target is not wired into CMake yet; configuration will fail until it lands.
goto :backend_ready

:backend_unsupported
echo Error: %BACKEND% is not available on Windows. Use USE_OPENGL, USE_D3D12 or USE_D3D12_WARP. 1>&2
exit /b 1

:backend_ready
set "CACHE_DIR=%WORKSPACE%\test\baseline\.cache\%BACKEND_NAME%"
set "BUILD_DIR=%WORKSPACE%\build-update-baseline"

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

where cl >nul 2>nul
if errorlevel 1 (
  call :Fail "cl.exe was not found. Install the Visual Studio C++ build tools."
  goto :cleanup
)
where rc >nul 2>nul
if errorlevel 1 (
  call :Fail "rc.exe was not found. Install the Windows 10/11 SDK."
  goto :cleanup
)
where cmake >nul 2>nul
if errorlevel 1 (
  call :Fail "cmake was not found."
  goto :cleanup
)
where ninja >nul 2>nul
if errorlevel 1 (
  call :Fail "ninja was not found."
  goto :cleanup
)
where git >nul 2>nul
if errorlevel 1 (
  call :Fail "git was not found."
  goto :cleanup
)
where node >nul 2>nul
if errorlevel 1 (
  call :Fail "Node.js was not found."
  goto :cleanup
)

if "%NO_CHECKOUT%"=="1" goto :skip_checkout

git fetch origin main
if errorlevel 1 (
  call :Fail "git fetch origin main failed."
  goto :cleanup
)
set "MAIN_COMMIT="
for /f "delims=" %%i in ('git rev-parse --verify "origin/main^{commit}" 2^>nul') do set "MAIN_COMMIT=%%i"
if not defined MAIN_COMMIT (
  call :Fail "origin/main was not found."
  goto :cleanup
)
git cat-file -e %MAIN_COMMIT%:update_baseline.bat >nul 2>nul
if errorlevel 1 (
  call :Fail "origin/main does not contain update_baseline.bat yet, so switching to it would remove this script mid-run. Re-run with --no-checkout to build the current working tree."
  goto :cleanup
)
git show %MAIN_COMMIT%:test/baseline/version.json > "%TEMP%\pag_main_version.json" 2>nul
if errorlevel 1 (
  call :Fail "Cannot read test\baseline\version.json from origin/main."
  goto :cleanup
)
if exist "%CACHE_DIR%\version.json" (
  git diff --no-index --quiet --ignore-cr-at-eol -- "%TEMP%\pag_main_version.json" "%CACHE_DIR%\version.json" >nul 2>nul
  if not errorlevel 1 (
    echo Baseline cache for '%BACKEND_NAME%' is up to date.
    exit /b 0
  )
)

set "CURRENT_BRANCH="
for /f "delims=" %%i in ('git rev-parse --abbrev-ref HEAD') do set "CURRENT_BRANCH=%%i"
set "CURRENT_COMMIT="
for /f "delims=" %%i in ('git rev-parse HEAD') do set "CURRENT_COMMIT=%%i"
set "STASH_BEFORE="
for /f "delims=" %%i in ('git rev-parse --verify -q refs/stash 2^>nul') do set "STASH_BEFORE=%%i"
git stash push --include-untracked --quiet
if errorlevel 1 (
  call :Fail "git stash push failed."
  goto :cleanup
)
set "STASH_AFTER="
for /f "delims=" %%i in ('git rev-parse --verify -q refs/stash 2^>nul') do set "STASH_AFTER=%%i"
if not "%STASH_BEFORE%"=="%STASH_AFTER%" set "STASH_CREATED=1"
git switch --detach %MAIN_COMMIT% --quiet
if errorlevel 1 (
  call :Fail "git switch --detach failed."
  goto :cleanup
)
set "RESTORE_NEEDED=1"

:skip_checkout
where depsync >nul 2>nul
if not errorlevel 1 goto :depsync_ready
echo Installing depsync...
call npm install -g depsync
if errorlevel 1 (
  call :Fail "Installing depsync failed."
  goto :cleanup
)
:depsync_ready
call depsync
if errorlevel 1 (
  call :Fail "depsync failed."
  goto :cleanup
)

if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
set "CMAKE_IMAGE_ARGS=-DPAG_SKIP_GENERATE_BASELINE_IMAGES=OFF"
if "%SKIP_IMAGES%"=="1" set "CMAKE_IMAGE_ARGS=-DPAG_SKIP_GENERATE_BASELINE_IMAGES=ON"
echo Building UpdateBaseline_%TARGET_SUFFIX% for '%BACKEND_NAME%'...
cmake -G Ninja -S "%WORKSPACE%" -B "%BUILD_DIR%" %CMAKE_BACKEND_ARGS% %CMAKE_IMAGE_ARGS% -DPAG_BUILD_TESTS=ON -DPAG_SKIP_BASELINE_CHECK=ON -DCMAKE_BUILD_TYPE=Debug
if errorlevel 1 (
  call :Fail "CMake configuration failed."
  goto :cleanup
)

set "BASELINE_TARGET=UpdateBaseline_%TARGET_SUFFIX%"
set "GENERATED_CACHE_DIR=%CACHE_DIR%"
if not exist "%BUILD_DIR%\CMakeFiles\%BASELINE_TARGET%.dir" (
  if /i "%TARGET_SUFFIX%"=="OpenGL" (
    if exist "%BUILD_DIR%\CMakeFiles\UpdateBaseline.dir" (
      set "BASELINE_TARGET=UpdateBaseline"
      set "GENERATED_CACHE_DIR=%WORKSPACE%\test\baseline\.cache"
      echo Using the legacy OpenGL baseline target.
    )
  )
)
if not exist "%BUILD_DIR%\CMakeFiles\%BASELINE_TARGET%.dir" (
  call :Fail "The configured project does not provide %BASELINE_TARGET%."
  goto :cleanup
)

cmake --build "%BUILD_DIR%" --target %BASELINE_TARGET% --parallel 12
if errorlevel 1 (
  call :Fail "Building %BASELINE_TARGET% failed."
  goto :cleanup
)
if exist "%GENERATED_CACHE_DIR%\md5.json" del /q "%GENERATED_CACHE_DIR%\md5.json"
if not exist "%BUILD_DIR%\%BASELINE_TARGET%.exe" (
  call :Fail "%BASELINE_TARGET%.exe was not produced."
  goto :cleanup
)
"%BUILD_DIR%\%BASELINE_TARGET%.exe"
if errorlevel 1 (
  if exist "%WORKSPACE%\test\out" (
    if not exist "%WORKSPACE%\result" mkdir "%WORKSPACE%\result"
    xcopy "%WORKSPACE%\test\out" "%WORKSPACE%\result\out\" /E /I /Y /Q >nul
  )
  call :Fail "Baseline cache refresh for '%BACKEND_NAME%' failed."
  goto :cleanup
)

call :RequireFile "%GENERATED_CACHE_DIR%\md5.json" "md5.json"
if errorlevel 1 goto :cleanup
call :RequireFile "%GENERATED_CACHE_DIR%\HEAD" "HEAD"
if errorlevel 1 goto :cleanup
git diff --no-index --quiet --ignore-cr-at-eol -- "%WORKSPACE%\test\baseline\version.json" "%GENERATED_CACHE_DIR%\version.json" >nul 2>nul
if errorlevel 1 (
  call :Fail "%BASELINE_TARGET% left a version ledger that does not match test\baseline\version.json."
  goto :cleanup
)

if /i "%GENERATED_CACHE_DIR%"=="%CACHE_DIR%" goto :cache_ready
if not exist "%CACHE_DIR%" mkdir "%CACHE_DIR%"
copy /y "%GENERATED_CACHE_DIR%\md5.json" "%CACHE_DIR%\" >nul
copy /y "%GENERATED_CACHE_DIR%\version.json" "%CACHE_DIR%\" >nul
copy /y "%GENERATED_CACHE_DIR%\HEAD" "%CACHE_DIR%\" >nul
if errorlevel 1 (
  call :Fail "Importing the legacy OpenGL cache failed."
  goto :cleanup
)
echo Imported the legacy OpenGL cache into '%BACKEND_NAME%'.

:cache_ready
if "%RESTORE_NEEDED%"=="1" (
  call :RestoreWorkspace
  if errorlevel 1 (
    call :Fail "Could not restore the original workspace. Keep the stash for recovery."
    goto :cleanup
  )
  set "RESTORE_NEEDED=0"
)
call depsync
if errorlevel 1 echo Warning: depsync failed after restoring the workspace.
echo Baseline cache for '%BACKEND_NAME%' refreshed successfully.
set "STATUS=0"
goto :cleanup

:usage
echo Usage: update_baseline.bat [USE_OPENGL^|USE_OPENGL_SWIFTSHADER^|USE_D3D12^|USE_D3D12_WARP^|USE_VULKAN] [--skip-images] [--no-checkout]
echo.
echo   USE_OPENGL             OpenGL (default backend).
echo   USE_OPENGL_SWIFTSHADER OpenGL on SwiftShader.
echo   USE_D3D12              D3D12 with hardware adapters, cache in .cache\d3d12.
echo   USE_D3D12_WARP         D3D12 forced onto WARP, cache in .cache\d3d12-warp.
echo   USE_VULKAN             Reserved for the Vulkan backend.
echo.
echo   --skip-images          Only refresh md5.json; do not generate _base.webp images.
echo   --no-checkout          Build the current working tree instead of origin/main.
echo.
echo By default the script stashes local changes and checks out origin/main, then restores the
echo original branch and stash. Run one backend at a time; test\out and version.json are shared.
exit /b 0

:usage_error
echo Usage: update_baseline.bat [USE_OPENGL^|USE_OPENGL_SWIFTSHADER^|USE_D3D12^|USE_D3D12_WARP^|USE_VULKAN] [--skip-images] [--no-checkout]
exit /b 1

:Fail
echo Error: %~1 1>&2
set "STATUS=1"
exit /b 0

:RestoreWorkspace
cd /d "%WORKSPACE%"
if /i "%CURRENT_BRANCH%"=="HEAD" (
  git checkout %CURRENT_COMMIT% --quiet
) else (
  git switch "%CURRENT_BRANCH%" --quiet
)
if errorlevel 1 exit /b 1
if "%STASH_CREATED%"=="1" (
  git stash pop --index --quiet
  if errorlevel 1 exit /b 1
  set "STASH_CREATED=0"
)
exit /b 0

:RequireFile
if not exist "%~1" (
  call :Fail "%~2 was not produced."
  exit /b 1
)
for %%F in ("%~1") do if %%~zF==0 (
  call :Fail "%~2 is empty."
  exit /b 1
)
exit /b 0

:cleanup
if "!RESTORE_NEEDED!"=="1" (
  call :RestoreWorkspace
  if errorlevel 1 (
    echo Error: could not restore the original workspace. Keep the stash for recovery. 1>&2
    set "STATUS=1"
  )
)
if "!STATUS!"=="0" if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
exit /b !STATUS!
