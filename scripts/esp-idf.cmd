@echo off
rem ESP-IDF for the ESP32 firmware, kept under .cache like the other fetched dependencies.
rem Usage:
rem   esp-idf.cmd install           downloads Python, ESP-IDF and the ESP32-S3 toolchain (once, about 2 GB)
rem   esp-idf.cmd ^<idf.py args^>      runs idf.py on the esp32 project, e.g. build, or -p COM5 flash monitor
rem   esp-idf.cmd --qemu ^<args^>      the same on the QEMU build (emulated Ethernet), e.g. --qemu build qemu monitor
setlocal
cd /d "%~dp0.."

set "IDF_VERSION=v5.5.6"
set "PYTHON_VERSION=3.12.10"

set "CACHE_DIR=%CD%\.cache"
set "PYTHON_DIR=%CACHE_DIR%\python"
set "IDF_PATH=%CACHE_DIR%\esp-idf"
set "IDF_TOOLS_PATH=%CACHE_DIR%\espressif"
rem Dependencies come from FetchContent, not the IDF component manager.
set "IDF_COMPONENT_MANAGER=0"
rem Ahead of the Microsoft Store's python.exe stub.
set "PATH=%PYTHON_DIR%;%PYTHON_DIR%\Scripts;%PATH%"

if /i "%~1"=="install" goto install

if not exist "%IDF_TOOLS_PATH%\python_env" (
    echo ESP-IDF isn't installed. Run: scripts\esp-idf.cmd install
    exit /b 1
)
call "%IDF_PATH%\export.bat" >nul || exit /b 1
rem Build output and the generated sdkconfig stay in build\esp32; lasting settings go in esp32\sdkconfig.defaults.
set "BUILD_DIR=build\esp32"
set "SDKCONFIG_DEFAULTS=sdkconfig.defaults"
if /i not "%~1"=="--qemu" goto idf
set "BUILD_DIR=build\esp32-qemu"
set "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.qemu"
rem The arguments after --qemu, quotes intact (shift wouldn't change %*).
call :idf %2 %3 %4 %5 %6 %7 %8 %9
exit /b %errorlevel%

:idf
rem Through python rather than the idf.py.exe launcher, which splits quoted arguments.
python "%IDF_PATH%\tools\idf.py" -C esp32 -B %BUILD_DIR% -D SDKCONFIG="%CD%\%BUILD_DIR%\sdkconfig" -D SDKCONFIG_DEFAULTS="%SDKCONFIG_DEFAULTS%" %*
exit /b %errorlevel%

:install
if not exist "%CACHE_DIR%" mkdir "%CACHE_DIR%"

rem ESP-IDF needs a full Python; the python.org build published on NuGet unpacks without an installer.
if not exist "%PYTHON_DIR%\python.exe" (
    echo Downloading Python %PYTHON_VERSION%...
    curl -L --fail -o "%CACHE_DIR%\python.nupkg" "https://www.nuget.org/api/v2/package/python/%PYTHON_VERSION%" || exit /b 1
    if exist "%CACHE_DIR%\python-package" rmdir /s /q "%CACHE_DIR%\python-package"
    mkdir "%CACHE_DIR%\python-package"
    tar -xf "%CACHE_DIR%\python.nupkg" -C "%CACHE_DIR%\python-package" || exit /b 1
    move "%CACHE_DIR%\python-package\tools" "%PYTHON_DIR%" >nul || exit /b 1
    rmdir /s /q "%CACHE_DIR%\python-package"
    del "%CACHE_DIR%\python.nupkg"
)

if not exist "%IDF_PATH%\tools\idf.py" (
    echo Downloading ESP-IDF %IDF_VERSION%...
    git clone -b %IDF_VERSION% --depth 1 --recurse-submodules --shallow-submodules https://github.com/espressif/esp-idf.git "%IDF_PATH%" || exit /b 1
)

rem Compiler, CMake, Ninja and the Python environment, into IDF_TOOLS_PATH.
call "%IDF_PATH%\install.bat" esp32s3 || exit /b 1
rem Espressif's QEMU, for running the firmware without a board (idf.py qemu).
python "%IDF_PATH%\tools\idf_tools.py" install qemu-xtensa || exit /b 1
echo ESP-IDF %IDF_VERSION% installed. Build with: scripts\esp-idf.cmd build
