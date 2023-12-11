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

extern void RemoteLaunch(bool SD_nUSB, const char *FileNamePath);

void ToLcaseASSCII(uint8_t *FromPETSCII)
{
   *FromPETSCII &= 0x7f; //bit 7 is Cap in Graphics mode
   if (*FromPETSCII & 0x40) *FromPETSCII |= 0x20;  //conv to lower case
}

void SendPETSCIICharImmediate(uint8_t CharToSend)
{
   //wait for c64 to be ready or NMI timeout
   while(!ReadyToSendRx()) if(!CheckRxNMITimeout()) return;

   if (BrowserMode)  //perhaps only called from browser mode anyway?
   {  //count printable chars only
      if (CharToSend>=160 || (CharToSend >=32 && CharToSend <128)) PageCharsReceived++; //printable char
      else if (CharToSend==PETSCIIclearScreen) PageCharsReceived = 0;  //reset count on screen clear
      else if (CharToSend==PETSCIIreturn) PageCharsReceived += 40-(PageCharsReceived % 40); //add to start of next row
   }
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
   SendPETSCIICharImmediate(PETSCIIwhite);
   Printf_dbg("*Err: %s\n", CharsToSend);
}

FLASHMEM void SendCommandSummaryImmediate(bool Paused)
{
   SendPETSCIICharImmediate(PETSCIIreturn);
   SendPETSCIICharImmediate(PETSCIIpurple);
   if (Paused) 
   {
      SendPETSCIICharImmediate(PETSCIIrvsOn);
      SendASCIIStrImmediate("pause (ret or ");
   }
   else SendASCIIStrImmediate("\rfinished (");
   SendASCIIStrImmediate("#,s,p,r,u,b,d,x,?)\r");
   //SendPETSCIICharImmediate(PETSCIIrvsOff);
   SendPETSCIICharImmediate(PETSCIIlightGreen);   
}

FLASHMEM void SendBrowserCommandsImmediate()
{
   SendPETSCIICharImmediate(PETSCIIhiLoChrSet);
   SendPETSCIICharImmediate(PETSCIIreturn);
   SendPETSCIICharImmediate(PETSCIIpurple); 
   SendPETSCIICharImmediate(PETSCIIrvsOn); 
   SendASCIIStrImmediate("Browser Commands:\r");
   SendPETSCIICharImmediate(PETSCIIgreen); 
   SendASCIIStrImmediate("S [Term]: Search         Bx: Bookmarks\rU");
   SendPETSCIICharImmediate(PETSCIIpink); 
   SendASCIIStrImmediate("m");
   SendPETSCIICharImmediate(PETSCIIgreen); 
   SendASCIIStrImmediate(" [URL]: Go to URL  Return: Continue\r[Link#]");
   SendPETSCIICharImmediate(PETSCIIpink); 
   SendASCIIStrImmediate("m");
   SendPETSCIICharImmediate(PETSCIIgreen); 
   SendASCIIStrImmediate(": Go to link  D d:p: Set DL dir\r      R");
   SendPETSCIICharImmediate(PETSCIIpink); 
   SendASCIIStrImmediate("m");
   SendPETSCIICharImmediate(PETSCIIgreen); 
   SendASCIIStrImmediate(": Reload page     X: Exit\r");
   SendASCIIStrImmediate("       P: Prev Page       ?: This List\r");
   SendPETSCIICharImmediate(PETSCIIpink); 
   SendASCIIStrImmediate("m");
   //SendPETSCIICharImmediate(PETSCIIpurple); 
   SendASCIIStrImmediate("=(D)ownload,(F)ilter,(R)aw,(none)deflt\r");
   SendPETSCIICharImmediate(PETSCIIlightGreen);
}

FLASHMEM uint8_t HexCharToInt(uint8_t HexChar)
{
   // https://stackoverflow.com/questions/10156409/convert-hex-string-char-to-int
   return (((HexChar & 0xF) + (HexChar >> 6)) | ((HexChar >> 3) & 0x8));
}

FLASHMEM bool CheckAndDecode(const char *ptrChars, uint8_t *ptrRetChar)
{  //check if next 3 chars are decodable, decode if they are, otherwise return false
   if(*ptrChars == '%' && ptrChars[1] && ptrChars[2]) //something there, not end of chars
   {
      *ptrRetChar = (HexCharToInt(ptrChars[1])<<4) | HexCharToInt(ptrChars[2]);
      return true;
   }
   return false;
}

FLASHMEM void CopyDecode(const char *FromEncoded, char *ToDecoded)
{  //copy or overwrite str, decode special chars, terminate new end
   uint16_t NewCharNum = 0;
   uint16_t OrigCharNum = 0;
   uint8_t NextChar;
   
   Printf_dbg("Dec From: %s\n", FromEncoded);
   while(FromEncoded[OrigCharNum])
   {
      if(CheckAndDecode(FromEncoded+OrigCharNum, &NextChar)) OrigCharNum+=3;
      else NextChar = FromEncoded[OrigCharNum++];
      
      ToDecoded[NewCharNum++] = NextChar;
   }
   ToDecoded[NewCharNum] = 0;
   Printf_dbg("To: %s\n", ToDecoded);
}

FLASHMEM void CopyEncode(const char *FromRaw, char *ToEncoded)
{
   uint16_t EncCharNum = 0;
   char HexChar[] = "0123456789abcdef";
   
   Printf_dbg("Enc From: %s\n", FromRaw);
   //encode special chars:
   //https://www.eso.org/~ndelmott/url_encode.html
   for(uint16_t RawCharNum=0; RawCharNum <= strlen(FromRaw); RawCharNum++) //include terminator
   {
      uint8_t NextChar = FromRaw[RawCharNum];
      if((NextChar >= 'a' && NextChar <= 'z') ||
         (NextChar >= 'A' && NextChar <= 'Z') ||
         (NextChar >= '.' && NextChar <= '9') ||  //   ./0123456789
          NextChar == 0)                          //include terminator
      {      
         ToEncoded[EncCharNum++] = NextChar;  //normal char, just copy  
      }
      else
      {
         //encode character (%xx hex val)
         ToEncoded[EncCharNum++] = '%';
         ToEncoded[EncCharNum++] = HexChar[NextChar >> 4];
         ToEncoded[EncCharNum++] = HexChar[NextChar & 0x0f];
      }
   }
   Printf_dbg("To: %s\n", ToEncoded);
}

void DumpQueueUnPausePage()
{
   RxQueueHead = RxQueueTail = 0;
   UnPausePage();
}

void UnPausePage()
{
   UsedPageLinkBuffs = 0;
   PageCharsReceived = 0;
   PagePaused = false;   
}

void ParseHTMLTag()
{ //retrieve and interpret HTML Tag
  //https://www.w3schools.com/tags/
  
   //pull tag from queue until '>', or queue empty
   char TagBuf[MaxTagSize];
   uint16_t BufCnt = 0;
   bool ToLowerCase = true;
   
   while (RxQueueUsed > 0)
   {
      uint8_t NextChar = PullFromRxQueue();
      if(NextChar == '>') break;
      if(BufCnt < MaxTagSize-1)
      { //keep incrementing until full, then keep going until > or empty queue
         if(NextChar == '=') ToLowerCase = false; //stop lower case at/after '=' separator (URL)
         
         TagBuf[BufCnt++] = ToLowerCase ? tolower(NextChar) : NextChar;
      }
   }
   TagBuf[BufCnt] = 0;  //terminate it
   
   //check for known tags and do formatting, etc
   if(strcmp(TagBuf, "br")==0 || strcmp(TagBuf, "p")==0 || strcmp(TagBuf, "/p")==0 || strcmp(TagBuf, "/ul")==0) 
   {
      SendPETSCIICharImmediate(PETSCIIreturn);
   }
   
   else if(strcmp(TagBuf, "/b")==0) SendPETSCIICharImmediate(PETSCIIwhite); //unbold
   
   else if(strcmp(TagBuf, "b")==0) //bold, but don't change hyperlink color
   {
      if(!PrintingHyperlink) SendPETSCIICharImmediate(PETSCIIyellow);
   } 
   
   else if(strcmp(TagBuf, "eoftag")==0) // special tag to signal page complete
   {
      //SendPETSCIICharImmediate(PETSCIIgrey); 
      //SendASCIIStrImmediate("\r\r-end-");
      SendCommandSummaryImmediate(false);
   }
   
   else if(strcmp(TagBuf, "title")==0) //page title
   {
      uint8_t CharNum = 0;
      
      while (RxQueueUsed > 0 && CharNum < eepBMTitleSize-1)
      {
         uint8_t InChar = PullFromRxQueue();
         if(InChar == '<') //found the end of title, asumes Title tag has no embedded tags
         {
            while (RxQueueUsed > 0 && InChar != '>') InChar = PullFromRxQueue();
            break;
         }
         else CurrPageTitle[CharNum++] = InChar;
      }
      CurrPageTitle[CharNum]=0;
      Printf_dbg("Title: %s\n", CurrPageTitle);
   }
   
   else if(strcmp(TagBuf, "li")==0) //list item
   {
      SendPETSCIICharImmediate(PETSCIIdarkGrey); 
      SendASCIIStrImmediate("\r * ");
      SendPETSCIICharImmediate(PETSCIIwhite); 
   }
   
   else if(strncmp(TagBuf, "petscii", 7)==0) // custom tag for PETSCII chars
   {
      uint8_t NextChar;
      char * ptrCharNum = TagBuf+7; //start after "petscii" offset
      while(*ptrCharNum==' ') ptrCharNum++;  //Allow for spaces after petscii
      
      //only acceptable argument is encoded values %xx, spaces allowed in between
      while(CheckAndDecode(ptrCharNum, &NextChar) == true)
      {
         SendPETSCIICharImmediate(NextChar); 
         ptrCharNum+=3;
         while(*ptrCharNum==' ') ptrCharNum++;  //Allow for spaces after values
      }
   }
   
   else if(strncmp(TagBuf, "a ", 2)==0) //start of hyperlink text, save hyperlink
   { 
      //allow for things like <a data-pw="storyLink" href="/...">
      char * ptrCharNum = strstr(TagBuf, "href=");
      if (ptrCharNum == NULL) return; //not really a link?
      
      ptrCharNum += 6; //skip href= and openning quote
            
      //Printf_dbg("LinkTag: %s\n", ptrCharNum);
      SendPETSCIICharImmediate(PETSCIIpurple); 
      SendPETSCIICharImmediate(PETSCIIrvsOn); 
      if (UsedPageLinkBuffs < NumPageLinkBuffs)
      {
         for(uint16_t CharNum = 0; CharNum < strlen(ptrCharNum); CharNum++) 
         { //terminate at first 
            if(ptrCharNum[CharNum]==' '  || 
               ptrCharNum[CharNum]=='\'' ||
               ptrCharNum[CharNum]=='\"' ||
               ptrCharNum[CharNum]=='#') ptrCharNum[CharNum] = 0; //terminate at space, #, ', or "
         }
         strcpy(PageLinkBuff[UsedPageLinkBuffs], ptrCharNum);
         
         Printf_dbg("Link #%d: %s\n", UsedPageLinkBuffs+1, PageLinkBuff[UsedPageLinkBuffs]);
         UsedPageLinkBuffs++;
         
         if (UsedPageLinkBuffs > 9) SendPETSCIICharImmediate('0' + UsedPageLinkBuffs/10);
         SendPETSCIICharImmediate('0' + (UsedPageLinkBuffs%10));
      }
      else SendPETSCIICharImmediate('*');
      
      SendPETSCIICharImmediate(PETSCIIlightBlue); 
      SendPETSCIICharImmediate(PETSCIIrvsOff);
      PrintingHyperlink = true;
   }
   
   else if(strcmp(TagBuf, "/a")==0) //end of hyperlink text
   {
      SendPETSCIICharImmediate(PETSCIIwhite); 
      PrintingHyperlink = false;
   }
   
   else if(strcmp(TagBuf, "html")==0) //Start of HTML
   {
      SendPETSCIICharImmediate(PETSCIIclearScreen);
      UnPausePage();
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
   URLParse.postpath[0] = 0;
   
   //Find/skip access ID
   if(strstr(URL, "http://") == URL || strstr(URL, "https://") == URL) 
   {  //parse full URL
      const char * ptrServerName = strstr(URL, "://")+3; //move past the access ID
      char * ptrPort = strstr(ptrServerName, ":");  //find port identifier
      char * ptrPath = strstr(ptrServerName, "/");  //find path identifier
      bool PortPresent = false;  //Is the Port defined in the URL?
      
      //need to check for userid? http://userid@example.com:8080/
         
      //check for port presence
      if (ptrPort != NULL) //there's a potential port ID 
      {
         if(ptrPath != NULL)
         {
            if (ptrPort < ptrPath) PortPresent = true; //is the : before the path start? 
         }
         else PortPresent = true; //port, but no path
      }
      
      //set host name & port (if present):
      if (PortPresent)
      {
         URLParse.port = atoi(ptrPort+1);  //skip the ":"
         strncpy(URLParse.host, ptrServerName, ptrPort-ptrServerName);
         URLParse.host[ptrPort-ptrServerName]=0; //terminate it
      }
      else
      {
         if (ptrPath != NULL)  //there's a path; set host name
         {
            strncpy(URLParse.host, ptrServerName, ptrPath-ptrServerName);
            URLParse.host[ptrPath-ptrServerName]=0; //terminate it
         }
         else strcpy(URLParse.host, ptrServerName);  //no path
      }
      
      //copy path, if present
      if (ptrPath != NULL) strcpy(URLParse.path, ptrPath);
      else strcpy(URLParse.path, "/");
   }
   else
   {  //no access ID, relative path only
      strcat(URLParse.path, URL);
   }

   
   //check for query or fragment;
   char * ptrPostPath = strstr(URLParse.path, "?"); //check for query first
   if (ptrPostPath == NULL) ptrPostPath = strstr(URLParse.path, "#"); //if not, then check for fragment

   if (ptrPostPath != NULL) //capture Post Path info separately.
   {
      strcpy(URLParse.postpath, ptrPostPath);
      *ptrPostPath=0; //terminate URLParse.path there
   }

   Printf_dbg("\nOrig = \"%s\"\nParsed:\n", URL);
   Printf_dbg(" serv = \"%s\"\n", URLParse.host);
   Printf_dbg(" port = %d\n", URLParse.port);
   Printf_dbg(" path = \"%s\"\n", URLParse.path);
   Printf_dbg(" post = \"%s\"\n", URLParse.postpath);
} 

bool ReadClientLine(char* linebuf, uint16_t MaxLen)
{
   uint16_t charcount = 0;
   
   while (client.connected()) 
   {
      while (client.available()) 
      {
         uint8_t c = client.read();
         if(charcount < MaxLen-1) //leave room for term 
         {
            linebuf[charcount++] = c;
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

void ClearClientStop()
{  //clear client buffer and stop client
   while (client.available()) client.read(); 
   client.stop();
}

void AddToPrevURLQueue(const stcURLParse *URL) //add URL to the prev queue
{
   if (++PrevURLQueueNum == NumPrevURLQueues) PrevURLQueueNum = 0; //inc/wrap around top
   memcpy(PrevURLQueue[PrevURLQueueNum], URL, sizeof(stcURLParse)); //overwrite previous entry
}

uint32_t WebConnect(const stcURLParse *DestURL)
{
   if (DestURL->host[0] == 0)
   {
      SendASCIIErrorStrImmediate("No Host");
      return 0;
   }
   
   ClearClientStop();

   DumpQueueUnPausePage();
   strcpy(CurrPageTitle, "Unknown"); //gets populated via title tag 
      
   
   bool Connected;
   IPAddress HostIP;
   char buf[20];
   
   Printf_dbg("Connect: \"%s:%d%s%s\"\n", DestURL->host, DestURL->port, DestURL->path, DestURL->postpath);
   
   SendPETSCIICharImmediate(PETSCIIpurple);
   SendASCIIStrImmediate("\rConnecting to:\r");
   SendPETSCIICharImmediate(PETSCIIyellow);
   SendASCIIStrImmediate(DestURL->host);
   sprintf(buf, ":%d", DestURL->port);
   SendASCIIStrImmediate(buf);
   SendASCIIStrImmediate(DestURL->path);
   SendASCIIStrImmediate(DestURL->postpath);
   SendPETSCIICharImmediate(PETSCIIreturn);

   if(inet_aton(DestURL->host, HostIP)) 
   {
      SendPETSCIICharImmediate(PETSCIIlightGrey);
      SendASCIIStrImmediate("Using IPaddr\r");
      Connected = client.connect(HostIP, DestURL->port);
   }
   else Connected = client.connect(DestURL->host, DestURL->port);
   
   if (Connected)
   {
      bool ShowHeader = true; //initially true
      uint32_t Length = 0;
      const uint16_t MaxBuf = 350;
      char inbuf[MaxBuf];
      
      client.printf("GET %s%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", 
         DestURL->path, DestURL->postpath, DestURL->host);

      //get response header
      //https://www.tutorialspoint.com/http/http_responses.htm
      SendPETSCIICharImmediate(PETSCIIbrown);  // header info color
      while(ReadClientLine(inbuf, MaxBuf)==true)
      {
         //Printf_dbg("H: %s", inbuf);  //causes lost data
         if (ShowHeader) 
         {
            if (strstr(inbuf, "HTTP/1.1 200") != NULL) ShowHeader = false;
            else SendASCIIStrImmediate(inbuf);
         }
         
         if (strncmp(inbuf, "Content-Length: ", 16) == 0) Length = strtoul(inbuf+16, NULL, 10);
         
         if (strcmp(inbuf, "\r\n") == 0) //blank line indicates end of header
         {
            SendPETSCIICharImmediate(PETSCIIwhite);
            if(!client.connected()) SendASCIIStrImmediate("Not "); //may have been header only
            SendASCIIStrImmediate("Connected\r");
            return Length;
         }
      }
      ClearClientStop();
   }

   SendASCIIErrorStrImmediate("Connect Failed");
   return 0;
}

FLASHMEM void DoSearch(const char *Term)
{
   stcURLParse URL =
   {
      "www.frogfind.com", //strcpy(URL.host = "www.frogfind.com");
      80,                 //URL.port = 80;
      "/",                //strcpy(URL.path = "/");
      "?q=",              //strcpy(URL.postpath = "?q=");
   };
   
   CopyEncode(Term, URL.postpath+strlen(URL.postpath)); //add encoded to end

   WebConnect(&URL);
   AddToPrevURLQueue(&URL);
}

FLASHMEM bool InitCheckSD()
{
   if (!SD.begin(BUILTIN_SDCARD))  // refresh, takes 3 seconds for fail/unpopulated, 20-200mS populated
   {
      SendASCIIErrorStrImmediate("No SD card");  
      return false;      
   }
   return true;
}

FLASHMEM void DownloadFile(stcURLParse *DestURL)
{  // Modifies (decodes) FileName

   char FileName[MaxURLPathSize]; //local copy for decoded version

   const char* ptrOrigFilename = strrchr(DestURL->path, '/'); //pointer to nav orig path/file name, find last slash
   if (ptrOrigFilename == NULL) ptrOrigFilename = DestURL->path; //use the whole thing if no slash
   else ptrOrigFilename++; //skip the slash

   CopyDecode(ptrOrigFilename, FileName);

   SendPETSCIICharImmediate(PETSCIIpurple);
   SendASCIIStrImmediate("\rDownload:\r");
   SendPETSCIICharImmediate(PETSCIIyellow);
   SendASCIIStrImmediate("File \"");
   SendASCIIStrImmediate(FileName);
   SendASCIIStrImmediate("\"\rPath ");

   char FileNamePath[TxMsgMaxSize+MaxURLPathSize];
   FS *sourceFS;
   uint8_t USB_SD = EEPROM.read(eepAdDLPathSD_USB);
   EEPreadStr(eepAdDLPath, FileNamePath); 
   
   if(USB_SD == Drive_SD)
   {
      if (!InitCheckSD()) return;      
      SendASCIIStrImmediate("sd:");
      sourceFS = &SD;
   }
   else
   {
      SendASCIIStrImmediate("usb:");
      sourceFS = &firstPartition;      
   }
   
   SendASCIIStrImmediate(FileNamePath);
   SendASCIIStrImmediate("\r");

   strcat(FileNamePath, FileName); //add filename to dest path

   if (sourceFS->exists(FileNamePath))
   {
      //Prompt for overwrite
      SendPETSCIICharImmediate(PETSCIIorange); 
      SendASCIIStrImmediate("File exists, overwrite?(y/n) ");
      while(SwiftRegStatus & SwiftStatusTxEmpty) 
      {
         if (BtnPressed || Serial.available()) 
         {  //watch for button press or serial during this blocking loop 
            SendASCIIErrorStrImmediate("btn/ser int"); 
            return; 
         }
      }
      
      SendPETSCIICharImmediate(SwiftTxBuf);
      ToLcaseASSCII(&SwiftTxBuf);
      if (SwiftTxBuf != 'y')
      {
         SendASCIIErrorStrImmediate("\raborting"); 
         SwiftRegStatus |= SwiftStatusTxEmpty; //clear the flag after last SwiftTxBuf access
         return;         
      }
      SwiftRegStatus |= SwiftStatusTxEmpty; //clear the flag after last SwiftTxBuf access

      SendPETSCIICharImmediate(PETSCIIyellow); 
      SendASCIIStrImmediate("\rOverwriting\r");
   }
   
   uint32_t Length = WebConnect(DestURL);
   AddToPrevURLQueue(DestURL);
   
   if (!client.connected() || Length == 0)    
   {
      SendASCIIErrorStrImmediate("No data");  
      return;      
   }
   
   File dataFile = sourceFS->open(FileNamePath, FILE_WRITE);
   if (!dataFile) 
   {
      SendASCIIErrorStrImmediate("Error opening file");
      return;
   }   
   
   char buf[50];
   const uint16_t MaxChunkSize = 1460; //matches client buffer size
   uint8_t DataChunk[MaxChunkSize];
   
   sprintf(buf, "Downloading %lu bytes\r", Length);
   SendASCIIStrImmediate(buf);

   uint32_t DotThresh = Length; //when to print a dot
   while (client.connected()) 
   {
      uint32_t ChunkSize = client.available();
      if (ChunkSize)
      {
         if (ChunkSize > MaxChunkSize) ChunkSize = MaxChunkSize;
         client.read(DataChunk, ChunkSize);
         dataFile.write(DataChunk, ChunkSize);
         Printf_dbg("Chunk: %d\n", ChunkSize);
         if (ChunkSize > Length)
         {
            dataFile.close();
            SendASCIIErrorStrImmediate("\rExtra data received");
            return;
         }
         Length -= ChunkSize;
         if(Length == 0)
         {
            dataFile.close();
            SendASCIIStrImmediate("\rDownload Complete\r"); 
            return;            
         }
         if (DotThresh > Length)
         {
            if(DotThresh < BytesPerDot) DotThresh=0;
            else DotThresh -= BytesPerDot;
            SendPETSCIICharImmediate('.');
         }
      }
   }      
   dataFile.close();
   SendASCIIErrorStrImmediate("\rConnection lost, incomplete");
}

FLASHMEM bool ValidModifier(const char cMod)
{
   char ValidMods[] = "dofr ";
   for (uint8_t charnum=0; charnum<=strlen(ValidMods); charnum++) // <= to include term check as valid
   {
      if(cMod == ValidMods[charnum]) return true;
   }
   
   SendASCIIErrorStrImmediate("Unexpected modifier");         
   return false;
}

bool isURLFiltered(const stcURLParse *URL)
{
   return (strcmp(URL->host, "www.frogfind.com")==0); 
}

FLASHMEM bool DLExtension(const char * Filename)
{
   char * Extension = strrchr(Filename, '.');
   Printf_dbg("*--Ext: ");
   if (Extension != NULL)
   {
      Extension++; //skip the '.'
      Printf_dbg("%s\n", Extension);
   }
   else 
   {
      Printf_dbg("none\n");
      return false;
   }
      
   uint8_t ExtNum = 0;
   char ExtLower[strlen(Extension)+1]; //for lower case copy w/ term
   const char DLExts [][5] =
   {
      "prg",
      "crt",
      "sid",
      "hex",
   };   

   //copy to lower case local str
   for(uint16_t CharNum=0; CharNum <= strlen(Extension); CharNum++) ExtLower[CharNum] = tolower(Extension[CharNum]);
   
   while(ExtNum < sizeof(DLExts)/sizeof(DLExts[0]))
   {
      if(strcmp(ExtLower, DLExts[ExtNum++])==0) return true;
   }
   return false;
}

void ModWebConnect(stcURLParse *DestURL, char cMod)
{  //Do WebConnect, apply Modifier argument
   //assumes char already qualified via ValidModifier()
   
   if (cMod == ' ' || cMod == 0) //modifier not specified, check for auto-download
   {
      if(DLExtension(DestURL->path)) cMod = 'd'; //don't overwrite on auto-download
   }
   
   switch (cMod)
   {
      case 'd': //download
         DownloadFile(DestURL);
         ClearClientStop();  //in case of unfinished/error, don't read it in as text
         break;
         
      case 'f':  //filter via FrogFind
         if(isURLFiltered(DestURL)) WebConnect(DestURL); //go now if already filtered
         else 
         {  //make filtered URL & connect
            stcURLParse URL =
            {
               "www.frogfind.com",     //strcpy(URL.host = "www.frogfind.com");
               80,                     //URL.port = 80;
               "/read.php",            //strcpy(URL.path = "/read.php?a=http://");
               "?a=http://"            //URL.postpath
            };
                  
            strcat(URL.postpath, DestURL->host);
            strcat(URL.postpath, DestURL->path);
            strcat(URL.postpath, DestURL->postpath);
            //write back filtered URL in case it's pointing to a history queue item (ie 'rf' command) 
            //  so link patchs will be relative to correct server
            memcpy(DestURL, &URL, sizeof(stcURLParse)); 
            
            WebConnect(&URL);
         }
         break;
         
      case 'r': //Raw, no filterring
         if(!isURLFiltered(DestURL)) WebConnect(DestURL); //go now if already raw
         else
         {  //strip off frogfind
            char * ptrURL = strstr(DestURL->postpath, "http");  // "/read.php?a=http://"
            if (ptrURL == NULL)
            {
               SendASCIIErrorStrImmediate("Could not remove filter");
               WebConnect(DestURL);
            }
            else
            {
               stcURLParse URL;
               ParseURL(ptrURL, URL);
               
               //write back unfiltered URL in case it's pointing to a history queue item (ie 'rr' command) 
               //  so link patchs will be relative to correct server
               memcpy(DestURL, &URL, sizeof(stcURLParse)); 
                  
               WebConnect(&URL);  
            }
         }
         break;
      default:   
         WebConnect(DestURL); //no mod, default to reader, prev defined filterring
         break;
   }
}

FLASHMEM void BC_Bookmarks(char* CmdMsg) 
{  //*CmdMsg == 'b'
   DumpQueueUnPausePage();
   
   CmdMsg++; //past the 'b'
   
   if (*CmdMsg == 0 || *CmdMsg == 'u') //no modifier or show URLs
   {  //print bookmark list via buffer
      char bufURL[eepBMURLSize];
      char bufTitle[eepBMTitleSize];

      AddRawStrToRxQueue("<html><b>Saved Bookmarks:</b>"); 
      for (uint8_t BMNum=0; BMNum < eepNumBookmarks; BMNum++)
      {
         if (*CmdMsg == 'u')
         {
            Add_BR_ToRxQueue();
            AddRawStrToRxQueue("<petscii%1e>#"); //green
            AddRawCharToRxQueue('1'+BMNum);
            AddRawStrToRxQueue(":");
         }
         else AddRawStrToRxQueue("<li>");
         
         AddRawStrToRxQueue("<a href=\"");
         EEPreadStr(eepAdBookmarks+BMNum*(eepBMTitleSize+eepBMURLSize)+eepBMTitleSize,bufURL); //URL
         Printf_dbg("BM#%d- %s", BMNum, bufURL);
         AddRawStrToRxQueue(bufURL);
         AddRawStrToRxQueue("\">");
         EEPreadStr(eepAdBookmarks+BMNum*(eepBMTitleSize+eepBMURLSize),bufTitle); //Title
         Printf_dbg("- %s\n", bufTitle);
         AddRawStrToRxQueue(bufTitle);         
         AddRawStrToRxQueue("</a>");
         if (*CmdMsg == 'u')
         {
            Add_BR_ToRxQueue();
            AddRawStrToRxQueue(" ");
            if(strncmp(bufURL, "http://", 7)==0) AddRawStrToRxQueue(bufURL+7);
            else AddRawStrToRxQueue(bufURL);
         }
      }
      AddRawStrToRxQueue("<eoftag>");
   }
   
   else if(*CmdMsg >= '1' && *CmdMsg <= '9')
   {  //jump to bookmark #
      stcURLParse URL;
      char buf[eepBMURLSize];
      uint8_t BMNum = *CmdMsg - '1'; //zero based
      
      EEPreadStr(eepAdBookmarks+BMNum*(eepBMTitleSize+eepBMURLSize)+eepBMTitleSize, buf); //URL
      ParseURL(buf, URL);
      
      if(DLExtension(URL.path)) DownloadFile(&URL);
      else WebConnect(&URL);
      AddToPrevURLQueue(&URL);
   }
   
   else if(*CmdMsg == 's' && *(CmdMsg+1) >= '1' && *(CmdMsg+1) <= '9')
   {  //set bookmark # to current page

      //re-encode to maximize eeprom usage, but could be too long...
      char strURL[MaxURLHostSize+MaxURLPathSize+MaxURLPathSize+12]; //   +"HTTP:// & :Prt"
      
      sprintf(strURL, "http://%s:%d%s%s",
         PrevURLQueue[PrevURLQueueNum]->host,
         PrevURLQueue[PrevURLQueueNum]->port,
         PrevURLQueue[PrevURLQueueNum]->path,
         PrevURLQueue[PrevURLQueueNum]->postpath);
      
      if (strlen(strURL) >= eepBMURLSize)
      {
         SendASCIIErrorStrImmediate("URL too long");
         return;              
      }
      CmdMsg++;
      uint8_t BMNum = *CmdMsg - '1'; //zero based
      while(!ReadyToSendRx()) CheckRxNMITimeout(); //Let any outstanding NMIs clear before EEPROM writes (resource hog)
      EEPwriteStr(eepAdBookmarks+BMNum*(eepBMTitleSize+eepBMURLSize), CurrPageTitle); //Title
      EEPwriteStr(eepAdBookmarks+BMNum*(eepBMTitleSize+eepBMURLSize)+eepBMTitleSize, strURL); //URL
      
      //Send confirmation
      Add_BR_ToRxQueue();
      AddRawStrToRxQueue("<b>Bookmark #"); 
      AddRawCharToRxQueue(*CmdMsg);
      AddRawStrToRxQueue(" updated to:</b><br>\"");
      AddRawStrToRxQueue(CurrPageTitle);
      AddRawStrToRxQueue("\" at");
      Add_BR_ToRxQueue();
      AddRawStrToRxQueue(strURL);
      AddRawStrToRxQueue("<eoftag>");
   }
   else if(*CmdMsg == 'r' && *(CmdMsg+1) >= '1' && *(CmdMsg+1) <= '9')
   {  //rename bookmark # to argument
      CmdMsg++; //past the 'r'
      char cBMNum = *CmdMsg;
      CmdMsg++; //past the #
      while(*CmdMsg==' ') CmdMsg++;  //Allow for spaces after command   

      while(!ReadyToSendRx()) CheckRxNMITimeout(); //Let any outstanding NMIs clear before EEPROM writes (resource hog)
      EEPwriteStr(eepAdBookmarks+(cBMNum - '1')*(eepBMTitleSize+eepBMURLSize), CmdMsg); //rename, cBMNum to zero based

      Add_BR_ToRxQueue();
      AddRawStrToRxQueue("<b>Bookmark #"); 
      AddRawCharToRxQueue(cBMNum);
      AddRawStrToRxQueue(" renamed to:</b><br>\"");
      AddRawStrToRxQueue(CmdMsg);
      AddRawStrToRxQueue("\"");
      AddRawStrToRxQueue("<eoftag>");
   }
   else
   {
      SendASCIIErrorStrImmediate("Unk Bookmark Mod");
      return;  
   }   
}

FLASHMEM void BC_Downloads(char* CmdMsg)
{  //*CmdMsg == 'd'   List DL files, set DL path
   uint8_t USB_SD;
   FS *sourceFS;
   
   CmdMsg++; //past the 'd'
   
   if (*CmdMsg=='l')
   {  //List downloads command
      char FileNamePath[TxMsgMaxSize+MaxURLPathSize];
      bool Empty = true;
      
      USB_SD = EEPROM.read(eepAdDLPathSD_USB);
      EEPreadStr(eepAdDLPath, FileNamePath); 
      
      DumpQueueUnPausePage();
      AddRawStrToRxQueue("<html><b>Download Directory:<br> ");
      if(USB_SD == Drive_SD)
      {
         if (!InitCheckSD()) return;      
         AddRawStrToRxQueue("sd:");
         sourceFS = &SD;
      }
      else
      {
         AddRawStrToRxQueue("usb:");
         sourceFS = &firstPartition;      
      }
      
      AddRawStrToRxQueue(FileNamePath);
      AddRawStrToRxQueue("</b><br>Select Link to Lauch<br>");

      File dir = sourceFS->open(FileNamePath);
      while (File entry = dir.openNextFile()) 
      {
         AddRawStrToRxQueue("<li>"); //return and bullet
         Empty = false;
         if (entry.isDirectory())
         {  //not linking to sub-dirs, for now...
            AddRawCharToRxQueue('/');
            AddRawStrToRxQueue(entry.name());
         }
         else 
         {  //it's a file. add as local hyperlink          
            AddRawStrToRxQueue("<a href=\"lcl:");
            CopyEncode(entry.name(), FileNamePath);
            AddRawStrToRxQueue(FileNamePath);
            AddRawStrToRxQueue("\">");
            AddRawStrToRxQueue(entry.name()); //name shown
            AddRawStrToRxQueue("</a>");
         }
         entry.close();
      }
      if (Empty) AddRawStrToRxQueue(" -empty-");
      Add_BR_ToRxQueue();
      AddRawStrToRxQueue("<eoftag>");      
      return;
   } 
   else if (*CmdMsg=='s')
   {  //Set download path
      CmdMsg++; //past the 's'
      while(*CmdMsg==' ') CmdMsg++;  //Allow for spaces after command   
      if (strncmp(CmdMsg, "usb:", 4) == 0)
      {
         CmdMsg+=4;
         USB_SD = Drive_USB;
         sourceFS = &firstPartition;
      }
      else if (strncmp(CmdMsg, "sd:", 3) == 0)
      {
         CmdMsg+=3;
         USB_SD = Drive_SD;
         if (!InitCheckSD()) return;      
         sourceFS = &SD; 
      }
      else
      {
         SendASCIIErrorStrImmediate("sd: or usb: missing");
         return;  
      }
      
      //check that path exists
      if (sourceFS->exists(CmdMsg))
      {
         if(CmdMsg[strlen(CmdMsg)-1] != '/') strcat(CmdMsg, "/");
         while(!ReadyToSendRx()) CheckRxNMITimeout(); //Let any outstanding NMIs clear before EEPROM writes (resource hog)
         EEPROM.write(eepAdDLPathSD_USB, USB_SD);
         EEPwriteStr(eepAdDLPath, CmdMsg); 
         
         SendPETSCIICharImmediate(PETSCIIyellow);
         SendASCIIStrImmediate("Download path updated\r");
         SendPETSCIICharImmediate(PETSCIIwhite);
      }
      else
      {
         SendASCIIErrorStrImmediate("Path not found");
      }
   }
   else SendASCIIErrorStrImmediate("Unknown arg");

}

FLASHMEM void BC_FollowHyperlink(char* CmdMsg) 
{  // *CmdMsg >= '0' && *CmdMsg <= '9'
   uint8_t CmdMsgVal = atoi(CmdMsg);
   
   if (CmdMsgVal > 0 && CmdMsgVal <= UsedPageLinkBuffs)
   {
      while (*CmdMsg >='0' && *CmdMsg <='9') CmdMsg++;  //move pointer past numbers
      if (!ValidModifier(*CmdMsg)) return; 
      
      //we have a valid modifier and link # to follow...
      char *LinkBuff = PageLinkBuff[CmdMsgVal-1];
      
      if(strncmp(LinkBuff, "lcl:", 4)==0)
      {  //special local (download dir) link to launch
         char FileNamePath[TxMsgMaxSize+MaxURLPathSize];
         bool SD_nUSB = (EEPROM.read(eepAdDLPathSD_USB) == Drive_SD);
         
         LinkBuff +=4; //past "lcl:"
         EEPreadStr(eepAdDLPath, FileNamePath); 
         CopyDecode(LinkBuff, LinkBuff); //overwrite
         strcat(FileNamePath, LinkBuff);
         RemoteLaunch(SD_nUSB, FileNamePath);
         return; //get back to main loop for reset execution
      }

      stcURLParse URL;
      
      ParseURL(LinkBuff, URL); //zero based

      if(URL.host[0] == 0) //relative path, use same server/port, append path
      {         
         if (URL.path[0] == 0)
         {
            SendASCIIErrorStrImmediate("Empty Link");
            return;  
         }
       
         if(URL.path[0] != '/') //if not root ref, add previous path to beginning
         {  
            char temp[MaxURLPathSize];
            strcpy(temp, URL.path); //store the path temprarily
            strcpy(URL.path, PrevURLQueue[PrevURLQueueNum]->path); 
            char * ptrLastSlash = strrchr(URL.path, '/'); // find last slash
            if (ptrLastSlash != NULL) *(ptrLastSlash+1) = 0; //terminate after last slash
            strcat(URL.path, temp); //add rel path back to end
         }
         URL.port = PrevURLQueue[PrevURLQueueNum]->port;
         strcpy(URL.host, PrevURLQueue[PrevURLQueueNum]->host);
         //leave postpath data in-tact
      }
      ModWebConnect(&URL, *CmdMsg);
      AddToPrevURLQueue(&URL);
   }
   else
   {
      SendASCIIErrorStrImmediate("Link# Unknown");
   }
}

FLASHMEM void ProcessBrowserCommand()
{
   char* CmdMsg = TxMsg; //local pointer for manipulation
  
   //make lower case until first space or end
   while(*CmdMsg != ' ' && *CmdMsg) 
   {
      *CmdMsg = tolower(*CmdMsg);
      CmdMsg++;
   }
   CmdMsg = TxMsg;
   
   if(strcmp(CmdMsg, "p") ==0) // Previous web page
   {
      if (PrevURLQueueNum == 0) PrevURLQueueNum = NumPrevURLQueues - 1; //wrap around bottom
	   else  PrevURLQueueNum--;
      
      Printf_dbg("PrevURL# %d\n", PrevURLQueueNum);
      WebConnect(PrevURLQueue[PrevURLQueueNum]); //not updating PrevURLQueue
   }
   
   else if(*CmdMsg == 'b') // Bookmark Commands
   {
      BC_Bookmarks(CmdMsg);
   }   
   
   else if(*CmdMsg == 'r') // Reload web page
   {
      CmdMsg++; //past the 'r'
      if (!ValidModifier(*CmdMsg)) return; 
      
      Printf_dbg("CurrURL# %d\n", PrevURLQueueNum);
      ModWebConnect(PrevURLQueue[PrevURLQueueNum], *CmdMsg); //no Add To PrevURLQueue
   }
   
   else if(*CmdMsg >= '0' && *CmdMsg <= '9') //Hyperlink #
   {
      BC_FollowHyperlink(CmdMsg);
   }
   
   else if(*CmdMsg == 'u') //URL
   {
      CmdMsg++; //past the 'u'
      char Mod = *CmdMsg;
      if (!ValidModifier(Mod)) return; 
      if (Mod) CmdMsg++; //past the Mod or first space
      while(*CmdMsg==' ') CmdMsg++;  //Allow for more spaces after command
      
      stcURLParse URL;
      char httpServer[MaxTagSize] = "http://";
      strcat(httpServer, CmdMsg);
      ParseURL(httpServer, URL);
      ModWebConnect(&URL, Mod);
      AddToPrevURLQueue(&URL);
   }
   
   else if(*CmdMsg == 's') //search
   {
      CmdMsg++; //past the 's'
      while(*CmdMsg==' ') CmdMsg++;  //Allow for spaces after command   
      DoSearch(CmdMsg);  //includes WebConnect
   }
   
   else if(*CmdMsg == '?') //List commands
   {
      SendBrowserCommandsImmediate();
   }
   
   else if(*CmdMsg == 'd') //download path update
   {
      BC_Downloads(CmdMsg);
   }
   
   else if(strcmp(CmdMsg, "x") ==0) //Exit browse mode
   {
      ClearClientStop();
      BrowserMode = false;
      DumpQueueUnPausePage();
      AddToPETSCIIStrToRxQueueLN("\rBrowser mode exit");
   }

   else if(*CmdMsg != 0) //something there, but not recognized command
   {
      SendASCIIErrorStrImmediate("Unknown Command");
   }
   
   else if(PagePaused) //empty command, and paused;  un-pause on return key alone
   { 
      //SendPETSCIICharImmediate(PETSCIIcursorUp); //Cursor up to overwrite prompt & scroll on
      //SendPETSCIICharImmediate(PETSCIIclearScreen); //clear screen for next page
      UnPausePage();
   }
   
}
