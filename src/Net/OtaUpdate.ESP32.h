#pragma once

// Over-the-air firmware updates. An HTTP server on port 80 takes a new firmware image
// (build/esp32/clanker_cleaner.bin) as the body of POST /update, with the header
// "Authorization: Bearer <Secrets::OtaToken>", writes it into the spare app slot and restarts
// into it; scripts\ota-update.cmd sends it. GET /version reports what's running.
//
// The new firmware boots unconfirmed. It confirms itself once it has run for a minute with a
// network address; one that can't get an address within about five minutes restarts, and the
// bootloader goes back to the previous firmware, as it does after any reset before then.

namespace ClankerCleaner::Net::OtaUpdate
{
    // Call after Network::Start and before Run configures the pins. With no OtaToken in
    // esp32/Secrets.h the server stays off, but an unconfirmed update is still confirmed.
    void Start();
}
