// SPDX-License-Identifier: MIT
//
// TeensyROM's USB product name and serial number strings, shared by the main and
// minimal images so both present one identical USB device.  They replace the core's
// weak defaults ("USB Serial"/"Teensy MIDI" and a bare number).
//
// C only: #include this from a .c file (Source/Teensy/StartupHooks.c for main,
// MinimalBoot/Min_UsbNames.c for minimal), never from an .ino or a header compiled as
// C++.  usb_string_descriptor_struct ends in a flexible array member, and the
// initializers below are valid C99 but not reliably accepted by g++.

#include "usb_names.h"
#include "avr_functions.h"
#include "avr/pgmspace.h"
#include "ChipSerial.h"

// length must match the number of characters in the name.
// The trailing 8 zeros reserve room for the unique chip ID (up to 8 decimal
// digits), filled in at runtime by UsbNames_AppendUniqueID().

#define PRODUCT_BASE_LEN  10  // "TeensyROM-"
#define PRODUCT_NAME   {'T','e','e','n','s','y','R','O','M','-','0','0','0','0','0','0','0','0'}
#define PRODUCT_NAME_LEN  18

#define SERIAL_BASE_LEN  17  // "TeensyROM-Serial-"
#define SERIAL_NAME {'T','e','e','n','s','y','R','O','M','-','S','e','r','i','a','l','-','0','0','0','0','0','0','0','0'}
#define SERIAL_NAME_LEN  25

// Do not change this part.  This exact format is required by USB.

struct usb_string_descriptor_struct usb_string_product_name = {
        2 + PRODUCT_NAME_LEN * 2,
        3,
        PRODUCT_NAME
};

struct usb_string_descriptor_struct usb_string_serial_number = {
        2 + SERIAL_NAME_LEN * 2,
        3,
        SERIAL_NAME
};

// Overwrites the reserved trailing digits of each name above with this chip's
// unique ID (TR_ChipSerialNum(), the same value PJRC's own usb_init_serialnumber()
// uses), so each unit has its own product and serial names -- a DAW can tell two
// TeensyROMs apart -- and shrinks bLength to the actual digit count.
//
// Runs from each image's startup_middle_hook() (StartupHooks.c for main,
// Min_UsbNames.c for minimal), so both strings are final before usb_init() attaches
// the device and the host reads them.
FLASHMEM void UsbNames_AppendUniqueID(void)
{
	char buf[11];
	uint32_t i;

	ultoa(TR_ChipSerialNum(), buf, 10);
	for (i = 0; buf[i]; i++) {
		usb_string_product_name.wString[PRODUCT_BASE_LEN + i] = buf[i];
		usb_string_serial_number.wString[SERIAL_BASE_LEN + i] = buf[i];
	}
	usb_string_product_name.bLength = 2 + (PRODUCT_BASE_LEN + i) * 2;
	usb_string_serial_number.bLength = 2 + (SERIAL_BASE_LEN + i) * 2;
}
