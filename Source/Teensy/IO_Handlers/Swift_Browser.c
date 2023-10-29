// MIT License
// 
// Copyright (c) 2023 Travis Smith
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software 
// and associated documentation files (the "Software"), to deal in the Software without 
// restriction, including without limitation the rights to use, copy, modify, merge, publish, 
// distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom 
// the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or 
// substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING 
// BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


// Swiftlink Browser Functions


void SendPETSCIICharImmediate(char CharToSend)
{
   //wait for c64 to be ready or NMI timeout
   while(!ReadyToSendRx()) if(!CheckRxNMITimeout()) return;

   if (BrowserMode) PageCharsReceived++; //only called from browser mode?
   
   SendRxByte(CharToSend);
}

void SendASCIIStrImmediate(const char* CharsToSend)
{
   for(uint16_t CharNum = 0; CharNum < strlen(CharsToSend); CharNum++)
      SendPETSCIICharImmediate(ToPETSCII(CharsToSend[CharNum]));
}

void SendASCIIErrorStrImmediate(const char* CharsToSend)
{
   SendPETSCIICharImmediate(PETSCIIpink);
   SendASCIIStrImmediate(CharsToSend);
   SendPETSCIICharImmediate(PETSCIIreturn);
}

FLASHMEM void SendBrowserCommandsImmediate()
{
   PageCharsReceived = 0;
   PagePaused = false;

   SendPETSCIICharImmediate(PETSCIIreturn);
   SendPETSCIICharImmediate(PETSCIIpurple); 
   SendPETSCIICharImmediate(PETSCIIrvsOn); 
   SendASCIIStrImmediate("Browser Commands:\r");
   SendASCIIStrImmediate("S [Term]: Search   [Link#]m: Go to link\r");
   SendASCIIStrImmediate("Um [URL]: Go to URL       X: Exit\r");
   SendASCIIStrImmediate("  Return: Continue        P: Prev Page\r");
   
   //SendASCIIStrImmediate("  Rm: Reload page     Bm: Bookmark #/s/?\r");
   //SendASCIIStrImmediate("       D[s/u/?] [path]: Set Download dir");
   SendPETSCIICharImmediate(PETSCIIlightGreen);
}

void ParseHTMLTag()
{ //retrieve and interpret HTML Tag
  //https://www.w3schools.com/tags/
  
   char TagBuf[MaxTagSize];
   uint16_t BufCnt = 0;
   
   //pull tag from queue until >, queue empty, or buff max size
   while (RxQueueUsed > 0)
   {
      TagBuf[BufCnt] = PullFromRxQueue();
      if(TagBuf[BufCnt] == '>') break;
      if(++BufCnt == MaxTagSize-1) break;
   }
   TagBuf[BufCnt] = 0;  //terminate it

   //check for known tags and do formatting, etc
   if(strcmp(TagBuf, "br")==0 || strcmp(TagBuf, "p")==0 || strcmp(TagBuf, "/p")==0) 
   {
      SendPETSCIICharImmediate(PETSCIIreturn);
      PageCharsReceived += 40-(PageCharsReceived % 40);
   }
   else if(strcmp(TagBuf, "/b")==0) SendPETSCIICharImmediate(PETSCIIwhite); //unbold
   else if(strcmp(TagBuf, "b")==0) //bold, but don't change hyperlink color
   {
      if(!PrintingHyperlink) SendPETSCIICharImmediate(PETSCIIyellow);
   } 
   else if(strcmp(TagBuf, "eoftag")==0) SendBrowserCommandsImmediate();  // special tag to signal complete
   else if(strcmp(TagBuf, "li")==0) //list item
   {
      SendPETSCIICharImmediate(PETSCIIdarkGrey); 
      SendASCIIStrImmediate("\r * ");
      SendPETSCIICharImmediate(PETSCIIwhite); 
      PageCharsReceived += 40-(PageCharsReceived % 40)+3;
   }
   else if(strncmp(TagBuf, "a href=", 7)==0) 
   { //start of hyperlink text, save hyperlink
      //Printf_dbg("LinkTag: %s\n", TagBuf);
      SendPETSCIICharImmediate(PETSCIIpurple); 
      SendPETSCIICharImmediate(PETSCIIrvsOn); 
      if (UsedPageLinkBuffs < NumPageLinkBuffs)
      {
         for(uint16_t CharNum = 8; CharNum < strlen(TagBuf); CharNum++) //skip a href="
         { //terminate at first 
            if(TagBuf[CharNum]==' '  || 
               TagBuf[CharNum]=='\'' ||
               TagBuf[CharNum]=='\"' ||
               TagBuf[CharNum]=='#') TagBuf[CharNum] = 0; //terminate at space, #, ', or "
         }
         strcpy(PageLinkBuff[UsedPageLinkBuffs], TagBuf+8); // remove quote from beginning
         
         Printf_dbg("Link #%d: %s\n", UsedPageLinkBuffs+1, PageLinkBuff[UsedPageLinkBuffs]);
         UsedPageLinkBuffs++;
         
         if (UsedPageLinkBuffs > 9) SendPETSCIICharImmediate('0' + UsedPageLinkBuffs/10);
         SendPETSCIICharImmediate('0' + (UsedPageLinkBuffs%10));
      }
      else SendPETSCIICharImmediate('*');
      
      SendPETSCIICharImmediate(PETSCIIlightBlue); 
      SendPETSCIICharImmediate(PETSCIIrvsOff);
      PageCharsReceived++;
      PrintingHyperlink = true;
   }
   else if(strcmp(TagBuf, "/a")==0)
   { //end of hyperlink text
      SendPETSCIICharImmediate(PETSCIIwhite); 
      PrintingHyperlink = false;
   }
   else if(strcmp(TagBuf, "html")==0)
   { //Start of HTML
      SendPETSCIICharImmediate(PETSCIIclearScreen); // comment these two lines out to 
      UsedPageLinkBuffs = 0;                        //  scroll header instead of clear
      PageCharsReceived = 0;
      PagePaused = false;
   }
   //else Printf_dbg("Unk Tag: <%s>\n", TagBuf);  //There can be a lot of these...
   
} 

void ParseURL(const char * URL, stcURLParse &URLParse)
{
   //https://en.wikipedia.org/wiki/URL
   //https://www.w3.org/Library/src/HTParse.html
   //https://stackoverflow.com/questions/726122/best-ways-of-parsing-a-url-using-c
   //https://gist.github.com/j3j5/8336b0224167636bed462950400ff2df       Test URLs
   //the format of a URI is as follows: "ACCESS :// HOST : PORT / PATH # ANCHOR"
   
   URLParse.host[0] = 0;
   URLParse.port = 80;
   URLParse.path[0] = 0;
   
   //Find/skip access ID
   if(strstr(URL, "http://") != URL && strstr(URL, "https://") != URL) //no access ID, relative path only
   {
      strcat(URLParse.path, URL);
   }
   else
   {
      const char * ptrServerName = strstr(URL, "://")+3; //move past the access ID
      char * ptrPort = strstr(ptrServerName, ":");  //find port identifier
      char * ptrPath = strstr(ptrServerName, "/");  //find path identifier
      
      //need to check for userid? http://userid@example.com:8080/
      
      //finalize server name and update port, if present
      if (ptrPort != NULL) //there's a port ID
      {
         URLParse.port = atoi(ptrPort+1);  //skip the ":"
         strncpy(URLParse.host, ptrServerName, ptrPort-ptrServerName);
         URLParse.host[ptrPort-ptrServerName]=0; //terminate it
      }
      else if (ptrPath != NULL)  //there's a path
      {
         strncpy(URLParse.host, ptrServerName, ptrPath-ptrServerName);
         URLParse.host[ptrPath-ptrServerName]=0; //terminate it
      }
      else strcpy(URLParse.host, ptrServerName);  //no port or path
   
      //copy path, if present
      if (ptrPath != NULL) strcpy(URLParse.path, ptrPath);
      else strcpy(URLParse.path, "/");
   }

   Printf_dbg("\nOrig  = \"%s\"\n", URL);
   Printf_dbg(" serv = \"%s\"\n", URLParse.host);
   Printf_dbg(" port = %d\n", URLParse.port);
   Printf_dbg(" path = \"%s\"\n", URLParse.path);
} 

bool ReadClientLine(char* linebuf, uint16_t MaxLen)
{
   uint16_t charcount = 0;
   
   while (client.connected()) 
   {
      while (client.available()) 
      {
         uint8_t c = client.read();
         linebuf[charcount++] = c;
         if(charcount == MaxLen) 
         {
            SendASCIIErrorStrImmediate("Hdr: line too long");
            return false;
         }
         if (c=='\n')
         {
            linebuf[charcount] = 0; //terminate it
            return true;
         }
      }
   }
   SendASCIIErrorStrImmediate("Hdr: dropped");
   return false;
}

bool WebConnect(const stcURLParse *DestURL)
{
   //   case wc_Filter:   strcpy(UpdWebPage, "/read.php?a=http://");
   
   memcpy(PrevURLQueue[PrevURLQueueNum], DestURL, sizeof(stcURLParse)); //overwrite previous entry
   if (++PrevURLQueueNum == NumPrevURLQueues) PrevURLQueueNum = 0; //inc/wrap around top
 
   while (client.available()) client.read(); //clear client buffer
   client.stop();

   RxQueueHead = RxQueueTail = 0; //dump the queue
   
   Printf_dbg("Connect: \"%s%s\"\n", DestURL->host, DestURL->path);
   
   SendASCIIStrImmediate("\rConnecting to: ");
   SendASCIIStrImmediate(DestURL->host);
   SendASCIIStrImmediate(DestURL->path);
   SendPETSCIICharImmediate(PETSCIIreturn);
   
   if (client.connect(DestURL->host, DestURL->port))
   {
      const uint16_t MaxBuf = 200;
      char inbuf[MaxBuf];
      
      client.printf("GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", 
         DestURL->path, DestURL->host);

      //get response header
      while(ReadClientLine(inbuf, MaxBuf)==true)
      {
         Printf_dbg("H: %s", inbuf); 
         if (strcmp(inbuf, "\r\n") == 0) 
         {
            SendASCIIStrImmediate("Connected\r");
            return true; //blank line indicates end of header
         }
      }
      while (client.available()) client.read(); //clear client buffer
      client.stop();
   }

   SendASCIIErrorStrImmediate("Connect Failed");
   return false;
}

void DoSearch(const char *Term)
{
   char HexChar[] = "01234567890abcdef";
   stcURLParse URL =
   {
      "www.frogfind.com", //strcpy(URL.host = "www.frogfind.com");
      80,                 //URL.port = 80;
      "/?q=",             //strcpy(URL.path = "/?q=");
   };
      
   uint16_t UWPCharNum = strlen(URL.path);
   
   //encode special chars:
   //https://www.eso.org/~ndelmott/url_encode.html
   for(uint16_t CharNum=0; CharNum <= strlen(Term); CharNum++) //include terminator
   {
      //already lower case(?)
      uint8_t NextChar = Term[CharNum];
      if((NextChar >= 'a' && NextChar <= 'z') ||
         (NextChar >= 'A' && NextChar <= 'Z') ||
         (NextChar >= '.' && NextChar <= '9') ||  //   ./0123456789
          NextChar == 0)                          //include terminator
      {      
         URL.path[UWPCharNum++] = NextChar;      
      }
      else
      {
         //encode character (%xx hex val)
         URL.path[UWPCharNum++] = '%';
         URL.path[UWPCharNum++] = HexChar[NextChar >> 4];
         URL.path[UWPCharNum++] = HexChar[NextChar & 0x0f];
      }
   }
   
   WebConnect(&URL);
}

uint8_t HexCharToInt(uint8_t HexChar)
{
   // https://stackoverflow.com/questions/10156409/convert-hex-string-char-to-int
   return (((HexChar & 0xF) + (HexChar >> 6)) | ((HexChar >> 3) & 0x8));
}

void DownloadFile(const char *origPathName)
{  // Modifies (decodes) FileName
   // assumes client connected, header read, and ready for download

   char FileName[MaxURLPathSize]; //local copy for decoded version

   const char* ptrOrigFilename = strrchr(origPathName, '/'); //pointer to nav orig path/file name, find last slash
   if (ptrOrigFilename == NULL) ptrOrigFilename = origPathName; //use the whole thing if no slash
   else ptrOrigFilename++; //skip the slash

   //copy/decode special chars
   uint16_t NewCharNum = 0;
   uint16_t OrigCharNum = 0;
   while(ptrOrigFilename[OrigCharNum])
   {
      uint8_t NextChar = ptrOrigFilename[OrigCharNum];
      if(NextChar == '%' && ptrOrigFilename[OrigCharNum+1] && ptrOrigFilename[OrigCharNum+2])
      {
         NextChar = HexCharToInt(ptrOrigFilename[++OrigCharNum])<<4;
         NextChar |= HexCharToInt(ptrOrigFilename[++OrigCharNum]);
      }
      FileName[NewCharNum++] = NextChar;
      OrigCharNum++;
   }
   FileName[NewCharNum] = 0;

   SendASCIIStrImmediate("File: \"");
   SendASCIIStrImmediate(FileName);
   SendASCIIStrImmediate("\"\r");

   if (!client.connected())    
   {
      SendASCIIErrorStrImmediate("No data");  
      return;      
   }

   if (!SD.begin(BUILTIN_SDCARD))  // refresh, takes 3 seconds for fail/unpopulated, 20-200mS populated
   {
      SendASCIIErrorStrImmediate("No SD card");  
      return;      
   }
   
   //if (sourceFS->exists(FileNamePath))
   if (SD.exists(FileName))
   {
      SendASCIIErrorStrImmediate("File already exists");  
      return;      
   }
   
   File dataFile = SD.open(FileName, FILE_WRITE);
   if (!dataFile) 
   {
      SendASCIIErrorStrImmediate("Error opening file");
      return;
   }   
   
   SendASCIIStrImmediate("Downloading\r");

   uint32_t BytesRead = 0;
   while (client.connected()) 
   {
      while (client.available()) 
      {
         dataFile.write(client.read());
         BytesRead++;
      }
   }      
   dataFile.close();
   char buf[100];
   sprintf(buf, "Finished: %lu bytes\r", BytesRead);
   SendASCIIStrImmediate(buf);
}

void ModWebConnect(const stcURLParse *DestURL, char* strMods)
{  //Do WebConnect, apply Modifier argument
          
   if (*strMods == 'd') //download
   {
      if (WebConnect(DestURL)==true)
      {                     
         DownloadFile(DestURL->path);
         while (client.available()) client.read(); //clear client buffer
         client.stop();  //in case of unfinished/error, don't read it in as text
      }
   }  
   else WebConnect(DestURL); //default to reader, prev defined filterring
   
}

void ProcessBrowserCommand()
{
   char* CmdMsg = TxMsg; //local copy for manipulation
   
   if(strcmp(CmdMsg, "x") ==0) //Exit browse mode
   {
      while (client.available()) client.read(); //clear client buffer
      client.stop();
      BrowserMode = false;
      RxQueueHead = RxQueueTail = 0; //dump the queue
      AddToPETSCIIStrToRxQueueLN("\rBrowser mode exit");
   }
   
   else if(strcmp(CmdMsg, "p") ==0) // Previous web page
   {
      if (PrevURLQueueNum<2) PrevURLQueueNum += NumPrevURLQueues-2; //wrap around bottom
      else PrevURLQueueNum -= 2;
      
      Printf_dbg("PrevURL# %d\n", PrevURLQueueNum);
      WebConnect(PrevURLQueue[PrevURLQueueNum]);
   }
   
   else if(*CmdMsg == 'r') // Reload web page
   {
      if (PrevURLQueueNum == 0) PrevURLQueueNum = NumPrevURLQueues - 1; //wrap around bottom
	   else  PrevURLQueueNum--;
      
      Printf_dbg("CurrURL# %d\n", PrevURLQueueNum);
      ModWebConnect(PrevURLQueue[PrevURLQueueNum], ++CmdMsg);
   }
   
   else if(*CmdMsg >= '0' && *CmdMsg <= '9') //Hyperlink #
   {
      uint8_t CmdMsgVal = atoi(CmdMsg);
      
      if (CmdMsgVal > 0 && CmdMsgVal <= UsedPageLinkBuffs)
      {
         //we have a valid link # to follow...
         stcURLParse URL;
         
         ParseURL(PageLinkBuff[CmdMsgVal-1], URL); //zero based
         while (*CmdMsg >='0' && *CmdMsg <='9') CmdMsg++;  //move pointer past numbers
         
         if(URL.host[0] == 0) //relative path, use same server/port, append path
         {
            uint8_t CurQueuNum;
            if (PrevURLQueueNum == 0) CurQueuNum = NumPrevURLQueues - 1;
            else  CurQueuNum = PrevURLQueueNum - 1;
            
            if(URL.path[0] != '/') //if not root ref, add previous path to beginning
            {  
               char temp[MaxURLPathSize];
               strcpy(temp, URL.path); 
               strcpy(URL.path, PrevURLQueue[CurQueuNum]->path);
               strcat(URL.path, temp);
            }
            URL.port = PrevURLQueue[CurQueuNum]->port;
            strcpy(URL.host, PrevURLQueue[CurQueuNum]->host);
         }
         ModWebConnect(&URL, CmdMsg);
      }
   }
   
   else if(*CmdMsg == 'u') //URL
   {
      CmdMsg++; //past the 'u'
      while(*CmdMsg==' ') CmdMsg++;  //Allow for spaces after command
      
      stcURLParse URL;
      char httpServer[MaxTagSize] = "http://";
      strcat(httpServer, CmdMsg);
      ParseURL(httpServer, URL);
      WebConnect(&URL);
   }
   
   else if(*CmdMsg == 's') //search
   {
      CmdMsg++; //past the 's'
      while(*CmdMsg==' ') CmdMsg++;  //Allow for spaces after command   
      DoSearch(CmdMsg);  //includes WebConnect
   }
   
   else if(*CmdMsg != 0) //unrecognized command
   {
      SendASCIIErrorStrImmediate("Unknown Command");
   }
   
   else if(PagePaused) //empty command, and paused
   { 
      SendPETSCIICharImmediate(PETSCIIcursorUp); //Cursor up to overwrite prompt & scroll on
      //SendPETSCIICharImmediate(PETSCIIclearScreen); //clear screen for next page
   }
   
   SendPETSCIICharImmediate(PETSCIIwhite); 
   PageCharsReceived = 0; //un-pause on any command, or just return key
   PagePaused = false;
   UsedPageLinkBuffs = 0;
}
