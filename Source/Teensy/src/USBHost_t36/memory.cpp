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


// Memory allocation for Device_t, Pipe_t and Transfer_t structures.
//
// To provide an Arduino-friendly experience, the memory allocation of
// these item is primarily done by the instances of device driver objects,
// which are typically created as static objects near the beginning of
// the Arduino sketch.  Static allocation allows Arduino's memory usage
// summary to accurately show the amount of RAM this library is using.
// Users can choose which devices they wish to support and how many of
// each by creating more object instances.
//
// Device driver objects "contribute" their copies of these structures.
// When ehci.cpp allocates Pipe_t and Transfer_t, or enumeration.cpp
// allocates Device_t, the memory actually comes from these structures
// physically located within the device driver instances.  The usage
// model looks like traditional malloc/free dynamic memory on the heap,
// but in fact it's a simple memory pool from the drivers.
//
// Timing is deterministic and fast, because each pool allocates only
// a single fixed size object.  In theory, each driver should contribute
// the number of items it will use, so we should not ever end up with
// a situation where an item can't be allocated when it's needed.  Well,
// unless there's a bug or oversight...


// Lists of "free" memory
static Device_t * free_Device_list = NULL;
static Pipe_t * free_Pipe_list = NULL;
static Transfer_t * free_Transfer_list = NULL;
static strbuf_t * free_strbuf_list = NULL;
// A small amount of non-driver memory, just to get things started
// TODO: is this really necessary?  Can these be eliminated, so we
// use only memory from the drivers?
static Device_t memory_Device[1];
static Pipe_t memory_Pipe[1] __attribute__ ((aligned(32)));
static Transfer_t memory_Transfer[4] __attribute__ ((aligned(32)));

void USBHost::init_Device_Pipe_Transfer_memory(void)
{
	contribute_Devices(memory_Device, sizeof(memory_Device)/sizeof(Device_t));
	contribute_Pipes(memory_Pipe, sizeof(memory_Pipe)/sizeof(Pipe_t));
	contribute_Transfers(memory_Transfer, sizeof(memory_Transfer)/sizeof(Transfer_t));
}

Device_t * USBHost::allocate_Device(void)
{
	Device_t *device = free_Device_list;
	if (device) free_Device_list = *(Device_t **)device;
	return device;
}

void USBHost::free_Device(Device_t *device)
{
	*(Device_t **)device = free_Device_list;
	free_Device_list = device;
}

Pipe_t * USBHost::allocate_Pipe(void)
{
	Pipe_t *pipe = free_Pipe_list;
	if (pipe) free_Pipe_list = *(Pipe_t **)pipe;
	return pipe;
}

void USBHost::free_Pipe(Pipe_t *pipe)
{
	*(Pipe_t **)pipe = free_Pipe_list;
	free_Pipe_list = pipe;
}

Transfer_t * USBHost::allocate_Transfer(void)
{
	Transfer_t *transfer = free_Transfer_list;
	if (transfer) free_Transfer_list = *(Transfer_t **)transfer;
	return transfer;
}

void USBHost::free_Transfer(Transfer_t *transfer)
{
	*(Transfer_t **)transfer = free_Transfer_list;
	free_Transfer_list = transfer;
}

strbuf_t * USBHost::allocate_string_buffer(void)
{
	strbuf_t *strbuf = free_strbuf_list;
	if (strbuf) {
		free_strbuf_list = *(strbuf_t **)strbuf;
		strbuf->iStrings[strbuf_t::STR_ID_MAN] = 0;  // Set indexes into string buffer to say not there...
		strbuf->iStrings[strbuf_t::STR_ID_PROD] = 0;
		strbuf->iStrings[strbuf_t::STR_ID_SERIAL] = 0;
		strbuf->buffer[0] = 0;	// have trailing NULL..
	} 
	return strbuf;
}

void USBHost::free_string_buffer(strbuf_t *strbuf) 
{
	*(strbuf_t **)strbuf = free_strbuf_list;
	free_strbuf_list = strbuf;
}

void USBHost::contribute_Devices(Device_t *devices, uint32_t num)
{
	Device_t *end = devices + num;
	for (Device_t *device = devices ; device < end; device++) {
		free_Device(device);
	}
}

void USBHost::contribute_Pipes(Pipe_t *pipes, uint32_t num)
{
	Pipe_t *end = pipes + num;
	for (Pipe_t *pipe = pipes; pipe < end; pipe++) {
		free_Pipe(pipe);
	}

}

void USBHost::contribute_Transfers(Transfer_t *transfers, uint32_t num)
{
	Transfer_t *end = transfers + num;
	for (Transfer_t *transfer = transfers ; transfer < end; transfer++) {
		free_Transfer(transfer);
	}
}

void USBHost::contribute_String_Buffers(strbuf_t *strbufs, uint32_t num)
{
	strbuf_t *end = strbufs + num;
	for (strbuf_t *str = strbufs ; str < end; str++) {
		free_string_buffer(str);
	}
}

// for debugging, hopefully never needed...
void USBHost::countFree(uint32_t &devices, uint32_t &pipes, uint32_t &transfers, uint32_t &strs)
{
	uint32_t ndev=0, npipe=0, ntransfer=0, nstr=0;
	__disable_irq();
	Device_t *dev = free_Device_list;
	while (dev) {
		ndev++;
		dev = *(Device_t **)dev;
	}
	Pipe_t *pipe = free_Pipe_list;
	while (pipe) {
		npipe++;
		pipe = *(Pipe_t **)pipe;
	}
	Transfer_t *transfer = free_Transfer_list;
	while (transfer) {
		ntransfer++;
		transfer = *(Transfer_t **)transfer;
	}
	strbuf_t *str = free_strbuf_list;
	while (str) {
		nstr++;
		str = *(strbuf_t **)str;
	}
	__enable_irq();
	devices = ndev;
	pipes = npipe;
	transfers = ntransfer;
	strs = nstr;
}
