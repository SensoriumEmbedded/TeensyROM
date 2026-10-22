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


// USB devices are managed from this file.


// List of all connected devices, regardless of their status.  If
// it's connected to the EHCI port or any port on any hub, it needs
// to be linked into this list.
static Device_t  *devlist=NULL;

// List of all inactive drivers.  At the end of enumeration, when
// drivers claim the device or its interfaces, they are removed
// from this list and linked into the list of active drivers on
// that device.  When devices disconnect, the drivers are returned
// to this list, making them again available for enumeration of new
// devices.
static USBDriver *available_drivers = NULL;

// Static buffers used during enumeration.  One a single USB device
// may enumerate at once, because USB address zero is used, and
// because this static buffer & state info can't be shared.
static uint8_t enumbuf[2048] __attribute__ ((aligned(16)));
static setup_t enumsetup __attribute__ ((aligned(16)));
static uint16_t enumlen;

// True while any device is present but not yet fully configured.
// Only one USB device may be in this state at a time (responding
// to address zero) and using the enumeration static buffer.
volatile bool USBHost::enumeration_busy = false;



static void pipe_set_maxlen(Pipe_t *pipe, uint32_t maxlen);
static void pipe_set_addr(Pipe_t *pipe, uint32_t addr);

#define print   USBHost::print_
#define println USBHost::println_

// The main user function to cause internal state to update.  Since we do
// almost everything with DMA and interrupts, the only work to do here is
// call all the active driver Task() functions.
void USBHost::Task()
{
	for (Device_t *dev = devlist; dev; dev = dev->next) {
		for (USBDriver *driver = dev->drivers; driver; driver = driver->next) {
			(driver->Task)();
		}
	}
}

// Drivers call this after they've completed initialization, so get themselves
// added to the list of inactive drivers available for new devices during
// enumeraton.  Typically this is called from constructors, so hardware access
// or even printing debug messages should be avoided here.  Just initialize
// lists and return.
//
void USBHost::driver_ready_for_device(USBDriver *driver)
{
	driver->device = NULL;
	driver->next = NULL;
	if (available_drivers == NULL) {
		available_drivers = driver;
	} else {
		// append to end of list
		USBDriver *last = available_drivers;
		while (last->next) last = last->next;
		last->next = driver;
	}
}

// Create a new device and begin the enumeration process
//
Device_t * USBHost::new_Device(uint32_t speed, uint32_t hub_addr, uint32_t hub_port)
{
	Device_t *dev;

	print("new_Device: ");
	switch (speed) {
	  case 0: print("12"); break;
	  case 1: print("1.5"); break;
	  case 2: print("480"); break;
	  default: print("??");
	}
	println(" Mbit/sec");
	dev = allocate_Device();
	if (!dev) return NULL;
	memset(dev, 0, sizeof(Device_t));
	dev->speed = speed;
	dev->address = 0;
	dev->hub_address = hub_addr;
	dev->hub_port = hub_port;
	dev->control_pipe = new_Pipe(dev, 0, 0, 0, 8);
	if (!dev->control_pipe) {
		free_Device(dev);
		return NULL;
	}
	dev->strbuf = allocate_string_buffer();  // try to allocate a string buffer; 
	dev->control_pipe->callback_function = &enumeration;
	dev->control_pipe->direction = 1; // 1=IN
	// Here is where the enumeration process officially begins.
	// Only a single device can enumerate at a time.
	USBHost::enumeration_busy = true;
	mk_setup(enumsetup, 0x80, 6, 0x0100, 0, 8); // 6=GET_DESCRIPTOR
	queue_Control_Transfer(dev, &enumsetup, enumbuf, NULL);
	if (devlist == NULL) {
		devlist = dev;
	} else {
		Device_t *p;
		for (p = devlist; p->next; p = p->next) ; // walk devlist
		p->next = dev;
	}
	return dev;
}


// Control transfer callback function.  ALL control transfers from all
// devices call this function when they complete.  When control transfers
// are created by drivers, the driver is called to handle the result.
// Otherwise, the control transfer is part of the enumeration process,
// which is implemented here.
//
void USBHost::enumeration(const Transfer_t *transfer)
{
	Device_t *dev;
	uint32_t len;

	// If a driver created this control transfer, allow it to process the result
	if (transfer->driver) {
		transfer->driver->control(transfer);
		return;
	}

	println("enumeration:");
	//print_hexbytes(transfer->buffer, transfer->length);
	//print(transfer);
	dev = transfer->pipe->device;

	while (1) {
		// Within this large switch/case, "break" means we've done
		// some work, but more remains to be done in a different
		// state.  Generally break is used after parsing received
		// data, but what happens next could be different states.
		// When completed, return is used.  Generally, return happens
		// only after a new control transfer is queued, or when
		// enumeration is complete and no more communication is needed.
		switch (dev->enum_state) {
		case 0: // read 8 bytes of device desc, set max packet, and send set address
			pipe_set_maxlen(dev->control_pipe, enumbuf[7]);
			mk_setup(enumsetup, 0, 5, assign_address(), 0, 0); // 5=SET_ADDRESS
			queue_Control_Transfer(dev, &enumsetup, NULL, NULL);
			dev->enum_state = 1;
			return;
		case 1: // request all 18 bytes of device descriptor
			dev->address = enumsetup.wValue;
			pipe_set_addr(dev->control_pipe, enumsetup.wValue);
			mk_setup(enumsetup, 0x80, 6, 0x0100, 0, 18); // 6=GET_DESCRIPTOR
			queue_Control_Transfer(dev, &enumsetup, enumbuf, NULL);
			dev->enum_state = 2;
			return;
		case 2: // parse 18 device desc bytes
			print_device_descriptor(enumbuf);
			dev->bDeviceClass = enumbuf[4];
			dev->bDeviceSubClass = enumbuf[5];
			dev->bDeviceProtocol = enumbuf[6];
			dev->idVendor = enumbuf[8] | (enumbuf[9] << 8);
			dev->idProduct = enumbuf[10] | (enumbuf[11] << 8);
			enumbuf[0] = enumbuf[14];
			enumbuf[1] = enumbuf[15];
			enumbuf[2] = enumbuf[16];
			if ((enumbuf[0] | enumbuf[1] | enumbuf[2]) > 0) {
				dev->enum_state = 3;
			} else {
				dev->enum_state = 11;
			}
			break;
		case 3: // request Language ID
			len = sizeof(enumbuf) - 4;
			mk_setup(enumsetup, 0x80, 6, 0x0300, 0, len); // 6=GET_DESCRIPTOR
			queue_Control_Transfer(dev, &enumsetup, enumbuf + 4, NULL);
			dev->enum_state = 4;
			return;
		case 4: // parse Language ID
			if (enumbuf[4] < 4 || enumbuf[5] != 3) {
				dev->enum_state = 11;
			} else {
				dev->LanguageID = enumbuf[6] | (enumbuf[7] << 8);
				if (enumbuf[0]) dev->enum_state = 5;
				else if (enumbuf[1]) dev->enum_state = 7;
				else if (enumbuf[2]) dev->enum_state = 9;
				else dev->enum_state = 11;
			}
			break;
		case 5: // request Manufacturer string
			len = sizeof(enumbuf) - 4;
			mk_setup(enumsetup, 0x80, 6, 0x0300 | enumbuf[0], dev->LanguageID, len);
			queue_Control_Transfer(dev, &enumsetup, enumbuf + 4, NULL);
			dev->enum_state = 6;
			return;
		case 6: // parse Manufacturer string
			print_string_descriptor("Manufacturer: ", enumbuf + 4);
			convertStringDescriptorToASCIIString(0, dev, transfer);
			// TODO: receive the string...
			if (enumbuf[1]) dev->enum_state = 7;
			else if (enumbuf[2]) dev->enum_state = 9;
			else dev->enum_state = 11;
			break;
		case 7: // request Product string
			len = sizeof(enumbuf) - 4;
			mk_setup(enumsetup, 0x80, 6, 0x0300 | enumbuf[1], dev->LanguageID, len);
			queue_Control_Transfer(dev, &enumsetup, enumbuf + 4, NULL);
			dev->enum_state = 8;
			return;
		case 8: // parse Product string
			print_string_descriptor("Product: ", enumbuf + 4);
			convertStringDescriptorToASCIIString(1, dev, transfer);
			if (enumbuf[2]) dev->enum_state = 9;
			else dev->enum_state = 11;
			break;
		case 9: // request Serial Number string
			len = sizeof(enumbuf) - 4;
			mk_setup(enumsetup, 0x80, 6, 0x0300 | enumbuf[2], dev->LanguageID, len);
			queue_Control_Transfer(dev, &enumsetup, enumbuf + 4, NULL);
			dev->enum_state = 10;
			return;
		case 10: // parse Serial Number string
			print_string_descriptor("Serial Number: ", enumbuf + 4);
			convertStringDescriptorToASCIIString(2, dev, transfer);
			dev->enum_state = 11;
			break;
		case 11: // request first 9 bytes of config desc
			mk_setup(enumsetup, 0x80, 6, 0x0200, 0, 9); // 6=GET_DESCRIPTOR
			queue_Control_Transfer(dev, &enumsetup, enumbuf, NULL);
			dev->enum_state = 12;
			return;
		case 12: // read 9 bytes, request all of config desc
			enumlen = enumbuf[2] | (enumbuf[3] << 8);
			println("Config data length = ", enumlen);
			if (enumlen > sizeof(enumbuf)) {
				enumlen = sizeof(enumbuf);
				// TODO: how to handle device with too much config data
			}
			mk_setup(enumsetup, 0x80, 6, 0x0200, 0, enumlen); // 6=GET_DESCRIPTOR
			queue_Control_Transfer(dev, &enumsetup, enumbuf, NULL);
			dev->enum_state = 13;
			return;
		case 13: // read all config desc, send set config
			print_config_descriptor(enumbuf, sizeof(enumbuf));
			dev->bmAttributes = enumbuf[7];
			dev->bMaxPower = enumbuf[8];
			// TODO: actually do something with interface descriptor?
			mk_setup(enumsetup, 0, 9, enumbuf[5], 0, 0); // 9=SET_CONFIGURATION
			queue_Control_Transfer(dev, &enumsetup, NULL, NULL);
			dev->enum_state = 14;
			return;
		case 14: // device is now configured
			claim_drivers(dev);
			dev->enum_state = 15;
			// unlock exclusive access to enumeration process.  If any
			// more devices are waiting, the hub driver is responsible
			// for resetting their ports and starting their enumeration
			// when the port enables.
			USBHost::enumeration_busy = false;
			return;
		case 15: // control transfers for other stuff?
			// TODO: handle other standard control: set/clear feature, etc
		default:
			return;
		}
	}
}

void  USBHost::convertStringDescriptorToASCIIString(uint8_t string_index, Device_t *dev, const Transfer_t *transfer) {
	strbuf_t *strbuf = dev->strbuf; 
	if (!strbuf) return;	// don't have a buffer

	uint8_t *buffer = (uint8_t*)transfer->buffer;
	uint8_t buf_index = string_index? strbuf->iStrings[string_index]+1 : 0;

	// Try to verify - The first byte should be length and the 2nd byte should be 0x3
	if (!buffer || (buffer[1] != 0x3)) {
		return;	// No string so can simply return
	}

	strbuf->iStrings[string_index] = buf_index;	// remember our starting positio
	uint8_t count_bytes_returned = buffer[0];
	if ((buf_index + count_bytes_returned/2) >= DEVICE_STRUCT_STRING_BUF_SIZE)
		count_bytes_returned = (DEVICE_STRUCT_STRING_BUF_SIZE - buf_index) * 2;

	// Now copy into our storage buffer. 
	for (uint8_t i = 2; (i < count_bytes_returned) && (buf_index < (DEVICE_STRUCT_STRING_BUF_SIZE -1)); i += 2) {
		strbuf->buffer[buf_index++] = buffer[i];
	} 
	strbuf->buffer[buf_index] = 0;	// null terminate. 

	// Update other indexes to point to null character
	while (++string_index < 3) {
		strbuf->iStrings[string_index] = buf_index;	// point to trailing NULL character
	}
}


void USBHost::claim_drivers(Device_t *dev)
{
	USBDriver *driver, *prev=NULL;

	// first check if any driver wishes to claim the entire device
	for (driver=available_drivers; driver != NULL; driver = driver->next) {
		if (driver->device != NULL) continue;
		if (driver->claim(dev, 0, enumbuf + 9, enumlen - 9)) {
			if (prev) {
				prev->next = driver->next;
			} else {
				available_drivers = driver->next;
			}
			driver->device = dev;
			driver->next = NULL;
			dev->drivers = driver;
			return;
		}
		prev = driver;
	}
	// parse interfaces from config descriptor
	const uint8_t *p = enumbuf + 9;
	const uint8_t *end = enumbuf + enumlen;
	while (p < end) {
		uint8_t desclen = *p;
		uint8_t desctype = *(p+1);
		print("Descriptor ");
		print(desctype);
		print(" = ");
		if (desctype == 4) println("INTERFACE");
		else if (desctype == 5) println("ENDPOINT");
		else if (desctype == 6) println("DEV_QUALIFIER");
		else if (desctype == 7) println("OTHER_SPEED");
		else if (desctype == 11) println("IAD");
		else if (desctype == 33) println("HID");
		else println(" ???");
		if (desctype == 11 && desclen == 8) {
			// TODO: parse IAD, ask drivers for claim
			// TODO: how to skip over all interfaces IAD represented
		}
		if (desctype == 4 && desclen == 9) {
			// found an interface, ask available drivers if they want it
			prev = NULL;
			for (driver=available_drivers; driver != NULL; driver = driver->next) {
				if (driver->device != NULL) continue;
				// TODO: should parse ahead and give claim()
				// an accurate length.  (end - p) is the rest
				// of ALL descriptors, likely more interfaces
				// this driver has no business parsing
				if (driver->claim(dev, 1, p, end - p)) {
					// this driver claims iface
					// remove it from available_drivers list
					if (prev) {
						prev->next = driver->next;
					} else {
						available_drivers = driver->next;
					}
					// add to list of drivers using this device
					driver->next = dev->drivers;
					dev->drivers = driver;
					driver->device = dev;
					// not done, may be more interface for more drivers
				}
				prev = driver;
			}
		}
		p += desclen;
	}
}

static bool address_in_use(uint32_t addr)
{
	for (Device_t *p = devlist; p; p = p->next) {
		if (p->address == addr) return true;
	}
	return false;
}

uint32_t USBHost::assign_address(void)
{
	static uint8_t last_assigned_address=0;
	uint32_t addr = last_assigned_address;
	while (1) {
		if (++addr > 127) addr = 1;
		if (!address_in_use(addr)) {
			last_assigned_address = addr;
			return addr;
		}
	}
}

static void pipe_set_maxlen(Pipe_t *pipe, uint32_t maxlen)
{
	pipe->qh.capabilities[0] = (pipe->qh.capabilities[0] & 0xF800FFFF) | (maxlen << 16);
}

static void pipe_set_addr(Pipe_t *pipe, uint32_t addr)
{
	pipe->qh.capabilities[0] = (pipe->qh.capabilities[0] & 0xFFFFFF80) | addr;
}


void USBHost::disconnect_Device(Device_t *dev)
{
	if (!dev) return;
	println("disconnect_Device:");

	// Disconnect all drivers using this device.  If this device is
	// a hub, the hub driver is responsible for recursively calling
	// this function to disconnect its downstream devices.
	print_driverlist("available_drivers", available_drivers);
	print_driverlist("dev->drivers", dev->drivers);
	for (USBDriver *p = dev->drivers; p; ) {
		println("disconnect driver ", (uint32_t)p, HEX);
		p->disconnect();
		p->device = NULL;
		USBDriver *next = p->next;
		p->next = available_drivers;
		available_drivers = p;
		p = next;
	}
	print_driverlist("available_drivers", available_drivers);

	// delete all the pipes
	for (Pipe_t *p = dev->data_pipes; p; ) {
		Pipe_t *next = p->next;
		delete_Pipe(p);
		p = next;
	}
	delete_Pipe(dev->control_pipe);

	// remove device from devlist and free its Device_t
	Device_t *prev_dev = NULL;
	for (Device_t *p = devlist; p; p = p->next) {
		if (p == dev) {
			if (prev_dev == NULL) {
				devlist = p->next;
			} else {
				prev_dev->next = p->next;
			}
			println("removed Device_t from devlist");
			if (p->strbuf != nullptr ) {
				free_string_buffer(p->strbuf);
			}
			free_Device(p);
			break;
		}
		prev_dev = p;
	}
}


