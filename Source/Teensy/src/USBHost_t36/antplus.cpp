/* USB EHCI Host for Teensy 3.6
 * Copyright 2017 Michael McElligott
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

// https://forum.pjrc.com/threads/43110
// https://github.com/PaulStoffregen/antplus

#include <Arduino.h>
#include "USBHost_t36.h"  // Read this header first for key info

#include "antplusdefs.h"  // Ant internal defines, not for Arduino sketches

#define ANTPLUS_VID     0x0FCF
#define ANTPLUS_2_PID   0x1008
#define ANTPLUS_M_PID   0x1009

#define print   USBHost::print_
#define println USBHost::println_


#define ENABLE_SERIALPRINTF   1

#if ENABLE_SERIALPRINTF
#undef printf
#define printf(...) Serial.printf(__VA_ARGS__); Serial.write("\r\n")
#else
#undef printf
#define printf(...)    
#endif


void AntPlus::init()
{
	contribute_Pipes(mypipes, sizeof(mypipes)/sizeof(Pipe_t));
	contribute_Transfers(mytransfers, sizeof(mytransfers)/sizeof(Transfer_t));
	contribute_String_Buffers(mystring_bufs, sizeof(mystring_bufs)/sizeof(strbuf_t));
	driver_ready_for_device(this);
	user_onStatusChange = NULL;
	user_onDeviceID = NULL;
	user_onHeartRateMonitor = NULL;
	user_onSpeedCadence = NULL;
	user_onSpeed = NULL;
	user_onCadence = NULL;
	wheelCircumference = WHEEL_CIRCUMFERENCE;
}

bool AntPlus::claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len)
{
	if (type != 1) return false;
	println("AntPlus claim this=", (uint32_t)this, HEX);
	if (dev->idVendor != ANTPLUS_VID) return false;
	if (dev->idProduct != ANTPLUS_2_PID && dev->idProduct != ANTPLUS_M_PID) return false;
	println("found AntPlus, pid=", dev->idProduct, HEX);
	rxpipe = txpipe = NULL;
	const uint8_t *p = descriptors;
	const uint8_t *end = p + len;
	int descriptorLength = p[0];
	int descriptorType = p[1];
	if (descriptorLength < 9 || descriptorType != 4) return false;
	p += descriptorLength;
	while (p < end) {
		descriptorLength = p[0];
		if (p + descriptorLength > end) return false; // reject if beyond end of data
		descriptorType = p[1];
		if (descriptorType == 5) { // 5 = endpoint
			uint8_t epAddr = p[2];
			uint8_t epType = p[3] & 0x03;
			uint16_t epSize = p[4] | (p[5] << 8);
			if (epType == 2 && (epAddr & 0xF0) == 0x00) { // Bulk OUT
				txpipe = new_Pipe(dev, 2, epAddr, 0, epSize);
			} else if (epType == 2 && (epAddr & 0xF0) == 0x80) { // Bulk IN
				rxpipe = new_Pipe(dev, 2, epAddr & 0x0F, 1, epSize);
			}
		}
		p += descriptorLength;
	}
	if (rxpipe && txpipe) {
		rxpipe->callback_function = rx_callback;
		txpipe->callback_function = tx_callback;
		txhead = 0;
		txtail = 0;
		//rxhead = 0;
		//rxtail = 0;
		memset(txbuffer, 0, sizeof(txbuffer));
		first_update = true;
		txready = true;
		updatetimer.start(500000);
		queue_Data_Transfer(rxpipe, rxpacket, 64, this);
		rxlen = 0;
		do_polling = false;
		return true;
	}
	return false;
}

void AntPlus::disconnect()
{
	updatetimer.stop();
	//txtimer.stop();
}


void AntPlus::rx_callback(const Transfer_t *transfer)
{
	if (!transfer->driver) return;
	((AntPlus *)(transfer->driver))->rx_data(transfer);
}

void AntPlus::tx_callback(const Transfer_t *transfer)
{
        if (!transfer->driver) return;
        ((AntPlus *)(transfer->driver))->tx_data(transfer);
}

void AntPlus::rx_data(const Transfer_t *transfer)
{
	uint32_t len = transfer->length - ((transfer->qtd.token >> 16) & 0x7FFF);
	//println("ant rx, len=", len);
	//print_hexbytes(transfer->buffer, len);
	if (len < 1 || len > 64) {
		queue_Data_Transfer(rxpipe, rxpacket, 64, this);
		rxlen = 0;
	} else {
		rxlen = len; // signal arrival of data to Task()
		// TODO: should someday use EventResponder to call from yield()
	}
}

void AntPlus::tx_data(const Transfer_t *transfer)
{
	uint8_t *p = (uint8_t *)transfer->buffer;
	//print("tx_data, len=", *(p-1));
	//print(", tail=", (p-1) - txbuffer);
	//println(", tail=", txtail);
	uint32_t tail = txtail;
	uint8_t size = *(p-1);
	tail += size + 1;
	if (tail >= sizeof(txbuffer)) tail -= sizeof(txbuffer);
	txtail = tail;
	//println("new tail=", tail);
	txready = true;
	transmit();
	//txtimer.start(8000);
	// adjust tail...
	// start timer if more data to send
}


size_t AntPlus::write(const void *data, const size_t size)
{
	//print("write ", size);
	//print(" bytes: ");
	//print_hexbytes(data, size);
	if (size > 64) return 0;
	uint32_t head = txhead;
	if (++head >= sizeof(txbuffer)) head = 0;
	uint32_t remain = sizeof(txbuffer) - head;
	if (remain < size + 1) {
		// not enough space at end of buffer
		txbuffer[head] = 0xFF;
		head = 0;
	}
	uint32_t avail;
	uint8_t attempts = 100;
	do {
		uint32_t tail = txtail;
		if (head > tail) {
			avail = sizeof(txbuffer) - head + tail;
		} else {
			avail = tail - head;
		}
		attempts--;
	} while ((avail < size + 1) && attempts > 0 ); // wait for space in buffer
	txbuffer[head] = size;
	memcpy(txbuffer + head + 1, data, size);
	txhead = head + size;
	//print("head=", txhead);
	//println(", tail=", txtail);
	//print_hexbytes(txbuffer, 60);
	NVIC_DISABLE_IRQ(IRQ_USBHS);
	transmit();
	NVIC_ENABLE_IRQ(IRQ_USBHS);
	return size;
}

void AntPlus::transmit()
{
	if (!txready) return;
	uint32_t head = txhead;
	uint32_t tail = txtail;
	if (head == tail) {
		//println("no data to transmit");
		return; // no data to transit
	}
	if (++tail >= sizeof(txbuffer)) tail = 0;
	uint32_t size = txbuffer[tail];
	//print("tail=", tail);
	//println(", tx size=", size);
	if (size == 0xFF) {
		txtail = 0;
		tail = 0;
		size = txbuffer[0];
		//println("tx size=", size);
	}
	//txtail = tail + size;
	queue_Data_Transfer(txpipe, txbuffer + tail + 1, size, this);
	//txtimer.start(8000);
	txready = false;
}

void AntPlus::timer_event(USBDriverTimer *whichTimer)
{
	if (whichTimer == &updatetimer) {
		updatetimer.start(250000);
		if (first_update) {
			ResetSystem();
			first_update = false;
		} else {
			do_polling = true;
		}
		//println("ant update timer");
	}
	/* else if (whichTimer == &txtimer) {
		println("ant tx timer");
		//txtimer.stop(); // TODO: why is this needed?
		txready = true;
		transmit();
	} */
}


void AntPlus::Task()
{
	uint32_t len = rxlen;
	if (len) {
		handleMessages(rxpacket, len);
		NVIC_DISABLE_IRQ(IRQ_USBHS);
		queue_Data_Transfer(rxpipe, rxpacket, 64, this);
		rxlen = 0;
		NVIC_ENABLE_IRQ(IRQ_USBHS);
	}
	if (do_polling) {
		do_polling = false;
		for (int i = 0; i < PROFILE_TOTAL; i++) {
			TDCONFIG *cfg = &ant.dcfg[i];
			if (!(cfg->flags.profileValid)) continue;
			//printf("#### %i %i: %i %i %i ####", i, cfg->channel,
			// cfg->flags.channelStatus, cfg->flags.keyAccepted,
			// cfg->flags.chanIdOnce);
			if (cfg->flags.channelStatus) {
				RequestMessage(cfg->channel, MESG_CHANNEL_STATUS_ID);
			} else {
				AssignChannel(cfg->channel, cfg->channelType, cfg->networkNumber);
				RequestMessage(cfg->channel, MESG_CHANNEL_STATUS_ID);
				if (!cfg->flags.keyAccepted && !cfg->flags.chanIdOnce) {
					SetNetworkKey(cfg->networkNumber, getAntKey(ant.key));
				}
			}
		}
	}
}



enum _akeys {
	KEY_ANTSPORT,
	KEY_SUUNTO,
	KEY_GARMIN,
	KEY_ANTPLUS,
	KEY_TOTAL,
	KEY_DEFAULT = KEY_ANTSPORT
};
static const uint8_t antkeys[KEY_TOTAL][8] = {
{0xB9,0xA5,0x21,0xFB,0xBD,0x72,0xC3,0x45},  // Ant+ sport key  KEY_ANTSPORT
{0xB9,0xAD,0x32,0x28,0x75,0x7E,0xC7,0x4D},  // Suunto          KEY_SUUNTO
{0xA8,0xA4,0x23,0xB9,0xF5,0x5E,0x63,0xC1},  // Garmin          KEY_GARMIN
{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}   // Ant+ key        KEY_ANTPLUS (add your key here)
//{0xFD,0x38,0xBE,0xA6,0x40,0x5D,0x26,0x99}
};


uint8_t AntPlus::calcMsgChecksum(const uint8_t *buffer, const uint8_t len)
{
	uint8_t checksum = 0x00;
	for (uint8_t i = 0; i < len; i++)
		checksum ^= buffer[i];
	return checksum;
}

uint8_t * AntPlus::findStreamSync(uint8_t *stream, const size_t rlen, int *pos)
{
	// find and sync with input stream
	*pos = 0;
	while (*pos < (int)rlen /*&& *pos < INPUTBUFFERSIZE-3*/){
		if (stream[*pos] == MESG_TX_SYNC)
			return stream + *pos;
		(*pos)++;
	}
	return NULL;
}

int AntPlus::msgCheckIntegrity(uint8_t *stream, const int len)
{
	// min message length is 5
	if (len < 5) return 0;

	int crc = stream[STREAM_SYNC];
	crc ^= stream[STREAM_LENGTH];
	crc ^= stream[STREAM_MESSAGE];
	int mlen = 0;

	do{
		crc ^= stream[STREAM_DATA+mlen];
	} while (++mlen < stream[STREAM_LENGTH]);

	//printf("crc == 0x%X: msg crc = 0x%X\n", crc, stream[stream[STREAM_LENGTH] + 3]);
	return (crc == stream[stream[STREAM_LENGTH] + 3]);
}

int AntPlus::msgGetLength (uint8_t *stream)
{
	// eg; {A4 1 6F 20 EA} = {SYNC DATALEN MSGID DATA CRC}
	return stream[STREAM_LENGTH] + 4;
}

int AntPlus::handleMessages(uint8_t *buffer, int tBytes)
{
	int syncOffset = 0;
	//uint8_t buffer[ANTPLUS_MAXPACKETSIZE];
	uint8_t *stream = buffer;

	//int tBytes = antplus_read(ant, buffer, ant->ioVar.readBufferSize);
	//if (tBytes <= 0) return tBytes;

	//int tBytes = ANTPLUS_MAXPACKETSIZE;

	while (tBytes > 0){
		stream = findStreamSync(stream, tBytes, &syncOffset);
		if (stream == NULL){
			//printf("stream sync not found {size:%i}\n", tBytes);
			return 0;
		}
		tBytes -= syncOffset;

		if (!msgCheckIntegrity(stream, tBytes)){
			//printf("stream integrity failed {size:%i}\n", tBytes);
			return 0;
		}

		//we have a valid message
		//if (dispatchMessage(stream, tBytes) == -1){
			//printf("quiting..\n");
			//return 0;
		//}
		message_event(stream[STREAM_CHANNEL], stream[STREAM_MESSAGE],
			&stream[STREAM_DATA], (size_t)stream[STREAM_LENGTH]);

		int len = msgGetLength(stream);
		stream += len;
		tBytes -= len;
	}
	return 1;
}


void AntPlus::sendMessageChannelStatus(TDCONFIG *cfg, const uint32_t channelStatus)
{
	cfg->flags.channelStatus = channelStatus;
	if (cfg->flags.channelStatus != cfg->flags.channelStatusOld) {
		if (user_onStatusChange) {
			(*user_onStatusChange)(cfg->channel, cfg->flags.channelStatus);
		}
		cfg->flags.channelStatusOld = cfg->flags.channelStatus;
	}
}

void AntPlus::message_channel(const int chan, const int eventId,
	const uint8_t *payload, const size_t dataLength)
{
	//printf(" $ chan event: chan:%i, msgId:0x%.2X, payload:%p, dataLen:%i, uPtr:%p", chan, eventId, payload, (int)dataLength, uPtr);
	//dump_hexbytes(payload, dataLength);

	TDCONFIG *cfg = &(ant.dcfg[chan]);

	switch (eventId){
	  case EVENT_RX_SEARCH_TIMEOUT:
	  	printf(" $ event RX search timeout");
	  	break;

	  case EVENT_RX_FAIL:
	  	//printf(" $ event RX fail");
	  	break;

	  case EVENT_TX:
	  	//printf(" $ event TX");
	  	break;

	  case EVENT_RX_BROADCAST:
	  	//printf(" $ event RX broadcast ");
	  	if (!cfg->flags.chanIdOnce) {
	  		cfg->flags.chanIdOnce = 1;
	  		RequestMessage(cfg->channel, MESG_CHANNEL_ID_ID);
	  	}
		//dump_hexbytes(payload, dataLength);
		dispatchPayload(cfg, payload, dataLength);
		break;
	 }
}

void AntPlus::message_response(const int chan, const int msgId,
	const uint8_t *payload, const size_t dataLength)
{
	//printf(" # response event: msgId:0x%.2X, payload:%p, dataLen:%i, uPtr:%p", msgId, payload, dataLength, uPtr);
	TDCONFIG *cfg = (TDCONFIG*)&(ant.dcfg[chan]);

	switch (msgId){
	  case MESG_EVENT_ID:
	  	//printf(" * event");
	  	message_channel(chan, payload[STREAM_EVENT_EVENTID], payload, dataLength);
	  	break;

	  case MESG_NETWORK_KEY_ID:
	  	printf("[%i] * network key accepted", chan);
	  	cfg->flags.keyAccepted = 1;
	  	if (cfg->transType == ANT_TRANSMISSION_MASTER)
	  		AssignChannel(cfg->channel, PARAMETER_TX_NOT_RX, cfg->networkNumber);
	  	else
	  		AssignChannel(cfg->channel, cfg->channelType, cfg->networkNumber);
	  	break;

	  case MESG_ASSIGN_CHANNEL_ID:
	  	printf("[%i]  * channel assign accepted", chan);
	  	SetChannelPeriod(cfg->channel, cfg->channelPeriod);
	  	break;

	  case MESG_CHANNEL_MESG_PERIOD_ID:
		printf("[%i]  * channel mesg period accepted", chan);
		SetChannelSearchTimeout(cfg->channel, cfg->searchTimeout);
		break;

	  case MESG_CHANNEL_SEARCH_TIMEOUT_ID:
	  	printf("[%i]  * search timeout period accepted", chan);
	  	SetChannelRFFreq(cfg->channel, cfg->RFFreq);
	  	break;

	  case MESG_CHANNEL_RADIO_FREQ_ID:
	  	printf("[%i]  * radio freq accepted", chan);
	  	SetSearchWaveform(cfg->channel, cfg->searchWaveform);
	  	break;

	  case MESG_SEARCH_WAVEFORM_ID:
	  	printf("[%i]  * search waveform accepted", chan);
	  	SetChannelId(cfg->channel, cfg->deviceNumber, cfg->deviceType, cfg->transType);
	  	break;

	  case MESG_CHANNEL_ID_ID:
	  	printf("[%i]  * set channel id accepted", chan);
	  	OpenChannel(cfg->channel);
	  	break;

	  case MESG_OPEN_CHANNEL_ID:
	  	printf("[%i]  * open channel accepted", chan);
	  	//cfg->flags.channelStatus = 1;
	  	RequestMessage(cfg->channel, MESG_CHANNEL_STATUS_ID);
	  	RequestMessage(cfg->channel, MESG_CAPABILITIES_ID);
	  	RequestMessage(cfg->channel, MESG_VERSION_ID);
	  	break;

 	  case MESG_UNASSIGN_CHANNEL_ID:
		printf("[%i]  * channel Unassigned", chan);
		break;

	  case MESG_CLOSE_CHANNEL_ID:
		printf("[%i]  * channel CLOSED", chan);
		cfg->flags.keyAccepted = 0;
		sendMessageChannelStatus(cfg, ANT_CHANNEL_STATUS_UNASSIGNED);
		break;

 	  case CHANNEL_IN_WRONG_STATE:
		printf("[%i]  * channel in wrong state", chan);
		break;

 	  case CHANNEL_NOT_OPENED:
		printf("[%i]  * channel not opened", chan);
		break;

 	  case CHANNEL_ID_NOT_SET: //??
		printf("[%i]  * channel ID not set", chan);
		break;

 	  case CLOSE_ALL_CHANNELS: // Start RX Scan mode
		printf("[%i]  * close all channels", chan);
		break;

 	  case TRANSFER_IN_PROGRESS: // TO ack message ID
		printf("[%i]  * tranfer in progress", chan);
		break;

 	  case TRANSFER_SEQUENCE_NUMBER_ERROR:
		printf("[%i]  * transfer sequence number error", chan);
		break;

 	  case TRANSFER_IN_ERROR:
		printf("[%i]  * transfer in error", chan);
		break;

 	  case INVALID_MESSAGE:
		printf("[%i]  * invalid message", chan);
		break;

 	  case INVALID_NETWORK_NUMBER:
		printf("[%i]  * invalid network number", chan);
		break;

 	  case INVALID_LIST_ID:
		printf("[%i]  * invalid list ID", chan);
		break;

 	  case INVALID_SCAN_TX_CHANNEL:
		printf("[%i]  * invalid Scanning transmit channel", chan);
		break;

 	  case INVALID_PARAMETER_PROVIDED:
		printf("[%i]  * invalid parameter provided", chan);
   		break;

 	  case EVENT_QUE_OVERFLOW:
		printf("[%i]  * queue overflow", chan);
		break;

	  default:
	  	printf("[i] #### unhandled response id %i", chan, msgId);
		;
	};
}


void AntPlus::message_event(const int channel, const int msgId,
	const uint8_t *payload, const size_t dataLength)
{
	//printf(" @ msg event cb: Chan:%i, Id:0x%.2X, payload:%p, len:%i, ptr:%p", channel, msgId, payload, (int)dataLength, uPtr);
	//dump_hexbytes(payload, dataLength);

	uint8_t chan = 0;
	if (channel >= 0 && channel < PROFILE_TOTAL) chan = channel;

	switch(msgId) {
	  case MESG_BROADCAST_DATA_ID:
	  	//printf(" @ broadcast data \n");
		//dumpPayload(payload, dataLength);
		message_channel(chan, EVENT_RX_BROADCAST, payload, dataLength);
	  	break;

	  case MESG_STARTUP_MESG_ID:
	  	// reason == ANT_STARTUP_RESET_xxxx
	  	//printf(" @ start up mesg reason: 0x%X", payload[STREAM_STARTUP_REASON]);
	  	//TDCONFIG *cfg = &(ant.dcfg[0]);
		//SetNetworkKey(cfg->networkNumber, getAntKey(cfg->keyIdx));
		SetNetworkKey(ant.dcfg[0].networkNumber, getAntKey(ant.key));
		break;

	  case MESG_RESPONSE_EVENT_ID:
	  	message_response(payload[STREAM_EVENT_CHANNEL_ID],
			payload[STREAM_EVENT_RESPONSE_ID], payload, dataLength);
	  	break;

	  case MESG_CHANNEL_STATUS_ID:
	  	//printf(" @ channel status for channel %i is %i",
		//   payload[STREAM_CHANNEL_ID], payload[STREAM_CHANNEL_STATUS]);
	  	//TDCONFIG *cfg = (TDCONFIG*)&ant->dcfg[payload[STREAM_CHANNEL_ID]];
	  	sendMessageChannelStatus(&(ant.dcfg[payload[STREAM_CHANNEL_ID]]),
			payload[STREAM_CHANNELSTATUS_STATUS] & ANT_CHANNEL_STATUS_MASK);
		//if (cfg->flags.channelStatus != STATUS_TRACKING_CHANNEL)
		//	printf("channel %i status: %s", payload[STREAM_CHANNEL_ID],
		//	 channelStatusStr[cfg->flags.channelStatus]);
	  	break;

	  case MESG_CAPABILITIES_ID:
		printf(" @ capabilities:");
		printf("   Max ANT Channels: %i",payload[STREAM_CAP_MAXCHANNELS]);
		printf("   Max ANT Networks: %i",payload[STREAM_CAP_MAXNETWORKS]);
		printf("   Std. option: 0x%X",payload[STREAM_CAP_STDOPTIONS]);
		printf("   Advanced: 0x%X",payload[STREAM_CAP_ADVANCED]);
		printf("   Advanced2: 0x%X",payload[STREAM_CAP_ADVANCED2]);
	  	break;

	case MESG_CHANNEL_ID_ID:
		//TDCONFIG *cfg = (TDCONFIG*)&ant->dcfg[chan];
        	//ant.dcfg[chan].dev.deviceId = payload[STREAM_CHANNELID_DEVNO_LO] | (payload[STREAM_CHANNELID_DEVNO_HI] << 8);
		//ant.dcfg[chan].dev.deviceType = payload[STREAM_CHANNELID_DEVTYPE];
		//ant.dcfg[chan].dev.transType = payload[STREAM_CHANNELID_TRANTYPE];
		//printf(" @ CHANNEL ID: channel %i, deviceId:%i, deviceType:%i, transType:%i)", chan, cfg->dev.deviceId, cfg->dev.deviceType, cfg->dev.transType);
		if (user_onDeviceID) {
			int devid = payload[STREAM_CHANNELID_DEVNO_LO];
			devid |= payload[STREAM_CHANNELID_DEVNO_HI] << 8;
			int devtype = payload[STREAM_CHANNELID_DEVTYPE];
			int transtype = payload[STREAM_CHANNELID_TRANTYPE];
			(*user_onDeviceID)(chan, devid, devtype, transtype);
		}
#if 0
		if (cfg->dev.scidDeviceType != cfg->deviceType){
			printf(" @ CHANNEL ID: this is not the device we're looking for");
			printf(" @ CHANNEL ID: expecting 0x%X but found 0x%X", cfg->deviceType, cfg->dev.scidDeviceType);
         		//CloseChannel(cfg->channel);
         		return;
         	}
#endif
		break;

	case MESG_VERSION_ID:
		printf(" @ version: '%s'", (char*)&payload[STREAM_VERSION_STRING]);
		break;
	};
}


int AntPlus::ResetSystem()
{
	//println("libantplus_ResetSystem");
	uint8_t msg[5];
	msg[0] = MESG_TX_SYNC;			// sync
	msg[1] = 1;						// length
	msg[2] = MESG_SYSTEM_RESET_ID;	// msg id
	msg[3] = 0;						// nop
	msg[4] = calcMsgChecksum(msg, 4);
	return write(msg, 5);
}

int AntPlus::RequestMessage(const int channel, const int message)
{
	//println("libantplus_RequestMessage");
	uint8_t msg[6];
	msg[0] = MESG_TX_SYNC;			// sync
	msg[1] = 2;						// length
	msg[2] = MESG_REQUEST_ID;		// msg id
	msg[3] = (uint8_t)channel;
	msg[4] = (uint8_t)message;
	msg[5] = calcMsgChecksum(msg, 5);
	return write(msg, 6);
}

int AntPlus::SetNetworkKey(const int netNumber, const uint8_t *key)
{
	uint8_t msg[13];
	msg[0] = MESG_TX_SYNC;
	msg[1] = 9;
	msg[2] = MESG_NETWORK_KEY_ID;
	msg[3] = (uint8_t)netNumber;
	msg[4] = key[0];
	msg[5] = key[1];
	msg[6] = key[2];
	msg[7] = key[3];
	msg[8] = key[4];
	msg[9] = key[5];
	msg[10] = key[6];
	msg[11] = key[7];
	msg[12] = calcMsgChecksum(msg, 12); 			// xor checksum
	return write(msg, 13);
}

int AntPlus::SetChannelSearchTimeout(const int channel, const int searchTimeout)
{
	uint8_t msg[6];
	msg[0] = MESG_TX_SYNC;			// sync
	msg[1] = 2;						// length
	msg[2] = MESG_CHANNEL_SEARCH_TIMEOUT_ID;
	msg[3] = (uint8_t)channel;
	msg[4] = (uint8_t)searchTimeout;
	msg[5] = calcMsgChecksum(msg, 5);
	return write(msg, 6);
}

int AntPlus::SetChannelPeriod(const int channel, const int period)
{
	uint8_t msg[7];
	msg[0] = MESG_TX_SYNC;			// sync
	msg[1] = 3;						// length
	msg[2] = MESG_CHANNEL_MESG_PERIOD_ID;
	msg[3] = (uint8_t)channel;
	msg[4] = (uint8_t)(period & 0xFF);
	msg[5] = (uint8_t)(period >> 8);
	msg[6] = calcMsgChecksum(msg, 6);
	return write(msg, 7);
}

int AntPlus::SetChannelRFFreq(const int channel, const int freq)
{
	uint8_t msg[6];
	msg[0] = MESG_TX_SYNC;			// sync
	msg[1] = 2;						// length
	msg[2] = MESG_CHANNEL_RADIO_FREQ_ID;
	msg[3] = (uint8_t)channel;
	msg[4] = (uint8_t)freq;
	msg[5] = calcMsgChecksum(msg, 5);
	return write(msg, 6);
}

int AntPlus::SetSearchWaveform(const int channel, const int wave)
{
	uint8_t msg[7];
	msg[0] = MESG_TX_SYNC;			// sync
	msg[1] = 3;						// length
	msg[2] = MESG_SEARCH_WAVEFORM_ID;		// msg id
	msg[3] = (uint8_t)channel;
	msg[4] = (uint8_t)wave & 0xFF;
	msg[5] = (uint8_t)wave >> 8;
	msg[6] = calcMsgChecksum(msg, 6);
	return write(msg, 7);
}

int AntPlus::OpenChannel(const int channel)
{
	uint8_t msg[5];
	msg[0] = MESG_TX_SYNC;			// sync
	msg[1] = 1;						// length
	msg[2] = MESG_OPEN_CHANNEL_ID;	// msg id
	msg[3] = (uint8_t)channel;
	msg[4] = calcMsgChecksum(msg, 4);
	return write(msg, 5);
}

int AntPlus::CloseChannel(const int channel)
{
	uint8_t msg[5];
	msg[0] = MESG_TX_SYNC;			// sync
	msg[1] = 1;						// length
	msg[2] = MESG_CLOSE_CHANNEL_ID;	// msg id
	msg[3] = (uint8_t)channel;
	msg[4] = calcMsgChecksum(msg, 4);
	return write(msg, 5);
}

int AntPlus::AssignChannel(const int channel, const int channelType, const int network)
{
	uint8_t msg[7];
	msg[0] = MESG_TX_SYNC;
	msg[1] = 3;
	msg[2] = MESG_ASSIGN_CHANNEL_ID;
	msg[3] = (uint8_t)channel;
	msg[4] = (uint8_t)channelType;
	msg[5] = (uint8_t)network;
	msg[6] = calcMsgChecksum(msg, 6);
	return write(msg, 7);
}

int AntPlus::SetChannelId(const int channel, const int deviceNum, const int deviceType, const int transmissionType)
{
	uint8_t msg[9];
	msg[0] = MESG_TX_SYNC;
	msg[1] = 5;
	msg[2] = MESG_CHANNEL_ID_ID;
	msg[3] = (uint8_t)channel;
	msg[4] = (uint8_t)(deviceNum & 0xFF);
	msg[5] = (uint8_t)(deviceNum >> 8);
	msg[6] = (uint8_t)deviceType;
	msg[7] = (uint8_t)transmissionType;
	msg[8] = calcMsgChecksum(msg, 8);
	return write(msg, 9);
}

int AntPlus::SendBurstTransferPacket(const int channelSeq, const uint8_t *data)
{
	uint8_t msg[13];
	msg[0] = MESG_TX_SYNC;
	msg[1] = 9;
	msg[2] = MESG_BURST_DATA_ID;
	msg[3] = (uint8_t)channelSeq;
	msg[4] = data[0];
	msg[5] = data[1];
	msg[6] = data[2];
	msg[7] = data[3];
	msg[8] = data[4];
	msg[9] = data[5];
	msg[10] = data[6];
	msg[11] = data[7];
	msg[12] = calcMsgChecksum(msg, 12); 			// xor checksum
	return write(msg, 13);
}

int AntPlus::SendBurstTransfer(const int channel, const uint8_t *data, const int nunPackets)
{
	int ret = 0;
	int seq = 0;
	for (int i = 0; i < nunPackets; i++) {
		if (i == nunPackets-1) seq |= 0x04;
		ret = SendBurstTransferPacket((seq<<5) | (channel&0x1F), data+(i<<3));
		seq = (seq+1)&0x03;
	}
	return ret;
}

int AntPlus::SendBroadcastData(const int channel, const uint8_t *data)
{
	uint8_t msg[13];
	msg[0] = MESG_TX_SYNC;
	msg[1] = 9;
	msg[2] = MESG_BROADCAST_DATA_ID;
	msg[3] = (uint8_t)channel;
	msg[4] = data[0];
	msg[5] = data[1];
	msg[6] = data[2];
	msg[7] = data[3];
	msg[8] = data[4];
	msg[9] = data[5];
	msg[10] = data[6];
	msg[11] = data[7];
	msg[12] = calcMsgChecksum(msg, 12);
	return write(msg, 13);
}

int AntPlus::SendAcknowledgedData(const int channel, const uint8_t *data)
{
	uint8_t msg[13];
	msg[0] = MESG_TX_SYNC;
	msg[1] = 9;
	msg[2] = MESG_ACKNOWLEDGED_DATA_ID;
	msg[3] = (uint8_t)channel;
	msg[4] = data[0];
	msg[5] = data[1];
	msg[6] = data[2];
	msg[7] = data[3];
	msg[8] = data[4];
	msg[9] = data[5];
	msg[10] = data[6];
	msg[11] = data[7];
	msg[12] = calcMsgChecksum(msg, 12);
	return write(msg, 13);
}

int AntPlus::SendExtAcknowledgedData(const int channel, const int devNum, const int devType, const int TranType, const uint8_t *data)
{
	uint8_t msg[17];
	msg[0] = MESG_TX_SYNC;
	msg[1] = 13;
	msg[2] = MESG_EXT_ACKNOWLEDGED_DATA_ID;
	msg[3] = (uint8_t)channel;
	msg[4] = (uint8_t)(devNum & 0xFF);
	msg[5] = (uint8_t)(devNum >> 8);
	msg[6] = (uint8_t)devType;
	msg[7] = (uint8_t)TranType;
	msg[8] = data[0];
	msg[9] = data[1];
	msg[10] = data[2];
	msg[11] = data[3];
	msg[12] = data[4];
	msg[13] = data[5];
	msg[14] = data[6];
	msg[15] = data[7];
	msg[16] = calcMsgChecksum(msg, 16);
	return write(msg, 17);
}

int AntPlus::SendExtBroadcastData(const int channel, const int devNum, const int devType, const int TranType, const uint8_t *data)
{
	uint8_t msg[17];
	msg[0] = MESG_TX_SYNC;
	msg[1] = 13;
	msg[2] = MESG_EXT_BROADCAST_DATA_ID;
	msg[3] = (uint8_t)channel;
	msg[4] = (uint8_t)(devNum & 0xFF);
	msg[5] = (uint8_t)(devNum >> 8);
	msg[6] = (uint8_t)devType;
	msg[7] = (uint8_t)TranType;
	msg[8] = data[0];
	msg[9] = data[1];
	msg[10] = data[2];
	msg[11] = data[3];
	msg[12] = data[4];
	msg[13] = data[5];
	msg[14] = data[6];
	msg[15] = data[7];
	msg[16] = calcMsgChecksum(msg, 16);
	return write(msg, 17);
}

int AntPlus::SendExtBurstTransferPacket(const int chanSeq, const int devNum, const int devType,
	const int TranType, const uint8_t *data)
{
	uint8_t msg[17];
	msg[0] = MESG_TX_SYNC;
	msg[1] = 13;
	msg[2] = MESG_EXT_BROADCAST_DATA_ID;
	msg[3] = (uint8_t)chanSeq;
	msg[4] = (uint8_t)(devNum & 0xFF);
	msg[5] = (uint8_t)(devNum >> 8);
	msg[6] = (uint8_t)devType;
	msg[7] = (uint8_t)TranType;
	msg[8] = data[0];
	msg[9] = data[1];
	msg[10] = data[2];
	msg[11] = data[3];
	msg[12] = data[4];
	msg[13] = data[5];
	msg[14] = data[6];
	msg[15] = data[7];
	msg[16] = calcMsgChecksum(msg, 16);
	return write(msg, 17);
}

int AntPlus::SendExtBurstTransfer(const int channel, const int devNum, const int devType,
	const int tranType, const uint8_t *data, const int nunPackets)
{
	int ret = 0;
	int seq = 0;
	for (int i = 0; i < nunPackets; i++){
		if (i == nunPackets-1) seq |= 0x04;
		ret = SendExtBurstTransferPacket((seq<<5) | (channel&0x1F),
			devNum, devType, tranType, data+(i<<3));
		seq = (seq+1)&0x03;
	}
	return ret;
}



void AntPlus::profileSetup_HRM(TDCONFIG *cfg, const uint32_t deviceId)
{
	cfg->deviceNumber = deviceId;		// 0
	cfg->deviceType = ANT_DEVICE_HRM;
	cfg->transType = ANT_TRANSMISSION_SLAVE;
	cfg->channelType = ANT_CHANNEL_TYPE_SLAVE;
	cfg->networkNumber = 0;
	cfg->channel = PROFILE_HRM;
	cfg->channelPeriod = ANT_PERIOD_HRM;
	cfg->RFFreq = ANT_FREQUENCY_SPORT;
	cfg->searchTimeout = 255;
	cfg->searchWaveform = 0x53;//316;//0x53;
	//cfg->keyIdx = KEY_ANTSPORT;
	cfg->flags.chanIdOnce = 0;
	cfg->flags.channelStatus = ANT_CHANNEL_STATUS_UNASSIGNED;
	cfg->flags.channelStatusOld = 0xFF;
	cfg->flags.keyAccepted = 0;
	cfg->flags.profileValid = 1;
}

void AntPlus::profileSetup_SPDCAD(TDCONFIG *cfg, const uint32_t deviceId)
{
	cfg->deviceNumber = deviceId;		// 0
	cfg->deviceType = ANT_DEVICE_SPDCAD;
	cfg->transType = ANT_TRANSMISSION_SLAVE;		// 5
	cfg->channelType = ANT_CHANNEL_TYPE_SLAVE;
	cfg->networkNumber = 0;
	cfg->channel = PROFILE_SPDCAD;
	cfg->channelPeriod = ANT_PERIOD_SPDCAD;
	cfg->RFFreq = ANT_FREQUENCY_SPORT;
	cfg->searchTimeout = 255;
	cfg->searchWaveform = 0x53;
	//cfg->keyIdx = KEY_ANTSPORT;
	cfg->flags.chanIdOnce = 0;
	cfg->flags.channelStatus = ANT_CHANNEL_STATUS_UNASSIGNED;
	cfg->flags.channelStatusOld = 0xFF;
	cfg->flags.keyAccepted = 0;
	cfg->flags.profileValid = 1;
}

void AntPlus::profileSetup_POWER(TDCONFIG *cfg, const uint32_t deviceId)
{
	cfg->deviceNumber = deviceId;		// 0
	cfg->deviceType = ANT_DEVICE_POWER;
	cfg->transType = ANT_TRANSMISSION_SLAVE;		// 5
	cfg->channelType = ANT_CHANNEL_TYPE_SLAVE;
	cfg->networkNumber = 0;
	cfg->channel = PROFILE_POWER;
	cfg->channelPeriod = ANT_PERIOD_POWER;
	cfg->RFFreq = ANT_FREQUENCY_SPORT;
	cfg->searchTimeout = 255;
	cfg->searchWaveform = 0x53;
	//cfg->keyIdx = KEY_ANTSPORT;
	cfg->flags.chanIdOnce = 0;
	cfg->flags.channelStatus = ANT_CHANNEL_STATUS_UNASSIGNED;
	cfg->flags.channelStatusOld = 0xFF;
	cfg->flags.keyAccepted = 0;
	cfg->flags.profileValid = 1;
}

void AntPlus::profileSetup_STRIDE(TDCONFIG *cfg, const uint32_t deviceId)
{
	cfg->deviceNumber = deviceId;		// 0
	cfg->deviceType = ANT_DEVICE_STRIDE;
	cfg->transType = ANT_TRANSMISSION_SLAVE;		// 5
	cfg->channelType = ANT_CHANNEL_TYPE_SLAVE;
	cfg->networkNumber = 0;
	cfg->channel = PROFILE_STRIDE;
	cfg->channelPeriod = ANT_PERIOD_STRIDE;
	cfg->RFFreq = ANT_FREQUENCY_STRIDE;
	cfg->searchTimeout = 255;
	cfg->searchWaveform = 0x53;
	//cfg->keyIdx = KEY_ANTSPORT;
	cfg->flags.chanIdOnce = 0;
	cfg->flags.channelStatus = ANT_CHANNEL_STATUS_UNASSIGNED;
	cfg->flags.channelStatusOld = 0xFF;
	cfg->flags.keyAccepted = 0;
	cfg->flags.profileValid = 1;
}

void AntPlus::profileSetup_SPEED(TDCONFIG *cfg, const uint32_t deviceId)
{
	cfg->deviceNumber = deviceId;		// 0
	cfg->deviceType = ANT_DEVICE_SPEED;
	cfg->transType = ANT_TRANSMISSION_SLAVE;		// 5
	cfg->channelType = ANT_CHANNEL_TYPE_SLAVE;
	cfg->networkNumber = 0;
	cfg->channel = PROFILE_SPEED;
	cfg->channelPeriod = ANT_PERIOD_SPEED;
	cfg->RFFreq = ANT_FREQUENCY_SPORT;
	cfg->searchTimeout = 255;
	cfg->searchWaveform = 0x53;
	//cfg->keyIdx = KEY_ANTSPORT;
	cfg->flags.chanIdOnce = 0;
	cfg->flags.channelStatus = ANT_CHANNEL_STATUS_UNASSIGNED;
	cfg->flags.channelStatusOld = 0xFF;
	cfg->flags.keyAccepted = 0;
	cfg->flags.profileValid = 1;
}

void AntPlus::profileSetup_CADENCE(TDCONFIG *cfg, const uint32_t deviceId)
{
	cfg->deviceNumber = deviceId;		// 0
	cfg->deviceType = ANT_DEVICE_CADENCE;
	cfg->transType = ANT_TRANSMISSION_SLAVE;		// 5
	cfg->channelType = ANT_CHANNEL_TYPE_SLAVE;
	cfg->networkNumber = 0;
	cfg->channel = PROFILE_CADENCE;
	cfg->channelPeriod = ANT_PERIOD_CADENCE;
	cfg->RFFreq = ANT_FREQUENCY_SPORT;
	cfg->searchTimeout = 255;
	cfg->searchWaveform = 0x53;
	//cfg->keyIdx = KEY_ANTSPORT;
	cfg->flags.chanIdOnce = 0;
	cfg->flags.channelStatus = ANT_CHANNEL_STATUS_UNASSIGNED;
	cfg->flags.channelStatusOld = 0xFF;
	cfg->flags.keyAccepted = 0;
	cfg->flags.profileValid = 1;
}


/*
uint64_t factory_passkey (uint64_t device_id, uint8_t *buffer)
{
	uint64_t n = (((uint64_t)device_id ^ 0x7d215abb) << 32) + ((uint64_t)device_id ^ 0x42b93f06);

	for (uint8_t i = 0; i < 8; i++)
		buffer[i] = n >> (8*i)&0xFF;

	return n;
}
*/


void AntPlus::begin(const uint8_t key)
{
	ant.key = (key < KEY_TOTAL) ? key : 0;
}



void AntPlus::dispatchPayload(TDCONFIG *cfg, const uint8_t *payload, const int len)
{
	switch (cfg->channel) {
	  case PROFILE_HRM:
		payload_HRM(cfg, payload, len);
		break;
	  case PROFILE_SPDCAD:
		payload_SPDCAD(cfg, payload, len);
		break;
	  case PROFILE_POWER:
		payload_POWER(cfg, payload, len);
		break;
	  case PROFILE_STRIDE:
		payload_STRIDE(cfg, payload, len);
		break;
	  case PROFILE_SPEED:
		payload_SPEED(cfg, payload, len);
		break;
	  case PROFILE_CADENCE:
		payload_CADENCE(cfg, payload, len);
		break;
	}
}

const uint8_t * AntPlus::getAntKey(const uint8_t keyIdx)
{
	if (keyIdx >= KEY_TOTAL) return NULL;
	return antkeys[keyIdx];
}


void AntPlus::payload_HRM(TDCONFIG *cfg, const uint8_t *data, const size_t dataLength)
{
	uint8_t bpm = data[STREAM_RXBROADCAST_DEV120_HR];
	uint8_t sequence = data[STREAM_RXBROADCAST_DEV120_SEQ];
	if ((sequence == hrm.previous.sequence && bpm == hrm.previous.bpm) || bpm == 0) {
		return;
	}
	uint16_t time = (data[STREAM_RXBROADCAST_DEV120_BEATLO]
		+ (data[STREAM_RXBROADCAST_DEV120_BEATHI] << 8));
	int interval = (uint16_t)(time - hrm.previous.time) * (uint32_t)1000 / (uint32_t)1024;
	if (user_onHeartRateMonitor) {
		(*user_onHeartRateMonitor)(bpm, interval, sequence);
	}
	hrm.previous.time = time;
	hrm.previous.sequence = sequence;
	hrm.previous.bpm = bpm;
	//printf("payload_HRM: page:%i, Sequence:%i, BPM:%i, %i %i",
	//  data[1]&0x0F, sequence, bpm, time, interval);
}

void AntPlus::payload_SPDCAD(TDCONFIG *cfg, const uint8_t *data, const size_t dataLength)
{
	uint16_t cadenceTime = data[1] | (data[2] << 8);
	uint16_t cadenceCt = data[3] | (data[4] << 8);
	uint16_t speedTime = data[5] | (data[6] << 8);
	uint16_t speedCt = data[7] | (data[8] << 8);
	if (cadenceTime == spdcad.previous.cadenceTime
	  && cadenceCt == spdcad.previous.cadenceCt
	  && speedTime == spdcad.previous.speedTime
	  && speedCt == spdcad.previous.speedCt) {
		return; // no change
	}
	uint16_t cadence = (60 * (cadenceCt - spdcad.previous.cadenceCt) * 1024) / (cadenceTime - spdcad.previous.cadenceTime);
	// number wheel revolutions
	uint32_t speedRotationDelta = speedCt - spdcad.previous.speedCt;
	// time for above revolutions
	float speedTimeDelta = (float)(speedTime - spdcad.previous.speedTime) / 1024.0f;
	// calculated distance (meters) travelled as per above
	float distance = (speedRotationDelta * (float)wheelCircumference) / 1000.0f;
	spdcad.distance += distance;
	float speed = (distance / (speedTimeDelta / 3600.0f)) / 1000.0f;
	if (user_onSpeedCadence) {
		(*user_onSpeedCadence)(speed, spdcad.distance * 0.001f, cadence);
	}
	spdcad.previous.cadenceTime = cadenceTime;
	spdcad.previous.cadenceCt = cadenceCt;
	spdcad.previous.speedTime = speedTime;
	spdcad.previous.speedCt = speedCt;
	//printf("payload_SPDCAD: speed: %.2f, cadence: %i, total distance: %.2f",
	//  speed, cadence, spdcad.distance);
}

void AntPlus::payload_POWER(TDCONFIG *cfg, const uint8_t *data, const size_t dataLength)
{
	//printf("payload_POWER: len:%i", dataLength);
#if 0
	uint8_t eventCount = data[2];
	uint8_t pedalPowerContribution = ((data[3] != 0xFF) && (data[3]&0x80)) ; // left/right is defined if NOT 0xFF (= no Pedal Power) AND BIT 7 is set
	uint8_t pedalPower = (data[3]&0x7F); // right pedalPower % - stored in bit 0-6
	uint8_t instantCadence = data[4];
	uint16_t sumPower = data[5] + (data[6]<<8);
	uint16_t instantPower = data[7] + (data[8]<<8);
#endif
	//sendMessage(ANTP_MSG_PROFILE_DATA, (intptr_t*)pwr, PROFILE_POWER);
}

void AntPlus::payload_STRIDE(TDCONFIG *cfg, const uint8_t *data, const size_t dataLength)
{
	//printf("payload_STRIDE: len:%i", dataLength);
	int page = data[1];
	if (page == 0) {
		//stride.current.strides = data[7];
		//sendMessage(ANTP_MSG_PROFILE_DATA, (intptr_t*)&stride, PROFILE_STRIDE);
		//stride.previous.strides = stride.current.strides;
	} else if (page == 1) {
		//stride.current.speed = ((float)(data[4]&0x0f) + (float)(data[5]/256.0f));
		//stride.current.cadence = ((float)data[3] + (float)((data[4] << 4) / 16.0f));
		//sendMessage(ANTP_MSG_PROFILE_DATA, (intptr_t*)&stride, PROFILE_STRIDE);
		//stride.previous.speed = stride.current.speed;
		//stride.previous.cadence = stride.current.cadence;
	}
}

void AntPlus::payload_SPEED(TDCONFIG *cfg, const uint8_t *data, const size_t dataLength)
{
	//printf("payload_SPEED: len:%i", dataLength);
	uint16_t speedTime = data[5] | (data[6] << 8);
	uint16_t speedCt = data[7] | (data[8] << 8);
	if (speedTime == spd.previous.speedTime && speedCt == spd.previous.speedCt) {
		return; // no change
	}
	// number wheel revolutions
	uint32_t speedRotationDelta = speedCt - spd.previous.speedCt;
	// time for above revolutions
	float speedTimeDelta = (float)(speedTime - spd.previous.speedTime) / 1024.0f;
	// calculated distance (meters) travelled as per above
	float distance = (speedRotationDelta * (float)wheelCircumference) / 1000.0f;
	spd.distance += distance;
	float speed = (distance / (speedTimeDelta / 3600.0f)) / 1000.0f;
	if (user_onSpeed) {
		(*user_onSpeed)(speed, spd.distance);
	}
	spd.previous.speedTime = speedTime;
	spd.previous.speedCt = speedCt;
}

void AntPlus::payload_CADENCE(TDCONFIG *cfg, const uint8_t *data, const size_t dataLength)
{
	//printf("payload_CADENCE: len:%i", dataLength);
	uint16_t cadenceTime = data[5] | (data[6] << 8);
	uint16_t cadenceCt = data[7] | (data[8] << 8);
	if (cadenceTime == cad.previous.cadenceTime
	  && cadenceCt == cad.previous.cadenceCt) {
		return; // no change
	}
	uint16_t cadence = (60 * (cadenceCt - cad.previous.cadenceCt) * 1024) / (cadenceTime - cad.previous.cadenceTime);
	if (user_onCadence) {
		(*user_onCadence)(cadence);
	}
	cad.previous.cadenceTime = cadenceTime;
	cad.previous.cadenceCt = cadenceCt;
}




