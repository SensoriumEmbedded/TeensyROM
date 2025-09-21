
#ifdef FeatTCPListen


#define WaitForTCPDataStartmS   500

void ServiceTCP(EthernetClient &tcpclient)
{
   IPAddress ip = tcpclient.remoteIP();
   Printf_dbg("New Client, IP: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);   

   uint32_t StartTOMillis = millis();
   while(tcpclient.connected() && !tcpclient.available())
   {
      if ((millis() - StartTOMillis) > WaitForTCPDataStartmS)
      {
         tcpclient.stop();
         Printf_dbg("Client Data Timeout!\n");
         return;
      }
   }

   Printf_dbg("  Data available in %lu mS\n", millis() - StartTOMillis);

   ServiceSerial(&tcpclient);
   CmdChannel  = &Serial; //restore to serial stream
   
   //delay(10);
   tcpclient.stop();
   Printf_dbg("Client disconnected\n");
}
   
   
#endif
