@echo off
rem Copies the Pi's system and compiler headers into .cache\pi-sysroot for IntelliSense.
rem Usage: fetch-pi-headers.cmd <ssh-host>
setlocal
set "PI_HOST=%~1"
cd /d "%~dp0.."

if exist .cache\pi-sysroot rmdir /s /q .cache\pi-sysroot
mkdir .cache\pi-sysroot

rem -h follows symlinks, since Windows can't create the Linux ones.
ssh %PI_HOST% "tar -chf - /usr/include $(dirname $(g++ -print-libgcc-file-name))/include 2>/dev/null" | tar -xf - -C .cache/pi-sysroot 2>nul

if not exist .cache\pi-sysroot\usr\include\gpiod.hpp (
    echo Header fetch failed: gpiod.hpp missing.
    exit /b 1
)
echo Headers copied to .cache\pi-sysroot. Reload the window if IntelliSense doesn't pick them up.
