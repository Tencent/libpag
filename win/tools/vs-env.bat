@echo off
rem Configures MSVC, CMake, Ninja and the Windows SDK in the caller's environment.
rem Callers must use `call` and keep running in the same cmd process so the environment survives.

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
if defined WindowsSdkDir set "SDK_ROOT=%WindowsSdkDir%"
set "SDK_VERSION="
if defined WindowsSDKVersion call :SelectSDK "%WindowsSDKVersion:\=%"
if not defined SDK_VERSION if exist "%SDK_ROOT%\Include" for /f "delims=" %%i in ('dir /b /ad /o:n "%SDK_ROOT%\Include" 2^>nul') do call :SelectSDK "%%i"
if not defined SDK_VERSION (
  echo Error: no complete x64 Windows SDK was found in "%SDK_ROOT%". 1>&2
  exit /b 1
)
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

where cl >nul 2>nul
if errorlevel 1 exit /b 1
where rc >nul 2>nul
if errorlevel 1 exit /b 1
where cmake >nul 2>nul
if errorlevel 1 exit /b 1
where ninja >nul 2>nul
if errorlevel 1 exit /b 1
exit /b 0

:SelectSDK
rem Skip partial SDK installations instead of hiding an older complete desktop SDK.
if not exist "%SDK_ROOT%\Include\%~1\ucrt\stdio.h" exit /b 0
if not exist "%SDK_ROOT%\Include\%~1\shared\sdkddkver.h" exit /b 0
if not exist "%SDK_ROOT%\Include\%~1\um\Windows.h" exit /b 0
if not exist "%SDK_ROOT%\Lib\%~1\ucrt\x64\ucrt.lib" exit /b 0
if not exist "%SDK_ROOT%\Lib\%~1\um\x64\kernel32.lib" exit /b 0
if not exist "%SDK_ROOT%\bin\%~1\x64\rc.exe" exit /b 0
if not exist "%SDK_ROOT%\bin\%~1\x64\mt.exe" exit /b 0
set "SDK_VERSION=%~1"
exit /b 0
