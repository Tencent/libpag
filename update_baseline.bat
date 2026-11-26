@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Refresh the selected backend's local baseline cache on Windows.
rem
rem Usage:
rem   update_baseline.bat [USE_OPENGL^|USE_OPENGL_SWIFTSHADER^|USE_D3D12^|USE_D3D12_WARP^|USE_VULKAN]
rem                       [--skip-images] [--no-checkout] [--no-fetch] [--clang-cl]
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
rem
rem --clang-cl builds with clang-cl instead of MSVC so private-access test cases are compiled in.
rem Both scripts build Release by default. The cache is keyed by backend only, not by compiler or
rem build type, so refresh it with the same compiler and build type you pass to autotest.bat. A
rem clang-cl refresh always rebuilds, because an existing cache cannot prove that the
rem private-access cases are present.

cd /d "%~dp0"
set "WORKSPACE=%CD%"
set "RECOVERY_FILE=%WORKSPACE%\build-update-baseline-recovery.txt"
set "RECOVERY_OWNED=0"
set "STATUS=0"
set "RESTORE_NEEDED=0"
set "STASH_CREATED=0"
set "BACKEND="
set "SKIP_IMAGES=0"
set "NO_CHECKOUT=0"
set "NO_FETCH=0"
set "USE_CLANG_CL=0"

:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="--skip-images" goto :arg_skip_images
if /i "%~1"=="--no-checkout" goto :arg_no_checkout
if /i "%~1"=="--no-fetch" goto :arg_no_fetch
if /i "%~1"=="--clang-cl" goto :arg_clang_cl
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

:arg_no_fetch
set "NO_FETCH=1"
shift
goto :parse_args

:arg_clang_cl
set "USE_CLANG_CL=1"
shift
goto :parse_args

:too_many_backends
echo Error: specify only one backend. 1>&2
exit /b 1

:args_done
if not defined BACKEND set "BACKEND=USE_OPENGL"
if exist "%RECOVERY_FILE%" (
  echo Error: recovery details already exist at "!RECOVERY_FILE!". 1>&2
  echo Restore and verify the previous workspace before removing that file and running again. 1>&2
  exit /b 1
)

call "win\tools\backend-args.bat" "%BACKEND%"
if not "%PAG_BACKEND_SUPPORTED%"=="1" exit /b 1
set "CMAKE_BACKEND_ARGS=%PAG_CMAKE_ARGS%"
set "BACKEND_NAME=%PAG_BACKEND_NAME%"
set "TARGET_SUFFIX=%PAG_TARGET_SUFFIX%"

:backend_ready
set "CACHE_DIR=%WORKSPACE%\test\baseline\.cache\%BACKEND_NAME%"
set "BUILD_DIR=%WORKSPACE%\build-update-baseline"

rem Configure MSVC, CMake, Ninja and the Windows SDK in this cmd process.
call "win\tools\vs-env.bat"
if errorlevel 1 (
  call :Fail "Failed to configure the Visual Studio C++ toolchain, CMake, Ninja or the Windows SDK."
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

set "CMAKE_COMPILER_ARGS="
set "COMPILER_LABEL=MSVC"
if "%USE_CLANG_CL%"=="1" (
  set "COMPILER_LABEL=clang-cl"
  if defined VS_DIR if exist "%VS_DIR%\VC\Tools\Llvm\x64\bin" set "PATH=%VS_DIR%\VC\Tools\Llvm\x64\bin;%PATH%"
  where clang-cl >nul 2>nul
  if errorlevel 1 (
    call :Fail "clang-cl was not found. Install the C++ Clang tools for Windows component."
    goto :cleanup
  )
  python -c "import sys; sys.exit(0 if sys.version_info[0] == 3 else 1)" >nul 2>nul
  if not errorlevel 1 (
    set "CMAKE_COMPILER_ARGS=-DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_C_COMPILER_LAUNCHER="python;%WORKSPACE%\win\tools\filter_clang.py" -DCMAKE_CXX_COMPILER_LAUNCHER="python;%WORKSPACE%\win\tools\filter_clang.py""
  ) else (
    py -3 -c "import sys; sys.exit(0 if sys.version_info[0] == 3 else 1)" >nul 2>nul
    if errorlevel 1 (
      call :Fail "Python 3 through python or py -3 is required by the clang-cl compiler launcher."
      goto :cleanup
    )
    set "CMAKE_COMPILER_ARGS=-DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_C_COMPILER_LAUNCHER="py;-3;%WORKSPACE%\win\tools\filter_clang.py" -DCMAKE_CXX_COMPILER_LAUNCHER="py;-3;%WORKSPACE%\win\tools\filter_clang.py""
  )
)

if "%NO_CHECKOUT%"=="1" goto :skip_checkout

if "%NO_FETCH%"=="1" goto :skip_fetch
git fetch origin main
if errorlevel 1 (
  call :Fail "git fetch origin main failed. Use --no-fetch to use the locally known origin/main."
  goto :cleanup
)
goto :fetch_done

:skip_fetch
echo Warning: --no-fetch uses the locally known origin/main, which may be stale.

:fetch_done
set "MAIN_COMMIT="
for /f "delims=" %%i in ('git rev-parse --verify "origin/main^{commit}" 2^>nul') do set "MAIN_COMMIT=%%i"
if not defined MAIN_COMMIT (
  call :Fail "origin/main was not found."
  goto :cleanup
)
git diff --quiet -- update_baseline.bat
if errorlevel 1 (
  call :Fail "update_baseline.bat has uncommitted changes, so stashing it would corrupt this run. Commit the script or use --no-checkout."
  goto :cleanup
)
git diff --cached --quiet -- update_baseline.bat
if errorlevel 1 (
  call :Fail "update_baseline.bat has staged changes, so stashing it would corrupt this run. Commit the script or use --no-checkout."
  goto :cleanup
)
git diff --quiet HEAD %MAIN_COMMIT% -- update_baseline.bat
if errorlevel 1 (
  call :Fail "origin/main has a different update_baseline.bat, so switching would corrupt this run. Use --no-checkout."
  goto :cleanup
)
git show %MAIN_COMMIT%:test/baseline/version.json > "%TEMP%\pag_main_version.json" 2>nul
if errorlevel 1 (
  del "%TEMP%\pag_main_version.json" >nul 2>nul
  call :Fail "Cannot read test\baseline\version.json from origin/main."
  goto :cleanup
)
rem Both files must exist: a failed refresh deletes md5.json before regenerating it, so a lone
rem version.json can belong to a broken cache. This mirrors update_baseline.sh's `-f` checks.
rem A clang-cl refresh must not reuse the cache: the md5 file does not record which compiler built
rem it, and only a clang-cl build proves the private-access cases ran.
if "%USE_CLANG_CL%"=="1" goto :skip_cache_check
if exist "%CACHE_DIR%\version.json" if exist "%CACHE_DIR%\md5.json" (
  git diff --no-index --quiet --ignore-cr-at-eol -- "%TEMP%\pag_main_version.json" "%CACHE_DIR%\version.json" >nul 2>nul
  if not errorlevel 1 (
    del "%TEMP%\pag_main_version.json" >nul 2>nul
    echo Baseline cache for '%BACKEND_NAME%' is up to date.
    exit /b 0
  )
)
:skip_cache_check
del "%TEMP%\pag_main_version.json" >nul 2>nul

set "CURRENT_BRANCH="
for /f "delims=" %%i in ('git rev-parse --abbrev-ref HEAD') do set "CURRENT_BRANCH=%%i"
set "CURRENT_COMMIT="
for /f "delims=" %%i in ('git rev-parse HEAD') do set "CURRENT_COMMIT=%%i"
if not defined CURRENT_BRANCH (
  call :Fail "Cannot identify the original branch."
  goto :cleanup
)
if not defined CURRENT_COMMIT (
  call :Fail "Cannot identify the original commit."
  goto :cleanup
)
set "STASH_BEFORE="
for /f "delims=" %%i in ('git rev-parse --verify -q refs/stash 2^>nul') do set "STASH_BEFORE=%%i"
git stash push --include-untracked --quiet
if errorlevel 1 (
  call :Fail "git stash push failed."
  goto :cleanup
)
set "STASH_AFTER="
for /f "delims=" %%i in ('git rev-parse --verify -q refs/stash 2^>nul') do set "STASH_AFTER=%%i"
if defined STASH_AFTER if not "%STASH_BEFORE%"=="%STASH_AFTER%" set "STASH_CREATED=1"
set "RESTORE_NEEDED=1"
set "RECOVERY_SWITCH=git switch "!CURRENT_BRANCH!""
if /i "%CURRENT_BRANCH%"=="HEAD" set "RECOVERY_SWITCH=git switch --detach "!CURRENT_COMMIT!""
call :WriteRecoveryDetails
if errorlevel 1 (
  call :Fail "Cannot write complete recovery details. Stopping before switching branches and attempting to restore the original workspace."
  echo Original branch: !CURRENT_BRANCH! 1>&2
  echo Original commit: !CURRENT_COMMIT! 1>&2
  if "!STASH_CREATED!"=="1" echo Stash created by this run: !STASH_AFTER! 1>&2
  goto :cleanup
)
echo Recovery details written to "%RECOVERY_FILE%".
git switch --detach %MAIN_COMMIT% --quiet
if errorlevel 1 (
  call :Fail "git switch --detach failed."
  goto :cleanup
)

:skip_checkout
where depsync >nul 2>nul
if not errorlevel 1 goto :depsync_ready
echo Installing depsync...
call npm install -g depsync@1.4.6
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
echo Building UpdateBaseline_%TARGET_SUFFIX% for '%BACKEND_NAME%' with %COMPILER_LABEL%...
cmake -G Ninja -S "%WORKSPACE%" -B "%BUILD_DIR%" %CMAKE_BACKEND_ARGS% %CMAKE_COMPILER_ARGS% %CMAKE_IMAGE_ARGS% -DPAG_BUILD_TESTS=ON -DPAG_SKIP_BASELINE_CHECK=ON -DCMAKE_BUILD_TYPE=Release
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

cmake --build "%BUILD_DIR%" --target %BASELINE_TARGET% --parallel
if errorlevel 1 (
  call :Fail "Building %BASELINE_TARGET% failed."
  goto :cleanup
)
if exist "%GENERATED_CACHE_DIR%\md5.json" del /q "%GENERATED_CACHE_DIR%\md5.json"
if not exist "%BUILD_DIR%\%BASELINE_TARGET%.exe" (
  call :Fail "%BASELINE_TARGET%.exe was not produced."
  goto :cleanup
)
rem A stale baseline-out left by an aborted run must not be renamed in as this run's output.
if exist "%WORKSPACE%\test\baseline-out" rmdir /s /q "%WORKSPACE%\test\baseline-out"
if exist "%WORKSPACE%\test\baseline-out" (
  call :Fail "Cannot clear test\baseline-out. Close any program holding its files before retrying."
  goto :cleanup
)
"%BUILD_DIR%\%BASELINE_TARGET%.exe"
set "BASELINE_EXIT=%ERRORLEVEL%"
rem A crashed test binary returns a negative exit code, which `if errorlevel 1` misses because cmd
rem compares it as a signed value. Compare the captured code so abnormal termination is reported
rem as a failure instead of a missing md5.json.
if not "%BASELINE_EXIT%"=="0" (
  if exist "%WORKSPACE%\test\out" (
    if not exist "%WORKSPACE%\result" mkdir "%WORKSPACE%\result"
    xcopy "%WORKSPACE%\test\out" "%WORKSPACE%\result\out\" /E /I /Y /Q >nul
  )
  call :Fail "Baseline cache refresh for '%BACKEND_NAME%' failed (exit code %BASELINE_EXIT%)."
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
echo Usage: update_baseline.bat [USE_OPENGL^|USE_OPENGL_SWIFTSHADER^|USE_D3D12^|USE_D3D12_WARP^|USE_VULKAN] [--skip-images] [--no-checkout] [--no-fetch] [--clang-cl]
echo.
echo   USE_OPENGL             OpenGL (default backend).
echo   USE_OPENGL_SWIFTSHADER OpenGL on SwiftShader.
echo   USE_D3D12              D3D12 with hardware adapters, cache in .cache\d3d12.
echo   USE_D3D12_WARP         D3D12 forced onto WARP, cache in .cache\d3d12-warp.
echo   USE_VULKAN             Reserved for the Vulkan backend.
echo.
echo   --skip-images          Only refresh md5.json; do not generate _base.webp images.
echo   --no-checkout          Build the current working tree instead of origin/main.
echo   --no-fetch             Skip git fetch and use the locally known origin/main.
echo   --clang-cl             Build with clang-cl so private-access tests are compiled in.
echo.
echo By default the script stashes local changes and checks out origin/main, then restores the
echo original branch and changes. Any stash created by this run is applied by exact OID and retained.
echo Existing recovery details block a new run. Interruptions require manual recovery using that file.
echo Run one backend at a time; test\out and version.json are shared.
echo Both scripts build Release by default. The md5 cache is keyed by backend, not by compiler or
echo build type, so refresh it with the compiler and build type you intend to test with. A clang-cl
echo refresh always rebuilds instead of reusing an existing cache.
exit /b 0

:Fail
echo Error: %~1 1>&2
set "STATUS=1"
exit /b 0

:WriteRecoveryDetails
if exist "%RECOVERY_FILE%" (
  echo Error: recovery details appeared at "!RECOVERY_FILE!". Keeping that file unchanged. 1>&2
  exit /b 1
)
set "RECOVERY_OWNED=1"
set "RECOVERY_LINE=Workspace recovery details for update_baseline.bat."
call :ClearErrorLevel
> "%RECOVERY_FILE%" echo(!RECOVERY_LINE!
call :VerifyRecoveryLine
if errorlevel 1 exit /b 1
set "RECOVERY_LINE=Original branch: !CURRENT_BRANCH!"
call :WriteRecoveryLine
if errorlevel 1 exit /b 1
set "RECOVERY_LINE=Original commit: !CURRENT_COMMIT!"
call :WriteRecoveryLine
if errorlevel 1 exit /b 1
set "RECOVERY_LINE=Stash created: !STASH_CREATED!"
call :WriteRecoveryLine
if errorlevel 1 exit /b 1
if "%STASH_CREATED%"=="1" (
  set "RECOVERY_LINE=Stash created by this run: !STASH_AFTER!"
  call :WriteRecoveryLine
  if errorlevel 1 exit /b 1
)
set "RECOVERY_LINE=Recover with:"
call :WriteRecoveryLine
if errorlevel 1 exit /b 1
set "RECOVERY_LINE=  !RECOVERY_SWITCH!"
call :WriteRecoveryLine
if errorlevel 1 exit /b 1
set "RECOVERY_LINE=Verify HEAD matches the original branch and commit after switching."
call :WriteRecoveryLine
if errorlevel 1 exit /b 1
if not "%STASH_CREATED%"=="1" (
  set "RECOVERY_LINE=No stash was created by this run. Existing stash entries must be left unchanged."
  call :WriteRecoveryLine
  exit /b !ERRORLEVEL!
)
set "RECOVERY_LINE=Verify these changes have not already been restored before applying the stash."
call :WriteRecoveryLine
if errorlevel 1 exit /b 1
set "RECOVERY_LINE=  git stash apply --index "!STASH_AFTER!""
call :WriteRecoveryLine
if errorlevel 1 exit /b 1
set "RECOVERY_LINE=The applied stash is retained. Remove it manually only after verifying its exact identity and the restored workspace."
call :WriteRecoveryLine
exit /b %ERRORLEVEL%

:WriteRecoveryLine
if not exist "%RECOVERY_FILE%" exit /b 1
call :ClearErrorLevel
>> "%RECOVERY_FILE%" echo(!RECOVERY_LINE!
call :VerifyRecoveryLine
exit /b %ERRORLEVEL%

:VerifyRecoveryLine
rem Redirection failures do not reliably change ERRORLEVEL for echo. Verify the complete last line.
if not exist "%RECOVERY_FILE%" exit /b 1
set "WRITTEN_RECOVERY_LINE="
for /f "usebackq delims=" %%L in ("%RECOVERY_FILE%") do set "WRITTEN_RECOVERY_LINE=%%L"
if not "!WRITTEN_RECOVERY_LINE!"=="!RECOVERY_LINE!" exit /b 1
exit /b 0

:ClearErrorLevel
exit /b 0

:RestoreWorkspace
set "RESTORE_NEEDED=0"
cd /d "%WORKSPACE%"
if errorlevel 1 exit /b 1
rem A post-checkout hook may fail after HEAD has already changed. Restore unconditionally before
rem applying the stash, and never retry automatically after a failed restoration.
if /i "%CURRENT_BRANCH%"=="HEAD" (
  git switch --detach "%CURRENT_COMMIT%" --quiet
) else (
  git switch "%CURRENT_BRANCH%" --quiet
)
if errorlevel 1 exit /b 1
set "RESTORED_BRANCH="
for /f "delims=" %%i in ('git rev-parse --abbrev-ref HEAD 2^>nul') do set "RESTORED_BRANCH=%%i"
set "RESTORED_COMMIT="
for /f "delims=" %%i in ('git rev-parse HEAD 2^>nul') do set "RESTORED_COMMIT=%%i"
if not "%RESTORED_BRANCH%"=="%CURRENT_BRANCH%" exit /b 1
if not "%RESTORED_COMMIT%"=="%CURRENT_COMMIT%" exit /b 1
if "%STASH_CREATED%"=="1" (
  git stash apply --index --quiet "!STASH_AFTER!"
  if errorlevel 1 exit /b 1
  set "STASH_CREATED=0"
  rem Checking refs/stash before dropping it is not atomic across terminals. Keep the exact entry.
  echo Warning: restored changes from stash !STASH_AFTER!; that stash was retained. 1>&2
  echo Remove it manually only after verifying its exact identity and the restored workspace. 1>&2
)
if "%RECOVERY_OWNED%"=="1" (
  del "!RECOVERY_FILE!" >nul 2>nul
  if exist "!RECOVERY_FILE!" (
    echo Error: workspace restored but recovery details could not be removed at "!RECOVERY_FILE!". 1>&2
    echo Verify the restored workspace before removing that file. Do not apply the stash again. 1>&2
    exit /b 1
  )
  set "RECOVERY_OWNED=0"
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
if "!STATUS!"=="0" (
  if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
)
exit /b !STATUS!
