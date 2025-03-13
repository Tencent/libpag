@echo off
setlocal enabledelayedexpansion

@REM Set variables
set "ROOT_DIR=%~dp0..\..\.."
set "VENDOR_SOURCE_DIR=%ROOT_DIR%\third_party\cjson"

@REM Build function
call :build "Release"
call :build "Debug"
goto :eof

:build
set "BUILD_TYPE=%~1"

set "LIB_OUT_PATH=%ROOT_DIR%\third_party\out\cjson\lib\%BUILD_TYPE%"
set "INCLUDE_OUT_PATH=%ROOT_DIR%\third_party\out\cjson\include"

@REM Check if output directories exist
if exist "%LIB_OUT_PATH%" if exist "%INCLUDE_OUT_PATH%" (
    echo cJSON path[%LIB_OUT_PATH%] is exist, skip build
    exit /b 0
)

@REM Build
if "%BUILD_TYPE%"=="Debug" (
    node "%ROOT_DIR%\tools\vendor_tools\cmake-build" cjson-static -p win -a x64 -s "%VENDOR_SOURCE_DIR%" --verbose --debug ^
        -DENABLE_CJSON_UTILS=Off -DENABLE_CJSON_TEST=Off -DBUILD_SHARED_AND_STATIC_LIBS=On
) else (
    node "%ROOT_DIR%\tools\vendor_tools\cmake-build" cjson-static -p win -s "%VENDOR_SOURCE_DIR%" --verbose ^
        -DENABLE_CJSON_UTILS=Off -DENABLE_CJSON_TEST=Off -DBUILD_SHARED_AND_STATIC_LIBS=On
)

@REM Create output directories
if not exist "%LIB_OUT_PATH%" mkdir "%LIB_OUT_PATH%"
if not exist "%INCLUDE_OUT_PATH%" mkdir "%INCLUDE_OUT_PATH%"

@REM Copy libraries
set "LIB_NAME=libcjson.lib"
copy /Y "%VENDOR_SOURCE_DIR%\out\x64\%LIB_NAME%" "%LIB_OUT_PATH%\%LIB_NAME%"

@REM Copy includes
copy /Y "%VENDOR_SOURCE_DIR%\cJSON.h" "%INCLUDE_OUT_PATH%\"

exit /b 0
