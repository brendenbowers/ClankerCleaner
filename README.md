# ClankerCleaner

Raspberry Pi controller for the MOVA P10 Ultra dock: drains the dirty-water tank, and later rinses it and refills the clean tank.
Wiring, parts and the control sequence: https://claude.ai/artifact/6QadxvJVBZY98JZLe1RgQR
Dock cloud properties and what they mean: [docs/DockProperties.md](docs/DockProperties.md)

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

## ESP32 firmware

The same sources also build as firmware for the Freenove ESP32-S3 WROOM board (N16R8), using Espressif's ESP-IDF. Unlike the Pi build, this compiles on Windows, so no board is needed to build. `esp32/` is the ESP-IDF project. It compiles `src/` with the `*.ESP32.cpp` platform files instead of the `*.RPi.cpp` ones, and `cmake/ClankerSources.cmake` holds the file list both builds share.

1. **Install ESP-IDF once:** run the task **ESP32: install ESP-IDF**, or `scripts\esp-idf.cmd install`. It downloads Python (the python.org build), ESP-IDF and the ESP32-S3 compiler into `.cache`, about 2 GB. Nothing is installed system-wide, and only Git is needed beforehand. The ESP-IDF version is pinned in `scripts/esp-idf.cmd`.
2. **Build:** run the task **ESP32: build**, or `scripts\esp-idf.cmd build`. The output goes to `build/esp32`.
3. **Flash:** connect the board's UART USB port, set `clankerCleaner.esp32Port` in `.vscode/settings.json`, then run **ESP32: flash and monitor**.

**Without a board:** run the task **ESP32: run in QEMU**, or `scripts\esp-idf.cmd --qemu build qemu --qemu-extra-args "-m 8M" monitor`. `--qemu` selects a separate build in `build/esp32-qemu` (`esp32/sdkconfig.qemu`). That build uses QEMU's emulated Ethernet, wired to your PC's network, instead of Wi-Fi, so the cloud client can be tried too. It also uses software crypto, because QEMU's accelerators can't read PSRAM. That build only runs in QEMU; flash the normal one. `-m 8M` matches the board's PSRAM: QEMU's default of 32 MB fills the address space, and then the flash partitions can't be mapped. Startup is slow, so allow a minute or two. Nothing is wired to the emulated GPIO, so every input reads LOW, which an NPN sensor reports as wet and a PNP one as dry. That's enough to check that the program starts and the flows run, not the sensor logic.

`scripts\esp-idf.cmd` passes its arguments to `idf.py`, e.g. `scripts\esp-idf.cmd size`. Chip settings (flash size, PSRAM, stack sizes) are in `esp32/sdkconfig.defaults`, and the partition table is `esp32/partitions.csv`. The build only applies those defaults to settings missing from its generated `sdkconfig`. After changing one, delete `build/esp32/sdkconfig` (or `build/esp32-qemu/sdkconfig`) so it's regenerated.

There's no config file on the ESP32. The settings, including the ESP32-S3 pin map, are compiled in from `src/Config/Config.ESP32.cpp`.

**Wi-Fi and the cloud client:** copy `esp32/Secrets.example.h` to `esp32/Secrets.h`, which git ignores, and fill in the Wi-Fi network and the MOVA/Dreame account. Without a Wi-Fi network the firmware runs offline. Without an account username the cloud client stays off. At startup the firmware joins the network, then sets the clock over SNTP, since certificate checks need it. The credentials are compiled into the firmware, so anyone who can read the board's flash can read them. HTTPS and TLS use ESP-IDF's bundle of common root certificates.

**Watching the log:** over USB, run `scripts\esp-idf.cmd -p COM3 monitor` (Ctrl+] exits). Over Wi-Fi, connect to the board's address on port 23 with `telnet` or PuTTY (connection type Telnet). The log server first sends the recent lines it still holds (16 KB), then each line as it's logged, for up to two viewers at a time. There's no password, so anyone on the network can read the log. Each sensor pin's level is logged at startup and whenever it changes, e.g. `gpio: GPIO21 now HIGH`.

**Updating over Wi-Fi:** once the board has been flashed over USB with the two-slot partition table (`esp32/partitions.csv`), set `clankerCleaner.esp32Host` in `.vscode/settings.json` to its address and run **ESP32: build and update over the network**, or build and then run `scripts\ota-update.cmd <address>`. The board writes the new firmware into its spare app slot, switching the relays off and holding them off while it does, then restarts into it. A new update confirms itself after running for a minute with a network address. If it resets before then, or doesn't get an address within about five minutes, the bootloader goes back to the previous firmware. Updates need `OtaToken` from `esp32/Secrets.h`; without one, they're off. `http://<address>/version` shows the running version and slot.

**MQTT and the dashboard:** with `MqttHost` set in `esp32/Secrets.h` (or `[mqtt]` in the Pi's config), the controller publishes its sensors, relays, run times, flow steps and the dock's status to that broker over TLS. The topics are listed in `src/Telemetry/Track.h`. It also takes commands on `commands/<name>` under the same prefix, with any non-empty payload: `drain` (the full drain and rinse cycle), `rinse`, `fill`, `stop` and `resume`. `src/Flows/Commands` hands each one to the tank flow's Idle state, so the same states run with the same checks, and a tank that's already busy refuses it. Only `stop` interrupts anything: it turns the outputs off and holds the tank flows until `resume` or a restart. Each command gets an answer on `command_result`. Retained commands are ignored, and commands sent while the board is offline are lost. Code reports events with `TELEMETRY_TRACK(Event, args...)`; configure with `-DCLANKER_MQTT=OFF` (`scripts\esp-idf.cmd -DCLANKER_MQTT=OFF build` on the ESP32) to leave the publishing and commands out, which turns those calls into no-ops. `dashboard/` is an installable web app that shows them. It has no server of its own: GitHub Pages hosts its files (`.github/workflows/dashboard.yml`), and it connects straight to the broker over MQTT on secure WebSockets (port 8884). The broker's certificate comes from its own CA, so each device has to accept it once: the app links to the broker's address for that when it can't connect. Open it in a browser and use **Install** (or **Add to Home Screen**) to keep it as an app. The broker's address isn't in the source: enter it at the first login (`wss://<host>:8884/mqtt`). The address and username are then saved in that browser, and the password too if **Remember on this device** is ticked. Tick **This login can send commands** to show the controls; that needs a login the broker lets publish to the commands topics. To try it locally, run `python -m http.server 5173 --directory dashboard` and open `http://localhost:5173`.

## libgpiod

The project targets Raspberry Pi OS **Trixie**, which ships libgpiod 2.x, and uses the v2 C++ API (`gpiod::chip`, `prepare_request()`, `line_settings`). CMake requires `libgpiodcxx >= 2.0` and prints the version it found. Bookworm's libgpiod 1.6 has a different API and won't build.

C++ API reference: https://libgpiod.readthedocs.io/

## Pinout

Pins are BCM GPIO line numbers, not physical header positions. Every one can be changed under `[gpio]` in the config file (see `config/clanker-cleaner.example.toml`); the defaults live in `src/GPIO/Constants.h`.

| Function | Direction | BCM GPIO | Header pin | Config key |
|---|---|---|---|---|
| Dirty tank high sensor | Input (falling edge = wet) | 22 | 15 | `dirty_tank_sensor_pin` |
| Clean tank high sensor | Input | 26 | 37 | `clean_tank_high_sensor_pin` |
| Clean tank low sensor | Input | 23 | 16 | `clean_tank_low_sensor_pin` |
| Pump relay (IN1) | Output | 17 | 11 | `pump_relay_pin` |
| Dirty tank rinse relay (IN2) | Output | 27 | 13 | `dirty_tank_rinse_relay_pin` |
| Clean tank fill relay (IN3) | Output | 5 | 29 | `clean_tank_fill_relay_pin` |
| Cleaner dosing relay (IN4) | Output | 6 | 31 | `cleaner_dosing_relay_pin` |

Set `relay_active_low = true` if the relay board's trigger jumper is on L. Two settings can't share a pin; the config loader rejects duplicates.

## Layout

```
CMakeLists.txt        Pi build definition (libgpiod via pkg-config)
CMakePresets.json     pi-debug / pi-release presets used by the tasks
cmake/                source list shared by the Pi and ESP32 builds
esp32/                ESP-IDF project for the ESP32-S3 firmware
src/main.cpp          the program (Run); EntryPoint.<Platform>.cpp starts it
scripts/esp-idf.cmd   installs ESP-IDF into .cache and runs idf.py
scripts/setup-pi.sh   dependency install, run on the Pi
scripts/sync-to-pi.cmd  copies changed files to the Pi
scripts/fetch-pi-headers.cmd  copies the Pi's headers for IntelliSense
.vscode/              tasks, launch, settings
```
