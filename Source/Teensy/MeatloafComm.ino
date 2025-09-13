
#include <CRC32.h>

#define MeatloafBaud      2000000   // 115200 460800 2000000
#define ChunkSize              64   //bytes to send in chunk before ack
#define ChunkAckChar          '+'   //char sent to ack chunk

FLASHMEM bool HostSerialAvailabeTimeout(uint32_t TimeoutmS)
{
   uint32_t StartTOMillis = millis();
   
   while(!USBHostSerial.available() && (millis() - StartTOMillis) < TimeoutmS); // timeout loop
   
   return (USBHostSerial.available());
}

FLASHMEM void FlushUSBHostRx()
{
   //make this debug print only?
   
   Printf_dbg("\n*/--");
   while(HostSerialAvailabeTimeout(50)) //50mS timeout for flush
   {
      char inval = USBHostSerial.read();
      Printf_dbg("%c", inval);
   }
   Printf_dbg("--/*\n");
}  

FLASHMEM bool WaitCheckresponse(const char *Name, const char CheckChar)
{
   //wait for response.
   if (!HostSerialAvailabeTimeout(1000)) //Wait up to 1 sec
   {
      SendMsgPrintfln("%s not received\r", Name);
      return false;
   }
   char Respchar = USBHostSerial.read();
   if (Respchar != CheckChar)
   {
      SendMsgPrintfln("Unexpected %s: `%c`\r", Name, Respchar);
      return false;
   }
   return true;
}

FLASHMEM void MountDxxFile()
{
   char DxxPathFilename[MaxPathLength];
   
   //get/print path+filename
   SelItemFullIdx = IO1[rwRegCursorItemOnPg]+(IO1[rwRegPageNumber]-1)*MaxItemsPerPage;
   IO1[rwRegScratch] = 0; //needed for GetCurrentFilePathName, also indicates success of this function
   GetCurrentFilePathName(DxxPathFilename);
   SendMsgPrintfln("%s\r", DxxPathFilename);
   
   //check for Dxx file type
   if (MenuSource[SelItemFullIdx].ItemType != rtD64 &&
       MenuSource[SelItemFullIdx].ItemType != rtD71 &&
       MenuSource[SelItemFullIdx].ItemType != rtD81)
   {
      SendMsgPrintfln("Invalid File Type (%d)\r", MenuSource[SelItemFullIdx].ItemType);
      return;
   }
   
   //check for Meatloaf enabled in TR setup
   //need to separate Meatloaf from TR Control?
   //if ((IO1[rwRegPwrUpDefaults2] & rpud2TRContEnabled) == 0)
   //{
   //   SendMsgPrintfln("TRCont/Meatloaf not enabled\r");
   //   return;
   //}
   
   //connect/ping meatloaf:
   SendMsgPrintfln("Connecting to Meatloaf");
   USBHostSerial.begin(MeatloafBaud, USBHOST_SERIAL_8N1);
   FlushUSBHostRx();
   USBHostSerial.printf("Z\r\n");  // Should echo it back (then unrecognized command)
   if (!WaitCheckresponse("Ping", 'Z')) 
   {   
      SendMsgPrintfln(" Reset ML and retry");
      return;      
   }
   
   //change to cache dir on ML SD card
   USBHostSerial.printf("cd /sd/\r\n"); 
   FlushUSBHostRx();
   
   //open file
   SendMsgPrintfln("File check");
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
   uint8_t buffer[ChunkSize];
   int bytesRead;
   CRC32 crc;
   
   //calc crc32
   while ((bytesRead = DxxFile.read(buffer, sizeof(buffer))) > 0)
   {
      for (uint16_t bytenum = 0; bytenum < bytesRead; bytenum++) crc.update(buffer[bytenum]);
   }
   DxxFile.seek(0); //back to start
   
   uint32_t FileCRC32 = crc.finalize();
   uint32_t StartmS = millis();
   
   Printf_dbg("\nSending Dxx: %s (%lu bytes, %08x crc32)\n", MenuSource[SelItemFullIdx].Name, FileSize, FileCRC32);
   SendMsgPrintfln("Transferring %lu Bytes", FileSize);
   USBHostSerial.printf("rx \"%s\"\r\n", MenuSource[SelItemFullIdx].Name);
   FlushUSBHostRx(); //required for some reason(?), min delay length?
   USBHostSerial.printf("%lu %08x\r\n", FileSize, FileCRC32);  
   //FlushUSBHostRx();  //size/crc is not echoed...

   // Send in chunks, wait for ack between them
   while ((bytesRead = DxxFile.read(buffer, ChunkSize)) > 0)
   {
      for (uint16_t bytenum = 0; bytenum < bytesRead; bytenum++) USBHostSerial.print((char)buffer[bytenum]);  
      //wait for ack char after every chunk
      Printf_dbg("bytes sent: %d\n",bytesRead);
      if (!WaitCheckresponse("Ack", ChunkAckChar))
      {
         DxxFile.close();
         return;      
      }      
      //Printf_dbg("Ack\n");
   }
   DxxFile.close();   
   USBHostSerial.printf("\r\n");
   //wait for pass/fail response.
   if (!WaitCheckresponse("Conf", '0'))
   {
      DxxFile.close();
      return;      
   }
   SendMsgPrintfln("Complete in %lu mS", millis()-StartmS );
   
   SendMsgPrintfln("Mounting as dev #8...");
   USBHostSerial.printf("mount 8 \"%s\"\r\n", MenuSource[SelItemFullIdx].Name);
   FlushUSBHostRx();   
   SendMsgPrintfln(" Finished\r\rLOAD\"*\",8,1 and RUN? (y/n) ");
   IO1[rwRegScratch] = 1; //Success indicator, prompt for load/run
}