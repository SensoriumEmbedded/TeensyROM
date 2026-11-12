/* Copyright 2018 Paul Stoffregen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <Arduino.h>
#include "NativeEthernet.h"
#include "NativeDns.h"
#include "utility/NativeW5100.h"

int EthernetClient::connect(const char * host, uint16_t port)
{
    static bool first_call = true;
    if(first_call){
        sockindex = Ethernet.socket_num;
        first_call = false;
    }
    
	DNSClient dns; // Look up the host first
	IPAddress remote_addr;

	if (sockindex < Ethernet.socket_num) {
		if (Ethernet.socketStatus(sockindex) != SnSR::CLOSED) {
			Ethernet.socketDisconnect(sockindex); // TODO: should we call stop()?
		}
		sockindex = Ethernet.socket_num;
	}
	dns.begin(Ethernet.dnsServerIP());
	if (!dns.getHostByName(host, remote_addr)) return 0; // TODO: use _timeout
#if FNET_CFG_TLS
    host_name = host;
#endif
	return connect(remote_addr, port);
}

int EthernetClient::connect(IPAddress ip, uint16_t port)
{
    static bool first_call = true;
    if(first_call){
        sockindex = Ethernet.socket_num;
        first_call = false;
    }
	if (sockindex < Ethernet.socket_num) {
		if (Ethernet.socketStatus(sockindex) != SnSR::CLOSED) {
			Ethernet.socketDisconnect(sockindex); // TODO: should we call stop()?
		}
		sockindex = Ethernet.socket_num;
	}
	if (ip == IPAddress(0ul) || ip == IPAddress(0xFFFFFFFFul)) return 0;
//    Using a port number of 0 causes the service provider to
//    assign a unique ephemeral port number to the socket with
//    a value between 49152 to 65535 (sugested by IANA)
	sockindex = Ethernet.socketBegin(SnMR::TCP, 0);
	if (sockindex >= Ethernet.socket_num) return 0;
	Ethernet.socketConnect(sockindex, rawIPAddress(ip), port);
    _port = port;
	uint32_t start = millis();
    _connectIP = ip;
    _connectPort = port;
    if(_connectPoll) return 1;
	while (1) {
        if(connectPoll()) return 1;
		if (millis() - start > _timeout) break;
		delay(1);
	}
	Ethernet.socketClose(sockindex);
	sockindex = Ethernet.socket_num;
	return 0;
}

int EthernetClient::connectPoll(){
    uint8_t stat = Ethernet.socketStatus(sockindex);
//    if(stat != 0x13){
//        Serial.print("Connect: ");
//        Serial.println(stat, HEX);
//        Serial.print("Sock Index: ");
//        Serial.println(sockindex);
//    }
    if (stat == SnSR::ESTABLISHED || stat == SnSR::CLOSE_WAIT) {
        _remoteIP = _connectIP;
        _remotePort = _connectPort;
#if FNET_CFG_TLS
        if(_tls_en && tls_desc != 0){
//                Serial.println("TLS socket create");
            fnet_tls_socket_t tls_socket = fnet_tls_socket(tls_desc, Ethernet.socket_ptr[sockindex]);
            if(tls_socket == FNET_NULL){
                Serial.println("Failed to create TLS client socket");
                EthernetServer::_tls[sockindex] = false;
            }
            else{
//                    Serial.println("TLS socket made");
                EthernetServer::_tls[sockindex] = true;
                EthernetServer::tls_socket_ptr[sockindex] = tls_socket;
                fnet_tls_socket_set_hostname(tls_socket, host_name);
                if(fnet_tls_socket_connect(tls_socket) == FNET_ERR){
                    Serial.println("TLS handshake failed");
                    if(EthernetServer::_tls[sockindex]){
                        fnet_tls_socket_close(EthernetServer::tls_socket_ptr[sockindex]);
                    }
                    EthernetServer::_tls[sockindex] = false;
                    fnet_tls_release(tls_desc);
                    tls_desc = 0;
                }
            }
        }
#endif
        return 1;
    }
    if (stat == SnSR::CLOSED) {
        Ethernet.socketClose(sockindex);
        return 0;
    }
    return 0;
}

#if FNET_CFG_TLS
int EthernetClient::connect(const char * host, uint16_t port, bool tls)
{
    _tls_en = true;
    if(_tls_en && tls_desc != 0){
        EthernetServer::_tls[sockindex] = true;
    }
    else if(_tls_en){
        tls_desc = fnet_tls_init(FNET_TLS_ROLE_CLIENT);
        if(tls_desc == 0){
            Serial.println("Failed to initialize TLS Client");
            EthernetServer::_tls[sockindex] = false;
            _tls_en = false;
        }
        else{
//            Serial.println("TLS Client made");
            if(fnet_tls_set_ca_certificate(tls_desc, ca_certificate_buffer, ca_certificate_buffer_size) == FNET_ERR)
            {
                Serial.println("TLS ca certificate error.");
                fnet_tls_release(tls_desc);
                EthernetServer::_tls[sockindex] = false;
                _tls_en = false;
            }
            else{
//                Serial.println("TLS socket true");
                _tls_en = true;
            }
        }
    }
    else{
//        _tls_en = false;
    }
    return connect(host, port);
}

int EthernetClient::connect(IPAddress ip, uint16_t port, bool tls)
{
    //Probably not supported, maybe
    _tls_en = false;
    return connect(ip, port);
}
#endif

int EthernetClient::availableForWrite(void)
{
	if (sockindex >= Ethernet.socket_num) return 0;
	return Ethernet.socketSendAvailable(sockindex);
}

size_t EthernetClient::write(uint8_t b)
{
	return write(&b, 1);
}

size_t EthernetClient::write(const uint8_t *buf, size_t size)
{
    if (sockindex >= Ethernet.socket_num) return 0;
    size_t ret = Ethernet.socketSend(sockindex, buf, size);
	if (ret) return ret;
	setWriteError();
	return 0;
}

int EthernetClient::available()
{
	if (sockindex >= Ethernet.socket_num) return 0;
    if (Ethernet.socket_ptr[sockindex] == nullptr) return 0;
	struct fnet_sockaddr _from;
    fnet_size_t fromlen = sizeof(_from);
    
    int ret = 0;
    if(_remaining == 0) {
#if FNET_CFG_TLS
        if(EthernetServer::_tls[sockindex]){
            fnet_socket_recvfrom(Ethernet.socket_ptr[sockindex], Ethernet.socket_buf_receive[sockindex], Ethernet.socket_size, MSG_PEEK, &_from, &fromlen);
            ret = fnet_tls_socket_recv(EthernetServer::tls_socket_ptr[sockindex], Ethernet.socket_buf_receive[sockindex], Ethernet.socket_size);
            Ethernet.socket_buf_index[sockindex] = 0;
        }
        else{
            ret = fnet_socket_recvfrom(Ethernet.socket_ptr[sockindex], Ethernet.socket_buf_receive[sockindex], Ethernet.socket_size, 0, &_from, &fromlen);
            Ethernet.socket_buf_index[sockindex] = 0;
        }
#else
        ret = fnet_socket_recvfrom(Ethernet.socket_ptr[sockindex], Ethernet.socket_buf_receive[sockindex], Ethernet.socket_size, 0, &_from, &fromlen);
        Ethernet.socket_buf_index[sockindex] = 0;
#endif
    }
    if(ret == -1){
        uint8_t s = Ethernet.socketStatus(sockindex);
        if (s == SnSR::CLOSE_WAIT || s == SnSR::CLOSED || s == SnSR::INIT) return 0;
        int8_t error_handler = fnet_error_get();
        if(error_handler == -20){
//            Serial.print("RecvErr: ");
//            Serial.send_now();
//            Serial.print(error_handler);
//            Serial.print("  Ret: ");
//            Serial.print(ret);
//            Serial.print("  Remaining: ");
//            Serial.println(_remaining);
//            stop();
            return 0;
        }
        Serial.print("SockIndex: ");
        Serial.print(sockindex, HEX);
        Serial.print("  SockStatus: ");
        Serial.print(s, HEX);
        Serial.print("  ");
        Serial.print("RecvErr: ");
        Serial.send_now();
        Serial.println(error_handler);
        _remaining = 0;
        return 0;
    }
    
    if(ret){
        _remoteIP = _from.sa_data;
        _remotePort = FNET_HTONS(_from.sa_port);
        _remaining = ret;
    }
    
    return _remaining;
}

int EthernetClient::read(uint8_t *buf, size_t size)
{
    if(_remaining > 0) if (size > (size_t)_remaining) size = _remaining;
	if (sockindex >= Ethernet.socket_num) return 0;
    int16_t ret = Ethernet.socketRecv(sockindex, buf, size);
    if (ret > 0) _remaining -= ret;
    return ret;
}

int EthernetClient::peek()
{
	if (sockindex >= Ethernet.socket_num) return -1;
	if (!available()) return -1;
	return Ethernet.socketPeek(sockindex);
}

int EthernetClient::read()
{
	uint8_t b;
    int16_t ret = Ethernet.socketRecv(sockindex, &b, 1);
    if (ret > 0) {
        _remaining -= ret;
        return b;
    }
	return -1;
}

void EthernetClient::flush()
{
	while (sockindex < Ethernet.socket_num) {
		uint8_t stat = Ethernet.socketStatus(sockindex);
		if (stat != SnSR::ESTABLISHED && stat != SnSR::CLOSE_WAIT) return;
		if (Ethernet.socketSendAvailable(sockindex) >= Ethernet.socket_size) return;
	}
}

void EthernetClient::stop()
{
	if (sockindex >= Ethernet.socket_num) return;
    if (EthernetClass::socket_ptr[sockindex] == nullptr) return;

	// attempt to close the connection gracefully (send a FIN to other side or disconnect if already closed)
	Ethernet.socketDisconnect(sockindex);
    Ethernet.socketClose(sockindex);
    sockindex = Ethernet.socket_num;
    return; // exit the loop
}

void EthernetClient::close(){
    Ethernet.socketDisconnect(sockindex); //Send FIN to other side, but keep connection open
}

uint8_t EthernetClient::connected()
{
	if (sockindex >= Ethernet.socket_num) return 0;

	uint8_t s = Ethernet.socketStatus(sockindex);
	return !(s == SnSR::LISTEN || s == SnSR::CLOSED || s == SnSR::FIN_WAIT ||
		(s == SnSR::CLOSE_WAIT && !available()));
}

uint8_t EthernetClient::status()
{
	if (sockindex >= Ethernet.socket_num) return SnSR::CLOSED;
	return Ethernet.socketStatus(sockindex);
}

// the next function allows us to use the client returned by
// EthernetServer::available() as the condition in an if-statement.
bool EthernetClient::operator==(const EthernetClient& rhs)
{
	if (sockindex != rhs.sockindex) return false;
	if (sockindex >= Ethernet.socket_num) return false;
	if (rhs.sockindex >= Ethernet.socket_num) return false;
    getClientAddress();
	return true;
}
