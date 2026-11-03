#pragma once

// Serves the captured log (Logging/LogCapture.ESP32.h) on TCP port 23, so `telnet <board>` or
// PuTTY shows it live: first the backlog still held, then each line as it's logged. Anyone on
// the network can connect; input is ignored.

namespace ClankerCleaner::Net::TelnetLog
{
    // Call after Network::Start; clients can connect whenever the board has an address.
    void Start();
}
