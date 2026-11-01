#pragma once

// Bringing up the ESP32's network before the program runs. Include only from *.ESP32.cpp files.

namespace ClankerCleaner::Net::Network
{
    // Starts the network stack, flash storage and the link (WiFi from esp32/Secrets.h, or the
    // emulated Ethernet in the QEMU build), then waits for an address and sets the clock over
    // SNTP, which certificate checks need. Returns false, with the reason logged, when there's
    // no network configured or it didn't come up in time; the program still runs, offline.
    // A WiFi network that drops later is rejoined in the background.
    bool Start();
}
