@echo off
setlocal enabledelayedexpansion

@REM Set variables
set "ROOT_DIR=%~dp0..\..\.."

@REM Set output paths
set "BUILD_TYPE=Release"
set "LIB_OUT_PATH=%ROOT_DIR%\third_party\out\winsparkle\lib\%BUILD_TYPE%"
set "INCLUDE_OUT_PATH=%ROOT_DIR%\third_party\out\winsparkle\include"

@REM Check if output directories exist
if exist "%LIB_OUT_PATH%" if exist "%INCLUDE_OUT_PATH%" (
    echo WinSparkle path[%LIB_OUT_PATH%, %INCLUDE_OUT_PATH%] is exist, skip build
    exit /b 0
)

@REM Set source directory
set "SOURCE_DIR=%ROOT_DIR%\third_party\winsparkle"

@REM Create output directories if they don't exist
if not exist "%LIB_OUT_PATH%" mkdir "%LIB_OUT_PATH%"
if not exist "%INCLUDE_OUT_PATH%" mkdir "%INCLUDE_OUT_PATH%"

@REM Copy library files
for /d %%d in ("%SOURCE_DIR%\WinSparkle-*") do (
    if exist "%%d\x64\%BUILD_TYPE%\WinSparkle.lib" (
        copy /Y "%%d\x64\%BUILD_TYPE%\WinSparkle.lib" "%LIB_OUT_PATH%\"
    )
    if exist "%%d\x64\%BUILD_TYPE%\WinSparkle.dll" (
        copy /Y "%%d\x64\%BUILD_TYPE%\WinSparkle.dll" "%LIB_OUT_PATH%\"
    )
    if exist "%%d\x64\%BUILD_TYPE%\WinSparkle.pdb" (
        copy /Y "%%d\x64\%BUILD_TYPE%\WinSparkle.pdb" "%LIB_OUT_PATH%\"
    )
)

@REM Copy includes
for /d %%d in ("%SOURCE_DIR%\WinSparkle-*") do (
    if exist "%%d\include\*.h" (
        copy /Y "%%d\include\*.h" "%INCLUDE_OUT_PATH%\"
    )
)

exit /b 0
