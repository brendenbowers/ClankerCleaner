@echo off
rem Copies files changed since the last sync to the Pi.
rem Usage: sync-to-pi.cmd <ssh-host> <remote-dir> [full]
setlocal
set "PI_HOST=%~1"
set "REMOTE_DIR=%~2"
cd /d "%~dp0.."
if not exist .cache mkdir .cache

set "MARKER=.cache\last-sync"
if /i "%~3"=="full" if exist "%MARKER%" del "%MARKER%"

set "NEWER_FILTER="
if exist "%MARKER%" set "NEWER_FILTER=--newer-mtime-than %MARKER%"

rem Stamp the start time first, so edits made during the copy go next time.
type nul > .cache\sync-start

tar -cf - %NEWER_FILTER% --exclude=./build --exclude=./.git --exclude=./.vscode --exclude=./.cache . | ssh %PI_HOST% "mkdir -p %REMOTE_DIR% && tar -xvf - -C %REMOTE_DIR%"
if errorlevel 1 (
    echo Sync failed.
    exit /b 1
)

move /y .cache\sync-start "%MARKER%" > nul
echo Sync done.
