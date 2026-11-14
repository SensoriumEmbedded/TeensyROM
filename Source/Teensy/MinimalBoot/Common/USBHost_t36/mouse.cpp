/* USB EHCI Host for Teensy 3.6
 * Copyright 2017 Paul Stoffregen (paul@pjrc.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <Arduino.h>
#include "USBHost_t36.h"  // Read this header first for key info


void MouseController::init()
{
	USBHIDParser::driver_ready_for_hid_collection(this);
	BluetoothController::driver_ready_for_bluetooth(this);
}


hidclaim_t MouseController::claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage)
{
	// only claim Desktop/Mouse
	//USBHDBGSerial.printf("MouseController::claim_collection(%p) Driver:%p(%u %u) Dev:%p Top:%x\n", this, driver, 
	//	driver->interfaceSubClass(), driver->interfaceProtocol(), dev, topusage);

	if ((topusage != 0x10002) && (topusage != 0x10001)) return CLAIM_NO;
	// only claim from one physical device
	if (mydevice != NULL && dev != mydevice) return CLAIM_NO;
	mydevice = dev;
	collections_claimed++;
	//USBHDBGSerial.printf("\tMouseController claim collection\n");
	return CLAIM_REPORT;
}

void MouseController::disconnect_collection(Device_t *dev)
{
	if (--collections_claimed == 0) {
		mydevice = NULL;
	}
}

void MouseController::hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax)
{
	// TODO: check if absolute coordinates
	hid_input_begin_ = true;
}

void MouseController::hid_input_data(uint32_t usage, int32_t value)
{
	//USBHDBGSerial.printf("Mouse: usage=%X, value=%d\n", usage, value);
	uint32_t usage_page = usage >> 16;
	usage &= 0xFFFF;
	if (usage_page == 9 && usage >= 1 && usage <= 8) {
		if (value == 0) {
			buttons &= ~(1 << (usage -1));
		} else {
			buttons |= (1 << (usage -1));
		}
	} else if (usage_page == 1) {
		switch (usage) {
		  case 0x30:
			mouseX = value;
			break;
		  case 0x31:
			mouseY = value;
			break;
		  case 0x32: // Apple uses this for horizontal scroll
			wheelH = value;
			break;
		  case 0x38:
			wheel = value;
			break;
		}
	} else if (usage_page == 12) {
		if (usage == 0x238) { // Microsoft uses this for horizontal scroll
			wheelH = value;
		}
	}
}

void MouseController::hid_input_end()
{
	if (hid_input_begin_) {
		mouseEvent = true;
		hid_input_begin_ = false;
	}
}

void MouseController::mouseDataClear() {
	mouseEvent = false;
	buttons = 0;
	mouseX  = 0;
	mouseY  = 0;
	wheel   = 0;
	wheelH  = 0;
}


hidclaim_t MouseController::claim_bluetooth(BluetoothConnection *btconnection, uint32_t bluetooth_class, uint8_t *remoteName, int type)
{
	// How to handle combo devices? 
	USBHDBGSerial.printf("MouseController Controller::claim_bluetooth - Class %x\n", bluetooth_class);
	// If we are already in use than don't grab another one.  Likewise don't grab if it is used as USB or HID object
	if (btconnect && (btconnection != btconnect)) return CLAIM_NO;
	if (mydevice != NULL) return CLAIM_NO;

	if ((bluetooth_class & 0x0f00) == 0x500) {
		// This is a peripheral class

		// So test for class of Mouse
		if (bluetooth_class & 0x80) {
			// We will claim this now
			// Test to link in BT HID parser code
			if (type == 1) {
				// They are telling me to grab it now. SO say yes
				USBHDBGSerial.printf("MouseController::claim_bluetooth TRUE\n");
				btconnect = btconnection;
				btdevice = (Device_t*)btconnect->btController_;	// remember this way 
				btdriver_ = btconnect->btController_;
				return CLAIM_INTERFACE;
			}
		}
		return CLAIM_REPORT; // let them know we may be interested if there is a HID REport Descriptor
	}
	return CLAIM_NO;
}

bool MouseController::process_bluetooth_HID_data(const uint8_t *data, uint16_t length) 
{
	// Example DATA from bluetooth keyboard:
	//                  0  1 2 3 4 5  6 7  8 910 1 2 3 4 5 6 7
	//                           LEN         D
	//BT rx2_data(14): b 20 a 0 6 0 71 0 a1 2 0 9 fe 0 
	//BT rx2_data(14): b 20 a 0 6 0 71 0 a1 2 0 8 fd 0 

	// So Len=9 passed in data starting at report ID=1... 
	if (length == 0) return false;
#ifdef USBHOST_PRINT_DEBUG
	USBHDBGSerial.printf("MouseController::process_bluetooth_HID_data %d\n", length);
	USBHDBGSerial.printf("  Mouse Data: ");
	const uint8_t *p = (const uint8_t *)data;
	uint16_t len = length;
	do {
		if (*p < 16) USBHDBGSerial.print('0');
		USBHDBGSerial.print(*p++, HEX);
		USBHDBGSerial.print(' ');
	} while (--len);
	USBHDBGSerial.println();
#endif	
	// Looks like report 2 is for the mouse info.
	if (data[0] != 2) return false;
	buttons = data[1];
	mouseX  = (int8_t)data[2];
	mouseY  = (int8_t)data[3];
	if (length >= 5) {
		wheel   = (int8_t)data[4];
		if (length >= 6) {
			wheelH = (int8_t)data[5];
		}
	}
	mouseEvent = true;

	return true;
}

void MouseController::release_bluetooth() 
{
	btdevice = nullptr;
}


hidclaim_t MouseController::bt_claim_collection(BluetoothConnection *btconnection, uint32_t bluetooth_class, uint32_t topusage)
{
	USBHDBGSerial.printf("MouseController::bt_claim_collection(%p) Connection:%p class:%x Top:%x\n", this, btconnection, bluetooth_class, topusage);


	if (mydevice != NULL) return CLAIM_NO;  // claimed by some other... 
	if (btconnect && (btconnect != btconnection)) return CLAIM_NO;
	// We will claim if BOOT Keyboard.

	if ((topusage != 0x10002) && (topusage != 0x10001)) return CLAIM_NO;

	USBHDBGSerial.printf("\tMouseController claim collection\n");
	btconnect = btconnection;
	btdevice = (Device_t*)btconnect->btController_;	// remember this way 
	return CLAIM_REPORT;
}

void MouseController::bt_hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax)
{
	hid_input_begin(topusage, type, lgmin, lgmax);	
}

void MouseController::bt_hid_input_data(uint32_t usage, int32_t value)
{
	hid_input_data(usage, value);
}

void MouseController::bt_hid_input_end()
{
	hid_input_end();
}

void MouseController::bt_disconnect_collection(Device_t *dev)
{
	disconnect_collection(dev);
}
