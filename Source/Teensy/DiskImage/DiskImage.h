// SPDX-License-Identifier: MIT
// The D64/D71/D81 engine that the GEOS support, the TR block device and VDrive share (docs/Architecture/Disk-Images.md).
// The firmware builds every .ino and the headers they include as one translation unit, and so does each host
// harness, so the code comes in once, here, behind the guard.
#pragma once
#ifdef ARDUINO
#include <avr/pgmspace.h>
#ifndef VD_COLD
#define VD_COLD FLASHMEM
#endif
#ifndef D64_FLASH
#define D64_FLASH PROGMEM
#endif
#endif

#include "d64.h"
#include "d64.c"
