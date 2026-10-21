# ClankerCleaner

Raspberry Pi controller for the MOVA P10 Ultra dock: drains the dirty-water tank, and later rinses it and refills the clean tank.
Wiring, parts and the control sequence: https://claude.ai/artifact/6QadxvJVBZY98JZLe1RgQR

You edit on Windows in VS Code. Each build copies only the files changed since the last sync to the Pi (`scripts/sync-to-pi.cmd`, tracked by `.cache/last-sync`), then compiles there with CMake. Running and debugging happen on the Pi; the debugger maps back to your local files.

Deleting or renaming a file isn't copied across. Run **Pi: full resync** afterwards: it clears the Pi's copy of the sources (keeping `build/`) and sends everything again.

## Requirements

- Windows with VS Code and the recommended extensions (C/C++, CMake Tools, Remote - SSH).
- Windows OpenSSH (`ssh`, `scp`) and `tar`, both built into Windows 10/11.
- A Raspberry Pi running Raspberry Pi OS Trixie with SSH enabled.

## First run

1. **Set your Pi** in `.vscode/settings.json`:
   - `clankerCleaner.piHost`: `user@hostname`, e.g. `pi@raspberrypi.local`
   - `clankerCleaner.remoteDir`: an absolute path in that user's home, e.g. `/home/pi/ClankerCleaner`
2. **Set up key-based SSH** so tasks and the debugger don't prompt for a password:
   ```powershell
   ssh-keygen -t ed25519
   type $env:USERPROFILE\.ssh\id_ed25519.pub | ssh pi@raspberrypi.local "mkdir -p ~/.ssh && cat >> ~/.ssh/authorized_keys"
   ```
3. **Install dependencies on the Pi:** run the task **Pi: install dependencies**. It installs the compiler, CMake, gdb and libgpiod, and adds your user to the `gpio` group. Log out and back in on the Pi (or reboot) afterwards.
4. **IntelliSense on Windows:** run **Pi: fetch headers for IntelliSense** once. It copies the Pi's `/usr/include` and g++ headers (about 23 MB) into `.cache/pi-sysroot`, and `.vscode/c_cpp_properties.json` points IntelliSense at them in the Pi compiler's search order. Rerun it after installing new `-dev` packages on the Pi. If the Pi's g++ moves past version 14, update the `14` in those paths.

## Everyday use

| Action | How |
|---|---|
| Build | `Ctrl+Shift+B` (**Pi: build (debug)**). Syncs sources first. |
| Run | Task **Pi: run (debug)**. `Ctrl+C` in the terminal stops it. |
| Debug | `F5` (**Pi: debug clanker_cleaner**). gdb runs on the Pi over SSH. |
| Release build | Task **Pi: build (release)** |
| Clean | Task **Pi: clean** (removes `build/` on the Pi) |
| Full resync | Task **Pi: full resync**, after deleting or renaming files |

## libgpiod

The project targets Raspberry Pi OS **Trixie**, which ships libgpiod 2.x, and uses the v2 C++ API (`gpiod::chip`, `prepare_request()`, `line_settings`). CMake requires `libgpiodcxx >= 2.0` and prints the version it found. Bookworm's libgpiod 1.6 has a different API and won't build.

C++ API reference: https://libgpiod.readthedocs.io/

## Layout

```
CMakeLists.txt        build definition (libgpiod via pkg-config)
CMakePresets.json     pi-debug / pi-release presets used by the tasks
src/main.cpp          entry point (stub)
scripts/setup-pi.sh   dependency install, run on the Pi
scripts/sync-to-pi.cmd  copies changed files to the Pi
scripts/fetch-pi-headers.cmd  copies the Pi's headers for IntelliSense
.vscode/              tasks, launch, settings
```
