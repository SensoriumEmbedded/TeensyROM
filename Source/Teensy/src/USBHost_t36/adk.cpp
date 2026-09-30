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
 */

#include <Arduino.h>
#include "USBHost_t36.h"  // Read this header first for key info

#define print   USBHost::print_
#define println USBHost::println_

#define ADK_VID   0x18D1
#define ADK_PID   0x2D00
#define ADB_PID   0x2D01
#define USB_SETUP_DEVICE_TO_HOST 	0x80
#define USB_SETUP_HOST_TO_DEVICE 	0x00 
#define USB_SETUP_TYPE_VENDOR 		0x40 
#define USB_SETUP_RECIPIENT_DEVICE 	0x00		
#define UHS_ADK_bmREQ_GET     		USB_SETUP_DEVICE_TO_HOST|USB_SETUP_TYPE_VENDOR|USB_SETUP_RECIPIENT_DEVICE
#define UHS_ADK_bmREQ_SEND    		USB_SETUP_HOST_TO_DEVICE|USB_SETUP_TYPE_VENDOR|USB_SETUP_RECIPIENT_DEVICE
#define UHS_ADK_GETPROTO      		51  // check USB accessory protocol version
#define UHS_ADK_SENDSTR       		52  // send identifying string
#define UHS_ADK_ACCSTART      		53  // start device in accessory mode
#define UHS_ADK_ID_MANUFACTURER   	0
#define UHS_ADK_ID_MODEL          	1
#define UHS_ADK_ID_DESCRIPTION    	2
#define UHS_ADK_ID_VERSION        	3
#define UHS_ADK_ID_URI            	4
#define UHS_ADK_ID_SERIAL         	5

static uint8_t adkbuf[256] __attribute__ ((aligned(16)));
static setup_t adksetup __attribute__ ((aligned(16)));

/************************************************************/
//  Initialization and claiming of devices & interfaces
/************************************************************/

void ADK::init()
{
	contribute_Pipes(mypipes, sizeof(mypipes)/sizeof(Pipe_t));
	contribute_Transfers(mytransfers, sizeof(mytransfers)/sizeof(Transfer_t));
	
	rx_head = 0;
	rx_tail = 0;
	driver_ready_for_device(this);
	
	state = 0;
}

bool ADK::claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len)
{
	// only claim at interface level
	if (type != 1) 
		return false;
	
	// Check for ADK or ADB PIDs
	if (dev->idVendor == ADK_VID && (dev->idProduct == ADK_PID || dev->idProduct == ADB_PID))
	{		
		const uint8_t *p = descriptors;
		const uint8_t *end = p + len;

		// Interface descriptor
		if (p[0] != 9 || p[1] != 4) 
			return false; 

		// bInterfaceClass: 255 Vendor Specific
		if (p[5] != 255) 
		{
			return false; 
		}
		// bInterfaceSubClass: 255
		if (p[6] != 255) 
		{
			return false;
		}
		
		println("ADK claim this=", (uint32_t)this, HEX);
		print("vid=", dev->idVendor, HEX);
		print(", pid=", dev->idProduct, HEX);
		print(", bDeviceClass = ", dev->bDeviceClass);
		print(", bDeviceSubClass = ", dev->bDeviceSubClass);
		println(", bDeviceProtocol = ", dev->bDeviceProtocol);
		println("  bInterfaceClass=", p[5]);
		println("  bInterfaceSubClass=", p[6]);
		print_hexbytes(descriptors, len);
		
		p += 9;
		rx_ep = 0;
		tx_ep = 0;

		while (p < end) 
		{
			len = *p;
			
			if (len < 4) 
				return false; // all desc are at least 4 bytes
			
			if (p + len > end) 
				return false; // reject if beyond end of data
			
			uint32_t type = p[1];
			if (type == 5) 
			{
				// endpoint descriptor
				if (p[0] < 7) 
					return false; // at least 7 bytes
				
				if (p[3] != 2) 
					return false; // must be bulk type
				
				println("    Endpoint: ", p[2], HEX);
				switch (p[2] & 0xF0) 
				{
				case 0x80:
					// IN endpoint
					if (rx_ep == 0) 
					{
						rx_ep = p[2] & 0x0F;
						rx_size = p[4] | (p[5] << 8);
						println(" rx_size = ", rx_size);
						println(" rx_ep = ", rx_ep);
					}
					break;
				case 0x00:
					// OUT endpoint
					if (tx_ep == 0) 
					{
						tx_ep = p[2];
						tx_size = p[4] | (p[5] << 8);
						println(" tx_size = ", tx_size);
						println(" tx_ep = ", tx_ep);
					}
					break;
				default:
					return false;
				}
			}
			
			// ADK uses the first two endpoints for communication
			if (rx_ep && tx_ep)
			{
				println("Found both rx and tx EPs");
				break;
			}
			p += len;
		}
		
		// if an IN endpoint was found, create its pipe
		if (rx_ep && rx_size <= MAX_PACKET_SIZE) 
		{
			rxpipe = new_Pipe(dev, 2, rx_ep, 1, rx_size);
			if (rxpipe) 
			{
				rxpipe->callback_function = rx_callback;
				queue_Data_Transfer(rxpipe, rx_buffer, rx_size, this);
				rx_packet_queued = true;
				println("Done creating RX pipe");
			}
		} 
		else 
		{
			rxpipe = NULL;
		}
		
		// if an OUT endpoint was found, create its pipe
		if (tx_ep && tx_size <= MAX_PACKET_SIZE) 
		{
			txpipe = new_Pipe(dev, 2, tx_ep, 0, tx_size);
			if (txpipe) 
			{
				txpipe->callback_function = tx_callback;
				println("Done creating TX pipe");
			}
		} 
		else 
		{
			txpipe = NULL;
		}
		
		rx_head = 0;
		rx_tail = 0;

		// claim if either pipe created
		bool created = (rxpipe || txpipe);
		
		if (created)
		{
			println("Done with init.");
			state = 8;
		}
		return created;
	
	}
	else
	{
		state = 0;
		println("Not in accessory mode.");
		
		// Kick off switch to Accessory Mode
		mk_setup(adksetup, UHS_ADK_bmREQ_GET, UHS_ADK_GETPROTO, 0, 0, 2);
		queue_Control_Transfer(dev, &adksetup, adkbuf, this);
		
		return true;
	}
	
	return false;
}
void ADK::sendStr(Device_t *dev, uint8_t index, char *str)
{ 
	strcpy((char *)adkbuf, str);
	mk_setup(adksetup, UHS_ADK_bmREQ_SEND, UHS_ADK_SENDSTR, 0, index, strlen(str));
	queue_Control_Transfer(dev, &adksetup, adkbuf, this);
}

void ADK::control(const Transfer_t *transfer)
{
	println("Control callback state=",state);
	
	switch (state)
	{
		case 0:
			print_hexbytes(transfer->buffer, transfer->length);
			state++;
			sendStr(device, UHS_ADK_ID_MANUFACTURER, manufacturer);
			break;
		case 1:
			print_hexbytes(transfer->buffer, transfer->length);
			state++;
			sendStr(device, UHS_ADK_ID_MODEL, model);
			break;
		case 2:
			print_hexbytes(transfer->buffer, transfer->length);
			state++;
			sendStr(device, UHS_ADK_ID_DESCRIPTION, desc);
			break;
		case 3:
			print_hexbytes(transfer->buffer, transfer->length);
			state++;
			sendStr(device, UHS_ADK_ID_VERSION, version);
			break;
		case 4:
			print_hexbytes(transfer->buffer, transfer->length);
			state++;
			sendStr(device, UHS_ADK_ID_URI, uri);
			break;
		case 5:
			print_hexbytes(transfer->buffer, transfer->length);
			state++;
			sendStr(device, UHS_ADK_ID_SERIAL, serial);
			break;
		case 6:
			print_hexbytes(transfer->buffer, transfer->length);
			state++;

			// Send ADK switch command
			mk_setup(adksetup, UHS_ADK_bmREQ_SEND, UHS_ADK_ACCSTART, 0, 0, 0);
			queue_Control_Transfer(device, &adksetup, NULL, this);
			break;
		case 7:
			println("After ACC switch command. Device should re-enumerate.");
			print_hexbytes(transfer->buffer, transfer->length);
			state++;
			break;
		default:
			break;
	}
}

void ADK::rx_callback(const Transfer_t *transfer)
{
	if (transfer->driver) {
		((ADK *)(transfer->driver))->rx_data(transfer);
	}
}

void ADK::tx_callback(const Transfer_t *transfer)
{
	if (transfer->driver) {
		((ADK *)(transfer->driver))->tx_data(transfer);
	}
}

void ADK::rx_data(const Transfer_t *transfer)
{
	uint32_t len = transfer->length - ((transfer->qtd.token >> 16) & 0x7FFF);
	
	println("ADK Receive rx_data");
	print("Len: ");
	print(len);
	print(" Data: ");
	print_hexbytes(transfer->buffer, len);
	
	uint32_t head = rx_head;
	
	uint8_t *p = (uint8_t *)transfer->buffer;
	if (p != NULL && len != 0)
	{
		do 
		{
			if (++head >= RX_QUEUE_SIZE) 
				head = 0;
		
			rx_queue[head] = *p++;
		} while (--len);
	}
	rx_head = head;
	
	rx_packet_queued = false;
	rx_queue_packets(rx_head, rx_tail);
}

void ADK::rx_queue_packets(uint32_t head, uint32_t tail)
{
	if (rx_packet_queued)
		return;
	
	uint32_t avail = (head < tail) ? tail - head - 1 : RX_QUEUE_SIZE - 1 - head + tail;
	
	println("rx_size = ", rx_size);
	println("avail = ", avail);
	if (avail >= rx_size) 
	{
		// enough space to accept another full packet
		println("queue another receive packet");
		
		queue_Data_Transfer(rxpipe, rx_buffer, rx_size, this);
		rx_packet_queued = true;
	} else {
		// queue can't accept another packet's data, so leave
		// the data waiting on the device until we can accept it
		println("wait to receive more packets");
		rx_packet_queued = false;
	}
}

void ADK::tx_data(const Transfer_t *transfer)
{
	println("ADK tx_data transmit complete");
	print("  Data: ");
	print_hexbytes(transfer->buffer, tx_size);
}


void ADK::disconnect()
{
	println("Disconnect");

	rxpipe = NULL;
	txpipe = NULL;
	
	state = 0;
}

void ADK::begin(char *adk_manufacturer, char *adk_model, char *adk_desc, char *adk_version, char *adk_uri, char *adk_serial)
{	
	manufacturer = adk_manufacturer;
	model = adk_model;
	desc = adk_desc;
	version = adk_version;
	uri = adk_uri;
	serial = adk_serial;
}

void ADK::end(void)
{
	// TODO: add end code
}

int ADK::available(void)
{
	if (!device) 
		return 0;
	
	uint32_t head = rx_head;
	uint32_t tail = rx_tail;
	
	if (head >= tail) 
		return head - tail;
	
	return RX_QUEUE_SIZE + head - tail;
}

int ADK::peek(void)
{
	if (!device) 
		return -1;
	
	uint32_t head = rx_head;
	uint32_t tail = rx_tail;
	
	if (head == tail) 
		return -1;
	
	if (++tail >= RX_QUEUE_SIZE) 
		tail = 0;
	
	return rx_queue[tail];
}

int ADK::read(void)
{
	if (!device) 
		return -1;
	
	uint32_t head = rx_head;
	uint32_t tail = rx_tail;
	
	if (head == tail) 
		return -1;
	
	if (++tail >= RX_QUEUE_SIZE) 
		tail = 0;
	
	int c = rx_queue[tail];
	rx_tail = tail;
	
	rx_queue_packets(head, tail);
	
	return c;
}

size_t ADK::write(size_t len, uint8_t *buf)
{
	memcpy(tx_buffer, buf, len);
	__disable_irq();
	queue_Data_Transfer(txpipe, tx_buffer, len, this);
	__enable_irq();
	
	return len;
}

bool ADK::ready()
{
	if (state > 7)
		return true;
	return false;
}
