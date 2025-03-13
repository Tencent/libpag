@echo off
setlocal enabledelayedexpansion

@REM Set variables
set "ROOT_DIR=%~dp0..\..\.."
set "VENDOR_SOURCE_DIR=%ROOT_DIR%\.."
set "QT_CONFIG_PATH=%ROOT_DIR%\QTCMAKE.cfg"

@REM Get QT config
if not exist "%QT_CONFIG_PATH%" (
    echo Failed to find QTCMAKE.cfg
    exit /b 1
)

@REM Read CMAKE_PREFIX_PATH from QTCMAKE.cfg
for /f "tokens=2 delims=( " %%a in ('findstr "CMAKE_PREFIX_PATH" "%QT_CONFIG_PATH%"') do (
    set "CMAKE_PREFIX_PATH=%%a"
)

if "%CMAKE_PREFIX_PATH%"=="" (
    echo Failed to get cmake prefix path
    exit /b 1
)

@REM Build function
call :build "Release"
call :build "Debug"
goto :eof

:build
set "BUILD_TYPE=%~1"

set "LIB_OUT_PATH=%ROOT_DIR%\third_party\out\libpag\lib\%BUILD_TYPE%"
set "INCLUDE_OUT_PATH=%ROOT_DIR%\third_party\out\libpag\include"

@REM Check if output directories exist
if exist "%LIB_OUT_PATH%" if exist "%INCLUDE_OUT_PATH%" (
    echo Libpag path[%LIB_OUT_PATH%] is exist, skip build
    exit /b 0
)

@REM Build
if "%BUILD_TYPE%"=="Debug" (
    node "%ROOT_DIR%\tools\vendor_tools\cmake-build" tgfx-vendor pag-vendor pag -p win -a x64 -s "%VENDOR_SOURCE_DIR%" --verbose --debug ^
        -DPAG_BUILD_SHARED=OFF -DPAG_USE_ENCRYPT_ENCODE=ON -DPAG_BUILD_FRAMEWORK=OFF -DPAG_USE_MOVIE=ON -DPAG_USE_QT=ON ^
        -DPAG_USE_RTTR=ON -DPAG_USE_LIBAVC=OFF -DPAG_BUILD_TESTS=OFF -DCMAKE_PREFIX_PATH="%CMAKE_PREFIX_PATH%"
) else (
    node "%ROOT_DIR%\tools\vendor_tools\cmake-build" tgfx-vendor pag-vendor pag -p win -a x64 -s "%VENDOR_SOURCE_DIR%" --verbose ^
        -DPAG_BUILD_SHARED=OFF -DPAG_USE_ENCRYPT_ENCODE=ON -DPAG_BUILD_FRAMEWORK=OFF -DPAG_USE_MOVIE=ON -DPAG_USE_QT=ON ^
        -DPAG_USE_RTTR=ON -DPAG_USE_LIBAVC=OFF -DPAG_BUILD_TESTS=OFF -DCMAKE_PREFIX_PATH="%CMAKE_PREFIX_PATH%"
)

@REM Create output directories
if not exist "%LIB_OUT_PATH%" mkdir "%LIB_OUT_PATH%"
if not exist "%INCLUDE_OUT_PATH%" mkdir "%INCLUDE_OUT_PATH%"

@REM  Copy libraries
set "LIBS=libpag.lib libtgfx-vendor.lib libpag-vendor.lib"
for %%L in (%LIBS%) do (
    if "%%L"=="libtgfx-vendor.lib" (
        copy /Y "%VENDOR_SOURCE_DIR%\out\x64\x64\tgfx\CMakeFiles\tgfx-vendor.dir\x64\%%L" "%LIB_OUT_PATH%\%%L"
    ) else (
        copy /Y "%VENDOR_SOURCE_DIR%\out\x64\%%L" "%LIB_OUT_PATH%\%%L"
    )
)

@REM Copy includes
xcopy /E /I /Y "%VENDOR_SOURCE_DIR%\include" "%INCLUDE_OUT_PATH%"

exit /b 0
