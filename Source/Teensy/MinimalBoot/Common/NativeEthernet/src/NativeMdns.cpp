/* Copyright 2020 Tino Hernandez (vjmuzik)
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
#include "NativeMdns.h"

void EthernetMDNS::begin(const char* host_name, uint8_t num_services){
    fnet_mdns_params_t mdns_params = {
        .netif_desc = fnet_netif_get_default(),
        .addr_family = AF_INET,
        .rr_ttl = FNET_CFG_MDNS_RR_TTL,
        .name = host_name
    };
    mdns_desc = fnet_mdns_init(&mdns_params);
    if(mdns_desc == 0){
        Serial.println("Failed to begin MDNS");
        return;
    }
    fnet_mdns_service_t mdns_service;
    mdns_service.name = (char*)host_name;
    fnet_mdns_service_update_name(mdns_desc, &mdns_service);
    service_desc = new fnet_mdns_service_desc_t[num_services];
    num_service_desc = num_services;
    for(uint8_t i = 0; i < num_service_desc; i++){
        service_desc[i] = 0;
    }
}

void EthernetMDNS::addService(const char* service_type, uint16_t service_port, const fnet_mdns_txt_key_t* (*service_get_txt)(void)){
    fnet_mdns_service_t mdns_service;
    mdns_service.service_type = service_type;
    mdns_service.service_port = FNET_HTONS(service_port);
    mdns_service.service_get_txt = service_get_txt;
    for(uint8_t i = 0; i < num_service_desc; i++){
        if(service_desc[i] == 0){
            service_desc[i] = fnet_mdns_service_register(mdns_desc, &mdns_service);
            if(service_desc[i] == 0){
                Serial.println("Failed to add MDNS Service");
            }
            return;
        }
    }
    Serial.println("Unable to add more MDNS Services");
}

void EthernetMDNS::removeService(const char* service_type){
    fnet_mdns_service_desc_t srv_desc = fnet_mdns_service_get_by_type(mdns_desc, service_type);
    fnet_mdns_service_unregister(srv_desc);
    for(uint8_t i = 0; i < num_service_desc; i++){
        if(service_desc[i] == srv_desc){
            service_desc[i] = 0;
        }
    }
}

void EthernetMDNS::setServiceName(const char* service_name){
    fnet_mdns_service_t mdns_service;
    mdns_service.name = (char*)service_name;
    fnet_mdns_service_update_name(mdns_desc, (const fnet_mdns_service_t*)&mdns_service);
}

EthernetMDNS MDNS;
