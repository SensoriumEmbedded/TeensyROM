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


// This HID driver claims a USB interface and parses its incoming
// data (reports).  It doesn't actually use the data, but it allows
// drivers which inherit the USBHIDInput base class to claim the
// top level collections within the reports.  Those drivers get
// callbacks with the arriving data full decoded to data/usage
// pairs.

#define print   USBHost::print_
#define println USBHost::println_

void USBHIDParser::init()
{
	contribute_Pipes(mypipes, sizeof(mypipes)/sizeof(Pipe_t));
	contribute_Transfers(mytransfers, sizeof(mytransfers)/sizeof(Transfer_t));
	contribute_String_Buffers(mystring_bufs, sizeof(mystring_bufs)/sizeof(strbuf_t));
	driver_ready_for_device(this);
}

bool USBHIDParser::claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len)
{
	println("HIDParser claim this=", (uint32_t)this, HEX);

	// only claim at interface level
	if (type != 1) return false;
	if (len < 9+9+7) return false;

	// interface descriptor
	uint32_t numendpoint = descriptors[4];
	if (numendpoint < 1 || numendpoint > 2) return false;
	if (descriptors[5] != 3) return false; // bInterfaceClass, 3 = HID
	println(" bInterfaceNumber =   ", descriptors[2]);
	println(" bInterfaceClass =    ", descriptors[5]);
	println(" bInterfaceSubClass = ", descriptors[6]);
	println(" bInterfaceProtocol = ", descriptors[7]);
	print("HID Parser Claim: ");
	print_hexbytes(descriptors, len);


	// hid interface descriptor
	uint32_t hidlen = descriptors[9];
	if (hidlen < 9) return false;
	if (descriptors[10] != 33) return false; // descriptor type, 33=HID
	if (descriptors[14] < 1) return false;  // must be at least 1 extra descriptor
	if (hidlen != (uint32_t)(6 + descriptors[14] * 3)) return false; // must be correct size
	if (9 + hidlen > len) return false;
	uint32_t i=0;
	while (1) {
		if (descriptors[15 + i * 3] == 34) { // found HID report descriptor
			descsize = descriptors[16 + i * 3] | (descriptors[17 + i * 3] << 8);
			println("report descriptor size = ", descsize);
			break;
		}
		i++;
		if (i >= descriptors[14]) return false;
	}
	if (descsize > _big_buffer_size) return false; // can't fit the report descriptor

	// endpoint descriptor(s)
	uint32_t offset = 9 + hidlen;
	if (len < offset + numendpoint * 7) return false; // not enough data
	if (numendpoint == 1) {
		println("Single endpoint HID:");
		if (descriptors[offset] != 7) return false;
		if (descriptors[offset+1] != 5) return false; // endpoint descriptor
		if (descriptors[offset+3] != 3) return false; // must be interrupt type
		uint32_t endpoint = descriptors[offset+2];
		uint32_t size = descriptors[offset+4] | (descriptors[offset+5] << 8);
		uint32_t interval = descriptors[offset+6];
		println("  endpoint = ", endpoint, HEX);
		println("   size = ", size);
		println("   interval = ", interval);
		if ((endpoint & 0x0F) == 0) return false;
		if ((endpoint & 0xF0) != 0x80) return false; // must be IN direction
		in_pipe = new_Pipe(dev, 3, endpoint & 0x0F, 1, size, interval);
		out_pipe = NULL;
		in_size = size;
	} else {
		println("Two endpoint HID:");
		if (descriptors[offset] != 7) return false;
		if (descriptors[offset+1] != 5) return false; // endpoint descriptor
		if (descriptors[offset+3] != 3) return false; // must be interrupt type
		uint32_t endpoint1 = descriptors[offset+2];
		uint32_t size1 = descriptors[offset+4] | (descriptors[offset+5] << 8);
		uint32_t interval1 = descriptors[offset+6];
		println("  endpoint = ", endpoint1, HEX);
		println("   size = ", size1);
		println("   interval = ", interval1);
		if ((endpoint1 & 0x0F) == 0) return false;
		if (descriptors[offset+7] != 7) return false;
		if (descriptors[offset+8] != 5) return false; // endpoint descriptor
		if (descriptors[offset+10] != 3) return false; // must be interrupt type
		uint32_t endpoint2 = descriptors[offset+9];
		uint32_t size2 = descriptors[offset+11] | (descriptors[offset+12] << 8);
		uint32_t interval2 = descriptors[offset+13];
		println("  endpoint = ", endpoint2, HEX);
		println("   size = ", size2);
		println("   interval = ", interval2);
		if ((endpoint2 & 0x0F) == 0) return false;
		if (((endpoint1 & 0xF0) == 0x80) && ((endpoint2 & 0xF0) == 0)) {
			// first endpoint is IN, second endpoint is OUT
			in_pipe = new_Pipe(dev, 3, endpoint1 & 0x0F, 1, size1, interval1);
			out_pipe = new_Pipe(dev, 3, endpoint2, 0, size2, interval2);
			in_size = size1;
			out_size = size2;
		} else if (((endpoint1 & 0xF0) == 0) && ((endpoint2 & 0xF0) == 0x80)) {
			// first endpoint is OUT, second endpoint is IN
			in_pipe = new_Pipe(dev, 3, endpoint2 & 0x0F, 1, size2, interval2);
			out_pipe = new_Pipe(dev, 3, endpoint1, 0, size1, interval1);
			in_size = size2;
			out_size = size1;
		} else {
			return false;
		}
		out_pipe->callback_function = out_callback;
	}
	in_pipe->callback_function = in_callback;
	for (uint32_t i=0; i < TOPUSAGE_LIST_LEN; i++) {
		//topusage_list[i] = 0;
		topusage_drivers[i] = NULL;
	}
	// request the HID report descriptor
	bInterfaceNumber = descriptors[2];	// save away the interface number; 
	bInterfaceSubClass = descriptors[6]; // likewise sub type and protocol.
	bInterfaceProtocol = descriptors[7];
	
	mk_setup(setup, 0x81, 6, 0x2200, descriptors[2], descsize); // get report desc
	queue_Control_Transfer(dev, &setup, _bigBuffer, this);
	return true;
}

void USBHIDParser::control(const Transfer_t *transfer)
{
	println("control callback (hid)");
	print_hexbytes(transfer->buffer, transfer->length);
	if (topusage_drivers[0]) {
		if (topusage_drivers[0]->hid_process_control(transfer)) {
			return; // the called function can tell us they processed it.
		}
	}

	// To decode hex dump to human readable HID report summary:
	//   http://eleccelerator.com/usbdescreqparser/
	uint32_t mesg = transfer->setup.word1;
	println("  mesg = ", mesg, HEX);
	if (mesg == 0x22000681 && transfer->length == descsize) { // HID report descriptor
		println("  got report descriptor");
		parse();
		// We need to setup the buffer pointers. 
		if (_rx1 == nullptr) {
			_rx1 = _bigBufferEnd - in_size;
			_rx2 = _rx1 - in_size;
			_bigBufferEnd = _rx2;
		}

		queue_Data_Transfer(in_pipe, _rx1, in_size, this);
		if (_rx2) queue_Data_Transfer(in_pipe, _rx2, in_size, this);
		if (_rx3) queue_Data_Transfer(in_pipe, _rx3, in_size, this);
		if (_rx4) queue_Data_Transfer(in_pipe, _rx4, in_size, this);

		if (device->idVendor == 0x054C && 
				((device->idProduct == 0x0268) || (device->idProduct == 0x042F)/* || (device->idProduct == 0x03D5)*/)) {
			println("send special PS3 feature command");
			mk_setup(setup, 0x21, 9, 0x03F4, 0, 4); // ps3 tell to send report 1?
			static uint8_t ps3_feature_F4_report[] = {0x42, 0x0c, 0x00, 0x00};
			queue_Control_Transfer(device, &setup, ps3_feature_F4_report, this);
		}
	}
}

void USBHIDParser::in_callback(const Transfer_t *transfer)
{
	if (transfer->driver) {
		((USBHIDParser*)(transfer->driver))->in_data(transfer);
	}
}

void USBHIDParser::out_callback(const Transfer_t *transfer)
{
	//println("USBHIDParser:: out_callback (static)");
	if (transfer->driver) {
		((USBHIDParser*)(transfer->driver))->out_data(transfer);
	}
}

// When the device goes away, we need to call disconnect_collection()
// for all drivers which claimed a top level collection
void USBHIDParser::disconnect()
{
	for (uint32_t i=0; i < TOPUSAGE_LIST_LEN; i++) {
		USBHIDInput *driver = topusage_drivers[i];
		if (driver) {
			driver->disconnect_collection(device);
			topusage_drivers[i] = NULL;
		}
	}
}

// Called when the HID device sends a report
void USBHIDParser::in_data(const Transfer_t *transfer)
{
	/*USBHDBGSerial.printf("HID: ");
	uint8_t *pb = (uint8_t*)transfer->buffer;
	for (uint8_t i = 0; i < transfer->length; i++) {
		USBHDBGSerial.printf("%02x ",pb[i]);
	}
	USBHDBGSerial.printf("\n"); */

	/*
	print("HID: ");
	print(use_report_id);
	print(" - ");
	print_hexbytes(transfer->buffer, transfer->length);
	*/
	const uint8_t *buf = (const uint8_t *)transfer->buffer;
	uint32_t len = transfer->length;

	// See if the first top report wishes to bypass the
	// parse...
	if (!(topusage_drivers[0] && topusage_drivers[0]->hid_process_in_data(transfer))) {

		if (use_report_id == false) {
			parse(0x0100, buf, len);
		} else {
			if (len > 1) {
				parse(0x0100 | buf[0], buf + 1, len - 1);
			}
		}
	}
	#if defined(__IMXRT1062__) // Teensy 4.x
    if ((uint32_t)buf >= 0x20200000u) arm_dcache_flush_delete((void*)buf, in_size);
	#endif
	queue_Data_Transfer(in_pipe, (void*)buf, in_size, this);
}


void USBHIDParser::out_data(const Transfer_t *transfer)
{
	//Serial.printf(">>>USBHIDParser::out_data\n");
	println("USBHIDParser:out_data called (instance)");
	// A packet completed. lets mark it as done and call back
	// to top reports handler.  We unmark our checkmark to
	// handle case where they may want to queue up another one. 
	uint8_t mask = 1;
	const uint8_t *buffer = (const uint8_t *)transfer->buffer;
	for(uint8_t i = 0; i < 4; i++) {
		if (buffer == _tx[i]) {
			_tx_state &= ~mask;
			break;
		}
		mask <<= 1;
	}
	if (topusage_drivers[0]) {
		topusage_drivers[0]->hid_process_out_data(transfer);
	}
}

void USBHIDParser::timer_event(USBDriverTimer *whichTimer)
{
	if (topusage_drivers[0]) {
		topusage_drivers[0]->hid_timer_event(whichTimer);
	}	
}


bool USBHIDParser::sendPacket(const uint8_t *buffer, int cb) {
	if (!out_size || !out_pipe) return false;	
	if (!_tx[0]) {
		// Was not init before, for now lets put it at end of descriptor
		// TODO: should verify that either don't exceed overlap descsize
		//       Or that we have taken over this device
		_tx[0] = _bigBufferEnd - out_size;
		_tx[1] = _tx[0] - out_size;
		_bigBufferEnd = _tx[1];
		_tx_mask = 3;
	}
	if ((_tx_state & _tx_mask) == _tx_mask) return false; 	// both transmit buffers are full
	if (cb == -1)
		cb = out_size;
	uint8_t mask = 0x1;
	uint8_t *p = _tx[0];
	for (uint8_t i = 0; i < 4; i++ ) {
		if ((mask & _tx_mask ) == 0) return false; // none found
		if ((mask & _tx_state) == 0) {
			_tx_state |= mask;
			p = _tx[i];
			break;
		}
		mask <<=1;
	}
	// copy the users data into our out going buffer
	memcpy(p, buffer, cb);	

#if defined(__IMXRT1062__) // Teensy 4.x
    if ((uint32_t)p >= 0x20200000u) arm_dcache_flush_delete(p, cb);
#endif
	println("USBHIDParser Send packet");
	print_hexbytes(buffer, cb);
	bool fReturn = queue_Data_Transfer(out_pipe, p, cb, this);
	println("    Queue_data transfer returned:", fReturn, DEC);
	return fReturn;
}

void USBHIDParser::setTXBuffers(uint8_t *buffer1, uint8_t *buffer2, uint8_t cb,
	uint8_t *buffer3, uint8_t* buffer4)
{
	uint8_t index = 0;
	if (buffer1) _tx[index++] = buffer1;
	if (buffer2) _tx[index++] = buffer2;
	if (buffer3) _tx[index++] = buffer3;
	if (buffer4) _tx[index++] = buffer4;
	_tx_mask = (1 << index) - 1; // 2 by default 1<< 2 =4 -1 = 3...
}

void USBHIDParser::setRXBuffers(uint8_t *buffer1, uint8_t *buffer2, uint8_t cb,
	uint8_t *buffer3, uint8_t* buffer4)
{
	_rx1 = buffer1;
	_rx2 = buffer2;
	_rx3 = buffer3;
	_rx4 = buffer4;
	#if defined(__IMXRT1062__) // Teensy 4.x
    if ((uint32_t)_rx1 >= 0x20200000u) arm_dcache_flush_delete(_rx1, in_size);
    if ((uint32_t)_rx2 >= 0x20200000u) arm_dcache_flush_delete(_rx2, in_size);
    if ((uint32_t)_rx3 >= 0x20200000u) arm_dcache_flush_delete(_rx3, in_size);
    if ((uint32_t)_rx4 >= 0x20200000u) arm_dcache_flush_delete(_rx4, in_size);
	#endif
}

bool USBHIDParser::sendControlPacket(uint32_t bmRequestType, uint32_t bRequest,
			uint32_t wValue, uint32_t wIndex, uint32_t wLength, void *buf)
{
	// Use setup structure to build packet 
	//USBHDBGSerial.printf(">>> SendControlPacket: %x %x %x %x %d", bmRequestType, bRequest, wValue, wIndex, wLength);
	mk_setup(setup, bmRequestType, bRequest, wValue, wIndex, wLength); // ps3 tell to send report 1?
	bool fReturn =  queue_Control_Transfer(device, &setup, buf, this);
	//USBHDBGSerial.printf(" return: %u\n", fReturn);
	return fReturn;
}


// This no-inputs parse is meant to be used when we first get the
// HID report descriptor.  It finds all the top level collections
// and allows drivers to claim them.  This is always where we
// learn whether the reports will or will not use a Report ID byte.
void USBHIDParser::parse()
{
	const uint8_t *p = _bigBuffer;
	const uint8_t *end = p + descsize;
	uint16_t usage_page = 0;
	uint16_t usage = 0;
	uint8_t collection_level = 0;
	uint8_t topusage_count = 0;

	use_report_id = false;
	while (p < end) {
		uint8_t tag = *p;
		if (tag == 0xFE) { // Long Item
			p += *p + 3;
			continue;
		}
		uint32_t val = 0;
		switch (tag & 0x03) { // Short Item data
		  case 0: val = 0;
			p++;
			break;
		  case 1: val = p[1];
			p += 2;
			break;
		  case 2: val = p[1] | (p[2] << 8);
			p += 3;
			break;
		  case 3: val = p[1] | (p[2] << 8) | (p[3] << 16) | (p[4] << 24);
			p += 5;
			break;
		}
		if (p > end) break;

		switch (tag & 0xFC) {
		  case 0x84: // Report ID (global)
			use_report_id = true;
			break;
		  case 0x04: // Usage Page (global)
			usage_page = val;
			break;
		  case 0x08: // Usage (local)
			usage = val;
			break;
		  case 0xA0: // Collection
			if (collection_level == 0 && topusage_count < TOPUSAGE_LIST_LEN) {
				uint32_t topusage = ((uint32_t)usage_page << 16) | usage;
				println("Found top level collection ", topusage, HEX);
				//topusage_list[topusage_count] = topusage;
				topusage_drivers[topusage_count] = find_driver(topusage);
				topusage_count++;
			}
			collection_level++;
			usage = 0;
			break;
		  case 0xC0: // End Collection
			if (collection_level > 0) {
				collection_level--;
			}
		  case 0x80: // Input
		  case 0x90: // Output
		  case 0xB0: // Feature
			usage = 0;
			break;
		}
	}
	while (topusage_count < TOPUSAGE_LIST_LEN) {
		//topusage_list[topusage_count] = 0;
		topusage_drivers[topusage_count] = NULL;
		topusage_count++;
	}
}

// This is a list of all the drivers inherited from the USBHIDInput class.
// Unlike the list of USBDriver (managed in enumeration.cpp), drivers stay
// on this list even when they have claimed a top level collection.
USBHIDInput * USBHIDParser::available_hid_drivers_list = NULL;

void USBHIDParser::driver_ready_for_hid_collection(USBHIDInput *driver)
{
	driver->next = NULL;
	if (available_hid_drivers_list == NULL) {
		available_hid_drivers_list = driver;
	} else {
		USBHIDInput *last = available_hid_drivers_list;
		while (last->next) last = last->next;
		last->next = driver;
	}
}

// When a new top level collection is found, this function asks drivers
// if they wish to claim it.  The driver taking ownership of the
// collection is returned, or NULL if no driver wants it.
USBHIDInput * USBHIDParser::find_driver(uint32_t topusage)
{
	println("find_driver");
	USBHIDInput *driver = available_hid_drivers_list;
	hidclaim_t claim_type;
	while (driver) {
		println("  driver ", (uint32_t)driver, HEX);
		if ((claim_type = driver->claim_collection(this, device, topusage)) != CLAIM_NO) {
			if (claim_type == CLAIM_INTERFACE) hid_driver_claimed_control_ = true;
			return driver;
		}
		driver = driver->next;
	}
	println("No Driver claimed topusage: ", topusage, HEX);
	return NULL;
}

// Extract 1 to 32 bits from the data array, starting at bitindex.
static uint32_t bitfield(const uint8_t *data, uint32_t bitindex, uint32_t numbits)
{
	uint32_t output = 0;
	uint32_t bitcount = 0;
	data += (bitindex >> 3);
	uint32_t offset = bitindex & 7;
	if (offset) {
		output = (*data++) >> offset;
		bitcount = 8 - offset;
	}
	while (bitcount < numbits) {
		output |= (uint32_t)(*data++) << bitcount;
		bitcount += 8;
	}
	if (bitcount > numbits && numbits < 32) {
		output &= ((1 << numbits) - 1);
	}
	return output;
}

// convert a number with the specified number of bits from unsigned to signed,
// so the result is a proper 32 bit signed integer.
static int32_t signext(uint32_t num, uint32_t bitcount)
{
	if (bitcount < 32 && bitcount > 0 && (num & (1 << (bitcount-1)))) {
		num |= ~((1 << bitcount) - 1);
	}
	return (int32_t)num;
}

// convert a tag's value to a signed integer.
static int32_t signedval(uint32_t num, uint8_t tag)
{
	tag &= 3;
	if (tag == 1) return (int8_t)num;
	if (tag == 2) return (int16_t)num;
	return (int32_t)num;
}

// parse the report descriptor and use it to feed the fields of the report
// to the drivers which have claimed its top level collections
void USBHIDParser::parse(uint16_t type_and_report_id, const uint8_t *data, uint32_t len)
{
	const uint8_t *p = _bigBuffer;
	const uint8_t *end = p + descsize;
	USBHIDInput *driver = NULL;
	uint32_t topusage = 0;
	uint8_t topusage_index = 0;
	uint8_t collection_level = 0;
	uint16_t usage[USAGE_LIST_LEN] = {0, 0};
	uint8_t usage_count = 0;
	uint8_t usage_min_max_count = 0;
	uint8_t usage_min_max_mask = 0;
	uint8_t report_id = 0;
	uint16_t report_size = 0;
	uint16_t report_count = 0;
	uint16_t usage_page = 0;
	uint32_t last_usage = 0;
	int32_t logical_min = 0;
	int32_t logical_max = 0;
	uint32_t bitindex = 0;

	while (p < end) {
		uint8_t tag = *p;
		if (tag == 0xFE) { // Long Item (unsupported)
			p += p[1] + 3;
			continue;
		}
		uint32_t val = 0;
		switch (tag & 0x03) { // Short Item data
		  case 0: val = 0;
			p++;
			break;
		  case 1: val = p[1];
			p += 2;
			break;
		  case 2: val = p[1] | (p[2] << 8);
			p += 3;
			break;
		  case 3: val = p[1] | (p[2] << 8) | (p[3] << 16) | (p[4] << 24);
			p += 5;
			break;
		}
		if (p > end) break;
		bool reset_local = false;
		switch (tag & 0xFC) {
		  case 0x04: // Usage Page (global)
			usage_page = val;
			break;
		  case 0x14: // Logical Minimum (global)
			logical_min = signedval(val, tag);
			break;
		  case 0x24: // Logical Maximum (global)
			logical_max = signedval(val, tag);
			break;
		  case 0x74: // Report Size (global)
			report_size = val;
			break;
		  case 0x94: // Report Count (global)
			report_count = val;
			break;
		  case 0x84: // Report ID (global)
			report_id = val;
			break;
		  case 0x08: // Usage (local)
			if (usage_count < USAGE_LIST_LEN) {
				// Usages: 0 is reserved 0x1-0x1f is sort of reserved for top level things like
				// 0x1 - Pointer - A collection... So lets try ignoring these
				if (val > 0x1f) {
					usage[usage_count++] = val;
				}
			}
			break;
		  case 0x18: // Usage Minimum (local)
		  	// Note: Found a report with multiple min/max
		  	if (usage_count != 255) {
				usage_count = 255;
			  	usage_min_max_count = 0;
				usage_min_max_mask = 0;
			}
			usage[usage_min_max_count * 2] = val;
			usage_min_max_mask |= 1;
			if (usage_min_max_mask == 3) {
		  		usage_min_max_count++;
				usage_min_max_mask = 0;					
		  	}
			break;
		  case 0x28: // Usage Maximum (local)
		  	if (usage_count != 255) {
				usage_count = 255;
			  	usage_min_max_count = 0;
				usage_min_max_mask = 0;
			}
			usage[usage_min_max_count * 2 + 1] = val;
			usage_min_max_mask |= 2;
			if (usage_min_max_mask == 3) {
		  		usage_min_max_count++;
				usage_min_max_mask = 0;					
		  	}
			break;
		  case 0xA0: // Collection
			if (collection_level == 0) {
				topusage = ((uint32_t)usage_page << 16) | usage[0];
				driver = NULL;
				if (topusage_index < TOPUSAGE_LIST_LEN) {
					driver = topusage_drivers[topusage_index++];
				}
			}
			// discard collection info if not top level, hopefully that's ok?
			collection_level++;
			reset_local = true;
			break;
		  case 0xC0: // End Collection
			if (collection_level > 0) {
				collection_level--;
				if (collection_level == 0 && driver != NULL) {
					driver->hid_input_end();
					driver = NULL;
				}
			}
			reset_local = true;
			break;
		  case 0x80: // Input
			if (use_report_id && (report_id != (type_and_report_id & 0xFF))) {
				// completely ignore and do not advance bitindex
				// for descriptors of other report IDs
				reset_local = true;
				break;
			}
			if ((val & 1) || (driver == NULL)) {
				// skip past constant fields or when no driver is listening
				bitindex += report_count * report_size;
			} else {
				println("begin, usage=", topusage, HEX);
				println("       type= ", val, HEX);
				println("       min=  ", logical_min);
				println("       max=  ", logical_max);
				println("       reportcount=", report_count);
				println("       usage count=", usage_count);
				println("       usage min max count=", usage_min_max_count);

				driver->hid_input_begin(topusage, val, logical_min, logical_max);
				println("Input, total bits=", report_count * report_size);
				if ((val & 2)) {
					// ordinary variable format
					uint32_t uindex = 0;
					uint32_t uindex_max = 0xffff;	// assume no MAX
					bool uminmax = false;
					uint8_t uminmax_index = 0;
					if (usage_count > USAGE_LIST_LEN) {
						// usage numbers by min/max, not from list
						uindex = usage[0];
						uindex_max = usage[1];
						uminmax = true;
					} else if ((report_count > 1) && (usage_count <= 1)) {
						// Special cases:  Either only one or no usages specified and there are more than one 
						// report counts .  
						if (usage_count == 1) {
							uindex = usage[0];
						} else {
							// BUGBUG:: Not sure good place to start?  maybe round up from last usage to next higher group up of 0x100?
							uindex = (last_usage & 0xff00) + 0x100;
						}
						uminmax = true;
					}
					//USBHDBGSerial.printf("TU:%x US:%x %x %d %d: C:%d, %d, MM:%d, %x %x\n", topusage, usage_page, val, logical_min, logical_max, 
					//			report_count, usage_count, uminmax, usage[0], usage[1]);
					for (uint32_t i=0; i < report_count; i++) {
						uint32_t u;
						if (uminmax) {
							u = uindex;
							if (uindex < uindex_max) uindex++;
							else if (uminmax_index < usage_min_max_count) {
								uminmax_index++;
								uindex = usage[uminmax_index * 2];
								uindex_max = usage[uminmax_index * 2 + 1];
								//USBHDBGSerial.printf("$$ next min/max pair: %u %u %u\n", uminmax_index, uindex, uindex_max);
							}
						} else {
							u = usage[uindex++];
							if (uindex >= USAGE_LIST_LEN-1) {
								uindex = USAGE_LIST_LEN-1;
							}
						}
						last_usage = u;	// remember the last one we used... 
						u |= (uint32_t)usage_page << 16;
						print("  usage = ", u, HEX);

						uint32_t n = bitfield(data, bitindex, report_size);
						if (logical_min >= 0) {
							println("  data = ", n);
							driver->hid_input_data(u, n);
						} else {
							int32_t sn = signext(n, report_size);
							println("  sdata = ", sn);
							driver->hid_input_data(u, sn);
						}
						bitindex += report_size;
					}
				} else {
					// array format, each item is a usage number
					// maybe act like the 2 case...
					if (usage_min_max_count && (report_size == 1)) {
						uint32_t uindex = usage[0];
						uint32_t uindex_max = usage[1];
						uint8_t uminmax_index = 0;
						uint32_t u;

						for (uint32_t i=0; i < report_count; i++) {
							u = uindex;
							if (uindex < uindex_max) uindex++;
							else if (uminmax_index < usage_min_max_count) {
								uminmax_index++;
								uindex = usage[uminmax_index * 2];
								uindex_max = usage[uminmax_index * 2 + 1];
								//USBHDBGSerial.printf("$$ next min/max pair: %u %u %u\n", uminmax_index, uindex, uindex_max);
							}

							u |= (uint32_t)usage_page << 16;
							uint32_t n = bitfield(data, bitindex, report_size);
							if (logical_min >= 0) {
								println("  data = ", n);
								driver->hid_input_data(u, n);
							} else {
								int32_t sn = signext(n, report_size);
								println("  sdata = ", sn);
								driver->hid_input_data(u, sn);
							}

							bitindex += report_size;
						}

					} else {
						for (uint32_t i=0; i < report_count; i++) {
							uint32_t u = bitfield(data, bitindex, report_size);
							int n = u;
							if (n >= logical_min && n <= logical_max) {
								u |= (uint32_t)usage_page << 16;
								print("  usage = ", u, HEX);
								println("  data = 1");
								driver->hid_input_data(u, 1);
							} else {
								print ("  usage =", u, HEX);
								print(" out of range: ", logical_min, HEX);
								println(" ", logical_max, HEX);
							}
							bitindex += report_size;
						}
					}
				}
			}
			reset_local = true;
			break;
		  case 0x90: // Output
			// TODO.....
			reset_local = true;
			break;
		  case 0xB0: // Feature
			// TODO.....
			reset_local = true;
			break;

		  case 0x34: // Physical Minimum (global)
		  case 0x44: // Physical Maximum (global)
		  case 0x54: // Unit Exponent (global)
		  case 0x64: // Unit (global)
			break; // Ignore these commonly used tags.  Hopefully not needed?

		  case 0xA4: // Push (yikes! Hope nobody really uses this?!)
		  case 0xB4: // Pop (yikes! Hope nobody really uses this?!)
		  case 0x38: // Designator Index (local)
		  case 0x48: // Designator Minimum (local)
		  case 0x58: // Designator Maximum (local)
		  case 0x78: // String Index (local)
		  case 0x88: // String Minimum (local)
		  case 0x98: // String Maximum (local)
		  case 0xA8: // Delimiter (local)
		  default:
			println("Ruh Roh, unsupported tag, not a good thing Scoob ", tag, HEX);
			break;
		}
		if (reset_local) {
			usage_count = 0;
			usage_min_max_count = 0;
			usage[0] = 0;
			usage[1] = 0;
		}
	}
}

