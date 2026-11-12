/*
 *  Udp.cpp: Library to send/receive UDP packets with the Arduino ethernet shield.
 *  This version only offers minimal wrapping of socket.cpp
 *  Drop Udp.h/.cpp into the Ethernet library directory at hardware/libraries/Ethernet/
 *
 * MIT License:
 * Copyright (c) 2008 Bjoern Hartmann
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * bjoern@cs.stanford.edu 12/30/2008
 */

#include <Arduino.h>
#include "NativeEthernet.h"
#include "NativeDns.h"
#include "utility/NativeW5100.h"

/* Start EthernetUDP socket, listening at local port PORT */
uint8_t EthernetUDP::begin(uint16_t port)
{
    static bool first_call = true;
    if(first_call){
        sockindex = Ethernet.socket_num;
        first_call = false;
    }
    if (sockindex < Ethernet.socket_num) {
        Ethernet.socketClose(sockindex);
    }
	sockindex = Ethernet.socketBegin(SnMR::UDP, port);
    if (sockindex >= Ethernet.socket_num) {
        return 0;
    }
	_port = port;
	_remaining = 0;
	return 1;
}

/* return number of bytes available in the current packet,
   will return zero if parsePacket hasn't been called yet */
int EthernetUDP::available()
{
	return _remaining;
}

/* Release any resources being used by this EthernetUDP instance */
void EthernetUDP::stop()
{
	if (sockindex < Ethernet.socket_num) {
		Ethernet.socketClose(sockindex);
		sockindex = Ethernet.socket_num;
	}
}

int EthernetUDP::beginPacket(const char *host, uint16_t port)
{
	// Look up the host first
	int ret = 0;
	DNSClient dns;
	IPAddress remote_addr;
	dns.begin(Ethernet.dnsServerIP());
	ret = dns.getHostByName(host, remote_addr);
	if (ret != 1) return ret;
	return beginPacket(remote_addr, port);
}

int EthernetUDP::beginPacket(IPAddress ip, uint16_t port)
{
	_offset = 0;
//	Serial.printf("UDP beginPacket\n");
	return Ethernet.socketStartUDP(sockindex, rawIPAddress(ip), port);
}

int EthernetUDP::endPacket()
{
    return Ethernet.socketSendUDP(sockindex);
}

int EthernetUDP::endPacket(fnet_flag_t flags)
{
    int ret = Ethernet.socketSendUDP(sockindex, flags);
#if FNET_CFG_CPU_ETH_ADJUSTABLE_TIMER
    if(flags == MSG_TIMESTAMP){
        send_timestamp = fnet_socket_send_timestamp(Ethernet.socket_ptr[sockindex]);
        send_timestamp_ns = fnet_socket_send_timestamp_ns(Ethernet.socket_ptr[sockindex]);
    }
#endif /*FNET_CFG_CPU_ETH_ADJUSTABLE_TIMER*/
    return ret;
}

size_t EthernetUDP::write(uint8_t byte)
{
	return write(&byte, 1);
}

size_t EthernetUDP::write(const uint8_t *buffer, size_t size)
{
	//Serial.printf("UDP write %d\n", size);
	uint16_t bytes_written = Ethernet.socketBufferData(sockindex, _offset, buffer, size);
	_offset += bytes_written;
	return bytes_written;
}

int EthernetUDP::parsePacket()
{
	// discard any remaining bytes in the last packet
//    if(_remaining){
//        Serial.println("Remaining Data Discarded!");
//        Serial.print("Socket Index: ");
//        Serial.println(sockindex);
//        Serial.send_now();
//        fnet_socket_recv(Ethernet.socket_ptr[sockindex], &tmpBuf, sizeof(tmpBuf), 0);
//        _remaining = 0;
//    }
//    Serial.println("p2");
//    Serial.send_now();

	if ((_remaining = Ethernet.socketRecvAvailable(sockindex)) > 0) {
        //HACK - hand-parse the UDP packet using TCP recv method
        struct fnet_sockaddr from;
        fnet_size_t fromlen = sizeof(from);
        
		fnet_ssize_t ret = fnet_socket_recvfrom(Ethernet.socket_ptr[sockindex], Ethernet.socket_buf_receive[sockindex], Ethernet.socket_size, 0, &from, &fromlen);
#if FNET_CFG_CPU_ETH_ADJUSTABLE_TIMER
        recv_timestamp = fnet_socket_recv_timestamp(Ethernet.socket_ptr[sockindex]);
        recv_timestamp_ns = fnet_socket_recv_timestamp_ns(Ethernet.socket_ptr[sockindex]);
#endif /*FNET_CFG_CPU_ETH_ADJUSTABLE_TIMER*/
        Ethernet.socket_buf_index[sockindex] = 0;

		//read 8 header bytes and get IP and port from it
        if(ret == -1){
//            int8_t error_handler = fnet_error_get();
//            Serial.print("RecvErr: ");
//            Serial.send_now();
//            Serial.println(error_handler);
//            Serial.print("Socket Index: ");
//            Serial.println(sockindex);
//            Serial.print("Remaining: ");
//            Serial.println(_remaining);
//            Serial.send_now();
            return 0;
        }
		if (ret > 0) {
			_remoteIP = from.sa_data;
            _remotePort = FNET_HTONS(from.sa_port);
		}
//        _remaining = 0;
		return ret;
	}
	// There aren't any packets available
	return 0;
}
/*{
    return 2;
}*/

int EthernetUDP::read()
{
	uint8_t byte;

	if ((_remaining > 0) && (Ethernet.socketRecv(sockindex, &byte, 1) > 0)) {
		// We read things without any problems
		_remaining--;
		return byte;
	}

	// If we get here, there's no data available
	return -1;
}

int EthernetUDP::read(unsigned char *buffer, size_t len)
{
	if (_remaining > 0) {
		int got;
		if (_remaining <= len) {
			// data should fit in the buffer
			got = Ethernet.socketRecv(sockindex, buffer, _remaining);
		} else {
			// too much data for the buffer,
			// grab as much as will fit
			got = Ethernet.socketRecv(sockindex, buffer, len);
		}
		if (got > 0) {
			_remaining -= got;
			//Serial.printf("UDP read %d\n", got);
			return got;
		}
	}
	// If we get here, there's no data available or recv failed
	return -1;
}

int EthernetUDP::peek()
{
	// Unlike recv, peek doesn't check to see if there's any data available, so we must.
	// If the user hasn't called parsePacket yet then return nothing otherwise they
	// may get the UDP header
	if (sockindex >= Ethernet.socket_num || _remaining == 0) return -1;
	return Ethernet.socketPeek(sockindex);
}

void EthernetUDP::flush()
{
	// TODO: we should wait for TX buffer to be emptied
}

/* Start EthernetUDP socket, listening at local port PORT */
uint8_t EthernetUDP::beginMulticast(IPAddress ip, uint16_t port)
{
	if (sockindex < Ethernet.socket_num) Ethernet.socketClose(sockindex);
	sockindex = Ethernet.socketBeginMulticast(SnMR::UDP | SnMR::MULTI, ip, port);
	if (sockindex >= Ethernet.socket_num) return 0;
	_port = port;
	_remaining = 0;
	return 1;
}

