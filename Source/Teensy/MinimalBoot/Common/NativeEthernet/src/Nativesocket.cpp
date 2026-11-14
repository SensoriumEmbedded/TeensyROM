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

#if ARDUINO >= 156 && !defined(ARDUINO_ARCH_PIC32)
extern void yield(void);
#else
#define yield()
#endif

// TODO: randomize this when not using DHCP, but how?
static uint16_t local_port = 49152;  // 49152 to 65535



/*****************************************/
/*          Socket management            */
/*****************************************/


void EthernetClass::socketPortRand(uint16_t n)
{
	n &= 0x3FFF;
	local_port ^= n;
	//Serial.printf("socketPortRand %d, srcport=%d\n", n, local_port);
}

uint8_t EthernetClass::socketBegin(uint8_t protocol, uint16_t port)
{
        uint8_t s, maxindex = socket_num;
        // look at all the hardware sockets, use any that are closed (unused)
        for (s=0; s < maxindex; s++) {
            if(socket_ptr[s] == nullptr){
                
                goto makesocket;
            }
        }
        // As a last resort, forcibly close any already closing
        for (s=0; s < maxindex; s++) {
            uint8_t stat = socketStatus(s);
            if (stat == SnSR::FIN_WAIT) goto closemakesocket;
            if (stat == SnSR::CLOSE_WAIT) goto closemakesocket;
        }
        return socket_num;
    
closemakesocket:
    socketClose(s);
makesocket:
    struct fnet_sockaddr_in local_addr;
    
    const fnet_uint32_t bufsize_option = socket_size;
    const fnet_int32_t      tcpnodelay_option = 1;
       if(protocol == SnMR::UDP){
           socket_ptr[s] = fnet_socket(AF_INET, SOCK_DGRAM, 0);
       }
       else if(protocol == SnMR::TCP){
           socket_ptr[s] = fnet_socket(AF_INET, SOCK_STREAM, 0);
       }
       else{
//           Serial.println("Invalid Protocol!");
           return socket_num;
       }

    // create listen socket
    if (socket_ptr[s] == FNET_NULL) {
//        Serial.println("UDP/IP: Socket creation error.");
        return socket_num;
    }

    fnet_memset(&local_addr, 0, sizeof(local_addr));

    local_addr.sin_port = FNET_HTONS(port); //fnet_htons(UDP_PORT);
    local_addr.sin_addr.s_addr = INADDR_ANY; //fnet_htonl(INADDR_ANY);
    local_addr.sin_family = AF_INET;
    
// bind the socket to the port
    if (FNET_ERR == fnet_socket_bind(socket_ptr[s], (struct fnet_sockaddr*)(&local_addr), sizeof(local_addr))) {
//        Serial.println("UDP/IP: Socket bind error.");
        fnet_socket_close(socket_ptr[s]);
        return socket_num;
    }
    
    fnet_socket_setopt(socket_ptr[s], SOL_SOCKET, SO_RCVBUF, &bufsize_option, sizeof(bufsize_option));
    fnet_socket_setopt(socket_ptr[s], SOL_SOCKET, SO_SNDBUF, &bufsize_option, sizeof(bufsize_option));
    if(protocol == SnMR::TCP){
        const struct fnet_linger    linger_option =
        {
            .l_onoff = FNET_TRUE,
            .l_linger = 4 /*sec*/
        };
        fnet_socket_setopt(socket_ptr[s], IPPROTO_TCP, TCP_NODELAY, &tcpnodelay_option, sizeof(tcpnodelay_option));
        fnet_socket_setopt(socket_ptr[s], SOL_SOCKET, SO_LINGER, &linger_option, sizeof(linger_option));
//        fnet_socket_setopt(socket_ptr[s], IPPROTO_TCP, TCP_MSS, &bufsize_option, sizeof(bufsize_option));
    }
    
    EthernetServer::server_port[s] = 0;
    return s;
}

// multicast version to set fields before open  thd
uint8_t EthernetClass::socketBeginMulticast(uint8_t protocol, IPAddress ip, uint16_t port)
{
    uint8_t s = socketBegin(SnMR::UDP, port);
    if(s == socket_num) return socket_num;
    
    struct fnet_ip_mreq mreq; /* Multicast group information.*/

    mreq.imr_multiaddr.s_addr = FNET_IP4_ADDR_INIT(ip[0], ip[1], ip[2], ip[3]);
    mreq.imr_interface = 0;

    /* Join multicast group. */
    if(fnet_socket_setopt(socket_ptr[s], IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == FNET_ERR)
    {
//        Serial.print("Join Multicast Group Error: ");
//        Serial.println(fnet_error_get());
    }
    /* Set IPv4 TTL. */
    fnet_uint32_t option = 255;
    if(fnet_socket_setopt(socket_ptr[s], IPPROTO_IP, IP_MULTICAST_TTL, &option, sizeof(option)) == FNET_ERR)
    {
//        Serial.print("Join Multicast TTL Error: ");
//        Serial.println(fnet_error_get());
    }
    if(fnet_socket_setopt(socket_ptr[s], IPPROTO_IP, IP_TTL, &option, sizeof(option)) == FNET_ERR)
    {
//        Serial.print("Join IP TTL Error: ");
//        Serial.println(fnet_error_get());
    }
    return s;
}
// Return the socket's status
//
uint8_t EthernetClass::socketStatus(uint8_t s)
{
    if(socket_ptr[s] == nullptr){
        return SnSR::CLOSED;
    }
    fnet_socket_state_t state;
    fnet_size_t state_size = sizeof(state);
    if(fnet_socket_getopt(socket_ptr[s], SOL_SOCKET, SO_STATE, &state, &state_size) == FNET_ERR){
        int8_t error_handler = fnet_error_get();
        Serial.print("StateErr: ");
        Serial.send_now();
        Serial.println(error_handler);
        Serial.send_now();
    }
    switch (state) {
        case SS_CLOSED:
            return SnSR::CLOSED;
        case SS_CLOSING:
            return SnSR::CLOSE_WAIT;
        case SS_CONNECTING:
            return SnSR::INIT;
        case SS_CONNECTED:
            return SnSR::ESTABLISHED;
        case SS_LISTENING:
            return SnSR::LISTEN;
        default:
            break;
    }
	return true;
}

// Immediately close.  If a TCP connection is established, the
// remote host is left unaware we closed.
//
void EthernetClass::socketClose(uint8_t s)
{
#if FNET_CFG_TLS
    if(EthernetServer::_tls[s]){
        fnet_tls_socket_close(EthernetServer::tls_socket_ptr[s]);
    }
    EthernetServer::_tls[s] = false;
#endif
    fnet_socket_close(socket_ptr[s]);
    
//    while(Ethernet.socketStatus(s) != 1){
//        
//    }
    socket_ptr[s] = nullptr;
}


// Place the socket in listening (server) mode
//
uint8_t EthernetClass::socketListen(uint8_t s)
{
    fnet_return_t ret = fnet_socket_listen(socket_ptr[s], 1);
	if (ret != FNET_OK) {
		return 0;
	}
    else{
      return 1;
    }
}


// establish a TCP connection in Active (client) mode.
//
void EthernetClass::socketConnect(uint8_t s, uint8_t * addr, uint16_t port)
{
	// set destination IP
//    Serial.println("Socket Connect");
    struct fnet_sockaddr_in remoteaddr;
    remoteaddr.sin_family = AF_INET;
    remoteaddr.sin_port = FNET_HTONS(port);
    remoteaddr.sin_addr.s_addr = *(fnet_ip4_addr_t*)addr;
//    fnet_socket_connect(socket_ptr[s], (struct fnet_sockaddr*)&remoteaddr, sizeof(remoteaddr));
    fnet_return_t ret = fnet_socket_connect(socket_ptr[s], (struct fnet_sockaddr*)&remoteaddr, sizeof(remoteaddr));
    if(ret == FNET_ERR){
        int8_t error_handler = fnet_error_get();
        Serial.print("Connect Err: ");
        Serial.send_now();
        Serial.println(error_handler);
        Serial.send_now();
    }
}



// Gracefully disconnect a TCP connection.
//
void EthernetClass::socketDisconnect(uint8_t s)
{
//    socketClose(s);
    if(fnet_socket_shutdown(socket_ptr[s], SD_WRITE) == FNET_ERR){
//        Serial.println("Socket Shutdown Error");
//        int8_t error_handler = fnet_error_get();
//        Serial.print("RemainingErr: ");
//        Serial.println(error_handler);
//        Serial.send_now();
    };
}



/*****************************************/
/*    Socket Data Receive Functions      */
/*****************************************/

// Receive data.  Returns size, or -1 for no data, or 0 if connection closed
//
int EthernetClass::socketRecv(uint8_t s, uint8_t *buf, int16_t len)
{
    if(socket_buf_index[s] == socket_size) return -1;
    if(socket_buf_index[s] + len < socket_size){
        if(buf != NULL) fnet_memcpy(buf, socket_buf_receive[s] + socket_buf_index[s], len);
        socket_buf_index[s] += len;
        return len;
    }
    else if(socket_buf_index[s] + len >= socket_size){
        uint16_t truncate = socket_size - socket_buf_index[s];
        if(buf != NULL) fnet_memcpy(buf, socket_buf_receive[s] + socket_buf_index[s], truncate);
        socket_buf_index[s] += truncate;
        return truncate;
    }
    return -1;
}

uint16_t EthernetClass::socketRecvAvailable(uint8_t s)
{
    int ret = fnet_socket_recv(Ethernet.socket_ptr[s], socket_buf_receive[s], Ethernet.socket_size, MSG_PEEK);
    if(ret == -1){
//        int8_t error_handler = fnet_error_get();
//            Serial.print("SocketRecvAvailErr: ");
//            Serial.send_now();
//            Serial.println(error_handler);
//        Serial.print("Socket Index: ");
//        Serial.println(s);
//            Serial.send_now();
        return 0;
    }
    
    return ret;
}

// get the first byte in the receive queue (no checking)
//
uint8_t EthernetClass::socketPeek(uint8_t s)
{
    return socket_buf_receive[s][socket_buf_index[s]];
}



/*****************************************/
/*    Socket Data Transmit Functions     */
/*****************************************/

/**
 * @brief	This function used to send the data in TCP mode
 * @return	1 for success else 0.
 */
uint16_t EthernetClass::socketSend(uint8_t s, const uint8_t * buf, uint16_t len)
{
    while(socketSendAvailable(s) < len){}
    fnet_ssize_t ret = -1;
#if FNET_CFG_TLS
    if(EthernetServer::_tls[s]){
        ret = fnet_tls_socket_send(EthernetServer::tls_socket_ptr[s], buf, len);
    }
    else{
        ret = fnet_socket_send(socket_ptr[s], buf, len, 0);
    }
#else
    ret = fnet_socket_send(socket_ptr[s], buf, len, 0);
#endif
    if(ret == -1) {
        int8_t error_handler = fnet_error_get();
        Serial.print("SendErr: ");
        Serial.send_now();
        Serial.println(error_handler);
        return 0;
    }
    return  ret;
}

uint16_t EthernetClass::socketSendAvailable(uint8_t s)
{
    fnet_ssize_t _max, _pending;
    fnet_size_t _max_size, _pending_size;
    _pending_size = sizeof(_pending);
    _max_size = sizeof(_max_size);
    if(fnet_socket_getopt(socket_ptr[s], SOL_SOCKET, SO_SNDNUM, &_pending, &_pending_size) == FNET_ERR){
        return 0;
    }
    if(fnet_socket_getopt(socket_ptr[s], SOL_SOCKET, SO_SNDBUF, &_max, &_max_size) == FNET_ERR){
        return 0;
    }
    return (uint16_t)(_max - _pending);
}

uint16_t EthernetClass::socketBufferData(uint8_t s, uint16_t offset, const uint8_t* buf, uint16_t len)
{
	//Serial.printf("  bufferData, offset=%d, len=%d\n", offset, len);
    uint8_t* _buf = (uint8_t*)buf;
    if(offset + len >= FNET_SOCKET_DEFAULT_SIZE){
        fnet_memcpy(socket_buf_transmit[s] + offset, _buf, FNET_SOCKET_DEFAULT_SIZE - offset);
        socket_buf_len[s] += FNET_SOCKET_DEFAULT_SIZE - offset;
        return FNET_SOCKET_DEFAULT_SIZE - offset;
    }
    else{
        fnet_memcpy(socket_buf_transmit[s] + offset, _buf, len);
        socket_buf_len[s] += len;
        
        return len;
    }
}

bool EthernetClass::socketStartUDP(uint8_t s, uint8_t* addr, uint16_t port)
{
	if ( ((addr[0] == 0x00) && (addr[1] == 0x00) && (addr[2] == 0x00) && (addr[3] == 0x00)) ||
	  ((port == 0x00)) ) {
		return false;
	}
    socket_buf_len[s] = 0;
    socket_addr[s][0] = addr[0];
    socket_addr[s][1] = addr[1];
    socket_addr[s][2] = addr[2];
    socket_addr[s][3] = addr[3];
    socket_port[s] = FNET_HTONS(port);
	return true;
}

bool EthernetClass::socketSendUDP(uint8_t s, fnet_flag_t flags)
{
    struct fnet_sockaddr_in remoteaddr;
          remoteaddr.sin_family = AF_INET;
          remoteaddr.sin_port = socket_port[s];
          remoteaddr.sin_addr.s_addr = FNET_IP4_ADDR_INIT(socket_addr[s][0], socket_addr[s][1], socket_addr[s][2], socket_addr[s][3]);
       
       return fnet_socket_sendto(socket_ptr[s], socket_buf_transmit[s], socket_buf_len[s], flags, (struct fnet_sockaddr*)&remoteaddr, sizeof(remoteaddr)) != FNET_ERR ? true : false;
}

