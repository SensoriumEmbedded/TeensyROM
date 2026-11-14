// Arduino DNS client for WizNet5100-based Ethernet shield
// (c) Copyright 2009-2010 MCQN Ltd.
// Released under Apache License, version 2.0

#ifndef DNSClient_h
#define DNSClient_h

#include "NativeEthernet.h"

class DNSClient
{
public:
	void begin(const IPAddress& aDNSServer);

	/** Convert a numeric IP address string into a four-byte IP address.
	    @param aIPAddrString IP address to convert
	    @param aResult IPAddress structure to store the returned IP address
	    @result 1 if aIPAddrString was successfully converted to an IP address,
	            else error code
	*/
	int inet_aton(const char *aIPAddrString, IPAddress& aResult);

	/** Resolve the given hostname to an IP address.
	    @param aHostname Name to be resolved
	    @param aResult IPAddress structure to store the returned IP address
	    @result 1 if aIPAddrString was successfully converted to an IP address,
	            else error code
	*/
	int getHostByName(const char* aHostname, IPAddress& aResult, uint16_t timeout=5000);

protected:
    static void fnet_dns_callback(const fnet_dns_resolved_addr_t* addr_list, long unsigned int addr_list_size, const char* host_name, void* cookie);

	IPAddress iDNSServer;
    volatile fnet_ssize_t resolveDone;
    IPAddress resolvedIP;
};

#endif
