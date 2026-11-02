#pragma once

// esp32/Secrets.h when it exists, otherwise the empty example (so the firmware runs offline).
// esp32/CMakeLists.txt checks for it. Include only from *.ESP32.cpp files.

#if CLANKER_HAS_SECRETS
#include <Secrets.h>
#else
#include <Secrets.example.h>
#endif
