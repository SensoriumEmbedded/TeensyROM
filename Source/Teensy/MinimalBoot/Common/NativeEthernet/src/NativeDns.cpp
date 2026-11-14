// Arduino DNS client for WizNet5100-based Ethernet shield
// (c) Copyright 2009-2010 MCQN Ltd.
// Released under Apache License, version 2.0

#include <Arduino.h>
#include "NativeEthernet.h"
#include "NativeDns.h"

void DNSClient::begin(const IPAddress& aDNSServer)
{
	iDNSServer = aDNSServer;
}


int DNSClient::inet_aton(const char* address, IPAddress& result)
{
    fnet_char_t* cp = (fnet_char_t*)address;
    struct fnet_in_addr addr;
    fnet_return_t ret = fnet_inet_aton(cp, &addr);
    if(ret == FNET_ERR) return 0;
    result = addr.s_addr;
    return 1;
}

void DNSClient::fnet_dns_callback(const fnet_dns_resolved_addr_t* addr_list, long unsigned int addr_list_size, const char* host_name, void* cookie)
{
    DNSClient* dns_p = (DNSClient*)cookie;
    if(addr_list == FNET_NULL){
        dns_p->resolveDone = -1;
        return;
    }
    if(addr_list_size == 0){
        dns_p->resolveDone = -1;
        return;
    }
    
    for(long unsigned int i = 0; i < addr_list_size; i++){ //Search for first valid IPv4 address and use it
        if(addr_list[i].resolved_addr.sa_family == AF_INET){
            dns_p->resolvedIP[0] = addr_list[i].resolved_addr.sa_data[0];
            dns_p->resolvedIP[1] = addr_list[i].resolved_addr.sa_data[1];
            dns_p->resolvedIP[2] = addr_list[i].resolved_addr.sa_data[2];
            dns_p->resolvedIP[3] = addr_list[i].resolved_addr.sa_data[3];
            dns_p->resolveDone = 1;
            return;
        }
    }
}


int DNSClient::getHostByName(const char* aHostname, IPAddress& aResult, uint16_t timeout)
{
    // See if it's a numeric IP address
    if (inet_aton(aHostname, aResult)) 
    {
      // It is, our work here is done
      return 1;
    }
	
    resolveDone = 0;
    struct fnet_dns_params dns_params = {
        .dns_server_addr = {
            .sa_family = AF_INET,
            .sa_port = FNET_CFG_DNS_PORT,
            .sa_scope_id = fnet_netif_get_scope_id(fnet_netif_get_default()),
            .sa_data = {iDNSServer[0], iDNSServer[1], iDNSServer[2], iDNSServer[3]}
        },
        .host_name = aHostname,
        .addr_family = AF_INET,
        .callback = fnet_dns_callback,
        .cookie = this
    };
    
    fnet_dns_desc_t fnet_dns_desc = fnet_dns_init(&dns_params);
    if(fnet_dns_desc == FNET_NULL){
        return 0;
    }
    while(resolveDone == 0){
        
    }
    if(resolveDone == -1) return 0;
    else if(resolveDone == 1) {
        aResult = resolvedIP;
        return 1;
    }
    return 0 ;
}
