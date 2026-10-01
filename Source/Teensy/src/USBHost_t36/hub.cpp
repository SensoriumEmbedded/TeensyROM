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

// True when any hub port is in the reset or reset recovery phase.
// Only one USB device may be reset at a time, because it will
// begin responding to address zero.
volatile bool USBHub::reset_busy = false;

#define print   USBHost::print_
#define println USBHost::println_

void USBHub::init()
{
	contribute_Devices(mydevices, sizeof(mydevices)/sizeof(Device_t));
	contribute_Pipes(mypipes, sizeof(mypipes)/sizeof(Pipe_t));
	contribute_Transfers(mytransfers, sizeof(mytransfers)/sizeof(Transfer_t));
	contribute_String_Buffers(mystring_bufs, sizeof(mystring_bufs)/sizeof(strbuf_t));
	driver_ready_for_device(this);
}

bool USBHub::claim(Device_t *dev, int type, const uint8_t *d, uint32_t len)
{
	// only claim entire device, never at interface level
	if (type != 0) return false;

	println("USBHub memory usage = ", sizeof(USBHub));
	println("USBHub claim_device this=", (uint32_t)this, HEX);

	resettimer.pointer = (void *)"Hello, I'm resettimer";
	debouncetimer.pointer = (void *)"Debounce Timer";

	// check for HUB type
	if (dev->bDeviceClass != 9 || dev->bDeviceSubClass != 0) return false;
	// protocol must be 0=FS, 1=HS Single-TT, or 2=HS Multi-TT
	if (dev->bDeviceProtocol > 2) return false;

	interface_count = 0;
	while (len >= 16) {
		if (d[0] == 9 && d[1] == 4 &&		// valid interface descriptor
		  d[4] == 1 &&				// has 1 endpoint
		  d[5] == 9 &&				// bInterfaceClass is HUB type
		  d[7] >= 0 && d[7] <= 2 &&		// bInterfaceProtocol is ok
		  d[9] == 7 && d[10] == 5 &&		// valid endpoint descriptor
		  (d[11] & 0xF0) == 0x80 &&		// endpoint direction is IN
		  d[12] == 3 &&				// endpoint type is interrupt
		  d[13] == 1 && d[14] == 0) {		// max packet size is 1 byte
			println("found possible interface, altsetting=", d[3]);
			if (interface_count == 0) {
				interface_number = d[2];
				altsetting = d[3];
				protocol = d[7];
				endpoint = d[11] & 0x0F;
				interval = d[15];
			} else {
				if (d[2] != interface_number) break;
				if (d[7] > protocol) {
					altsetting = d[3];
					protocol = d[7];
					endpoint = d[11] & 0x0F;
					interval = d[15];
				}
			}
			interface_count++;
		}
		d += 16; // jump forward to next interface
		len -= 16;
	}
	if (interface_count == 0) return false; // no usable interface found
	println("number of interfaces found = ", interface_count);
	if (interface_count > 1) {
		print("best interface is ", interface_number);
		println(" using altsetting ", altsetting);
	}
	numports = 0; // unknown until hub descriptor is read
	changepipe = NULL;
	changebits = 0;
	sending_control_transfer = 0;
	port_doing_reset = 0;
	memset(portstate, 0, sizeof(portstate));
	memset(devicelist, 0, sizeof(devicelist));

	mk_setup(setup, 0xA0, 6, 0x2900, 0, sizeof(hub_desc));
	queue_Control_Transfer(dev, &setup, hub_desc, this);

	return true;
}


bool USBHub::can_send_control_now()
{
	if (sending_control_transfer) return false;
	sending_control_transfer = 1;
	return true;
}

void USBHub::send_poweron(uint32_t port)
{
	if (port == 0 || port > numports) return;
	if (can_send_control_now()) {
		mk_setup(setup, 0x23, 3, 8, port, 0);
		queue_Control_Transfer(device, &setup, NULL, this);
		send_pending_poweron &= ~(1 << port);
	} else {
		send_pending_poweron |= (1 << port);
	}
}

void USBHub::send_getstatus(uint32_t port)
{
	if (port > numports) return;
	if (can_send_control_now()) {
		println("getstatus, port = ", port);
		mk_setup(setup, ((port > 0) ? 0xA3 : 0xA0), 0, 0, port, 4);
		queue_Control_Transfer(device, &setup, &statusbits, this);
		send_pending_getstatus &= ~(1 << port);
	} else {
		println("deferred getstatus, port = ", port);
		send_pending_getstatus |= (1 << port);
	}
}

void USBHub::send_clearstatus_connect(uint32_t port)
{
	if (port == 0 || port > numports) return;
	if (can_send_control_now()) {
		mk_setup(setup, 0x23, 1, 16, port, 0); // 16=C_PORT_CONNECTION
		queue_Control_Transfer(device, &setup, NULL, this);
		send_pending_clearstatus_connect &= ~(1 << port);
	} else {
		send_pending_clearstatus_connect |= (1 << port);
	}
}

void USBHub::send_clearstatus_enable(uint32_t port)
{
	if (port == 0 || port > numports) return;
	if (can_send_control_now()) {
		mk_setup(setup, 0x23, 1, 17, port, 0); // 17=C_PORT_ENABLE
		queue_Control_Transfer(device, &setup, NULL, this);
		send_pending_clearstatus_enable &= ~(1 << port);
	} else {
		send_pending_clearstatus_enable |= (1 << port);
	}
}

void USBHub::send_clearstatus_suspend(uint32_t port)
{
	if (port == 0 || port > numports) return;
	if (can_send_control_now()) {
		mk_setup(setup, 0x23, 1, 18, port, 0); // 18=C_PORT_SUSPEND
		queue_Control_Transfer(device, &setup, NULL, this);
		send_pending_clearstatus_suspend &= ~(1 << port);
	} else {
		send_pending_clearstatus_suspend |= (1 << port);
	}
}

void USBHub::send_clearstatus_overcurrent(uint32_t port)
{
	if (port == 0 || port > numports) return;
	if (can_send_control_now()) {
		mk_setup(setup, 0x23, 1, 19, port, 0); // 19=C_PORT_OVER_CURRENT
		queue_Control_Transfer(device, &setup, NULL, this);
		send_pending_clearstatus_overcurrent &= ~(1 << port);
	} else {
		send_pending_clearstatus_overcurrent |= (1 << port);
	}
}

void USBHub::send_clearstatus_reset(uint32_t port)
{
	if (port == 0 || port > numports) return;
	if (can_send_control_now()) {
		mk_setup(setup, 0x23, 1, 20, port, 0); // 20=C_PORT_RESET
		queue_Control_Transfer(device, &setup, NULL, this);
		send_pending_clearstatus_reset &= ~(1 << port);
	} else {
		send_pending_clearstatus_reset |= (1 << port);
	}
}

void USBHub::send_setreset(uint32_t port)
{
	if (port == 0 || port > numports) return;
	println("send_setreset");
	if (can_send_control_now()) {
		mk_setup(setup, 0x23, 3, 4, port, 0); // set feature PORT_RESET
		queue_Control_Transfer(device, &setup, NULL, this);
		send_pending_setreset &= ~(1 << port);
	} else {
		send_pending_setreset |= (1 << port);
	}
}

void USBHub::send_setinterface()
{
	// assumes not already sending another control transfer
	mk_setup(setup, 1, 11, altsetting, interface_number, 0);
	queue_Control_Transfer(device, &setup, NULL, this);
	sending_control_transfer = 1;
}

static uint32_t lowestbit(uint32_t bitmask)
{
	return __builtin_ctz(bitmask);
}

void USBHub::control(const Transfer_t *transfer)
{
	println("USBHub control callback");
	print_hexbytes(transfer->buffer, transfer->length);

	sending_control_transfer = 0;
	uint32_t port = transfer->setup.wIndex;
	uint32_t mesg = transfer->setup.word1;

	switch (mesg) {
	  case 0x290006A0: // read hub descriptor
		numports = hub_desc[2];
		characteristics = hub_desc[3];
		powertime = hub_desc[5];
		if (interface_count > 1) {
			send_setinterface();
		}
		// TODO: do we need to use the DeviceRemovable
		// bits to make synthetic device connect events?
		println("Hub ports = ", numports);
		for (uint32_t i=1; i <= numports; i++) {
			send_poweron(i);
		}
		break;
	  case 0x00080323: // power turned on
		if (port == numports && changepipe == NULL) {
			println("power turned on to all ports");
			println("device addr = ", device->address);
			changepipe = new_Pipe(device, 3, endpoint, 1, 1, interval);
			println("pipe cap1 = ", changepipe->qh.capabilities[0], HEX);
			changepipe->callback_function = callback;
			queue_Data_Transfer(changepipe, &changebits, 1, this);
		}
		break;

	  case 0x000000A0: // get hub status
		println("New Hub Status");
		break;
	  case 0x000000A3: // get port status
		println("New Port Status");
		if (transfer->length == 4) {
			uint32_t status = *(uint32_t *)(transfer->buffer);
			if (status != statusbits) println("ERROR: status not same");
			new_port_status(port, status);
		}
		//if (changebits & (1 << port)) {
			//changebits &= ~(1 << port);
			//send_clearstatus(port);
		//}
		break;
	  case 0x00100120: // clear hub status
		println("Hub Status Cleared");
		break;
	  case 0x00100123: // clear port status
		println("Port Status Cleared, port=", port);
		break;
	  default:
		println("unhandled setup, message = ", mesg, HEX);
	}
	// After we've completed processing for this control
	// transfer, check if any more need to be sent.  These
	// allow only a single control transfer to occur at once
	// which isn't fast, but requires only 3 Transfer_t and
	// allows reusing the setup and other buffers
	if (sending_control_transfer) return;
	if (send_pending_poweron) {
		send_poweron(lowestbit(send_pending_poweron));
	} else if (send_pending_clearstatus_connect) {
		send_clearstatus_connect(lowestbit(send_pending_clearstatus_connect));
	} else if (send_pending_clearstatus_enable) {
		send_clearstatus_enable(lowestbit(send_pending_clearstatus_enable));
	} else if (send_pending_clearstatus_suspend) {
		send_clearstatus_suspend(lowestbit(send_pending_clearstatus_suspend));
	} else if (send_pending_clearstatus_overcurrent) {
		send_clearstatus_overcurrent(lowestbit(send_pending_clearstatus_overcurrent));
	} else if (send_pending_clearstatus_reset) {
		send_clearstatus_reset(lowestbit(send_pending_clearstatus_reset));
	} else if (send_pending_getstatus) {
		send_getstatus(lowestbit(send_pending_getstatus));
	} else if (send_pending_setreset) {
		send_setreset(lowestbit(send_pending_setreset));
	}
}

void USBHub::callback(const Transfer_t *transfer)
{
	//println("HUB Callback (static)");
	if (transfer->driver) ((USBHub *)(transfer->driver))->status_change(transfer);
}

void USBHub::status_change(const Transfer_t *transfer)
{
	println("HUB Callback (member)");
	println("status = ", changebits, HEX);
	for (uint32_t i=0; i <= numports; i++) {
		if (changebits & (1 << i)) {
			send_getstatus(i);
		}
	}
	queue_Data_Transfer(changepipe, &changebits, 1, this);
}

void USBHub::new_port_status(uint32_t port, uint32_t status)
{
	if (port == 0 || port > numports) return;
#if 1
	print("  status=");
	print(status, HEX);
	println("  port=", port);
	println("  state=", portstate[port-1]);
	// status bits, USB 2.0: 11.24.2.7.1 page 427
	if (status & 0x0001) println("  Device is present: ");
	if (status & 0x0002) {
		print("  Enabled, speed = ");
		if (status & 0x0200) {
			print("1.5");
		} else {
			if (status & 0x0400) {
				print("480");
			} else {
				print("12");
			}
		}
		println(" Mbit/sec");
	}
	if (status & 0x0004) println("  Suspended");
	if (status & 0x0008) println("  Over-current");
	if (status & 0x0010) println("  Reset");
	if (status & 0x0100) println("  Has Power");
	if (status & 0x0800) println("  Test Mode");
	if (status & 0x1000) println("  Software Controls LEDs");
#endif
	uint8_t &state = portstate[port-1];
	switch (state) {
	  case PORT_OFF:
	  case PORT_DISCONNECT:
		if (status & 0x0001) { // connected
			state = PORT_DEBOUNCE1;
			start_debounce_timer(port);
			send_clearstatus_connect(port);
		}
		break;
	  case PORT_DEBOUNCE1:
	  case PORT_DEBOUNCE2:
	  case PORT_DEBOUNCE3:
	  case PORT_DEBOUNCE4:
	  case PORT_DEBOUNCE5:
		if (status & 0x0001) {
			if (++state > PORT_DEBOUNCE5) {
				if (USBHub::reset_busy || USBHost::enumeration_busy) {
					// wait in debounce state if another port is
					// resetting or a device is busy enumerating
					state = PORT_DEBOUNCE5;
					break;
				}
				USBHub::reset_busy = true;
				stop_debounce_timer(port);
				state = PORT_RESET;
				println("sending reset");
				send_setreset(port);
				port_doing_reset = port;
			}
		} else {
			stop_debounce_timer(port);
			state = PORT_DISCONNECT;
		}
		break;
	  case PORT_RESET:
		if (status & 0x0002) {
			// port is now enabled
			send_clearstatus_reset(port);
			state = PORT_RECOVERY;
			uint8_t speed=0;
			if (status & 0x0200) speed = 1;
			else if (status & 0x0400) speed = 2;
			port_doing_reset_speed = speed;
			resettimer.start(25000);
		} else if (!(status & 0x0001)) {
			send_clearstatus_connect(port);
			USBHub::reset_busy = false;
			state = PORT_DISCONNECT;
		}
		break;
	  case PORT_RECOVERY:
		if (!(status & 0x0001)) {
			send_clearstatus_connect(port);
			USBHub::reset_busy = false;
			state = PORT_DISCONNECT;
		}
		break;
	  case PORT_ACTIVE:
		if (!(status & 0x0001)) {
			disconnect_Device(devicelist[port-1]);
			devicelist[port-1] = NULL;
			send_clearstatus_connect(port);
			state = PORT_DISCONNECT;
		}
		break;
	}
}


void USBHub::timer_event(USBDriverTimer *timer)
{
	uint32_t us = micros() - timer->started_micros;
	print("timer event (");
	print(us);
	print(" us): ");
	print((char *)timer->pointer);
	print(", this = ");
	print((uint32_t)this, HEX);
	println(", timer = ", (uint32_t)timer, HEX);
	if (timer == &debouncetimer) {
		uint32_t in_use = debounce_in_use;
		println("ports in use bitmask = ", in_use, HEX);
		if (in_use) {
			for (uint32_t i=1; i <= numports; i++) {
				if (in_use & (1 << i)) send_getstatus(i);
			}
			debouncetimer.start(20000);
		}
	} else if (timer == &resettimer) {
		uint8_t port = port_doing_reset;
		println("port_doing_reset = ", port);
		if (port_doing_reset) {
			uint8_t &state = portstate[port-1];
			if (state == PORT_RECOVERY) {
				port_doing_reset = 0;
				println("PORT_RECOVERY");
				// begin enumeration process
				uint8_t speed = port_doing_reset_speed;
				devicelist[port-1] = new_Device(speed, device->address, port);
				// TODO: if return is NULL, what to do?  Panic?
				// Can we disable the port?  Will this device
				// play havoc if it sits unconfigured responding
				// to address zero?  Does that even matter?  Maybe
				// we have far worse issues when memory isn't
				// available?!
				USBHub::reset_busy = false;
				state = PORT_ACTIVE;
			}
		}
	}

	// TODO: testing only!!!
	//static uint32_t count=0;
	//if (++count > 36) while (1) ; // stop here
}

void USBHub::start_debounce_timer(uint32_t port)
{
	if (debounce_in_use == 0) debouncetimer.start(20000);
	debounce_in_use |= (1 << port);
}

void USBHub::stop_debounce_timer(uint32_t port)
{
	debounce_in_use &= ~(1 << port);
}


void USBHub::disconnect()
{
	// disconnect all downstream devices, which may be more hubs
	for (uint32_t i=0; i < numports; i++) {
		if (devicelist[i]) disconnect_Device(devicelist[i]);
	}
	numports = 0;
	changepipe = NULL;
	changebits = 0;
	sending_control_transfer = 0;
	port_doing_reset = 0;
	memset(portstate, 0, sizeof(portstate));
	memset(devicelist, 0, sizeof(devicelist));
	send_pending_poweron = 0;
	send_pending_getstatus = 0;
	send_pending_clearstatus_connect = 0;
	send_pending_clearstatus_enable = 0;
	send_pending_clearstatus_suspend = 0;
	send_pending_clearstatus_overcurrent = 0;
	send_pending_clearstatus_reset = 0;
	send_pending_setreset = 0;
	debounce_in_use = 0;
}


/*
config descriptor from a Multi-TT hub
09 02 29 00 01 01 00 E0 32
09 04 00 00 01 09 00 01 00
07 05 81 03 01 00 0C
09 04 00 01 01 09 00 02 00
07 05 81 03 01 00 0C
*/


