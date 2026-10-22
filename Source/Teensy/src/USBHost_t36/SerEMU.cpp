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

//#define  SEREMU_PRINT_DEBUG
#ifdef SEREMU_PRINT_DEBUG
#define DBGPrintf USBHDBGSerial.printf
#else
// not debug have it do nothing
inline void DBGPrintf(...) {
}
#endif



void USBSerialEmu::init()
{
	USBHost::contribute_Transfers(mytransfers, sizeof(mytransfers)/sizeof(Transfer_t));
	USBHIDParser::driver_ready_for_hid_collection(this);	
}

hidclaim_t USBSerialEmu::claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage)
{
	// only claim SerEMU devices currently: 16c0:0486
	DBGPrintf("SerEMU Claim: %x:%x usage: %x\n", dev->idVendor, dev->idProduct, topusage);

	if (dev->idVendor != 0x16c0) return CLAIM_NO;  //  NOT PJRC
	if (mydevice != NULL && dev != mydevice) return CLAIM_NO;
	if (usage_) return CLAIM_NO;			// Only claim one

	// make sure it is the SEREMU usage
	if (topusage != 0xffc90004) return CLAIM_NO; 			// Not the SEREMU

	mydevice = dev;
	collections_claimed++;
	usage_ = topusage;
	driver_ = driver;	// remember the driver. 
	rx_head_ = 0;// receive head
	rx_tail_ = 0;// receive tail
	tx_head_ = 0;
	rx_pipe_size_ = driver->inSize();
	tx_pipe_size_ = driver->outSize();
	tx_out_data_pending_ = 0;


	//if (setup.word1 == 0x03000921 && setup.word2 == ((4<<16)|SEREMU_INTERFACE)) {
	tx_buffer_[0] = 0;
	tx_buffer_[1] = 0;
	tx_buffer_[2] = 0;
	tx_buffer_[3] = 0;
	driver_->sendControlPacket( 0x21, 0x9, 0x300, driver_->interfaceNumber(), 4, tx_buffer_);

	return CLAIM_INTERFACE;  // We wa
}

void USBSerialEmu::disconnect_collection(Device_t *dev)
{
	if (--collections_claimed == 0) {
		mydevice = NULL;
		usage_ = 0;
	}
}

bool USBSerialEmu::hid_process_in_data(const Transfer_t *transfer) 
{
	uint16_t len = transfer->length;
	const uint8_t *buffer = (const uint8_t *)transfer->buffer;
	DBGPrintf("USBSerialEmu::hid_process_in_data: %x %d: %x %x %x\n", usage_, len, buffer[0], buffer[1], buffer[2]);
	// try using version like serial that uses memcpy.
	while ((len > 0) && (buffer[len-1] == 0)) len--; // find out the length
	// Copy data from packet buffer to circular buffer.
	uint32_t head = rx_head_;
	uint32_t tail = rx_tail_;
	uint32_t buffer_free;
	if (head >= tail) buffer_free =  RX_BUFFER_SIZE - (head - tail);
	else buffer_free =  head - tail;

	if (++head >= RX_BUFFER_SIZE) head = 0;

	if (len > 0) {
		DBGPrintf("\tH:%u T:%u A:%u L:%u\n", head, tail, buffer_free, len);
		if (buffer_free > len) buffer_free = len;
		memcpy(rx_buffer_ + head, buffer, buffer_free);
		if (len <= buffer_free) {
			head += buffer_free - 1;
			if (head >= RX_BUFFER_SIZE) head = 0;
		} else {
			head = len - buffer_free - 1;
			memcpy(rx_buffer_, buffer + RX_BUFFER_SIZE, head + 1);
		}
		rx_head_ = head;
		DBGPrintf("\tNH:%u T:%u\n", rx_head_, rx_tail_);
	}

	return true;
}

bool USBSerialEmu::hid_process_out_data(const Transfer_t *transfer) 
{
	DBGPrintf("USBSerialEmu::hid_process_out_data: %x\n", usage_);
	if (tx_out_data_pending_) {
		tx_out_data_pending_--;

	}
	return true;
}

bool USBSerialEmu::sendPacket() 
{
	DBGPrintf("SEMU: SendPacket\n");

	if (!driver_) return false;
	if (!driver_->sendPacket(tx_buffer_)) return false;
	tx_out_data_pending_++;
	tx_head_ = 0;
	return true;
}


int USBSerialEmu::available(void)
{
	if (!driver_) return 0;
	uint32_t head = rx_head_;
	uint32_t tail = rx_tail_;
	if (head >= tail) return head - tail;
	return RX_BUFFER_SIZE + head - tail;
}

int USBSerialEmu::peek(void)
{
	if (!driver_) return -1;
	if (rx_head_ == rx_tail_) return -1;	
	uint16_t tail = rx_tail_ + 1;
	if (tail >= RX_BUFFER_SIZE) tail = 0;
	return rx_buffer_[tail];
}

int USBSerialEmu::read(void)
{
	if (!driver_) return -1;
	if (rx_head_ == rx_tail_) return -1;
	if (++rx_tail_ >= RX_BUFFER_SIZE) rx_tail_ = 0;
	
	int c = rx_buffer_[rx_tail_];
	return c;
}

int USBSerialEmu::availableForWrite()
{
	if (!driver_) return 0;
	return tx_pipe_size_ - tx_head_ + (2-tx_out_data_pending_)*tx_pipe_size_;
}

size_t USBSerialEmu::write(uint8_t c)
{
	// Single buffer, as our HID device has double buffers. 
	#ifdef SEREMU_PRINT_DEBUG
	if (c >= ' ') DBGPrintf("SEMU: %c\n", c);
	else DBGPrintf("SEMU: 0x%x\n", c);
	#endif

	if (!driver_) return 0;

	if (tx_head_ == tx_pipe_size_) {
		while (!sendPacket()) yield();	// wait until the device above queues this packet 
	}
	tx_buffer_[tx_head_++] = c;

	// if this character filled it. then try to queue it again
	if (tx_head_ == tx_pipe_size_) sendPacket();
	driver_->stopTimer();
	driver_->startTimer(write_timeout_);
	return 1;
}

void USBSerialEmu::flush(void) 
{
	if (!driver_) return;

	DBGPrintf("SEMU: flush\n");
	driver_->stopTimer();  		// Stop longer timer.
	driver_->startTimer(100);		// Start a mimimal timeout
	if (tx_head_) sendPacket();

	// And wait for HID to say they were all sent.
	elapsedMillis em = 0;
	while (tx_out_data_pending_ &&  (em < 10000)) yield(); // wait up to 10 seconds?
}

void USBSerialEmu::hid_timer_event(USBDriverTimer *whichTimer)
{
	DBGPrintf("SEMU: Timer\n");
	if (!driver_) return;
	driver_->stopTimer();
	if (tx_head_) {
		memset(tx_buffer_ + tx_head_, 0, tx_pipe_size_ - tx_head_);	// clear the rest of bytes in buffer.
		sendPacket();
	}
}

void USBSerialEmu::hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax)
{
	// These should not be called as we are claiming the whole interface and not
	// allowing the parse to happen
	DBGPrintf("SerEMU::hid_input_begin %x %x %x %x\n", topusage, type, lgmin, lgmax);
	//hid_input_begin_ = true;
}

void USBSerialEmu::hid_input_data(uint32_t usage, int32_t value)
{
	// These should not be called as we are claiming the whole interface and not
	// allowing the parse to happen
#ifdef SEREMU_PRINT_DEBUG
	DBGPrintf("SerEMU: usage=%X, value=%d", usage, value);
	if ((value >= ' ') && (value <='~')) DBGPrintf("(%c)", value);
	USBHDBGSerial.println();
#endif
}

void USBSerialEmu::hid_input_end()
{
	// These should not be called as we are claiming the whole interface and not
	// allowing the parse to happen
#ifdef SEREMU_PRINT_DEBUG
	USBHDBGSerial.println("SerEMU::hid_input_end");
#endif
//	if (hid_input_begin_) {
//		hid_input_begin_ = false;
//	}
}
