
#ifdef FeatTCPListen

void ServiceTCP(EthernetClient &tcpClient)
{
   if (!tcpClient.connected())
   {
      Printf_dbg("TCP Client disconnected\n");
      tcpClient.stop();
      return;
   }
   int bytesAvailable = tcpClient.available();

   if(bytesAvailable == 0) return;

   if(bytesAvailable > 0)
   {
      Printf_dbg("\nTCP Data available: %d bytes, processing ServiceSerial\n", bytesAvailable);
      ServiceSerial(&tcpClient);
   }
}
   
// NativeEthernet's static-IP begin() spins on the link with no timeout, so an
// unplugged cable hung boot. Its DHCP begin() with a zero timeout brings the
// stack and its link poll up and returns at once, so the link can be awaited
// with a bound before the static begin(). 5 s covers PHY auto-negotiation.
// The address is set before the wait: the library's link-up callback starts
// DHCP on an interface still at 0.0.0.0, so a cable plugged in after a timeout
// would otherwise swap the static IP for a lease.
FLASHMEM bool EthBeginStatic(uint8_t *mac, uint32_t ip, uint32_t dns, uint32_t gateway, uint32_t subnetmask)
{
   uint16_t DHCPRespTO;
   EEPROM.get(eepAdDHCPRespTO, DHCPRespTO);
   Ethernet.begin(mac, 0, DHCPRespTO);
   fnet_netif_desc_t netif = fnet_netif_get_default();
   fnet_netif_set_ip4_addr(netif, ip, subnetmask);
   fnet_netif_set_ip4_gateway(netif, gateway);
   fnet_netif_set_ip4_dns(netif, dns);
   uint32_t beginWait = millis();
   while (Ethernet.linkStatus() != LinkON)
   {
      if (millis() - beginWait >= 5000) return false;
   }
   Ethernet.begin(mac, ip, dns, gateway, subnetmask);
   return true;
}

FLASHMEM bool EthernetInit()
{
   uint32_t beginWait = millis();
   uint8_t  mac[6];
   bool retval = true;
   Serial.print("\nEthernet init ");
   
   EEPreadNBuf(eepAdMyMAC, mac, 6);

   if (EEPROM.read(eepAdDHCPEnabled))
   {
      Serial.print("via DHCP... ");

      uint16_t DHCPTimeout, DHCPRespTO;
      EEPROM.get(eepAdDHCPTimeout, DHCPTimeout);
      EEPROM.get(eepAdDHCPRespTO, DHCPRespTO);
      if (Ethernet.begin(mac, DHCPTimeout, DHCPRespTO) == 0)
      {
         Serial.println("*Failed!*");
         // Check for Ethernet hardware present
         if (Ethernet.hardwareStatus() == EthernetNoHardware) Serial.println("Ethernet HW was not found.");
         else if (Ethernet.linkStatus() == LinkOFF) Serial.println("Ethernet cable is not connected.");   
         retval = false;
      }
      else
      {
         Serial.println("passed.");
      }
   }
   else
   {
      Serial.println("using Static");
      uint32_t ip, dns, gateway, subnetmask;
      EEPROM.get(eepAdMyIP, ip);
      EEPROM.get(eepAdDNSIP, dns);
      EEPROM.get(eepAdGtwyIP, gateway);
      EEPROM.get(eepAdMaskIP, subnetmask);
      if (!EthBeginStatic(mac, ip, dns, gateway, subnetmask))
      {
         Serial.println("*Failed!* Ethernet cable is not connected.");
         retval = false;
      }
   }
   
   Serial.printf("Took %d mS\nIP: ", (millis() - beginWait));
   Serial.println(Ethernet.localIP());
   return retval;
}
   
#endif
