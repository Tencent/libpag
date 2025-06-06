
set ROOTDIR=%~dp0\..\..\

set BASEDIR=%~dp2
echo BASEDIR:%BASEDIR%
echo ROOTDIR:%ROOTDIR%
echo %1
if "%1" == "" (
  set deployqtDir=C:/Qt/6.8.1/msvc2022_64/bin/windeployqt.exe
) else (
  set deployqtDir=%1
)
echo deployqtDir:%deployqtDir%

cd %BASEDIR%
if exist QTDll rmdir /S /Q QTDll
mkdir QTDll
%deployqtDir% PAGExporter.aex --release --qmldir %ROOTDIR%\assets\qml --dir .\QTDll


:: pause
cd %~dp0
ECHO "success: QtInstall complete!"
exit /B 0

:err
cd %~dp0
ECHO "error: QtInstall failed!"
exit /B 1