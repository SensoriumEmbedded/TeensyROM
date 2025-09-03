
//extern RegMenuTypes RegMenuTypeFromFileName(char** ptrptrFileName);
//extern FS *FSfromSourceID(RegMenuTypes SourceID);

FLASHMEM bool HostSerialAvailabeTimeout()
{
   uint32_t StartTOMillis = millis();
   
   while(!USBHostSerial.available() && (millis() - StartTOMillis) < 50); // timeout loop
   
   return (USBHostSerial.available());
}

FLASHMEM void FlushUSBHostRx()
{
   Serial.printf("\n*/--");
   while(HostSerialAvailabeTimeout()) 
   {
      //char inval = USBHostSerial.read();
      //Serial.printf("%c (%d) ", inval, inval);
      Serial.print((char)USBHostSerial.read());
   }
   Serial.printf("--/*\n");
}  

FLASHMEM void MountDxxFile()
{
   SelItemFullIdx = IO1[rwRegCursorItemOnPg]+(IO1[rwRegPageNumber]-1)*MaxItemsPerPage;

   char DxxPathFilename[MaxPathLength];
   IO1[rwRegScratch] = 0;
   GetCurrentFilePathName(DxxPathFilename);
   SendMsgPrintfln("%s\r", DxxPathFilename);
   
   //check for Dxx file
   if (MenuSource[SelItemFullIdx].ItemType != rtD64 &&
       MenuSource[SelItemFullIdx].ItemType != rtD71 &&
       MenuSource[SelItemFullIdx].ItemType != rtD81)
   {
      SendMsgPrintfln("Invalid File Type (%d)\r", MenuSource[SelItemFullIdx].ItemType);
      return;
   }
   
   //check for Meatloaf enabled
   //if ((IO1[rwRegPwrUpDefaults2] & rpud2TRContEnabled) == 0)
   //{
   //   SendMsgPrintfln("TRCont/Meatloaf not enabled\r");
   //   return;
   //}
   
   //ping meatloaf
   SendMsgPrintfln("Pinging Meatloaf.\r");
   USBHostSerial.begin(2000000, USBHOST_SERIAL_8N1); // 115200 460800 2000000
   FlushUSBHostRx();
   USBHostSerial.printf("ls\r\n");
   FlushUSBHostRx();
   
   SendMsgPrintfln("Transferring...");
   char *ptrPathFileName = DxxPathFilename; //pointer to move past SD/USB/TR:
   RegMenuTypes MenuSourceID = RegMenuTypeFromFileName(&ptrPathFileName);
   FS *sourceFS = FSfromSourceID(MenuSourceID);
   
   
   File DxxFile = sourceFS->open(ptrPathFileName, FILE_READ);
   if (!DxxFile) 
   {
      SendMsgPrintfln("Error opening file\r");
      return;
   }   
   
   uint32_t FileSize = DxxFile.size(); 
   uint32_t FileCRC32 = 0;
   uint8_t buffer[256];
   int bytesRead;
   //char * LastSlash = strrchr(ptrPathFileName, '/');
   //if (LastSlash != NULL) ptrPathFileName = LastSlash+1; //make filename only.
   
   FlushUSBHostRx();
   
   //calc crc32
   while ((bytesRead = DxxFile.read(buffer, sizeof(buffer))) > 0)
   {
      for (uint16_t bytenum = 0; bytenum < bytesRead; bytenum++) ;
         // buffer[bytenum];
   }
   DxxFile.seek(0);

   Printf_dbg("Sending Dxx: %s (%lu bytes, %lu crc32)\n", MenuSource[SelItemFullIdx].Name, FileSize, FileCRC32);
   
   USBHostSerial.printf("rx \"%s\"\r\n", MenuSource[SelItemFullIdx].Name);
   USBHostSerial.printf("%lu %lu\r\n", FileSize, FileCRC32);

   FlushUSBHostRx();

   // Send in buffer size chunks
   while ((bytesRead = DxxFile.read(buffer, sizeof(buffer))) > 0)
   {
      for (uint16_t bytenum = 0; bytenum < bytesRead; bytenum++) 
         USBHostSerial.printf("%c", buffer[bytenum]);
   }

   DxxFile.close();   
   USBHostSerial.printf("\r\n");

   FlushUSBHostRx();
   
   SendMsgPrintfln("Mounting...");
   
   
   SendMsgPrintfln("  Complete.\roption to launch?\r");
   
}