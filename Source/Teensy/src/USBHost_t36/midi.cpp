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

#define print   USBHost::print_
#define println USBHost::println_

void MIDIDeviceBase::init()
{
	contribute_Pipes(mypipes, sizeof(mypipes)/sizeof(Pipe_t));
	contribute_Transfers(mytransfers, sizeof(mytransfers)/sizeof(Transfer_t));
	contribute_String_Buffers(mystring_bufs, sizeof(mystring_bufs)/sizeof(strbuf_t));
	handleNoteOff = NULL;
	handleNoteOn = NULL;
	handleVelocityChange = NULL;
	handleControlChange = NULL;
	handleProgramChange = NULL;
	handleAfterTouch = NULL;
	handlePitchChange = NULL;
	handleSysExPartial = NULL;
	handleSysExComplete = NULL;
	handleTimeCodeQuarterFrame = NULL;
	handleSongPosition = NULL;
	handleSongSelect = NULL;
	handleTuneRequest = NULL;
	handleClock = NULL;
	handleStart = NULL;
	handleContinue = NULL;
	handleStop = NULL;
	handleActiveSensing = NULL;
	handleSystemReset = NULL;
	handleRealTimeSystem = NULL;
	rx_head = 0;
	rx_tail = 0;
	rxpipe = NULL;
	txpipe = NULL;
	driver_ready_for_device(this);
}

// Audio Class-Specific Descriptor Types (audio 1.0, page 99)
//   CS_UNDEFINED     0x20
//   CS_DEVICE        0x21
//   CS_CONFIGURATION 0x22
//   CS_STRING        0x23
//   CS_INTERFACE     0x24
//   CS_ENDPOINT      0x25
// MS Class-Specific Interface Descriptor Subtypes (midi 1.0, page 36)
//   MS_DESCRIPTOR_UNDEFINED 0x00
//   MS_HEADER               0x01
//   MIDI_IN_JACK            0x02
//   MIDI_OUT_JACK           0x03
//   ELEMENT                 0x04
// MS Class-Specific Endpoint Descriptor Subtypes (midi 1.0, page 36)
//   DESCRIPTOR_UNDEFINED 0x00
//   MS_GENERAL           0x01
// MS MIDI IN and OUT Jack types (midi 1.0, page 36)
//   JACK_TYPE_UNDEFINED 0x00
//   EMBEDDED            0x01
//   EXTERNAL            0x02
// Endpoint Control Selectors (midi 1.0, page 36)
//   EP_CONTROL_UNDEFINED 0x00
//   ASSOCIATION_CONTROL  0x01

bool MIDIDeviceBase::claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len)
{
	// only claim at interface level
	if (type != 1) return false;
	println("MIDIDevice claim this=", (uint32_t)this, HEX);
	println("len = ", len);

	const uint8_t *p = descriptors;
	const uint8_t *end = p + len;

	if (p[0] != 9 || p[1] != 4) return false; // interface descriptor
	//println("  bInterfaceClass=", p[5]);
	//println("  bInterfaceSubClass=", p[6]);
	bool ismidi = false;
	if (p[5] == 1 && p[6] == 3) {
		println("  Interface is MIDI"); // p[5] is bInterfaceClass: 1 = Audio class
		ismidi = true;                  // p[6] is bInterfaceSubClass: 3 = MIDI
	} else {
		if (p[5] >= 2 && p[5] <= 18) return false; // definitely not MIDI
		// Yamaha uses vendor specific class, but can be
		// identified as MIDI from CS_INTERFACE descriptors.
		//  https://forum.pjrc.com/threads/55142?p=199162&viewfull=1#post199162
		println("  Interface is unknown (might be Yahama)");
	}
	p += 9;
	rx_ep = 0;
	tx_ep = 0;

	while (p < end) {
		len = *p;
		if (len < 4) return false; // all audio desc are at least 4 bytes
		if (p + len > end) return false; // reject if beyond end of data
		uint32_t type = p[1];
		print("type: ", type);
		println(", len: ", len);
		if (type == 4 || type == 11) break; // interface or IAD, not for us
		if (type == 0x24) {  // 0x24 = Audio CS_INTERFACE, audio 1.0, page 99
			uint32_t subtype = p[2];
			//println("subtype: ", subtype);
			if (subtype == 1) {
				// Interface Header, midi 1.0, page 21
				println("    MIDI Header (ignored)");
				ismidi = true;
			} else if (subtype == 2) {
				// MIDI IN Jack, midi 1.0, page 22
				println("    MIDI IN Jack (ignored)");
				ismidi = true;
			} else if (subtype == 3) {
				// MIDI OUT Jack, midi 1.0, page 22
				println("    MIDI OUT Jack (ignored)");
				ismidi = true;
			} else if (subtype == 4) {
				// Element Descriptor, midi 1.0, page 23-24
				println("    MIDI Element (ignored)");
				ismidi = true;
			} else if (subtype == 0xF1 && p[3] == 2) {
				// see Linux sound/usb/quirks.c create_roland_midi_quirk()
				println("    Roland vendor-specific (ignored)");
				ismidi = true;
			} else {
				println("    Unknown MIDI CS_INTERFACE descriptor!");
				return false; // unknown
			}
		} else if (type == 5) {
			// endpoint descriptor
			if (p[0] < 7) return false; // at least 7 bytes
			if (p[3] != 2 && p[3] != 3) return false; // must be bulk or interrupt type
			println("    MIDI Endpoint: ", p[2], HEX);
			switch (p[2] & 0xF0) {
			case 0x80:
				// IN endpoint
				if (rx_ep == 0) {
					rx_ep = p[2] & 0x0F;
					rx_ep_type = p[3];
					rx_size = p[4] | (p[5] << 8);
					println("      rx_size = ", rx_size);
				}
				break;
			case 0x00:
				// OUT endpoint
				if (tx_ep == 0) {
					tx_ep = p[2];
					tx_ep_type = p[3];
					tx_size = p[4] | (p[5] << 8);
					println("      tx_size = ", tx_size);
				}
				break;
			default:
				return false;
			}
		} else if (type == 37) {
			// MIDI endpoint info, midi 1.0: 6.2.2, page 26
			println("    MIDI Endpoint Jack Association (ignored)");
		} else {
			println("    Unknown descriptor, type=", type);
			return false; // unknown
		}
		p += len;
	}
	if (!ismidi) {
		println("This interface is not MIDI");
		return false;
	}
	// if an IN endpoint was found, create its pipe
	if (rx_ep && rx_size <= max_packet_size) {
		rxpipe = new_Pipe(dev, rx_ep_type, rx_ep, 1, rx_size);
		if (rxpipe) {
			rxpipe->callback_function = rx_callback;
			queue_Data_Transfer(rxpipe, rx_buffer, rx_size, this);
			rx_packet_queued = true;
		}
	} else {
		rxpipe = NULL;
	}
	// if an OUT endpoint was found, create its pipe
	if (tx_ep && tx_size <= max_packet_size) {
		txpipe = new_Pipe(dev, tx_ep_type, tx_ep, 0, tx_size);
		if (txpipe) {
			txpipe->callback_function = tx_callback;
			tx1_count = 0;
			tx2_count = 0;
		}
	} else {
		txpipe = NULL;
	}
	rx_head = 0;
	rx_tail = 0;
	msg_channel = 0;
	msg_type = 0;
	msg_data1 = 0;
	msg_data2 = 0;
	msg_sysex_len = 0;
	// claim if either pipe created
	return (rxpipe || txpipe);
}

void MIDIDeviceBase::rx_callback(const Transfer_t *transfer)
{
	if (transfer->driver) {
		((MIDIDevice *)(transfer->driver))->rx_data(transfer);
	}
}

void MIDIDeviceBase::tx_callback(const Transfer_t *transfer)
{
	if (transfer->driver) {
		((MIDIDevice *)(transfer->driver))->tx_data(transfer);
	}
}

void MIDIDeviceBase::rx_data(const Transfer_t *transfer)
{
	println("MIDIDevice Receive");
	print("  MIDI Data: ");
	uint32_t len = (transfer->length - ((transfer->qtd.token >> 16) & 0x7FFF)) >> 2;
	print_hexbytes(transfer->buffer, len * 4);
	uint32_t head = rx_head;
	uint32_t tail = rx_tail;
	for (uint32_t i=0; i < len; i++) {
		uint32_t msg = rx_buffer[i];
		if (msg) {
			if (++head >= rx_queue_size) head = 0;
			rx_queue[head] = msg;
		}
	}
	rx_head = head;
	rx_tail = tail;
	uint32_t avail = (head < tail) ? tail - head - 1 : rx_queue_size - 1 - head + tail;
	//println("rx_size = ", rx_size);
	println("avail = ", avail);
	if (avail >= (uint32_t)(rx_size>>2)) {
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

void MIDIDeviceBase::tx_data(const Transfer_t *transfer)
{
	println("MIDIDevice transmit complete");
	print("  MIDI Data: ");
	print_hexbytes(transfer->buffer, tx_size);
	if (transfer->buffer == tx_buffer1) {
		tx1_count = 0;
	} else if (transfer->buffer == tx_buffer2) {
		tx2_count = 0;
	}
}


void MIDIDeviceBase::disconnect()
{
	// should rx_queue be cleared?
	// as-is, the user can still read MIDI messages
	// which arrived before the device disconnected.
	rxpipe = NULL;
	txpipe = NULL;
}


void MIDIDeviceBase::write_packed(uint32_t data)
{
	if (!txpipe) return;
	uint32_t tx_max = tx_size / 4;
	while (1) {
		__disable_irq();
		uint32_t tx1 = tx1_count;
		uint32_t tx2 = tx2_count;
		if (tx1 < tx_max && (tx2 == 0 || tx2 >= tx_max)) {
			// use tx_buffer1
			tx_buffer1[tx1++] = data;
			tx1_count = tx1;
			txtimer.stop();
			if (tx1 >= tx_max) {
				queue_Data_Transfer(txpipe, tx_buffer1, tx_max*4, this);
			} else {
				txtimer.start(tx_max >= 128 ? 200 : 1500);
			}
			__enable_irq();
			return;
		}
		if (tx2 < tx_max) {
			// use tx_buffer2
			tx_buffer2[tx2++] = data;
			tx2_count = tx2;
			txtimer.stop();
			if (tx2 >= tx_max) {
				queue_Data_Transfer(txpipe, tx_buffer2, tx_max*4, this);
			} else {
				txtimer.start(tx_max >= 128 ? 200 : 1500);
			}
			__enable_irq();
			return;
		}
		__enable_irq();
		// TODO: call yield() ??
	}
}

void MIDIDeviceBase::timer_event(USBDriverTimer *timer)
{
	const uint32_t tx_max = tx_size / 4;
	uint32_t tx1 = tx1_count;
	if (tx1 > 0) {
		tx1_count = tx_max;
		queue_Data_Transfer(txpipe, tx_buffer1, tx1*4, this);
	}
	uint32_t tx2 = tx2_count;
	if (tx2 > 0) {
		tx2_count = tx_max;
		queue_Data_Transfer(txpipe, tx_buffer2, tx2*4, this);
	}
}

void MIDIDeviceBase::send_sysex_buffer_has_term(const uint8_t *data, uint32_t length, uint8_t cable)
{
	cable = (cable & 0x0F) << 4;
	while (length > 3) {
		write_packed(0x04 | cable | (data[0] << 8) | (data[1] << 16) | (data[2] << 24));
		data += 3;
		length -= 3;
	}
	if (length == 3) {
		write_packed(0x07 | cable | (data[0] << 8) | (data[1] << 16) | (data[2] << 24));
	} else if (length == 2) {
		write_packed(0x06 | cable | (data[0] << 8) | (data[1] << 16));
	} else if (length == 1) {
		write_packed(0x05 | cable | (data[0] << 8));
	}
}

void MIDIDeviceBase::send_sysex_add_term_bytes(const uint8_t *data, uint32_t length, uint8_t cable)
{
	cable = (cable & 0x0F) << 4;

	if (length == 0) {
		write_packed(0x06 | cable | (0xF0 << 8) | (0xF7 << 16));
		return;
	} else if (length == 1) {
		write_packed(0x07 | cable | (0xF0 << 8) | (data[0] << 16) | (0xF7 << 24));
		return;
	} else {
		write_packed(0x04 | cable | (0xF0 << 8) | (data[0] << 16) | (data[1] << 24));
		data += 2;
		length -= 2;
	}
	while (length >= 3) {
		write_packed(0x04 | cable | (data[0] << 8) | (data[1] << 16) | (data[2] << 24));
		data += 3;
		length -= 3;
	}
	if (length == 2) {
		write_packed(0x07 | cable | (data[0] << 8) | (data[1] << 16) | (0xF7 << 24));
	} else if (length == 1) {
		write_packed(0x06 | cable | (data[0] << 8) | (0xF7 << 16));
	} else {
		write_packed(0x05 | cable | (0xF7 << 8));
	}
}




bool MIDIDeviceBase::read(uint8_t channel)
{
	uint32_t n, head, tail, avail, ch, type1, type2, b1;
	__disable_irq();
	bool packet_queued = rx_packet_queued;
	head = rx_head;
	tail = rx_tail;
	__enable_irq();
	if (head == tail) return false;
	if (++tail >= rx_queue_size) tail = 0;
	n = rx_queue[tail];
	rx_tail = tail;
	if (!packet_queued && rxpipe) {
	        avail = (head < tail) ? tail - head - 1 : rx_queue_size - 1 - head + tail;
		if (avail >= (uint32_t)(rx_size>>2)) {
			rx_packet_queued = true;
			queue_Data_Transfer(rxpipe, rx_buffer, rx_size, this);
		}
	}
	println("read: ", n, HEX);

	type1 = n & 15;
	type2 = (n >> 12) & 15;
	b1 = (n >> 8) & 0xFF;
	ch = (b1 & 15) + 1;
	msg_cable = (n >> 4) & 15;
	if (type1 >= 0x08 && type1 <= 0x0E) {
		if (channel && channel != ch) {
			// ignore other channels when user wants single channel read
			return false;
		}
		if (type1 == 0x08 && type2 == 0x08) {
			msg_type = 0x80;		// 0x80 = Note off
			if (handleNoteOff) {
				(*handleNoteOff)(ch, (n >> 16), (n >> 24));
			}
		} else
		if (type1 == 0x09 && type2 == 0x09) {
			if ((n >> 24) > 0) {
				msg_type = 0x90;	// 0x90 = Note on
				if (handleNoteOn) {
					(*handleNoteOn)(ch, (n >> 16), (n >> 24));
				}
			} else {
				msg_type = 0x80;	// 0x80 = Note off
				if (handleNoteOff) {
					(*handleNoteOff)(ch, (n >> 16), (n >> 24));
				}
			}
		} else
		if (type1 == 0x0A && type2 == 0x0A) {
			msg_type = 0xA0;		// 0xA0 = AfterTouchPoly
			if (handleVelocityChange) {
				(*handleVelocityChange)(ch, (n >> 16), (n >> 24));
			}
		} else
		if (type1 == 0x0B && type2 == 0x0B) {
			msg_type = 0xB0;		// 0xB0 = Control Change
			if (handleControlChange) {
				(*handleControlChange)(ch, (n >> 16), (n >> 24));
			}
		} else
		if (type1 == 0x0C && type2 == 0x0C) {
			msg_type = 0xC0;		// 0xC0 = Program Change
			if (handleProgramChange) {
				(*handleProgramChange)(ch, (n >> 16));
			}
		} else
		if (type1 == 0x0D && type2 == 0x0D) {
			msg_type = 0xD0;		// 0xD0 = After Touch
			if (handleAfterTouch) {
				(*handleAfterTouch)(ch, (n >> 16));
			}
		} else
		if (type1 == 0x0E && type2 == 0x0E) {
			msg_type = 0xE0;		// 0xE0 = Pitch Bend
			if (handlePitchChange) {
				int value = ((n >> 16) & 0x7F) | ((n >> 17) & 0x3F80);
				value -= 8192; // 0 to 16383 --> -8192 to +8191
				(*handlePitchChange)(ch, value);
			}
		} else {
			return false;
		}
		return_message:
		msg_channel = ch;
		msg_data1 = (n >> 16);
		msg_data2 = (n >> 24);
		return true;
	}
	if (type1 == 0x02 || type1 == 0x03 || (type1 == 0x05 && b1 >= 0xF1 && b1 != 0xF7)) {
		// system common or system realtime message
		system_common_or_realtime:
		switch (b1) {
		case 0xF1: // usbMIDI.TimeCodeQuarterFrame
			if (handleTimeCodeQuarterFrame) {
				(*handleTimeCodeQuarterFrame)(n >> 16);
			}
			break;
		case 0xF2: // usbMIDI.SongPosition
			if (handleSongPosition) {
				(*handleSongPosition)(((n >> 16) & 0x7F) | ((n >> 17) & 0x3F80));
			}
			break;
		case 0xF3: // usbMIDI.SongSelect
			if (handleSongSelect) {
				(*handleSongSelect)(n >> 16);
			}
			break;
		case 0xF6: // usbMIDI.TuneRequest
			if (handleTuneRequest) {
				(*handleTuneRequest)();
			}
			break;
		case 0xF8: // usbMIDI.Clock
			if (handleClock) {
				(*handleClock)();
			} else if (handleRealTimeSystem) {
				(*handleRealTimeSystem)(0xF8);
			}
			break;
		case 0xFA: // usbMIDI.Start
			if (handleStart) {
				(*handleStart)();
			} else if (handleRealTimeSystem) {
				(*handleRealTimeSystem)(0xFA);
			}
			break;
		case 0xFB: // usbMIDI.Continue
			if (handleContinue) {
				(*handleContinue)();
			} else if (handleRealTimeSystem) {
				(*handleRealTimeSystem)(0xFB);
			}
			break;
		case 0xFC: // usbMIDI.Stop
			if (handleStop) {
				(*handleStop)();
			} else if (handleRealTimeSystem) {
				(*handleRealTimeSystem)(0xFC);
			}
			break;
		case 0xFE: // usbMIDI.ActiveSensing
			if (handleActiveSensing) {
				(*handleActiveSensing)();
			} else if (handleRealTimeSystem) {
				(*handleRealTimeSystem)(0xFE);
			}
			break;
		case 0xFF: // usbMIDI.SystemReset
			if (handleSystemReset) {
				(*handleSystemReset)();
			} else if (handleRealTimeSystem) {
				(*handleRealTimeSystem)(0xFF);
			}
			break;
		default:
			return false; // unknown message, ignore it
		}
		msg_type = b1;
		goto return_message;
	}
	if (type1 == 0x04) {
		sysex_byte(n >> 8);
		sysex_byte(n >> 16);
		sysex_byte(n >> 24);
		return false;
	}
	if (type1 >= 0x05 && type1 <= 0x07) {
		sysex_byte(b1);
		// allow for buggy devices which use code 5 to transmit 1 byte at a time
		// https://forum.pjrc.com/threads/43450?p=164596&viewfull=1#post164596
		if (type1 == 0x05 && b1 != 0xF7) return false;
		if (type1 >= 0x06) sysex_byte(n >> 16);
		if (type1 == 0x07) sysex_byte(n >> 24);
		uint16_t len = msg_sysex_len;
		msg_data1 = len;
		msg_data2 = len >> 8;
		msg_sysex_len = 0;
		msg_type = 0xF0;			// 0xF0 = SystemExclusive
		if (handleSysExPartial) {
			(*handleSysExPartial)(msg_sysex, len, 1);
		} else if (handleSysExComplete) {
			(*handleSysExComplete)(msg_sysex, len);
		}
		return true;
	}
	if (type1 == 0x0F) {
		if (b1 >= 0xF8) {
			goto system_common_or_realtime;
		}
		if (b1 == 0xF0 || msg_sysex_len > 0) {
			// Is this really needed?  Mac OS-X does this, but do any devices?
			sysex_byte(b1);
		}
	}
	return false;
}

void MIDIDeviceBase::sysex_byte(uint8_t b)
{
	if (handleSysExPartial && msg_sysex_len >= SYSEX_MAX_LEN) {
		// when buffer is full, send another chunk to partial handler.
		(*handleSysExPartial)(msg_sysex, msg_sysex_len, 0);
		msg_sysex_len = 0;
	}
	if (msg_sysex_len < SYSEX_MAX_LEN) {
		msg_sysex[msg_sysex_len++] = b;
	}
}




