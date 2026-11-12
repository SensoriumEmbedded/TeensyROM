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

#ifndef ethernet_h_
#define ethernet_h_

// All symbols exposed to Arduino sketches are contained in this header file
//
// Older versions had much of this stuff in EthernetClient.h, EthernetServer.h,
// and socket.h.  Including headers in different order could cause trouble, so
// these "friend" classes are now defined in the same header file.  socket.h
// was removed to avoid possible conflict with the C library header files.


// Configure the maximum number of sockets to support.  W5100 chips can have
// up to 4 sockets.  W5200 & W5500 can have up to 8 sockets.  Several bytes
// of RAM are used for each socket.  Reducing the maximum can save RAM, but
// you are limited to fewer simultaneous connections.
#if defined(RAMEND) && defined(RAMSTART) && ((RAMEND - RAMSTART) <= 2048)
#define MAX_SOCK_NUM 4
#else
#define MAX_SOCK_NUM 8
#endif
#define FNET_SOCKET_DEFAULT_SIZE 1024 * 2
#define FNET_STACK_HEAP_DEFAULT_SIZE 64u * 1024u //64k
#define FNET_POLL_TIME 1000 //Time in microseconds

// By default, each socket uses 2K buffers inside the Wiznet chip.  If
// MAX_SOCK_NUM is set to fewer than the chip's maximum, uncommenting
// this will use larger buffers within the Wiznet chip.  Large buffers
// can really help with UDP protocols like Artnet.  In theory larger
// buffers should allow faster TCP over high-latency links, but this
// does not always seem to work in practice (maybe Wiznet bugs?)
//#define ETHERNET_LARGE_BUFFERS


#include <Arduino.h>
#include <fnet.h>
#include "Client.h"
#include "Server.h"
#include "Udp.h"

enum EthernetLinkStatus {
	Unknown,
	LinkON,
	LinkOFF
};

enum EthernetHardwareStatus {
	EthernetNoHardware,
	EthernetW5100,
	EthernetW5200,
	EthernetW5500
};

class EthernetUDP;
class EthernetClient;
class EthernetServer;

class EthernetClass {
private:
	static IPAddress _dnsServerAddress;
    static DMAMEM uint8_t** socket_buf_transmit;
    static DMAMEM uint16_t* socket_buf_len;
    static DMAMEM uint16_t* socket_port;
    static DMAMEM uint8_t** socket_addr;
    static IntervalTimer _fnet_poll;
    static volatile boolean link_status;
    static uint8_t* stack_heap_ptr;
    static size_t stack_heap_size;
    static ssize_t socket_size;
    static uint8_t socket_num;
public:
    static volatile fnet_socket_t* socket_ptr;
    static DMAMEM uint8_t** socket_buf_receive;
    static DMAMEM uint16_t* socket_buf_index;
	// Initialise the Ethernet shield to use the provided MAC address and
	// gain the rest of the configuration through DHCP.
	// Returns 0 if the DHCP configuration failed, and 1 if it succeeded
    static void setStackHeap(uint8_t* stack_heap_ptr, size_t stack_heap_size); //Appoint your own buffer
    static void setStackHeap(size_t stack_heap_size); //Change allocated stack heap size
    static void setSocketSize(size_t _socket_size); //Change allocated socket size
    static void setSocketNum(uint8_t _socket_num); //Change allocated socket num
	static int begin(uint8_t *mac, unsigned long timeout = 60000, unsigned long responseTimeout = 4000);
	static int maintain();
	static EthernetLinkStatus linkStatus();
	static EthernetHardwareStatus hardwareStatus();

	// Manaul configuration
	static void begin(uint8_t *mac, IPAddress ip);
	static void begin(uint8_t *mac, IPAddress ip, IPAddress dns);
	static void begin(uint8_t *mac, IPAddress ip, IPAddress dns, IPAddress gateway);
	static void begin(uint8_t *mac, IPAddress ip, IPAddress dns, IPAddress gateway, IPAddress subnet);
	static void init(uint8_t sspin = 10);

	static void MACAddress(uint8_t *mac_address);
	static IPAddress localIP();
	static IPAddress subnetMask();
	static IPAddress gatewayIP();
    static IPAddress dhcpServerIP();
	static IPAddress dnsServerIP() { return fnet_netif_get_ip4_dns(fnet_netif_get_default()); }

	void setMACAddress(const uint8_t *mac_address);
	void setLocalIP(const IPAddress local_ip);
	void setSubnetMask(const IPAddress subnet);
	void setGatewayIP(const IPAddress gateway);
    void setDnsServerIP(const IPAddress dns_server);
	void setRetransmissionTimeout(uint16_t milliseconds);
	void setRetransmissionCount(uint8_t num);

	friend class EthernetClient;
	friend class EthernetServer;
	friend class EthernetUDP;
    static uint8_t socketStatus(uint8_t s);
private:
	// Opens a socket(TCP or UDP or IP_RAW mode)
	static uint8_t socketBegin(uint8_t protocol, uint16_t port);
	static uint8_t socketBeginMulticast(uint8_t protocol, IPAddress ip,uint16_t port);
	
	// Close socket
	static void socketClose(uint8_t s);
	// Establish TCP connection (Active connection)
	static void socketConnect(uint8_t s, uint8_t * addr, uint16_t port);
	// disconnect the connection
	static void socketDisconnect(uint8_t s);
	// Establish TCP connection (Passive connection)
	static uint8_t socketListen(uint8_t s);
	// Send data (TCP)
	static uint16_t socketSend(uint8_t s, const uint8_t * buf, uint16_t len);
	static uint16_t socketSendAvailable(uint8_t s);
	// Receive data (TCP)
	static int socketRecv(uint8_t s, uint8_t * buf, int16_t len);
	static uint16_t socketRecvAvailable(uint8_t s);
	static uint8_t socketPeek(uint8_t s);
	// sets up a UDP datagram, the data for which will be provided by one
	// or more calls to bufferData and then finally sent with sendUDP.
	// return true if the datagram was successfully set up, or false if there was an error
	static bool socketStartUDP(uint8_t s, uint8_t* addr, uint16_t port);
	// copy up to len bytes of data from buf into a UDP datagram to be
	// sent later by sendUDP.  Allows datagrams to be built up from a series of bufferData calls.
	// return Number of bytes successfully buffered
	static uint16_t socketBufferData(uint8_t s, uint16_t offset, const uint8_t* buf, uint16_t len);
	// Send a UDP datagram built up from a sequence of startUDP followed by one or more
	// calls to bufferData.
	// return true if the datagram was successfully sent, or false if there was an error
	static bool socketSendUDP(uint8_t s, fnet_flag_t flags = 0);
	// Initialize the "random" source port number
	static void socketPortRand(uint16_t n);
    
    static fnet_return_t teensy_mutex_init(fnet_mutex_t *mutex);
    static void teensy_mutex_release(fnet_mutex_t *mutex);
    static void teensy_mutex_lock(fnet_mutex_t *mutex);
    static void teensy_mutex_unlock(fnet_mutex_t *mutex);
    static fnet_time_t timer_get_ms(void);
    static void link_callback(fnet_netif_desc_t netif, fnet_bool_t connected, void *callback_param);
    static void dhcp_cln_callback_updated(fnet_dhcp_cln_desc_t _dhcp_desc, fnet_netif_desc_t netif, void *p);
};

extern EthernetClass Ethernet;


#define UDP_TX_PACKET_MAX_SIZE 24

class EthernetUDP : public UDP {
private:
	uint16_t _port; // local port to listen on
	IPAddress _remoteIP; // remote IP address for the incoming packet whilst it's being processed
	uint16_t _remotePort; // remote port for the incoming packet whilst it's being processed
	uint16_t _offset; // offset into the packet being sent

protected:
	
	volatile uint16_t _remaining; // remaining bytes of incoming packet yet to be processed

public:
    uint8_t sockindex;
    int32_t recv_timestamp = 0;
    int32_t send_timestamp = 0;
    uint32_t recv_timestamp_ns = 0;
    uint32_t send_timestamp_ns = 0;
	EthernetUDP() : sockindex(MAX_SOCK_NUM) {}  // Constructor
	virtual uint8_t begin(uint16_t);      // initialize, start listening on specified port. Returns 1 if successful, 0 if there are no sockets available to use
	virtual uint8_t beginMulticast(IPAddress, uint16_t);  // initialize, start listening on specified port. Returns 1 if successful, 0 if there are no sockets available to use
	virtual void stop();  // Finish with the UDP socket

	// Sending UDP packets

	// Start building up a packet to send to the remote host specific in ip and port
	// Returns 1 if successful, 0 if there was a problem with the supplied IP address or port
	virtual int beginPacket(IPAddress ip, uint16_t port);
	// Start building up a packet to send to the remote host specific in host and port
	// Returns 1 if successful, 0 if there was a problem resolving the hostname or port
	virtual int beginPacket(const char *host, uint16_t port);
	// Finish off this packet and send it
	// Returns 1 if the packet was sent successfully, 0 if there was an error
	virtual int endPacket();
    virtual int endPacket(fnet_flag_t flags);
	// Write a single byte into the packet
	virtual size_t write(uint8_t);
	// Write size bytes from buffer into the packet
	virtual size_t write(const uint8_t *buffer, size_t size);

	using Print::write;

	// Start processing the next available incoming packet
	// Returns the size of the packet in bytes, or 0 if no packets are available
	virtual int parsePacket();
	// Number of bytes remaining in the current packet
	virtual int available();
	// Read a single byte from the current packet
	virtual int read();
	// Read up to len bytes from the current packet and place them into buffer
	// Returns the number of bytes read, or 0 if none are available
	virtual int read(unsigned char* buffer, size_t len);
	// Read up to len characters from the current packet and place them into buffer
	// Returns the number of characters read, or 0 if none are available
	virtual int read(char* buffer, size_t len) { return read((unsigned char*)buffer, len); };
	// Return the next byte from the current packet without moving on to the next byte
	virtual int peek();
	virtual void flush(); // Finish reading the current packet

	// Return the IP address of the host who sent the current incoming packet
	virtual IPAddress remoteIP() { return _remoteIP; };
	// Return the port of the host who sent the current incoming packet
	virtual uint16_t remotePort() { return _remotePort; };
	virtual uint16_t localPort() { return _port; }
};




class EthernetClient : public Client {
public:
	EthernetClient() : sockindex(MAX_SOCK_NUM), _timeout(10000), _connectPoll(false), _remaining(0) { }
	EthernetClient(uint8_t s) : sockindex(s), _timeout(10000), _connectPoll(false), _remaining(0) { }

	uint8_t status();
#if FNET_CFG_TLS
    virtual int connect(IPAddress ip, uint16_t port, bool tls);
    virtual int connect(const char *host, uint16_t port, bool tls);
#endif
    virtual int connect(IPAddress ip, uint16_t port);
    virtual int connect(const char *host, uint16_t port);
    virtual int connectPoll();
    virtual void connectPoll(bool active) { _connectPoll = active; }
	virtual int availableForWrite(void);
	virtual size_t write(uint8_t);
	virtual size_t write(const uint8_t *buf, size_t size);
	virtual int available();
	virtual int read();
	virtual int read(uint8_t *buf, size_t size);
	virtual int peek();
	virtual void flush();
	virtual void stop();
    virtual void close();
	virtual uint8_t connected();
    virtual operator bool() { getClientAddress(); return sockindex < Ethernet.socket_num; }
    virtual bool operator==(const bool value) { getClientAddress(); return bool() == value; }
    virtual bool operator!=(const bool value) { getClientAddress(); return bool() != value; }
	virtual bool operator==(const EthernetClient&);
    virtual bool operator!=(const EthernetClient& rhs) { getClientAddress(); return !this->operator==(rhs); }
	uint8_t getSocketNumber() const { return sockindex; }
	// Return the IP address of the host who sent the current incoming packet
    virtual IPAddress remoteIP() { return _remoteIP; };
    // Return the port of the host who sent the current incoming packet
    virtual uint16_t remotePort() { return _remotePort; };
    virtual uint16_t localPort() { return _port; }
	virtual void setConnectionTimeout(uint16_t timeout) { _timeout = timeout; }
        
#if FNET_CFG_TLS
    void setCACert(const char* cert_buf, size_t cert_buf_len){
        ca_certificate_buffer = (fnet_uint8_t*)cert_buf;
        ca_certificate_buffer_size = cert_buf_len;
    }
#endif
	friend class EthernetServer;

	using Print::write;

private:
	uint8_t sockindex; // MAX_SOCK_NUM means client not in use
	uint16_t _timeout;
    uint16_t _port; // local port to listen on
    IPAddress _remoteIP; // remote IP address for the incoming packet whilst it's being processed
    uint16_t _remotePort; // remote port for the incoming packet whilst it's being processed
    IPAddress _connectIP; // remote IP address for the incoming packet whilst it's being connected
    uint16_t _connectPort; // remote port for the incoming packet whilst it's being connected
    boolean _connectPoll; //Determines if connectPoll is active or not
    int32_t _remaining;
    void getClientAddress() {
        if(sockindex < Ethernet.socket_num){
            struct fnet_sockaddr _from;
            fnet_size_t fromlen = sizeof(_from);
            fnet_return_t ret = fnet_socket_getpeername(Ethernet.socket_ptr[sockindex], &_from, &fromlen);
            if(ret == FNET_OK){
                _remoteIP = _from.sa_data;
                _remotePort = FNET_HTONS(_from.sa_port);
            }
        }
    }
        
#if FNET_CFG_TLS
    fnet_tls_desc_t tls_desc = 0;
    bool _tls_en = false;
    const char* host_name = NULL;
    fnet_uint8_t* ca_certificate_buffer = (fnet_uint8_t*)mbedtls_test_ca_crt; /* Certificate data. */
    fnet_size_t ca_certificate_buffer_size = mbedtls_test_ca_crt_len;              /* Size of the certificate buffer. */
#endif
};


class EthernetServer : public Server {
private:
    static void poll(void* cookie);
	
public:
#if FNET_CFG_TLS
	EthernetServer(uint16_t port) : _port(port), _tls_en(false) { }
	EthernetServer(uint16_t port, bool tls_en) : _port(port), _tls_en(tls_en) { }
    EthernetServer() : _port(0), _tls_en(false) { }
#else
    EthernetServer(uint16_t port) : _port(port) { }
    EthernetServer() : _port(0) { }
#endif
	EthernetClient available();
	EthernetClient accept();
	virtual void begin();
    void begin(uint16_t port) { _port = port; begin(); }
	virtual size_t write(uint8_t);
	virtual size_t write(const uint8_t *buf, size_t size);
	virtual operator bool();
	using Print::write;
	//void statusreport();

	// TODO: make private when socket allocation moves to EthernetClass
	static uint16_t* server_port;
    uint16_t _port;
    fnet_service_desc_t service_descriptor;
    
#if FNET_CFG_TLS
    void begin(uint16_t port, bool tls_en) { _port = port; _tls_en = tls_en; begin(); }
    void setSRVCert(const char* cert_buf, size_t cert_buf_len){
        certificate_buffer = (fnet_uint8_t*)cert_buf;
        certificate_buffer_size = cert_buf_len;
    }
    void setSRVKey(const char* key_buf, size_t key_buf_len){
        private_key_buffer = (fnet_uint8_t*)key_buf;
        private_key_buffer_size = key_buf_len;
    }
    
    static bool* _tls;
    fnet_tls_desc_t tls_desc = 0;
    bool _tls_en;
    static fnet_tls_socket_t* tls_socket_ptr;
    fnet_uint8_t* certificate_buffer = (fnet_uint8_t*)mbedtls_test_srv_crt; /* Certificate data. */
    fnet_size_t certificate_buffer_size = mbedtls_test_srv_crt_len;              /* Size of the certificate buffer. */
    fnet_uint8_t* private_key_buffer = (fnet_uint8_t*)mbedtls_test_srv_key; /* Private key. */
    fnet_size_t private_key_buffer_size = mbedtls_test_srv_key_len;             /* Size of the private key buffer. */
#endif
};


class EthernetMDNS{
public:
    void begin(const char* host_name, uint8_t num_services = 1);
    void addService(const char* service_type, uint16_t service_port, const fnet_mdns_txt_key_t* (*service_get_txt)(void) = EmptyTXTRecord);
    void removeService(const char* service_type);
    void setServiceName(const char* service_name);
protected:
private:
    fnet_mdns_desc_t mdns_desc;
    fnet_mdns_service_desc_t *service_desc;
    uint8_t num_service_desc;
    static const fnet_mdns_txt_key_t* EmptyTXTRecord(){
        static const fnet_mdns_txt_key_t NullTXTRecord[] = {
            {0,0}
        };
        return NullTXTRecord;
    }
};

extern EthernetMDNS MDNS;



#endif
