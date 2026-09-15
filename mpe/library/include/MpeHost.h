// SPDX-License-Identifier: MIT
// Public integration interface, copyright (c) 2026 Mean Hamster Software.
#pragma once
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <EEPROM.h>
#if !defined(__IMXRT1062__) || !defined(ARDUINO_TEENSY41) || !defined(USB_DISABLED) || !defined(Fab04_Features) || !defined(MHS_VM_PROFILE_192_320)
#error "MPE host requires Teensy 4.1 TR+, USB_DISABLED, and the reserved 192/320 RAM1 profile"
#endif
#define MPE_HOST_LIBRARY_VERSION "1.2.23"
#define MPE_HOST_LIBRARY_ABI 1
// Call only from the dedicated third image after its ResetHandler has run.
// These entry points own C64 bus handling and the complete MPE host lifecycle.
void mpeHostSetup();
void mpeHostLoop();
