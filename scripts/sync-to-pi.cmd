@echo off
rem Copies files changed since the last sync to the Pi, then removes files that no
rem longer exist here (deleted, moved or renamed). build/ on the Pi is left alone.
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
set "EXCLUDES=--exclude=./build --exclude=./.git --exclude=./.vscode --exclude=./.cache"

rem Stamp the start time first, so edits made during the copy go next time.
type nul > .cache\sync-start

tar -cf - %NEWER_FILTER% %EXCLUDES% . | ssh %PI_HOST% "mkdir -p %REMOTE_DIR% && tar -xvf - -C %REMOTE_DIR%"
if errorlevel 1 (
    echo Sync failed.
    exit /b 1
)

rem Mirror deletions: send the full local file list and remove anything else on the Pi.
tar -cf - %EXCLUDES% . | tar -tf - | ssh %PI_HOST% "cd %REMOTE_DIR% && tr -d '\r' | grep -v '/$' > /tmp/clanker-local-files && if [ -s /tmp/clanker-local-files ]; then find . -type f -not -path './build/*' | grep -vxFf /tmp/clanker-local-files > /tmp/clanker-stale-files; sed 's/^/removed /' /tmp/clanker-stale-files; xargs -r -d '\n' rm -f -- < /tmp/clanker-stale-files; find . -mindepth 1 -type d -empty -not -path './build*' -delete; fi; rm -f /tmp/clanker-local-files /tmp/clanker-stale-files"
if errorlevel 1 (
    echo Removing stale files failed.
    exit /b 1
)

move /y .cache\sync-start "%MARKER%" > nul
echo Sync done.
