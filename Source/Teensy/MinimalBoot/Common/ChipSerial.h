// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include "imxrt.h"

// This chip's unique number, as PJRC's core computes it for the default USB serial
// number: the 24-bit ID from the HW_OCOTP_MAC0 fuse, times 10 when it's under 8
// digits (the core's work-around for the OS-X CDC-ACM driver).  The USB name strings
// (UsbNames.c) and the build info reports all show this same value.
// static inline: included from C (UsbNames.c) and C++ (.ino) alike.
static inline uint32_t TR_ChipSerialNum(void)
{
   uint32_t serialNum = HW_OCOTP_MAC0 & 0xFFFFFF;
   if (serialNum < 10000000) serialNum *= 10;
   return serialNum;
}
