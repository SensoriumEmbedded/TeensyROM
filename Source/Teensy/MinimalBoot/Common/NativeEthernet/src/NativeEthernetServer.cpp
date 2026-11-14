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
#include "utility/NativeW5100.h"

uint16_t* EthernetServer::server_port;
#if FNET_CFG_TLS
fnet_tls_socket_t* EthernetServer::tls_socket_ptr;
bool* EthernetServer::_tls;
#endif

void EthernetServer::begin()
{
   uint8_t sockindex = Ethernet.socketBegin(SnMR::TCP, _port);
   if (sockindex < Ethernet.socket_num) {
	   if (Ethernet.socketListen(sockindex)) {
		   server_port[sockindex] = _port;
		   service_descriptor = fnet_service_register(poll, this);
#if FNET_CFG_TLS
		   if(_tls_en && tls_desc != 0){
			   _tls[sockindex] = true;
		   }
		   else if(_tls_en){
			   tls_desc = fnet_tls_init(FNET_TLS_ROLE_SERVER);
			   if(tls_desc == 0){
				   Serial.println("Failed to initialize TLS");
				   _tls[sockindex] = false;
				   _tls_en = false;
			   }
			   else{
				   if(fnet_tls_set_own_certificate(tls_desc, certificate_buffer, certificate_buffer_size, private_key_buffer, private_key_buffer_size) == FNET_ERR)
				   {
					   Serial.println("TLS certificate error.");
					   fnet_tls_release(tls_desc);
					   _tls[sockindex] = false;
					   _tls_en = false;
				   }
				   else{
					   _tls[sockindex] = true;
				   }
			   }
		   }
		   else{
			   _tls[sockindex] = false;
		   }
#endif
		   if(service_descriptor == 0){
			   Ethernet.socketClose(sockindex);
		   }
	   } else {
		   Ethernet.socketClose(sockindex);
	   }
   }
}

EthernetClient EthernetServer::available()
{
	bool listening = false;
	uint8_t sockindex = Ethernet.socket_num;
	uint8_t maxindex=Ethernet.socket_num;

	for (uint8_t i=0; i < maxindex; i++) {
		if (server_port[i] == _port) {
			uint8_t stat = Ethernet.socketStatus(i);
//		   static bool stat_toggle = false;
//		   if(stat != 0x14){
//			  Serial.print("ServerAvailStat: ");
//			  Serial.println(stat, HEX);
//			  Serial.send_now();
//			  stat_toggle = false;
//		   }
//		   else if(!stat_toggle){
//			  Serial.print("\nServerAvailStatListening: ");
//			  Serial.println(stat, HEX);
//			  Serial.send_now();
//			  stat_toggle = true;
//		   }
			if (stat == SnSR::ESTABLISHED || stat == SnSR::CLOSE_WAIT) {
			   fnet_ssize_t _pending;
			   fnet_size_t _pending_size = sizeof(_pending);
			   fnet_ssize_t ret = 0;
			   elapsedMillis timeout = 0;
			   
			   fnet_socket_poll_t socket_poll;
			   socket_poll.s = Ethernet.socket_ptr[i];
			   socket_poll.events = (fnet_socket_event_t)FNET_SOCKET_EVENT_ALL; /* Connection attempt successful or failed */

			   if(fnet_socket_poll(&socket_poll, 1))
			   {
				   if(socket_poll.events_occurred & FNET_SOCKET_EVENT_OUT || socket_poll.events_occurred & FNET_SOCKET_EVENT_IN) /* Connection successful */
				   {
             if(socket_poll.events_occurred & FNET_SOCKET_EVENT_ERR) /* Connection failed */
             {
               Serial.print("Socket Event Err: ");
               fnet_ssize_t error_handler = fnet_error_get();
               Serial.println(error_handler);
               Serial.send_now();
  //             break;
             }
					   if(socket_poll.events_occurred & (FNET_SOCKET_EVENT_OUT || FNET_SOCKET_EVENT_IN)) {
//						   Serial.println("Socket Event Both");
//						   break;
             }
             else if(socket_poll.events_occurred & FNET_SOCKET_EVENT_OUT){
//               Serial.println("Socket Event Out");
             }
             else {
//               Serial.println("Socket Event IN");
             }
              
           }
         }
			   
			   
			   do{
                   fnet_poll();
                   if(fnet_socket_getopt(Ethernet.socket_ptr[i], SOL_SOCKET, SO_RCVNUM, &_pending, &_pending_size) == FNET_ERR){
                       fnet_ssize_t error_handler = fnet_error_get();
                       Serial.print("SRVAvailErr: ");
                         Serial.send_now();
                         Serial.println(error_handler);
                         Serial.send_now();
                         break;
                   }
                   ret = _pending;
   //			    ret = fnet_socket_recv(Ethernet.socket_ptr[i], Ethernet.socket_buf_receive[i], Ethernet.socket_size, MSG_PEEK);
			   } while(timeout < 10000 && ret == 0);
			   
//			   Serial.print("Recv: ");
//			   Serial.println(_pending);
			   if (ret > 0) {
				  sockindex = i;
				  Ethernet.socket_buf_index[sockindex] = 0;
				  break;
			   } else {
					// remote host closed connection, our end still open
				  if (stat == SnSR::CLOSE_WAIT) {
					 server_port[i] = 0;
					 Ethernet.socketClose(i);
				  }
				  else{
					 Serial.println("No data available, closing socket");
//					 Ethernet.socketDisconnect(i);
					 server_port[i] = 0;
					 Ethernet.socketClose(i);
				  }
			   }
			} else if (stat == SnSR::LISTEN) {
			   listening = true;
			} else if (stat == SnSR::CLOSED) {
			   server_port[i] = 0;
			   Ethernet.socketClose(i);
			}
		}
	}
	if (!listening) begin();
//   if(sockindex != maxindex){
//	  Serial.print("ServerAvailSock: ");
//	  Serial.println(sockindex, HEX);
//	  Serial.send_now();
//   }
	return EthernetClient(sockindex);
}

EthernetClient EthernetServer::accept()
{
	bool listening = false;
	uint8_t sockindex = Ethernet.socket_num;
	uint8_t maxindex=Ethernet.socket_num;

	for (uint8_t i=0; i < maxindex; i++) {
		if (server_port[i] == _port) {
			uint8_t stat = Ethernet.socketStatus(i);
			if (sockindex == Ethernet.socket_num &&
			  (stat == SnSR::ESTABLISHED || stat == SnSR::CLOSE_WAIT)) {
				// Return the connected client even if no data received.
				// Some protocols like FTP expect the server to send the
				// first data.
				sockindex = i;
				server_port[i] = 0; // only return the client once
			} else if (stat == SnSR::LISTEN) {
				listening = true;
			} else if (stat == SnSR::CLOSED) {
				server_port[i] = 0;
			}
		}
	}
	if (!listening) begin();
	return EthernetClient(sockindex);
}

EthernetServer::operator bool()
{
	uint8_t maxindex=Ethernet.socket_num;
	for (uint8_t i=0; i < maxindex; i++) {
		if (server_port[i] == _port) {
			if (Ethernet.socketStatus(i) == SnSR::LISTEN) {
				return true; // server is listening for incoming clients
			}
		}
	}
	return false;
}

#if 0
void EthernetServer::statusreport()
{
	Serial.printf("EthernetServer, port=%d\n", _port);
	for (uint8_t i=0; i < Ethernet.socket_num; i++) {
		uint16_t port = server_port[i];
		uint8_t stat = Ethernet.socketStatus(i);
		const char *name;
		switch (stat) {
			case 0x00: name = "CLOSED"; break;
			case 0x13: name = "INIT"; break;
			case 0x14: name = "LISTEN"; break;
			case 0x15: name = "SYNSENT"; break;
			case 0x16: name = "SYNRECV"; break;
			case 0x17: name = "ESTABLISHED"; break;
			case 0x18: name = "FIN_WAIT"; break;
			case 0x1A: name = "CLOSING"; break;
			case 0x1B: name = "TIME_WAIT"; break;
			case 0x1C: name = "CLOSE_WAIT"; break;
			case 0x1D: name = "LAST_ACK"; break;
			case 0x22: name = "UDP"; break;
			case 0x32: name = "IPRAW"; break;
			case 0x42: name = "MACRAW"; break;
			case 0x5F: name = "PPPOE"; break;
			default: name = "???";
		}
		int avail = Ethernet.socketRecvAvailable(i);
		Serial.printf("  %d: port=%d, status=%s (0x%02X), avail=%d\n",
			i, port, name, stat, avail);
	}
}
#endif

size_t EthernetServer::write(uint8_t b)
{
	return write(&b, 1);
}

size_t EthernetServer::write(const uint8_t *buffer, size_t size)
{
	uint8_t maxindex=Ethernet.socket_num;
	available();
	for (uint8_t i=0; i < maxindex; i++) {
		if (server_port[i] == _port) {
			if (Ethernet.socketStatus(i) == SnSR::ESTABLISHED) {
				Ethernet.socketSend(i, buffer, size);
			}
		}
	}
	return size;
}

void EthernetServer::poll(void* cookie){
    EthernetServer* server = (EthernetServer*) cookie;
    uint8_t maxindex = Ethernet.socket_num;

    for (uint8_t i=0; i < maxindex; i++) {
        if (server->server_port[i] == server->_port) {
            uint8_t stat = Ethernet.socketStatus(i);
            if (stat == SnSR::LISTEN) {
                struct fnet_sockaddr from;
                fnet_size_t fromlen;
                fnet_socket_t tmp = fnet_socket_accept(Ethernet.socket_ptr[i], &from, &fromlen);
                if(tmp){
#if FNET_CFG_TLS
                    if(server->_tls[i]){
                        fnet_tls_socket_t tls_socket = fnet_tls_socket(server->tls_desc, tmp);
                        if(tls_socket == FNET_NULL){
                           Serial.println("Failed to create TLS socket");
                           server->_tls[i] = false;
                        }
                        else{
                           server->tls_socket_ptr[i] = tls_socket;
                        }
                    }
#endif
                    //Copied from socketBegin to find an open socket without closing server socket
                    uint8_t s, maxindex = Ethernet.socket_num;
                    // look at all the hardware sockets, use any that are closed (unused)
                    for (s=0; s < maxindex; s++) {
                       if(Ethernet.socket_ptr[s] == nullptr){
                           goto makesocket;
                       }
                    }
                    // As a last resort, forcibly close any already closing
                    for (s=0; s < maxindex; s++) {
                       uint8_t stat = Ethernet.socketStatus(s);
                       if (stat == SnSR::FIN_WAIT) goto closemakesocket;
                       if (stat == SnSR::CLOSE_WAIT) goto closemakesocket;
                    }
                    fnet_socket_close(tmp);
                    return;
closemakesocket:
				    Ethernet.socketClose(s);
makesocket:
//                    fnet_socket_close(Ethernet.socket_ptr[i]);
                    Ethernet.socket_ptr[s] = tmp;
                    //Don't unregister anymore since server is still listening
//                    fnet_service_unregister(server->service_descriptor);
//                    uint8_t stat = Ethernet.socketStatus(s);
                    server->server_port[s] = server->server_port[i];
//                    server->server_port[i] = 0;
//                    Serial.print("Server: ");
//                    Serial.println(i);
//                    Serial.print("Socket Accepted: ");
//                    Serial.println(s);
//                    Serial.print("Socket Accepted Stat: ");
//                    Serial.println(stat, HEX);
//                    Serial.send_now();
                }
            }
        }
    }
}

