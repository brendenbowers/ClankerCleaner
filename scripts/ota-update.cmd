@echo off
rem Sends the last ESP32 build to the board over the network (Net/OtaUpdate.ESP32.cpp), which
rem writes it into its spare app slot and restarts into it. Build first: esp-idf.cmd build.
rem Usage: ota-update.cmd <board-address>
rem The token is OtaToken from esp32\Secrets.h. curl.exe comes with Windows 10/11.
setlocal
cd /d "%~dp0.."
set "BOARD=%~1"
set "IMAGE=build\esp32\clanker_cleaner.bin"

if "%BOARD%"=="" (
    echo Usage: scripts\ota-update.cmd ^<board-address^>
    exit /b 1
)
if not exist "%IMAGE%" (
    echo %IMAGE% is missing. Build first: scripts\esp-idf.cmd build
    exit /b 1
)

set "TOKEN="
rem The text between the quotes on the OtaToken line.
for /f tokens^=2^ delims^=^" %%t in ('findstr /c:"OtaToken =" esp32\Secrets.h') do set "TOKEN=%%t"
if "%TOKEN%"=="" (
    echo No OtaToken in esp32\Secrets.h; see esp32\Secrets.example.h.
    exit /b 1
)

echo Sending %IMAGE% to %BOARD%...
curl.exe --fail-with-body --silent --show-error --max-time 180 -H "Authorization: Bearer %TOKEN%" -H "Content-Type: application/octet-stream" --data-binary "@%IMAGE%" "http://%BOARD%/update" || exit /b 1
echo The board restarts into the new firmware; it rolls back if that can't rejoin the network.
