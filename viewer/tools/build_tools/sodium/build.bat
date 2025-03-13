@echo off
setlocal enabledelayedexpansion

@REM Set root directory
set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%..\..\.."
set "VENDOR_SOURCE_DIR=%ROOT_DIR%\third_party\sodium"
set "OUT_PREFIX_DIR=%ROOT_DIR%\third_party\out\sodium"

@REM Check if output directory exists
if exist "%OUT_PREFIX_DIR%" (
    echo Sodium path[%OUT_PREFIX_DIR%] is exist, skip build
    exit /b 0
)

@REM Find Visual Studio installation
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" (
    echo Error: vswhere.exe not found
    exit /b 1
)

@REM Get Visual Studio version and installation path
for /f "tokens=*" %%i in ('call "!VSWHERE!" -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath') do (
    set "VS_PATH=%%i"
)

if not defined VS_PATH (
    echo Error: Visual Studio installation not found
    exit /b 1
)

@REM Apply patch
if exist "%SCRIPT_DIR%\sodium.patch" (
    pushd "%VENDOR_SOURCE_DIR%"
    git apply "%SCRIPT_DIR%\sodium.patch"
    popd
)

for /f "tokens=1,2 delims=:" %%a in ('call "!VSWHERE!" -latest -format json -products * -prerelease ^| findstr "installationVersion"') do (
    set "VS_VERSION=%%~b"
    set "VS_VERSION=!VS_VERSION:~2,2!"
    if "!VS_VERSION!"=="17" (
        set "VS_LINE_VERSION=2022"
        set "VS_DISPLAY_VERSION=17"
    ) else if "!VS_VERSION!"=="16" (
        set "VS_LINE_VERSION=2019"
        set "VS_DISPLAY_VERSION=16"
    ) else if "!VS_VERSION!"=="15" (
        set "VS_LINE_VERSION=2017"
        set "VS_DISPLAY_VERSION=15"
    ) else if "!VS_VERSION!"=="14" (
        set "VS_LINE_VERSION=2015"
        set "VS_DISPLAY_VERSION=14"
    ) else if "!VS_VERSION!"=="12" (
        set "VS_LINE_VERSION=2013"
        set "VS_DISPLAY_VERSION=12"
    ) else if "!VS_VERSION!"=="11" (
        set "VS_LINE_VERSION=2012"
        set "VS_DISPLAY_VERSION=11"
    ) else if "!VS_VERSION!"=="10" (
        set "VS_LINE_VERSION=2010"
        set "VS_DISPLAY_VERSION=10"
    ) else (
        echo Error: Unsupported Visual Studio version: !VS_VERSION!
        exit /b 1
    )
)

if not defined VS_LINE_VERSION (
    echo No supported Visual Studio version found
    exit /b 1
)

echo Use Visual Studio %VS_LINE_VERSION% %VS_DISPLAY_VERSION% to build sodium

@REM Build sodium
set "BUILD_SCRIPT=%VENDOR_SOURCE_DIR%\builds\msvc\build\buildbase.bat"
set "SLN_FILE=%VENDOR_SOURCE_DIR%\builds\msvc\vs%VS_LINE_VERSION%\libsodium.sln"

call "%BUILD_SCRIPT%" "%SLN_FILE%" %VS_DISPLAY_VERSION%
if errorlevel 1 (
    echo Failed to build sodium
    goto :cleanup
)

@REM Copy files
for %%C in (Debug Release) do (
    @REM Create output directories
    set "LIBS_OUT_PATH=%OUT_PREFIX_DIR%\lib\%%C"
    set "INCLUDES_OUT_PATH=%OUT_PREFIX_DIR%\include"
    
    if not exist "!LIBS_OUT_PATH!" mkdir "!LIBS_OUT_PATH!"
    if not exist "!INCLUDES_OUT_PATH!" mkdir "!INCLUDES_OUT_PATH!"

    @REM Copy libraries
    for /d %%D in ("%VENDOR_SOURCE_DIR%\bin\x64\%%C\*") do (
        if exist "%%D\dynamic\libsodium.dll" copy /Y "%%D\dynamic\libsodium.dll" "!LIBS_OUT_PATH!"
        if exist "%%D\dynamic\libsodium.lib" copy /Y "%%D\dynamic\libsodium.lib" "!LIBS_OUT_PATH!"
    )

    @REM Copy includes
    copy /Y "%VENDOR_SOURCE_DIR%\src\libsodium\include\sodium.h" "!INCLUDES_OUT_PATH!"
    xcopy /E /I /Y "%VENDOR_SOURCE_DIR%\src\libsodium\include\sodium" "!INCLUDES_OUT_PATH!\sodium\" >nul
)

:cleanup
@REM Restore git changes
if exist "%SCRIPT_DIR%\sodium.patch" (
    pushd "%VENDOR_SOURCE_DIR%"
    git restore .
    popd
)

exit /b 0
