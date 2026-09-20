// this code must be placed into a .c file
#include "usb_names.h"
#include "imxrt.h"
#include "avr_functions.h"
#include "avr/pgmspace.h"

// length must match the number of characters in the name.
// The trailing 8 zeros reserve room for the unique chip ID (up to 8 decimal
// digits), filled in at runtime by MidiDevName_AppendUniqueID().

#define MIDI_BASE_LEN  10  // "TeensyROM-"
#define MIDI_NAME   {'T','e','e','n','s','y','R','O','M','-','0','0','0','0','0','0','0','0'}
#define MIDI_NAME_LEN  18

#define SERIAL_BASE_LEN  17  // "TeensyROM-Serial-"
#define SERIAL_NAME {'T','e','e','n','s','y','R','O','M','-','S','e','r','i','a','l','-','0','0','0','0','0','0','0','0'}
#define SERIAL_NAME_LEN  25

// Do not change this part.  This exact format is required by USB.

struct usb_string_descriptor_struct usb_string_product_name = {
        2 + MIDI_NAME_LEN * 2,
        3,
        MIDI_NAME
};

struct usb_string_descriptor_struct usb_string_serial_number = {
        2 + SERIAL_NAME_LEN * 2,
        3,
        SERIAL_NAME
};

// Overwrites the reserved trailing digits of each name above with this
// chip's unique ID (same HW_OCOTP_MAC0 fuse value and OS-X work-around used
// by PJRC's own usb_init_serialnumber()), so left/right units get distinct
// product/serial names, and shrinks bLength to the actual digit count.
//
// Runs from startup_middle_hook() (StartupHooks.c), so both strings are final
// before usb_init() attaches the device and the host reads them.
FLASHMEM void MidiDevName_AppendUniqueID(void)
{
	char buf[11];
	uint32_t i;
	uint32_t serialNum = HW_OCOTP_MAC0 & 0xFFFFFF; // Read the unique 24-bit identifier from the hardware fuse
	if (serialNum < 10000000) serialNum *= 10; // Replicate the OS-X CDC-ACM driver work-around used by PJRC core

	ultoa(serialNum, buf, 10);
	for (i = 0; buf[i]; i++) {
		usb_string_product_name.wString[MIDI_BASE_LEN + i] = buf[i];
		usb_string_serial_number.wString[SERIAL_BASE_LEN + i] = buf[i];
	}
	usb_string_product_name.bLength = 2 + (MIDI_BASE_LEN + i) * 2;
	usb_string_serial_number.bLength = 2 + (SERIAL_BASE_LEN + i) * 2;
}
