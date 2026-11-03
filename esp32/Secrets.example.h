#pragma once

// Network and cloud credentials for the ESP32 firmware. Copy this file to esp32/Secrets.h
// (git ignores it) and fill it in; the build uses this example only when Secrets.h is missing.
// They're compiled into the firmware, so anyone who can read the board's flash can read them.

namespace ClankerCleaner::Secrets
{
    // Empty runs the firmware offline. The QEMU build ignores these and uses its emulated Ethernet.
    inline constexpr const char* WifiSsid = "";
    // Empty for an open network.
    inline constexpr const char* WifiPassword = "";

    // An empty username leaves the cloud client off.
    // "mova" for a MovaHome account, "dreame" for DreameHome.
    inline constexpr const char* DreameAccount = "mova";
    inline constexpr const char* DreameUsername = "";
    inline constexpr const char* DreamePassword = "";
    // Server region as chosen in the app, e.g. "us", "eu", "cn".
    inline constexpr const char* DreameCountry = "us";
    // Device ID or name; empty uses the first robot vacuum on the account.
    inline constexpr const char* DreameDevice = "";

    // Password for over-the-air updates (scripts\ota-update.cmd sends it); empty turns them
    // off. Any long random string, e.g. from: powershell -c "[guid]::NewGuid().ToString('N')"
    inline constexpr const char* OtaToken = "";
}
