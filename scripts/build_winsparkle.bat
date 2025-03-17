@echo off
setlocal EnableDelayedExpansion

goto :main

:: function GetMSBuildPath begin
:GetMSBuildPath
set "MSBUILD_PATH="
set "vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if not exist "%vswhere%" (
    echo Error: vswhere.exe not found
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%vswhere%" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do (
    set "VS_PATH=%%i"
)

if not defined VS_PATH (
    echo Error: Visual Studio installation not found
    exit /b 1
)

set "MSBUILD_PATH=%VS_PATH%\MSBuild\Current\Bin\MSBuild.exe"
if not exist "!MSBUILD_PATH!" (
    echo Error: MSBuild.exe not found
    exit /b 1
)

echo !MSBUILD_PATH!
exit /b 0
:: function GetMSBuildPath end

:: function Build begin
:Build
set "BUILD_TYPE=%~1"
set "LIB_OUT_PATH=%OUT_PATH%\lib\%ARCH%\%BUILD_TYPE%"
set "INCLUDE_OUT_PATH=%OUT_PATH%\include\winsparkle"

echo Building %BUILD_TYPE% configuration...
"%MSBUILD_PATH%" "%SLN_FILE_PATH%" /p:Platform=%ARCH% /p:Configuration=%BUILD_TYPE%
if %ERRORLEVEL% neq 0 (
    echo Failed to build %BUILD_TYPE% configuration
    exit /b 1
)

if not exist "%LIB_OUT_PATH%" mkdir "%LIB_OUT_PATH%"
if not exist "%INCLUDE_OUT_PATH%" mkdir "%INCLUDE_OUT_PATH%"

copy /Y "%VENDOR_SOURCE_DIR%\%ARCH%\%BUILD_TYPE%\WinSparkle.lib" "%LIB_OUT_PATH%"
copy /Y "%VENDOR_SOURCE_DIR%\%ARCH%\%BUILD_TYPE%\WinSparkle.dll" "%LIB_OUT_PATH%"
copy /Y "%VENDOR_SOURCE_DIR%\include\*.h" "%INCLUDE_OUT_PATH%"

echo Finished building %BUILD_TYPE% configuration
exit /b 0
:: function Build end

:: function main begin
:main
set "ROOT_DIR=%~dp0..\"
set "VENDOR_SOURCE_DIR=%ROOT_DIR%\third_party\winsparkle"
set "SLN_FILE_PATH=%VENDOR_SOURCE_DIR%\WinSparkle.sln"
set "ARCH=x64"
set "OUT_PATH=%ROOT_DIR%\third_party\out\winsparkle"

call :GetMSBuildPath
if %ERRORLEVEL% neq 0 (
    echo Failed to get MSBuild path
    exit /b 1
)

git submodule init
git submodule update

"%MSBUILD_PATH%" "%SLN_FILE_PATH%" /p:RestorePackagesConfig=true /t:Restore

:: Build Debug
call :Build Debug
if %ERRORLEVEL% neq 0 exit /b 1

:: Build Release
call :Build Release
if %ERRORLEVEL% neq 0 exit /b 1

exit /b 0
:: function main end

endlocal