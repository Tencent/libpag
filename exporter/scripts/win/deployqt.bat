
set ROOTDIR=%~dp0\..\..\

set BASEDIR=%~dp2
if "%3"=="" (
    set BUILDTYPE=release
) else (
    set BUILDTYPE=%3
)
echo BASEDIR:%BASEDIR%
echo ROOTDIR:%ROOTDIR%
echo BUILDTYPE:%BUILDTYPE%
echo %1
if "%1" == "" (
  echo "deployqtDir is null"
  exit 1
) else (
  set deployqtDir=%1
)
echo deployqtDir:%deployqtDir%

cd %BASEDIR%
if exist QTDll rmdir /S /Q QTDll
mkdir QTDll
%deployqtDir% PAGExporter.aex  --%BUILDTYPE% --qmldir %ROOTDIR%\assets\qml --dir .\QTDll


:: pause
cd %~dp0
ECHO "success: QtInstall complete!"
exit /B 0

:err
cd %~dp0
ECHO "error: QtInstall failed!"
exit /B 1
