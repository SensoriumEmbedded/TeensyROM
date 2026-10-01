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

// Printing of specific data structures.  When this is enabled,
// a tremendous amount of debug printing occurs.  It's done all
// from interrupt context, so this should never normally be
// enabled for regular programs that print from the Arduino sketch.

#ifdef USBHOST_PRINT_DEBUG

void USBHost::print_(const Transfer_t *transfer)
{
	if (!((uint32_t)transfer & 0xFFFFFFE0)) return;
	USBHDBGSerial.print("Transfer @ ");
	USBHDBGSerial.println(((uint32_t)transfer & 0xFFFFFFE0), HEX);
	USBHDBGSerial.print("   next:  ");
	USBHDBGSerial.println(transfer->qtd.next, HEX);
	USBHDBGSerial.print("   anext: ");
	USBHDBGSerial.println(transfer->qtd.alt_next, HEX);
	USBHDBGSerial.print("   token: ");
	USBHDBGSerial.println(transfer->qtd.token, HEX);
	USBHDBGSerial.print("   bufs:  ");
	for (int i=0; i < 5; i++) {
		USBHDBGSerial.print(transfer->qtd.buffer[i], HEX);
		if (i < 4) USBHDBGSerial.print(',');
	}
	USBHDBGSerial.println();
}

void USBHost::print_(const Transfer_t *first, const Transfer_t *last)
{
	USBHDBGSerial.print("Transfer Followup List ");
	USBHDBGSerial.print((uint32_t)first, HEX);
	USBHDBGSerial.print(" to ");
	USBHDBGSerial.println((uint32_t)last, HEX);
	USBHDBGSerial.println("    forward:");
	while (first) {
		USBHDBGSerial.print("    ");
		USBHDBGSerial.print((uint32_t)first, HEX);
		print_token(first->qtd.token);
		first = first->next_followup;
	}
	USBHDBGSerial.println("    backward:");
	while (last) {
		USBHDBGSerial.print("    ");
		USBHDBGSerial.print((uint32_t)last, HEX);
		print_token(last->qtd.token);
		last = last->prev_followup;
	}
}

void USBHost::print_token(uint32_t token)
{
	switch ((token >> 8) & 3) {
	case 0:
		USBHDBGSerial.print(" OUT ");
		USBHDBGSerial.println((token >> 16) & 0x7FFF);
		break;
	case 1:
		USBHDBGSerial.print(" IN ");
		USBHDBGSerial.println((token >> 16) & 0x7FFF);
		break;
	case 2:
		USBHDBGSerial.println(" SETUP");
		break;
	default:
		USBHDBGSerial.println(" unknown");
	}
}

void USBHost::print_(const Pipe_t *pipe)
{
	if (!((uint32_t)pipe & 0xFFFFFFE0)) return;
	USBHDBGSerial.print("Pipe ");
	if (pipe->type == 0) USBHDBGSerial.print("control");
	else if (pipe->type == 1) USBHDBGSerial.print("isochronous");
	else if (pipe->type == 2) USBHDBGSerial.print("bulk");
	else if (pipe->type == 3) USBHDBGSerial.print("interrupt");
	USBHDBGSerial.print(pipe->direction ? " IN" : " OUT");
	USBHDBGSerial.print("  @ ");
	USBHDBGSerial.println((uint32_t)pipe, HEX);
	USBHDBGSerial.print("  horiz link:  ");
	USBHDBGSerial.println(pipe->qh.horizontal_link, HEX);
	USBHDBGSerial.print("  capabilities: ");
	USBHDBGSerial.print(pipe->qh.capabilities[0], HEX);
	USBHDBGSerial.print(',');
	USBHDBGSerial.println(pipe->qh.capabilities[1], HEX);
	USBHDBGSerial.println("  overlay:");
	USBHDBGSerial.print("    cur:   ");
	USBHDBGSerial.println(pipe->qh.current, HEX);
	USBHDBGSerial.print("    next:  ");
	USBHDBGSerial.println(pipe->qh.next, HEX);
	USBHDBGSerial.print("    anext: ");
	USBHDBGSerial.println(pipe->qh.alt_next, HEX);
	USBHDBGSerial.print("    token: ");
	USBHDBGSerial.println(pipe->qh.token, HEX);
	USBHDBGSerial.print("    bufs:  ");
	for (int i=0; i < 5; i++) {
		USBHDBGSerial.print(pipe->qh.buffer[i], HEX);
		if (i < 4) USBHDBGSerial.print(',');
	}
	USBHDBGSerial.println();
	const Transfer_t *t = (Transfer_t *)pipe->qh.next;
	while (((uint32_t)t & 0xFFFFFFE0)) {
		print_(t);
		t = (Transfer_t *)t->qtd.next;
	}
	//USBHDBGSerial.print();
}

void USBHost::print_driverlist(const char *name, const USBDriver *driver)
{
	USBHDBGSerial.print("USBDriver (");
	USBHDBGSerial.print(name);
	USBHDBGSerial.print(") list: ");
	if (driver == NULL) {
		USBHDBGSerial.println("(empty");
		return;
	}
	uint32_t count=0;
	for (const USBDriver *p = driver; p; p = p->next) {
		USBHDBGSerial.print((uint32_t)p, HEX);
		if (p->next) USBHDBGSerial.print(" -> ");
		if (++count > 30) {
			USBHDBGSerial.println("abort:list too long");
			return;
		}
	}
	USBHDBGSerial.println();
}

void USBHost::print_qh_list(const Pipe_t *list)
{
	if (!list) {
		USBHDBGSerial.println("(empty)");
		return;
	}
	const Pipe_t *node = list;
	uint8_t max_print = 32; // max number of items..
	while (1) {
		USBHDBGSerial.print((uint32_t)node, HEX);
		const Pipe_t *next_node = (const Pipe_t *)(node->qh.horizontal_link & 0xFFFFFFE0);
		if (!next_node) break;
		// Lets check for loops
		const Pipe_t *nt = list;
		while (nt != node) {
			if (nt == next_node) break;
			nt = (const Pipe_t *)(nt->qh.horizontal_link & 0xFFFFFFE0);
		}

		if (nt != node) {
			USBHDBGSerial.print(" -> (loops) ");
			USBHDBGSerial.print((uint32_t)next_node, HEX);
			break;
		}	
		node = next_node;

		if (!max_print--) {
			USBHDBGSerial.print(" (Probable Loop)");
			break;
		}
		USBHDBGSerial.print(" -> ");
	}
	USBHDBGSerial.println();
}

static void print_class_subclass_protocol(uint8_t c, uint8_t s, uint8_t p)
{
	USBHDBGSerial.print(c);
	if (c == 3) USBHDBGSerial.print("(HID)");
	if (c == 8) USBHDBGSerial.print("(Mass Storage)");
	if (c == 9) USBHDBGSerial.print("(Hub)");
	USBHDBGSerial.print(" / ");
	USBHDBGSerial.print(s);
	if (c == 3 && s == 1) USBHDBGSerial.print("(Boot)");
	if (c == 8 && s == 6) USBHDBGSerial.print("(SCSI)");
	USBHDBGSerial.print(" / ");
	USBHDBGSerial.print(p);
	if (c == 3 && s == 1 && p == 1) USBHDBGSerial.print("(Keyboard)");
	if (c == 3 && s == 1 && p == 2) USBHDBGSerial.print("(Mouse)");
	if (c == 8 && s == 6 && p == 0x50) USBHDBGSerial.print("(Bulk Only)");
	if (c == 8 && s == 6 && p == 0x62) USBHDBGSerial.print("(UAS)");
	if (c == 9 && s == 0 && p == 1) USBHDBGSerial.print("(Single-TT)");
	if (c == 9 && s == 0 && p == 2) USBHDBGSerial.print("(Multi-TT)");
	USBHDBGSerial.println();
}

void USBHost::print_device_descriptor(const uint8_t *p)
{
	USBHDBGSerial.println("Device Descriptor:");
	USBHDBGSerial.print("  ");
	print_hexbytes(p, p[0]);
	if (p[0] != 18) {
		USBHDBGSerial.println("error: device must be 18 bytes");
		return;
	}
	if (p[1] != 1) {
		USBHDBGSerial.println("error: device must type 1");
		return;
	}
	USBHDBGSerial.printf("    VendorID = %04X, ProductID = %04X, Version = %04X",
		p[8] | (p[9] << 8), p[10] | (p[11] << 8), p[12] | (p[13] << 8));
	USBHDBGSerial.println();
	USBHDBGSerial.print("    Class/Subclass/Protocol = ");
	print_class_subclass_protocol(p[4], p[5], p[6]);
	USBHDBGSerial.print("    Number of Configurations = ");
	USBHDBGSerial.println(p[17]);
}

void USBHost::print_config_descriptor(const uint8_t *p, uint32_t maxlen)
{
	// Descriptor Types: (USB 2.0, page 251)
	USBHDBGSerial.println("Configuration Descriptor:");
	USBHDBGSerial.print("  ");
	print_hexbytes(p, p[0]);
	if (p[0] != 9) {
		USBHDBGSerial.println("error: config must be 9 bytes");
		return;
	}
	if (p[1] != 2) {
		USBHDBGSerial.println("error: config must type 2");
		return;
	}
	USBHDBGSerial.print("    NumInterfaces = ");
	USBHDBGSerial.println(p[4]);
	USBHDBGSerial.print("    ConfigurationValue = ");
	USBHDBGSerial.println(p[5]);

	uint32_t len = p[2] | (p[3] << 8);
	if (len > maxlen) len = maxlen;
	len -= p[0];
	p += 9;

	while (len > 0) {
		if (p[0] > len) {
			USBHDBGSerial.print("  ");
			print_hexbytes(p, len);
			USBHDBGSerial.println("  error: length beyond total data size");
			break;
		}
		USBHDBGSerial.print("  ");
		print_hexbytes(p, p[0]);
		if (p[0] == 9 && p[1] == 4) { // Interface Descriptor
			USBHDBGSerial.print("    Interface = ");
			USBHDBGSerial.println(p[2]);
			USBHDBGSerial.print("    Number of endpoints = ");
			USBHDBGSerial.println(p[4]);
			USBHDBGSerial.print("    Class/Subclass/Protocol = ");
			print_class_subclass_protocol(p[5], p[6], p[7]);
		} else if (p[0] >= 7 && p[0] <= 9 && p[1] == 5) { // Endpoint Descriptor
			USBHDBGSerial.print("    Endpoint = ");
			USBHDBGSerial.print(p[2] & 15);
			USBHDBGSerial.println((p[2] & 128) ? " IN" : " OUT");
			USBHDBGSerial.print("    Type = ");
			switch (p[3] & 3) {
				case 0: USBHDBGSerial.println("Control"); break;
				case 1: USBHDBGSerial.println("Isochronous"); break;
				case 2: USBHDBGSerial.println("Bulk"); break;
				case 3: USBHDBGSerial.println("Interrupt"); break;
			}
			USBHDBGSerial.print("    Max Size = ");
			USBHDBGSerial.println(p[4] | (p[5] << 8));
			USBHDBGSerial.print("    Polling Interval = ");
			USBHDBGSerial.println(p[6]);
		} else if (p[0] == 8 && p[1] == 11) { // IAD
			USBHDBGSerial.print("    Interface Association = ");
			USBHDBGSerial.print(p[2]);
			USBHDBGSerial.print(" through ");
			USBHDBGSerial.println(p[2] + p[3] - 1);
			USBHDBGSerial.print("    Class / Subclass / Protocol = ");
			print_class_subclass_protocol(p[4], p[5], p[7]);
		} else if (p[0] >= 9 && p[1] == 0x21) { // HID
			USBHDBGSerial.print("    HID, ");
			USBHDBGSerial.print(p[5]);
			USBHDBGSerial.print(" report descriptor");
			if (p[5] != 1) USBHDBGSerial.print('s');
			USBHDBGSerial.println();
		}
		len -= p[0];
		p += p[0];
	}
}

void USBHost::print_string_descriptor(const char *name, const uint8_t *p)
{
	uint32_t len = p[0];
	if (len < 4) return;
	USBHDBGSerial.print(name);
	len -= 2;
	p += 2;
	while (len >= 2) {
		uint32_t c = p[0] | (p[1] << 8);
		if (c < 0x80) {
			USBHDBGSerial.write(c);
		} else if (c < 0x800) {
			USBHDBGSerial.write((c >> 6) | 0xC0);
			USBHDBGSerial.write((c & 0x3F) | 0x80);
		} else {
			USBHDBGSerial.write((c >> 12) | 0xE0);
			USBHDBGSerial.write(((c >> 6) & 0x3F) | 0x80);
			USBHDBGSerial.write((c & 0x3F) | 0x80);
		}
		len -= 2;
		p += 2;
	}
	USBHDBGSerial.println();
	//print_hexbytes(p, p[0]);
}


void USBHost::print_hexbytes(const void *ptr, uint32_t len)
{
	if (ptr == NULL || len == 0) return;
	const uint8_t *p = (const uint8_t *)ptr;
	do {
		if (*p < 16) USBHDBGSerial.print('0');
		USBHDBGSerial.print(*p++, HEX);
		USBHDBGSerial.print(' ');
	} while (--len);
	USBHDBGSerial.println();
}

#endif
