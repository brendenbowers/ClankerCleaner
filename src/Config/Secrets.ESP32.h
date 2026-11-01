#pragma once

// esp32/Secrets.h when it exists, otherwise the empty example (so the firmware runs offline).
// Include only from *.ESP32.cpp files.

#if __has_include(<Secrets.h>)
#include <Secrets.h>
#else
#include <Secrets.example.h>
#endif
