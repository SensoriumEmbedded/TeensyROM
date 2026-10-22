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
 *
 * Note: special thanks to the Linux kernel for the CH341's method of operation, particularly how the baud rate is encoded.
 */

#include <Arduino.h>
#include "USBHost_t36.h"  // Read this header first for key info

#define print   USBHost::print_
#define println USBHost::println_

//#define ENABLE_DEBUG_PINS

#ifdef ENABLE_DEBUG_PINS
#define debugDigitalToggle(pin)  {digitalWriteFast(pin, !digitalReadFast(pin));}
#define debugDigitalWrite(pin, state) {digitalWriteFast(pin, state);}
#else
#define debugDigitalToggle(pin)  {;}
#define debugDigitalWrite(pin, state) {;}
#endif

/************************************************************/
//  Define mapping VID/PID - to Serial Device type.
/************************************************************/
USBSerialBase::product_vendor_mapping_t USBSerialBase::pid_vid_mapping[] = {
	// FTDI mappings. 
	{0x0403, 0x6001, USBSerialBase::FTDI, 0},
	{0x0403, 0x8088, USBSerialBase::FTDI, 1},  // 2 devices try to claim at interface level
	{0x0403, 0x6010, USBSerialBase::FTDI, 1},  // Also Dual Serial, so claim at interface level

	// PL2303
	{0x67B,0x2303, USBSerialBase::PL2303, 0}, 

	// CH341
	{0x4348, 0x5523, USBSerialBase::CH341, 0},
	{0x1a86, 0x7523, USBSerialBase::CH341, 0 },
	{0x1a86, 0x5523, USBSerialBase::CH341, 0 },

	// Silex CP210...
	{0x10c4, 0xea60, USBSerialBase::CP210X, 0 },
	{0x10c4, 0xea70, USBSerialBase::CP210X, 0 }
};


/************************************************************/
//  Initialization and claiming of devices & interfaces
/************************************************************/

void USBSerialBase::init()
{
	contribute_Pipes(mypipes, sizeof(mypipes)/sizeof(Pipe_t));
	contribute_Transfers(mytransfers, sizeof(mytransfers)/sizeof(Transfer_t));
	contribute_String_Buffers(mystring_bufs, sizeof(mystring_bufs)/sizeof(strbuf_t));
	driver_ready_for_device(this);
	format_ = USBHOST_SERIAL_8N1;
}

bool USBSerialBase::claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len)
{
	print("USBSerial(", _max_rxtx, DEC);
	println(")claim this=", (uint32_t)this, HEX);
	print("vid=", dev->idVendor, HEX);
	print(", pid=", dev->idProduct, HEX);
	print(", bDeviceClass = ", dev->bDeviceClass);
   	print(", bDeviceSubClass = ", dev->bDeviceSubClass);
   	println(", bDeviceProtocol = ", dev->bDeviceProtocol);
	print_hexbytes(descriptors, len);

	//---------------------------------------------------------------------------
	// Lets try to map CDCACM devices only at device level
	if ((dev->bDeviceClass == 2) && (dev->bDeviceSubClass == 0)) {
		if (type != 0) return false;

		// It is a communication device see if we can extract the data... 
		// Try some ttyACM types? 
		// This code may be similar to MIDI code. 
		// But first pass see if we can simply look at the interface...
		// Lets walk through end points and see if we 
		// can find an RX and TX bulk transfer end point.
		// 0  1  2  3  4  5  6  7  8 *9 10  1  2  3 *4  5  6  7 *8  9 20  1  2 *3  4  5  6  7  8  9*30  1  2  3  4  5  6  7  8 *9 40  1  2  3  4  5 *6  7  8  9 50  1  2 
		// USB2AX
		//09 04 00 00 01 02 02 01 00 05 24 00 10 01 04 24 02 06 05 24 06 00 01 07 05 82 03 08 00 FF 09 04 01 00 02 0A 00 00 00 07 05 04 02 10 00 01 07 05 83 02 10 00 01
		//09 04 01 00 02 0A 00 00 00 07 05 04 02 10 00 01 07 05 83 02 10 00 01 
	    // Teensy 3.6
	    //09 04 00 00 01 02 02 01 00 05 24 00 10 01 05 24 01 01 01 04 24 02 06 05 24 06 00 01 07 05 82 03 10 00 40 09 04 01 00 02 0A 00 00 00 07 05 03 02 40 00 00 07 05 84 02 40 00 00  
	    //09 04 01 00 02 0A 00 00 00 07 05 03 02 40 00 00 07 05 84 02 40 00 00 
		const uint8_t *p = descriptors;
		const uint8_t *end = p + len;

		if (p[0] != 9 || p[1] != 4) return false; // interface descriptor
		//println("  bInterfaceClass=", p[5]);
		//println("  bInterfaceSubClass=", p[6]);
		if (p[5] != 2) return false; // bInterfaceClass: 2 Communications
		if (p[6] != 2) return false; // bInterfaceSubClass: 2 serial 
		p += 9;
		println("  Interface is Serial");
		uint8_t rx_ep = 0;
		uint8_t tx_ep = 0;
		uint16_t rx_size = 0;
		uint16_t tx_size = 0;
		interface = 0;	// clear out any interface numbers passed in. 

		while (p < end) {
			len = *p;
			if (len < 4) return false; 
			if (p + len > end) return false; // reject if beyond end of data
			uint32_t type = p[1];
			//println("type: ", type);
			// Unlike Audio, we need to look at Interface as our endpoints are after them...
			if (type == 4 ) { // Interface - lets remember it's number...
				interface = p[2];
				println("    Interface: ", interface);
			}
			else if (type == 0x24) {  // 0x24 = CS_INTERFACE, 
				uint32_t subtype = p[2];
				print("    CS_INTERFACE - subtype: ", subtype);
				if (len >= 4) print(" ", p[3], HEX);
				if (len >= 5) print(" ", p[4], HEX);
				if (len >= 6) print(" ", p[5], HEX);
				switch (subtype) {
					case 0: println(" - Header Functional Descriptor"); break;
					case 1: println(" - Call Management Functional"); break;
					case 2: println(" - Abstract Control Management"); break;
					case 4: println(" - Telephone Ringer"); break;
					case 6: println("  - union Functional"); break;
					default: println(" - ??? other"); break; 
				}
				// First pass ignore...
			} else if (type == 5) {
				// endpoint descriptor
				if (p[0] < 7) return false; // at least 7 bytes
				if (p[3] == 2) {  // First try ignore the first one which is interrupt...
					println("     Endpoint: ", p[2], HEX);
					switch (p[2] & 0xF0) {
					case 0x80:
						// IN endpoint
						if (rx_ep == 0) {
							rx_ep = p[2] & 0x0F;
							rx_size = p[4] | (p[5] << 8);
							println("      rx_size = ", rx_size);
						}
						break;
					case 0x00:
						// OUT endpoint
						if (tx_ep == 0) {
							tx_ep = p[2];
							tx_size = p[4] | (p[5] << 8);
							println("      tx_size = ", tx_size);
						}
						break;
					default:
						println("  invalid end point: ", p[2]);
						return false;
					}
				}
			} else {
				println("  Unknown type: ", type);
				return false; // unknown
			}
			p += len;
		}
		print("  exited loop rx:", rx_ep);
		println(", tx:", tx_ep);
		if (!rx_ep || !tx_ep) return false; 	// did not get our two end points
		if (!init_buffers(rx_size, tx_size)) return false;
		println("  rx buffer size:", rxsize);
		println("  tx buffer size:", txsize);
		rxpipe = new_Pipe(dev, 2, rx_ep & 15, 1, rx_size);
		if (!rxpipe) return false;
		txpipe = new_Pipe(dev, 2, tx_ep, 0, tx_size);
		if (!txpipe) {
			// TODO: free rxpipe
			return false;
		}
		sertype = CDCACM;
		rxpipe->callback_function = rx_callback;
		queue_Data_Transfer(rxpipe, rx1, (rx_size < 64)? rx_size : 64, this);
		rxstate = 1;
		if (rx_size > 128) {
			queue_Data_Transfer(rxpipe, rx2, rx_size, this);
			rxstate = 3;
		}
		txstate = 0;
		txpipe->callback_function = tx_callback;
		//baudrate = 115200;  - use last begins setting or initial value of 115200
		// Wish I could just call Control to do the output... Maybe can defer until the user calls begin()
		// control requires that device is setup which is not until this call completes...
#if 0
		println("Control - CDCACM DTR...");
		// Need to setup  the data the line coding data
		mk_setup(setup, 0x21, 0x22, 3, 0, 0);
		queue_Control_Transfer(dev, &setup, NULL, this);
		control_queued = true;
		pending_control = 0x0;	// Maybe don't need to do...
		dtr_rts_ = 3;
#else
		control_queued = false;
		pending_control = 0x0;	// Maybe don't need to do...
		dtr_rts_ = 0;
#endif		
		return true;
	}

	//---------------------------------------------------------------------------
	// Else lets see if this is a PID/VID we know something about.
	// See if the vendor_id:product_id is in our list of products.
	sertype = UNKNOWN;
	// first see if the VID/PID is equal to the optional parameters specified in the Serial object constructor
	if ((dev->idVendor == _vid_to_claim) && (dev->idProduct == _pid_to_claim)) {
		if (_vid_pid_claim_at_type) {
			println("Serial device wants to map at interface level");
			return false;
		}
		sertype =_vid_pid_sertype;
	} else {
		for (uint8_t i = 0; i < (sizeof(pid_vid_mapping)/sizeof(pid_vid_mapping[0])); i++) {
			if ((dev->idVendor == pid_vid_mapping[i].idVendor) && (dev->idProduct == pid_vid_mapping[i].idProduct)) {
				sertype = pid_vid_mapping[i].sertype;
				if (pid_vid_mapping[i].claim_at_type != type) {
					println("Serial device wants to map at interface level");
					return false;
				}
				break;
			}
		}  
	}
	if (sertype == UNKNOWN) {
		// Not in our list see if CDCACM type...
		// only at the Interface level
		if (type != 1) return false;
		// TTYACM: <Composit device> 
		// 
		// We first tried to claim a simple ttyACM device like a teensy who is configured
		// only as Serial at the device level like what was done for midi
		//
		// However some devices are a compisit of multiple Interfaces, so see if this Interface
		// is of the CDC Interface class and 0 for SubClass and protocol
		// Todo: some of this can maybe be combined with the Whole device code above. 
		
		if (descriptors[0] != 9 || descriptors[1] != 4) return false; // interface descriptor
		if (descriptors[4] < 2) return false; 		// less than 2 end points
		if (descriptors[5] != 0xA) return false; // bInterfaceClass, 0xa = CDC data
		if (descriptors[6] != 0) return false; // bInterfaceSubClass
		if (descriptors[7] != 0) return false; // bInterfaceProtocol
		sertype = CDCACM;
		// Lets see if we can fold in the ACM stuff here...
	}
	// Lets try to locate the end points.  Code is common across these devices
	println("len = ", len);
	uint8_t count_end_points = descriptors[4];
	if (count_end_points < 2) return false; // not enough end points
	if (len < 23) return false;
	if (descriptors[0] != 9) return false; // length 9

	// Lets walk through end points and see if we 
	// can find an RX and TX bulk transfer end point.
	//Example vid=67B, pid=2303
	// 0  1  2  3  4  5  6  7  8  9 10  1  2  3  4  5  6  7  8  9 20  1  2  3  4  5  6  7  8  9
	//09 04 00 00 03 FF 00 00 00 07 05 81 03 0A 00 01 07 05 02 02 40 00 00 07 05 83 02 40 00 00 

	// lets see about FTD2232H
    //09 04 00 00 02 FF FF FF 02 07 05 81 02 00 02 00 07 05 02 02 00 02 00 09 04 01 00 02 FF FF FF 02 07 05 83 02 00 02 00 07 05 04 02 00 02 00 
	//09 04 01 00 02 FF FF FF 02 07 05 83 02 00 02 00 07 05 04 02 00 02 00 


	uint32_t rxep = 0;
	uint32_t txep = 0;
	uint16_t rx_size = 0;
	uint16_t tx_size = 0;
	uint32_t descriptor_index = 9; 
	while (count_end_points-- && ((rxep == 0) || txep == 0)) {
		if (descriptors[descriptor_index] != 7) return false; // length 7
		if (descriptors[descriptor_index+1] != 5) return false; // ep desc
		uint16_t ep_size = descriptors[descriptor_index+4] + (uint16_t)(descriptors[descriptor_index+5] << 8);
		if ((descriptors[descriptor_index+3] == 2) 
			&& (ep_size <= _max_rxtx) && (ep_size >= _min_rxtx)) {
			// have a bulk EP size 
			if (descriptors[descriptor_index+2] & 0x80 ) {
				rxep = descriptors[descriptor_index+2];
				rx_size = ep_size;
			} else {
				txep = descriptors[descriptor_index+2]; 
				tx_size = ep_size;
			}
		}
		descriptor_index += 7;  // setup to look at next one...
	}
	// Try to verify the end points. 
	if (!check_rxtx_ep(rxep, txep)) return false;
	print("USBSerial, rxep=", rxep & 15);
	print("(", rx_size);
	print("), txep=", txep);
	print("(", tx_size);
	println(")");

	if (!init_buffers(rx_size, tx_size)) return false;
	println("  rx buffer size:", rxsize);
	println("  tx buffer size:", txsize);

	rxpipe = new_Pipe(dev, 2, rxep & 15, 1, rx_size);
	if (!rxpipe) return false;
	txpipe = new_Pipe(dev, 2, txep, 0, tx_size);
	if (!txpipe) {
		//free_Pipe(rxpipe);
		return false;
	}
	rxpipe->callback_function = rx_callback;
	queue_Data_Transfer(rxpipe, rx1, rx_size, this);
	rxstate = 1;
	txstate = 0;
	txpipe->callback_function = tx_callback;
	baudrate = 115200;

	// Now do specific setup per type
	switch (sertype) {
	//---------------------------------------------------------------------
	// FTDI
	case FTDI:
		{
			pending_control = 0x0F;
			mk_setup(setup, 0x40, 0, 0, 0, 0); // reset port
			queue_Control_Transfer(dev, &setup, NULL, this);
			control_queued = true;
			return true;
		}
	//------------------------------------------------------------------------
	// Prolific
	// TODO: Note: there are probably more vendor/product pairs.. Maybe should create table of them
	case PL2303: 
		{
			//  First attempt keep it simple... 
			println("PL2303: readRegister(0x04)");
			// Need to setup  the data the line coding data
			mk_setup(setup, 0xC0, 0x1, 0x8484, 0, 1);  
			queue_Control_Transfer(dev, &setup, setupdata, this); 
			control_queued = true;
			setup_state = 1; 	// We are at step one of setup... 
			pending_control = 0x3f;
			return true;
		}
	//------------------------------------------------------------------------
	// CH341
	case CH341:
		{
			println("CH341:  0xC0, 0x5f, 0, 0, 8");
			// Need to setup  the data the line coding data
			mk_setup(setup, 0xC0, 0x5f, 0, 0, sizeof(setupdata));  
			queue_Control_Transfer(dev, &setup, setupdata, this); 
			control_queued = true;
			setup_state = 1; 	// We are at step one of setup... 
			pending_control = 0x7f;	
			return true;
		}
	//------------------------------------------------------------------------
	// CP210X
	case CP210X:
		{
			println("CP210X:  0x41, 0x11, 0, 0, 0 - reset port");
			// Need to setup  the data the line coding data
			mk_setup(setup, 0x41, 0x11, 0, 0, 0);  
			queue_Control_Transfer(dev, &setup, NULL, this); 
			control_queued = true;
			setup_state = 1; 	// We are at step one of setup... 
			pending_control = 0xf;	
			return true;
		}
	case CDCACM:
		{
			println("Control - CDCACM LINE_CODING");
			setupdata[0] = 0;  // Setup baud rate 115200 - 0x1C200
			setupdata[1] = 0xc2;
			setupdata[2] = 0x1;
			setupdata[3] = 0;
		    setupdata[4] = 0; // 0 - 1 stop bit, 1 - 1.5 stop bits, 2 - 2 stop bits
		    setupdata[5] = 0; // 0 - None, 1 - Odd, 2 - Even, 3 - Mark, 4 - Space
		    setupdata[6] = 8; // Data bits (5, 6, 7, 8 or 16)
			mk_setup(setup, 0x21, 0x20, 0, 0, 7);
			queue_Control_Transfer(dev, &setup, setupdata, this);
			pending_control = 0x04;	// Maybe don't need to do...
			control_queued = true;
			return true;
		}		
	//------------------------------------------------------------------------
	// PID:VID - not in our product list. 
	default:
		break;
	}
	return false;
}

// check if two legal endpoints, 1 receive & 1 transmit
bool USBSerialBase::check_rxtx_ep(uint32_t &rxep, uint32_t &txep)
{
	if ((rxep & 0x0F) == 0) return false;
	if ((txep & 0x0F) == 0) return false;
	uint32_t rxdir = rxep & 0xF0;
	uint32_t txdir = txep & 0xF0;
	if (rxdir == 0x80 && txdir == 0x00) {
		return true;
	}
	if (rxdir == 0x00 && txdir == 0x80) {
		std::swap(rxep, txep);
		return true;
	}
	return false;
}

// initialize buffer sizes and pointers
bool USBSerialBase::init_buffers(uint32_t rsize, uint32_t tsize)
{
	// buffer must be able to hold 2 of each packet, plus buffer
	// space to hold RX and TX data. 
	if (_big_buffer_size < (rsize + tsize) * 3 + 2) return false;
	rx1 = (uint8_t *)_bigBuffer;
	rx2 = rx1 + rsize;
	tx1 = rx2 + rsize;
	tx2 = tx1 + tsize;
	rxbuf = tx2 + tsize;
	// FIXME: this assume 50-50 split - not true when rsize != tsize
	rxsize = (_big_buffer_size - (rsize + tsize) * 2) / 2;
	txsize = rxsize;
	txbuf = rxbuf + rxsize;
	rxhead = 0;
	rxtail = 0;
	txhead = 0;
	txtail = 0;
	rxstate = 0;
	return true;
}

void USBSerialBase::disconnect()
{
}



void USBSerialBase::control(const Transfer_t *transfer)
{
	println("control callback (serial) ", pending_control, HEX);
	//Serial.printf("USerial control:callback %x %u\n", pending_control, transfer->length);

	control_queued = false;

	// We will split this up by Serial type, maybe different functions? 

	//-------------------------------------------------------------------------
	// First FTDI
	if (sertype == FTDI) {
		if (pending_control & 1) {
			pending_control &= ~1;
			// set data format
			uint16_t ftdi_format = format_ & 0xf;	// This should give us the number of bits.

			// now lets extract the parity from our encoding
			ftdi_format |= (format_ & 0xe0) << 3;	// they encode bits 9-11

			// See if two stop bits
			if (format_ & 0x100) ftdi_format |= (0x2 << 11);

			mk_setup(setup, 0x40, 4, ftdi_format, 0, 0); // data format 8N1
			queue_Control_Transfer(device, &setup, NULL, this);
			control_queued = true;
			return;
		}
		// set baud rate
		if (pending_control & 2) {
			pending_control &= ~2;
			uint32_t baudval = 3000000 / baudrate;
			mk_setup(setup, 0x40, 3, baudval, 0, 0);
			queue_Control_Transfer(device, &setup, NULL, this);
			control_queued = true;
			return;
		}
		// configure flow control
		if (pending_control & 4) {
			pending_control &= ~4;
			mk_setup(setup, 0x40, 2, 0, 1, 0);
			queue_Control_Transfer(device, &setup, NULL, this);
			control_queued = true; 
			return;
		}
		// set DTR
		if (pending_control & 8) {
			pending_control &= ~8;
			mk_setup(setup, 0x40, 1, 0x0101, 0, 0);
			queue_Control_Transfer(device, &setup, NULL, this);
			control_queued = true;
			dtr_rts_ = 1;
			return;
		}
		// clear DTR
		if (pending_control & 0x80) {
			pending_control &= ~0x80;
			println("FTDI clear DTR");
			mk_setup(setup, 0x40, 1, 0x0100, 0, 0);
			queue_Control_Transfer(device, &setup, NULL, this);
			control_queued = true;
			dtr_rts_ = 0;
			return;
		}

	}

	//-------------------------------------------------------------------------
	// Now CDCACM
	if (sertype == CDCACM) {
		if (pending_control & 2) {
			pending_control &= ~2;
			// Should probably use data structure, but that may depend on byte ordering...
			setupdata[0] = (baudrate) & 0xff;  // Setup baud rate 115200 - 0x1C200
			setupdata[1] = (baudrate >> 8) & 0xff;
			setupdata[2] = (baudrate >> 16) & 0xff;
			setupdata[3] = (baudrate >> 24) & 0xff;
	        setupdata[4] = (format_ & 0x100)? 2 : 0; 	// 0 - 1 stop bit, 1 - 1.5 stop bits, 2 - 2 stop bits
	        setupdata[5] = (format_ & 0xe0) >> 5; 		// 0 - None, 1 - Odd, 2 - Even, 3 - Mark, 4 - Space
	        setupdata[6] = format_ & 0x1f;				// Data bits (5, 6, 7, 8 or 16)
	        print("CDCACM setup: ");
	        print_hexbytes(&setupdata, 7);
			mk_setup(setup, 0x21, 0x20, 0, 0, 7);
			queue_Control_Transfer(device, &setup, setupdata, this);
			control_queued = true;
			return;
		}
		// configure flow control
		if (pending_control & 4) {
			pending_control &= ~4;
			println("Control - 0x21,0x22, 0x3");
			// Need to setup  the data the line coding data
			mk_setup(setup, 0x21, 0x22, 3, 0, 0);
			queue_Control_Transfer(device, &setup, NULL, this);
			dtr_rts_ = 3;
			control_queued = true;
			return;
		}
		if (pending_control & 0x80) {
			pending_control &= ~0x80;
			println("Control - 0x21,0x22, 0x0 - clear DTR");
			// Need to setup  the data the line coding data
			mk_setup(setup, 0x21, 0x22, 0, 0, 0);
			queue_Control_Transfer(device, &setup, NULL, this);
			dtr_rts_ = 0;
			control_queued = true;
			return;
		}
	}

	//-------------------------------------------------------------------------
	// Now PL2303 - Which appears to be a little more complicated
	if (sertype == PL2303) {
		if (pending_control & 1) {
			// Still in larger setup state mode
			switch (setup_state) {
				case 1:
					println("PL2303: writeRegister(0x04, 0x00)");
					mk_setup(setup, 0x40, 1, 0x0404, 0, 0); // 
					queue_Control_Transfer(device, &setup, NULL, this);
					setup_state = 2; 
					control_queued = true;
					return;
				case 2:
					println("PL2303: readRegister(0x04)");
					mk_setup(setup, 0xC0, 0x1, 0x8484, 0, 1);  
					queue_Control_Transfer(device, &setup, setupdata, this); 
					control_queued = true;
					setup_state = 3; 
					return;
				case 3:
					println("PL2303: v1 = readRegister(0x03)");
					mk_setup(setup, 0xC0, 0x1, 0x8383, 0, 1);  
					queue_Control_Transfer(device, &setup, setupdata, this); 
					control_queued = true;
					setup_state = 4; 
					return;
				case 4:
					println("PL2303: readRegister(0x04)");
					// Do we need this value long term or we could just leave in setup data? 
					pl2303_v1 = setupdata[0];	// save the first bye of version
					mk_setup(setup, 0xC0, 0x1, 0x8484, 0, 1);  
					queue_Control_Transfer(device, &setup, setupdata, this); 
					control_queued = true;
					setup_state = 5; 
					return;
				case 5:
					println("PL2303: writeRegister(0x04, 0x01)");
					mk_setup(setup, 0x40, 1, 0x0404, 1, 0); // 
					queue_Control_Transfer(device, &setup, NULL, this);
					setup_state = 6; 
					control_queued = true;
					return;
				case 6:
					println("PL2303: readRegister(0x04)");
					mk_setup(setup, 0xC0, 0x1, 0x8484, 0, 1);  
					queue_Control_Transfer(device, &setup, setupdata, this); 
					control_queued = true;
					setup_state = 7; 
					return;
				case 7:
					println("PL2303: v2 = readRegister(0x03)");
					mk_setup(setup, 0xC0, 0x1, 0x8383, 0, 1);  
					queue_Control_Transfer(device, &setup, setupdata, this); 
					control_queued = true;
					setup_state = 8; 
					return;
				case 8:
					pl2303_v2 = setupdata[0];	// save the first bye of version
					print(" PL2303 Version ", pl2303_v1, HEX);
					println(":", pl2303_v2, HEX);
					println("PL2303: writeRegister(0, 1)");
					mk_setup(setup, 0x40, 1, 0, 1, 0); // 
					queue_Control_Transfer(device, &setup, NULL, this);
					setup_state = 9; 
					control_queued = true;
					return;
				case 9:
					println("PL2303: writeRegister(1, 0)");
					mk_setup(setup, 0x40, 1, 1, 0, 0); // 
					queue_Control_Transfer(device, &setup, NULL, this);
					setup_state = 10; 
					control_queued = true;
					return;
				case 10:
					// lets try legacy..
					println("PL2303: writeRegister(2, 24)");
					mk_setup(setup, 0x40, 1, 2, 0x24, 0); // 
					//println("PL2303: writeRegister(2, 44)");
					//mk_setup(setup, 0x40, 1, 2, 0x44, 0); // 
					queue_Control_Transfer(device, &setup, NULL, this);
					setup_state = 11; 
					control_queued = true;
					return;
				case 11:
					println("PL2303: writeRegister(8, 0)");
					mk_setup(setup, 0x40, 1, 8, 0, 0); // 
					queue_Control_Transfer(device, &setup, NULL, this);
					setup_state = 12; 
					control_queued = true;
					return;
				case 12:
					println("PL2303: writeRegister(9, 0)");
					mk_setup(setup, 0x40, 1, 9, 0, 0); // 
					queue_Control_Transfer(device, &setup, NULL, this);
					setup_state = 13; 
					control_queued = true;
					return;
				case 13:
					println("PL2303: Read current Baud/control");
					mk_setup(setup, 0xA1, 0x21, 0, 0, 7);
					queue_Control_Transfer(device, &setup, setupdata, this);
					control_queued = true;
					setup_state = 99; // we are done
					break;
			}
			pending_control &= ~1;  // We are finally going to leave this list and join the rest
			if (control_queued) return;
		}

		// set baud rate
		if (pending_control & 2) {
			pending_control &= ~2;
			// See what the read returned earlier
			print("PL2303: Returned configuration data: ");
			print_hexbytes(setupdata, 7);

			// Should probably use data structure, but that may depend on byte ordering...
			setupdata[0] = (baudrate) & 0xff;  // Setup baud rate 115200 - 0x1C200
			setupdata[1] = (baudrate >> 8) & 0xff;
			setupdata[2] = (baudrate >> 16) & 0xff;
			setupdata[3] = (baudrate >> 24) & 0xff;
	        setupdata[4] = (format_ & 0x100)? 2 : 0; 	// 0 - 1 stop bit, 1 - 1.5 stop bits, 2 - 2 stop bits
	        setupdata[5] = (format_ & 0xe0) >> 5; 		// 0 - None, 1 - Odd, 2 - Even, 3 - Mark, 4 - Space
	        setupdata[6] = format_ & 0x1f;				// Data bits (5, 6, 7, 8 or 16)
	        print("PL2303: Set baud/control: ", baudrate, HEX);
	        print(" = ");
	        print_hexbytes(&setupdata, 7);
			mk_setup(setup, 0x21, 0x20, 0, 0, 7);
			queue_Control_Transfer(device, &setup, setupdata, this);
			control_queued = true;
			return;
		}
		if (pending_control & 4) {
			pending_control &= ~4;
			println("PL2303: writeRegister(0, 0)");
			mk_setup(setup, 0x40, 1, 0, 0, 0); // 
			queue_Control_Transfer(device, &setup, NULL, this);
			control_queued = true;
			return; 
		}
		if (pending_control & 8) {
			pending_control &= ~8;
			println("PL2303: Read current Baud/control");
			memset(setupdata, 0, sizeof(setupdata));	// clear it to see if we read it...
			mk_setup(setup, 0xA1, 0x21, 0, 0, 7);
			queue_Control_Transfer(device, &setup, setupdata, this);
			control_queued = true;
			return;
		}
		if (pending_control & 0x10) {
			pending_control &= ~0x10;
			print("PL2303: Returned configuration data: ");
			print_hexbytes(setupdata, 7);

			// This sets the control lines (0x1=DTR, 0x2=RTS)
			println("PL2303: 0x21, 0x22, 0x3");
			mk_setup(setup, 0x21, 0x22, 3, 0, 0); // 
			queue_Control_Transfer(device, &setup, NULL, this);
			dtr_rts_ = 3;
			control_queued = true;
			return;
		}
		if (pending_control & 0x20) {
			pending_control &= ~0x20;
			println("PL2303: 0x21, 0x22, 0x3");
			mk_setup(setup, 0x21, 0x22, 3, 0, 0); // 
			queue_Control_Transfer(device, &setup, NULL, this);
			control_queued = true;
			return;
		}
		if (pending_control & 0x80) {
			pending_control &= ~0x80;
			println("PL2303: 0x21, 0x22, 0x0");  // Clear DTR/RTS
			mk_setup(setup, 0x21, 0x22, 0, 0, 0); // 
			queue_Control_Transfer(device, &setup, NULL, this);
			dtr_rts_ = 0;
			control_queued = true;
			return;
		}
	}

	if (sertype == CH341) {
#if 0
		print("  Transfer: ");
		print_hexbytes(&transfer->setup, sizeof(setup_t));
		if (transfer->length) {
			print("  data: ");
			print_hexbytes(transfer->buffer, transfer->length);
		}
#endif
		if (pending_control & 1) {
			// Still in larger setup state mode
			switch (setup_state) {
				case 1:
					print("  Returned: ");
					print_hexbytes(transfer->buffer, transfer->length);
					println("CH341: 40, a1, 0, 0, 0");
					mk_setup(setup, 0x40, 0xa1, 0, 0, 0); // 
					queue_Control_Transfer(device, &setup, NULL, this);
					setup_state = 2; 
					control_queued = true;
					return;
				case 2:
					ch341_setBaud(0);	// send the first byte of the baud rate
					control_queued = true;
					setup_state = 3;
					return;
				case 3:
					ch341_setBaud(1);	// send the second byte of the baud rate
					control_queued = true;
					setup_state = 4;
					return;
				case 4:
					println("CH341: c0, 95, 2518, 0, 8");
					mk_setup(setup, 0xc0, 0x95, 0x2518, 0, sizeof(setup)); // 
					queue_Control_Transfer(device, &setup, setupdata, this);
					setup_state = 5; 
					control_queued = true;
					return;
				case 5:
					print("  Returned: ");
					print_hexbytes(transfer->buffer, transfer->length);
					println("CH341: 40, 0x9a, 0x2518, 0x0050, 0");
					mk_setup(setup, 0x40, 0x9a, 0x2518, 0x0050, 0); // 
					queue_Control_Transfer(device, &setup, NULL, this);
					setup_state = 6; 
					control_queued = true;
					return;
				case 6:
					println("CH341: c0, 95, 0x706, 0, 8 - get status");
					mk_setup(setup, 0xc0, 0x95, 0x706, 0, sizeof(setup)); // 
					queue_Control_Transfer(device, &setup, setupdata, this);
					setup_state = 7; 
					control_queued = true;
					return;
				case 7:
					print("  Returned: ");
					print_hexbytes(transfer->buffer, transfer->length);
					println("CH341: 40, 0xa1, 0x501f, 0xd90a, 0");
					mk_setup(setup, 0x40, 0xa1, 0x501f, 0xd90a, 0); // 
					queue_Control_Transfer(device, &setup, NULL, this);
					setup_state = 8; 
					control_queued = true;
					break;
			}
			pending_control &= ~1;  // We are finally going to leave this list and join the rest
			if (control_queued) return;
		}
		// set baud rate
		if (pending_control & 2) {
			pending_control &= ~2;
			ch341_setBaud(0);	// send the first byte of the baud rate
			control_queued = true;
			return;
		}
		if (pending_control & 4) {
			pending_control &= ~4;
			ch341_setBaud(1);	// send the first byte of the baud rate
			control_queued = true;
			return;
		}

		if (pending_control & 8) {
			pending_control &= ~8;
			uint16_t ch341_format;
			switch (format_) {
				default:
				// These values were observed when used on PC... Need to flush out others. 
				case USBHOST_SERIAL_8N1: ch341_format = 0xc3; break;
				case USBHOST_SERIAL_7E1: ch341_format = 0xda; break;
				case USBHOST_SERIAL_7O1: ch341_format = 0xca; break;
				case USBHOST_SERIAL_8N2: ch341_format = 0xc7; break;
			}
			println("CH341: 40, 0x9a, 0x2518: ", ch341_format, HEX);
			mk_setup(setup, 0x40, 0x9a, 0x2518, ch341_format, 0); // 
			queue_Control_Transfer(device, &setup, NULL, this);
			control_queued = true;
			return;
		}
		if (pending_control & 0x10) {
			pending_control &= ~0x10;
			// This is setting handshake need to figure out what...
			// 0x20=DTR, 0x40=RTS send ~ of values. 
			println("CH341: 0x40, 0xa4, 0xff9f, 0, 0 - Handshake");
			mk_setup(setup, 0x40, 0xa4, 0xff9f, 0, 0); // 
			queue_Control_Transfer(device, &setup, NULL, this);
			control_queued = true;
			return;
		}
		if (pending_control & 0x20) {
			pending_control &= ~0x20;
			// This is setting handshake need to figure out what...
			println("CH341: c0, 95, 0x706, 0, 8 - get status");
			mk_setup(setup, 0xc0, 0x95, 0x706, 0, sizeof(setup)); // 
			queue_Control_Transfer(device, &setup, setupdata, this);
			control_queued = true;
			return;
		}
		if (pending_control & 0x40) {
			pending_control &= ~0x40;
			print("  Returned: ");
			print_hexbytes(transfer->buffer, transfer->length);
			println("CH341: 0x40, 0x9a, 0x2727, 0, 0");
			mk_setup(setup, 0x40, 0x9a, 0x2727, 0, 0); // 
			queue_Control_Transfer(device, &setup, NULL, this);

			return;
		}

		if (pending_control & 0x80) {
			pending_control &= ~0x80;
			println("CH341: 0x40, 0xa4, 0xffff, 0, 0 - Handshake");
			mk_setup(setup, 0x40, 0xa4, 0xffff, 0, 0); // 
			queue_Control_Transfer(device, &setup, NULL, this);
			control_queued = true;
			return;
		}
	}
	//-------------------------------------------------------------------------
	// First CP210X
	if (sertype == CP210X) {
		if (pending_control & 1) {
			pending_control &= ~1;
			// set data format
			uint16_t cp210x_format = (format_ & 0xf) << 8;	// This should give us the number of bits.

			// now lets extract the parity from our encoding bits 5-7 and in theres 4-7
			cp210x_format |= (format_ & 0xe0) >> 1;	// they encode bits 9-11

			// See if two stop bits
			if (format_ & 0x100) cp210x_format |= 2;

			mk_setup(setup, 0x41, 3, cp210x_format, 0, 0); // data format 8N1
			println("CP210x setup, 0x41, 3, cp210x_format ",cp210x_format, HEX);
			queue_Control_Transfer(device, &setup, NULL, this);
			control_queued = true;
			return;
		}
		// set baud rate
		if (pending_control & 2) {
			pending_control &= ~2;
			setupdata[0] = (baudrate) & 0xff;  // Setup baud rate 115200 - 0x1C200
			setupdata[1] = (baudrate >> 8) & 0xff;
			setupdata[2] = (baudrate >> 16) & 0xff;
			setupdata[3] = (baudrate >> 24) & 0xff;
			mk_setup(setup, 0x40, 0x1e, 0, 0, 4);
			println("CP210x Set Baud  0x40, 0x1e");
			queue_Control_Transfer(device, &setup, setupdata, this);
			control_queued = true;
			return;
		}
		// Appears to be an enable command
		if (pending_control & 4) {
			pending_control &= ~4;
			memset(setupdata, 0, sizeof(setupdata));	// clear out the data
			println("CP210x 0x41, 0, 1");
			mk_setup(setup, 0x41, 0, 1, 0, 0);
			queue_Control_Transfer(device, &setup, NULL, this);
			control_queued = true; 
			return;
		}

		// MHS_REQUEST
		if (pending_control & 8) {
			pending_control &= ~0x88;
			mk_setup(setup, 0x41, 7, 0x0303, 0, 0);
			queue_Control_Transfer(device, &setup, NULL, this);
			control_queued = true;
			println("CP210x 0x41, 7, 0x0303");
			dtr_rts_ = 3;
			return;
		}

		if (pending_control & 0x80) {
			pending_control &= ~0x80;
			println("CP210x 0x41, 7, 0x0300");
			// clear dtr
			mk_setup(setup, 0x41, 7, 0x0300, 0, 0);
			queue_Control_Transfer(device, &setup, NULL, this);
			control_queued = true;
			dtr_rts_ = 0;
			return;
		}
	}
}

#define CH341_BAUDBASE_FACTOR 1532620800
#define CH341_BAUDBASE_DIVMAX 3
void USBSerialBase::ch341_setBaud(uint8_t byte_index) {
	if (byte_index == 0) {
		uint32_t factor;
		uint16_t divisor;

		factor = (CH341_BAUDBASE_FACTOR / baudrate);
		divisor = CH341_BAUDBASE_DIVMAX;

		while ((factor > 0xfff0) && divisor) {
			factor >>= 3;
			divisor--;
		}

		factor = 0x10000 - factor;

		factor = (factor & 0xff00) | divisor;
		setupdata[0] = factor & 0xff;  // save away the low byte for 2nd message


		println("CH341: 40, 0x9a, 0x1312... (Baud word 0):", factor, HEX);
		mk_setup(setup, 0x40, 0x9a, 0x1312, factor, 0); // 
	} else {
		// Second packet use the byte we saved away during the calculation above
		println("CH341: 40, 0x9a, 0x0f2c... (Baud word 1):", setupdata[0], HEX);
		mk_setup(setup, 0x40, 0x9a, 0x0f2c, setupdata[0], 0); // 
	}
	queue_Control_Transfer(device, &setup, setupdata, this);
	control_queued = true;
}


/************************************************************/
//  Interrupt-based Data Movement
/************************************************************/

void USBSerialBase::rx_callback(const Transfer_t *transfer)
{
	if (!transfer->driver) return;
	((USBSerial *)(transfer->driver))->rx_data(transfer);
}

void USBSerialBase::tx_callback(const Transfer_t *transfer)
{
	if (!transfer->driver) return;
	((USBSerial *)(transfer->driver))->tx_data(transfer);
}


void USBSerialBase::rx_data(const Transfer_t *transfer)
{
	uint32_t len = transfer->length - ((transfer->qtd.token >> 16) & 0x7FFF);

	debugDigitalToggle(6);
	// first update rxstate bitmask, since buffer is no longer queued
	if (transfer->buffer == rx1) {
		rxstate &= 0xFE;
	} else if (transfer->buffer == rx2) {
		rxstate &= 0xFD;
	}
	// get start of data and actual length
	const uint8_t *p = (const uint8_t *)transfer->buffer;
	if (sertype == FTDI) {
		if (len >= 2) {
			p += 2;
			len -= 2;
		} else {
			len = 0;
		}
	}
	if (len > 0) {
		//Serial.printf("rx token: %x TL:%u len:%u rx: ",  transfer->qtd.token, transfer->length, len);
		//for (uint16_t i = 0; i < len; i++) Serial.printf(" %02x", p[i]);
		//Serial.printf("\n");
		print("rx token: ", transfer->qtd.token, HEX);
		print(" transfer length: ", transfer->length, DEC);
		print(" len:", len, DEC);
		print(" - ", *p, HEX);
		println(" ", *(p+1), HEX);
		print("rx: ");
		print_hexbytes(p, len);
	}
	// Copy data from packet buffer to circular buffer.
	// Assume the buffer will always have space, since we
	// check before queuing the buffers
	uint32_t head = rxhead;
	uint32_t tail = rxtail;
	if (++head >= rxsize) head = 0;
	uint32_t avail;
	if (len > 0) {
		//print("head=", head);
		//print(", tail=", tail);
		avail = rxsize - head;
		//print(", avail=", avail);
		//println(", rxsize=", rxsize);
		if (avail > len) avail = len;
		memcpy(rxbuf + head, p, avail);
		if (len <= avail) {
			head += avail - 1;
			if (head >= rxsize) head = 0;
		} else {
			head = len - avail - 1;
			memcpy(rxbuf, p + avail, head + 1);
		}
		rxhead = head;
	}
	// TODO: can be this more efficient?  We know from above which
	// buffer is no longer queued, so possible skip most of this work?
	rx_queue_packets(head, tail);
}

// re-queue packet buffer(s) if possible
void USBSerialBase::rx_queue_packets(uint32_t head, uint32_t tail)
{
	uint32_t avail;
	if (head >= tail) {
		avail = rxsize - 1 - head + tail;
	} else {
		avail = tail - head - 1;
	}
	uint32_t packetsize = rx2 - rx1;
	if (avail >= packetsize) {
		if ((rxstate & 0x01) == 0) {
			queue_Data_Transfer(rxpipe, rx1, packetsize, this);
			rxstate |= 0x01;
		} else if ((rxstate & 0x02) == 0) {
			queue_Data_Transfer(rxpipe, rx2, packetsize, this);
			rxstate |= 0x02;
		}
		if ((rxstate & 0x03) != 0x03 && avail >= packetsize * 2) {
			if ((rxstate & 0x01) == 0) {
				queue_Data_Transfer(rxpipe, rx1, packetsize, this);
				rxstate |= 0x01;
			} else if ((rxstate & 0x02) == 0) {
				queue_Data_Transfer(rxpipe, rx2, packetsize, this);
				rxstate |= 0x02;
			}
		}
	}
}

void USBSerialBase::tx_data(const Transfer_t *transfer)
{
	uint32_t mask;
	uint8_t *p = (uint8_t *)transfer->buffer;
	debugDigitalWrite(5, HIGH);
	if (p == tx1) {
		println("tx1:");
		mask = 1;
		//txstate &= 0xFE;
	} else if (p == tx2) {
		println("tx2:");
		mask = 2;
		//txstate &= 0xFD;
	} else {
		debugDigitalWrite(5, LOW);
		return; // should never happen
	}
	// check how much more data remains in the transmit buffer
	uint32_t head = txhead;
	uint32_t tail = txtail;
	uint32_t count;
	if (head >= tail) {
		count = head - tail;
	} else {
		count = txsize + head - tail;
	}
	uint32_t packetsize = tx2 - tx1;
	// Only output full packets unless the flush bit was set.
	if ((count == 0) || ((count < packetsize) && ((txstate & 0x4) == 0) )) {
		// not enough data in buffer to fill a full packet
		txstate &= ~(mask | 4);	// turn off that transfer and make sure the flush bit is not set
		debugDigitalWrite(5, LOW);
		return;
	}
	// immediately transmit another full packet, if we have enough data
	if (count >= packetsize) count = packetsize;
	else txstate &= ~(mask | 4); // This packet will complete any outstanding flush

	println("TX:moar data!!!!");
	if (++tail >= txsize) tail = 0;
	uint32_t n = txsize - tail;
	if (n > count) n = count;
	memcpy(p, txbuf + tail, n);
	if (n >= count) {
		tail += n - 1;
		if (tail >= txsize) tail = 0;
	} else {
		uint32_t len = count - n;
		memcpy(p + n, txbuf, len);
		tail = len - 1;
	}
	txtail = tail;
	queue_Data_Transfer(txpipe, p, count, this);
	debugDigitalWrite(5, LOW);
}

void USBSerialBase::flush()
{
	print("USBSerialBase::flush");
 	if (txhead == txtail) {
 		println(" - Empty");
 		return;  // empty.
 	}
 	debugDigitalWrite(32, HIGH);
	NVIC_DISABLE_IRQ(IRQ_USBHS);
	txtimer.stop();  		// Stop longer timer.
	txtimer.start(100);		// Start a mimimal timeout
//	timer_event(nullptr);   // Try calling direct - fails to work 
	NVIC_ENABLE_IRQ(IRQ_USBHS);
	while (txstate & 3) ; // wait for all of the USB packets to be sent. 
	println(" completed");
 	debugDigitalWrite(32, LOW);
}



void USBSerialBase::timer_event(USBDriverTimer *whichTimer)
{
	debugDigitalWrite(7, HIGH);
	println("txtimer");
	uint32_t count;
	uint32_t head = txhead;
	uint32_t tail = txtail;
	if (head == tail) {
		println("  *** Empty ***");
		debugDigitalWrite(7, LOW);
		return; // nothing to transmit
	} else if (head > tail) {
		count = head - tail;
	} else {
		count = txsize + head - tail;
	}

	uint8_t *p;
	if ((txstate & 0x01) == 0) {
		p = tx1;
		txstate |= 0x01;
	} else if ((txstate & 0x02) == 0) {
		p = tx2;
		txstate |= 0x02;
	} else {
		txstate |= 4; 	// Tell the TX code to do flush code. 
		println(" *** No buffers ***");
		debugDigitalWrite(7, LOW);
		return; // no outgoing buffers available, try again later
	}

	uint32_t packetsize = tx2 - tx1;

	// Possible for remaining ? packet size and not have both? 
	if (count > packetsize) {
		txstate |= 4;	// One of the active transfers will handle the remaining parts
		count = packetsize;
	}

	if (++tail >= txsize) tail = 0;
	uint32_t n = txsize - tail;
	if (n > count) n = count;
	memcpy(p, txbuf + tail, n);
	if (n >= count) {
		tail += n - 1;
		if (tail >= txsize) tail = 0;
	} else {
		uint32_t len = count - n;
		memcpy(p + n, txbuf, len);
		tail = len - 1;
	}
	txtail = tail;
	print("  TX data (", count);
	print(") ");
	print_hexbytes(p, count);
	queue_Data_Transfer(txpipe, p, count, this);
	debugDigitalWrite(7, LOW);
}



/************************************************************/
//  User Functions - must disable USBHQ IRQ for EHCI access
/************************************************************/

void USBSerialBase::begin(uint32_t baud, uint32_t format)
{
	NVIC_DISABLE_IRQ(IRQ_USBHS);
	baudrate = baud;
	bool format_changed = format != format_;
	format_ = format; 
	switch (sertype) {
		default:
		case CDCACM: pending_control |= 0x6; break;
		case FTDI: pending_control |= (format_changed? 0xf : 0xe); break;	// Set BAUD, FLOW, DTR
		case PL2303: pending_control |= 0x1e; break;  // set more stuff...
		case CH341: pending_control |= 0x1e; break;
		case CP210X: pending_control |= 0xf; break;
	}
	if (!control_queued) control(NULL);
	NVIC_ENABLE_IRQ(IRQ_USBHS);
	// Wait until all packets have been queued before we return to caller. 
	elapsedMillis em; 
	while (pending_control && (em < 5000)) {
		yield();	// not sure if we want to yield or what? 
	}
	if (pending_control) {
		println("USBSerialBase::begin timeout", pending_control, HEX);
	}

}

void USBSerialBase::end(void)
{
	NVIC_DISABLE_IRQ(IRQ_USBHS);
	switch (sertype) {
		default:
		case CDCACM: pending_control |= 0x80; break;
		case FTDI: pending_control |= 0x80; break;	// clear DTR
		case PL2303: pending_control |= 0x80; break;
		case CH341: pending_control |= 0x80; break;
	}
	if (!control_queued) control(NULL);
	NVIC_ENABLE_IRQ(IRQ_USBHS);

	// Wait until all packets have been queued before we return to caller. 
	elapsedMillis em;
	while (pending_control && em < 1000) {
		yield();	// not sure if we want to yield or what? 
	}
	if (pending_control) {
		println("USBSerialBase::end timeout", pending_control, HEX);
	}
}

int USBSerialBase::available(void)
{
	if (!device) return 0;
	uint32_t head = rxhead;
	uint32_t tail = rxtail;
	if (head >= tail) return head - tail;
	return rxsize + head - tail;
}

int USBSerialBase::peek(void)
{
	if (!device) return -1;
	if (rxhead == rxtail) return -1;
	uint16_t tail = rxtail + 1;
	if (tail >= rxsize) tail = 0;
	return rxbuf[tail];
}

int USBSerialBase::read(void)
{
	if (!device) return -1;
	if (rxhead == rxtail) return -1;
	if (++rxtail >= rxsize) rxtail = 0;
	int c = rxbuf[rxtail];
	if ((rxstate & 0x03) != 0x03) {
		NVIC_DISABLE_IRQ(IRQ_USBHS);
		rx_queue_packets(rxhead, rxtail);
		NVIC_ENABLE_IRQ(IRQ_USBHS);
	}
	return c;
}

int USBSerialBase::availableForWrite()
{
	if (!device) return 0;
	uint32_t head = txhead;
	uint32_t tail = txtail;
	if (head >= tail) return txsize - 1 - head + tail;
	return tail - head - 1;
}

size_t USBSerialBase::write(uint8_t c)
{
	if (!device) return 0;
	uint32_t head = txhead;
	if (++head >= txsize) head = 0;
	while (txtail == head) {
		// wait...
	}
	txbuf[head] = c;
	txhead = head;
	//print("head=", head);
	//println(", tail=", txtail);

	// if full packet in buffer and tx packet ready, queue it
	NVIC_DISABLE_IRQ(IRQ_USBHS);
	uint32_t tail = txtail;
	if ((txstate & 0x03) != 0x03) {
		// at least one packet buffer is ready to transmit
		uint32_t count;
		if (head >= tail) {
			count = head - tail;
		} else {
			count = txsize + head - tail;
		}
		uint32_t packetsize = tx2 - tx1;
		if (count >= packetsize) {
			//println("txsize=", txsize);
			uint8_t *p;
			if ((txstate & 0x01) == 0) {
				p = tx1;
				txstate |= 0x01;
			} else /* if ((txstate & 0x02) == 0) */ {
				p = tx2;
				txstate |= 0x02;
			}
			// copy data to packet buffer
			if (++tail >= txsize) tail = 0;
			uint32_t n = txsize - tail;
			if (n > packetsize) n = packetsize;
			//print("memcpy, offset=", tail);
			//println(", len=", n);
			memcpy(p, txbuf + tail, n);
			if (n >= packetsize) {
				tail += n - 1;
				if (tail >= txsize) tail = 0;
			} else {
				//n = txsize - n;
				uint32_t len = packetsize - n;
				//println("memcpy, offset=0, len=", len);
				memcpy(p + n, txbuf, len);
				tail = len - 1;
			}
			txtail = tail;
			//println("queue tx packet, newtail=", tail);
			debugDigitalWrite(7, HIGH);
			queue_Data_Transfer(txpipe, p, packetsize, this);
			debugDigitalWrite(7, LOW);
			NVIC_ENABLE_IRQ(IRQ_USBHS);
			return 1;
		}
	}
	// otherwise, set a latency timer to later transmit partial packet
	txtimer.stop();
	txtimer.start(write_timeout_);
	NVIC_ENABLE_IRQ(IRQ_USBHS);
	return 1;
}

bool USBSerialBase::setDTR(bool fSet)
{
	println("setDTR: ", fSet, DEC);
	if (!device) return false;
	// NOT sure if we should check pending control and not allow it? OR???
	if (fSet) dtr_rts_ |= 1;
	else dtr_rts_ &= ~1;

	switch (sertype) {
		default: 
			return false; // Not sure how to do...
		case PL2303:
		case CDCACM: 
			mk_setup(setup, 0x21, 0x22, dtr_rts_, 0, 0);
			break;
		case FTDI: 
			println("  >>FTDI");
			// The high 8 is mask and low 8 is setting. 
			mk_setup(setup, 0x40, 1, fSet? 0x0101 : 0x0100, 0, 0);
			break;
		// not sure yet on these	
		//case CH341: 
		case CP210X: 
			// DTR(1) RTS(2)
			mk_setup(setup, 0x41, 7, fSet? 0x0101 : 0x0100, 0, 0);
			break;
	}

	pending_control = 0;
	control_queued = true;
	queue_Control_Transfer(device, &setup, NULL, this);

	// Lets wait until sent.. 
	elapsedMillis em;
	while (control_queued && (em < 500)) {
		yield();	// not sure if we want to yield or what? 
	}
	if (control_queued)println("setDtr: message timeout");
	return true;
}

// Lets split this up fro setting both
bool USBSerialBase::setRTS(bool fSet)
{
	println("setRTS: ", fSet, DEC);
	if (fSet) dtr_rts_ |= 2;
	else dtr_rts_ &= ~2;

	if (!device) return false;
	// NOT sure if we should check pending control and not allow it? OR???

	switch (sertype) {
		default: 
			return false; // Not sure how to do...
		case PL2303:
		case CDCACM: 
			mk_setup(setup, 0x21, 0x22, dtr_rts_, 0, 0);
			break;
		case FTDI: 
			println("  >>FTDI");
			// The high 8 is mask and low 8 is setting. 
			mk_setup(setup, 0x40, 1, fSet? 0x0202 : 0x0200, 0, 0);
			break;
		// not sure yet on these	
		//case CH341: 
		case CP210X: 
			// DTR(1) RTS(2)
			mk_setup(setup, 0x41, 7, fSet? 0x0202 : 0x0200, 0, 0);
			break;
	}

	pending_control = 0;
	control_queued = true;
	queue_Control_Transfer(device, &setup, NULL, this);

	// Lets wait until sent.. 
	elapsedMillis em;
	while (control_queued && (em < 500)) {
		yield();	// not sure if we want to yield or what? 
	}
	if (control_queued)println("setRTS: message timeout");
	return true;
}
