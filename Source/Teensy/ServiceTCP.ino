
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

#endif
