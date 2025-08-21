@echo on

set argC=0
for %%i in (%*) do set /A argC+=1

if not "%argC%"=="3" (
  echo Usage: %0 update_file private_key
  exit /b 1
)

set Base64Code="%~1" dgst -sha1 -binary < "%~2" | "%~1" dgst -sha1 -sign "%~3" | "%~1" enc -base64
echo %Base64Code%
echo %Base64Code% > base64code.txt
