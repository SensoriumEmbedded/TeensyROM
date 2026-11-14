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


void DigitizerController::init()
{
	USBHIDParser::driver_ready_for_hid_collection(this);
}


hidclaim_t DigitizerController::claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage)
{
	// only claim Desktop/Mouse
	if (topusage != 0xff0d0001) return CLAIM_NO;
	// only claim from one physical device
	if (mydevice != NULL && dev != mydevice) return CLAIM_NO;
	mydevice = dev;
	collections_claimed++;
	return CLAIM_REPORT;
}

void DigitizerController::disconnect_collection(Device_t *dev)
{
	if (--collections_claimed == 0) {
		mydevice = NULL;
	}
}

void DigitizerController::hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax)
{
	// TODO: check if absolute coordinates
	hid_input_begin_ = true;
}

void DigitizerController::hid_input_data(uint32_t usage, int32_t value)
{
	USBHDBGSerial.printf("Digitizer: usage=%X, value=%d\n", usage, value);
	uint32_t usage_page = usage >> 16;
	usage &= 0xFFFF;
	USBHDBGSerial.printf("Digitizer: &usage=%X, usage_page=%x\n", usage, usage_page);
	
	// This is Mikes version...
	if (usage_page == 0xff00 && usage >= 100 && usage <= 0x108) {
		switch (usage) {
		  case 0x102:
			mouseX = value;
			break;
		  case 0x103:
			mouseY = value;
			break;
		  case 0x32: // Apple uses this for horizontal scroll
			wheelH = value;
			break;
		  case 0x38:
			wheel = value;
			break;
		}
		digiAxes[usage & 0xf] = value;
	}
	if (usage_page == 0xff0D) {
		if (usage >= 0 && usage < 0x138) { //at least to start
			switch (usage) {
			  case 0x130: 
				mouseX = value;
				break;
			  case 0x131:
				mouseY = value;
				break;
			  case 0x132: // Apple uses this for horizontal scroll
				wheelH = value;
				break;
			  case 0x138:
				wheel = value;
				break;
			case 0x30: digiAxes[0] = value; break;
			case 0x32: digiAxes[1] = value; break;
			case 0x36: digiAxes[2] = value; break;
			case 0x42: digiAxes[3] = value; break;
			case 0x44: digiAxes[4] = value; break;
			case 0x5A: digiAxes[5] = value; break;
			case 0x5B: digiAxes[6] = value; break;
			case 0x5C: digiAxes[7] = value; break;
			case 0x77: digiAxes[8] = value; break;
			}
			//digiAxes[usage & 0xf] = value;
		} else if (usage >= 0x910 && usage <= 0x91f) {
			if (value == 0) {
				buttons &= ~(1 << (usage - 0x910));
			} else {
				buttons |= (1 << (usage - 0x910));
			}
		}
	}
}

void DigitizerController::hid_input_end()
{
	if (hid_input_begin_) {
		digitizerEvent = true;
		hid_input_begin_ = false;
	}
}

void DigitizerController::digitizerDataClear() {
	digitizerEvent = false;
	buttons = 0;
	mouseX  = 0;
	mouseY  = 0;
	wheel   = 0;
	wheelH  = 0;
}
