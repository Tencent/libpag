@echo off

set argC=0
for %%i in (%*) do set /A argC+=1

if not "%argC%"=="3" (
  echo Usage: %0 update_file private_key
  exit /b 1
)

"%~1" dgst -sha1 -binary < "%~2" | "%~1" dgst -sha1 -sign "%~3" | "%~1" enc -base64 > base64code.txt
type base64code.txt
