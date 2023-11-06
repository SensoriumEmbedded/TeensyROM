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
      SendASCIIStrImmediate("Paused (Ret, ");
   }
   else SendASCIIStrImmediate("\rFinished (");
   SendASCIIStrImmediate("#,S[],U[],X,B)");
   SendPETSCIICharImmediate(PETSCIIrvsOff);
   SendPETSCIICharImmediate(PETSCIIlightGreen);   
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
   SendASCIIStrImmediate("  Rm: Reload page   \r");
   
   //SendASCIIStrImmediate("  B#x: Bookmark #/s/?\r");
   //SendASCIIStrImmediate("       D[s/u/?] [path]: Set Download dir");
   SendPETSCIICharImmediate(PETSCIIlightGreen);
}

uint8_t HexCharToInt(uint8_t HexChar)
{
   // https://stackoverflow.com/questions/10156409/convert-hex-string-char-to-int
   return (((HexChar & 0xF) + (HexChar >> 6)) | ((HexChar >> 3) & 0x8));
}

bool CheckAndDecode(const char *ptrChars, uint8_t *ptrRetChar)
{  //check if next 3 chars are decodable, decode if they are, otherwise return false
   if(*ptrChars == '%' && ptrChars[1] && ptrChars[2]) //something there, not end of chars
   {
      *ptrRetChar = (HexCharToInt(ptrChars[1])<<4) | HexCharToInt(ptrChars[2]);
      return true;
   }
   return false;
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
   
   else if(strcmp(TagBuf, "eoftag")==0) // special tag to signal page complete
   {
      //SendPETSCIICharImmediate(PETSCIIgrey); 
      //SendASCIIStrImmediate("\r\r-end-");
      SendCommandSummaryImmediate(false);
      //SendBrowserCommandsImmediate();  
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
      PageCharsReceived += 40-(PageCharsReceived % 40)+3;
   }
   
   else if(strncmp(TagBuf, "petscii", 7)==0) // custom tag for PETSCII chars
   {
      uint8_t NextChar;
      char * ptrCharNum = TagBuf+7; //start after "petscii" offset
      while(*ptrCharNum==' ') ptrCharNum++;  //Allow for spaces after petscii

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
      PageCharsReceived++;
      PrintingHyperlink = true;
   }
   
   else if(strcmp(TagBuf, "/a")==0) //end of hyperlink text
   {
      SendPETSCIICharImmediate(PETSCIIwhite); 
      PrintingHyperlink = false;
   }
   
   else if(strcmp(TagBuf, "html")==0) //Start of HTML
   {
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
   //URLParse.port = 80;
   URLParse.path[0] = 0;
   URLParse.postpath[0] = 0;
   
   //Find/skip access ID
   if(strstr(URL, "http://") != URL && strstr(URL, "https://") != URL) //no access ID, relative path only
   {
      strcat(URLParse.path, URL);
   }
   else
   {
      const char * ptrServerName = strstr(URL, "://")+3; //move past the access ID
      //char * ptrPort = strstr(ptrServerName, ":");  //find port identifier
      char * ptrPath = strstr(ptrServerName, "/");  //find path identifier
      
      //need to check for userid? http://userid@example.com:8080/
      
      // skipping port parsing, always 80 anyway
      // "port" commented out throughout...
      
      //finalize server name and update port, if present
      //if (ptrPort != NULL) //there's a potential port ID 
      //{
      //   if (ptrPort < ptrPath) //is the : before the path start? 
      //   {  //capture and set host name
      //      URLParse.port = atoi(ptrPort+1);  //skip the ":"
      //      strncpy(URLParse.host, ptrServerName, ptrPort-ptrServerName);
      //      URLParse.host[ptrPort-ptrServerName]=0; //terminate it
      //   }
      //}
      //else //no port
      if (ptrPath != NULL)  //there's a path; set host name
      {
         strncpy(URLParse.host, ptrServerName, ptrPath-ptrServerName);
         URLParse.host[ptrPath-ptrServerName]=0; //terminate it
      }
      else strcpy(URLParse.host, ptrServerName);  //no path
   
      //copy path, if present
      if (ptrPath != NULL) strcpy(URLParse.path, ptrPath);
      else strcpy(URLParse.path, "/");
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
   //Printf_dbg(" port = %d\n", URLParse.port);
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

bool WebConnect(const stcURLParse *DestURL, bool AddToHist)
{
   if (DestURL->host[0] == 0)
   {
      SendASCIIErrorStrImmediate("No Host");
      return false;
   }
   
   if (AddToHist) //add URL to the prev queue
   {
      if (++PrevURLQueueNum == NumPrevURLQueues) PrevURLQueueNum = 0; //inc/wrap around top
      memcpy(PrevURLQueue[PrevURLQueueNum], DestURL, sizeof(stcURLParse)); //overwrite previous entry
   }
   
   while (client.available()) client.read(); //clear client buffer
   client.stop();

   RxQueueHead = RxQueueTail = 0; //dump the queue
   strcpy(CurrPageTitle, "Unknown"); //gets populated via title tag 
   
   Printf_dbg("Connect: \"%s%s%s\"\n", DestURL->host, DestURL->path, DestURL->postpath);
   
   SendPETSCIICharImmediate(PETSCIIyellow);
   SendASCIIStrImmediate("\rConnecting to: ");
   SendASCIIStrImmediate(DestURL->host);
   SendASCIIStrImmediate(DestURL->path);
   SendASCIIStrImmediate(DestURL->postpath);
   SendPETSCIICharImmediate(PETSCIIreturn);
   SendPETSCIICharImmediate(PETSCIIbrown);  // header info color
   
   if (client.connect(DestURL->host, 80))  //DestURL->port))
   {
      const uint16_t MaxBuf = 350;
      char inbuf[MaxBuf];
      bool ShowHeader = true; //initially true
      
      client.printf("GET %s%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", 
         DestURL->path, DestURL->postpath, DestURL->host);

      //get response header
      //https://www.tutorialspoint.com/http/http_responses.htm
      while(ReadClientLine(inbuf, MaxBuf)==true)
      {
         
         //Printf_dbg("H: %s", inbuf);  //causes lost data
         if (ShowHeader) 
         {
            if (strstr(inbuf, "HTTP/1.1 200") != NULL) ShowHeader = false;
            else SendASCIIStrImmediate(inbuf);
         }
         
         if (strcmp(inbuf, "\r\n") == 0) 
         {
            SendPETSCIICharImmediate(PETSCIIwhite);
            if(!client.connected()) SendASCIIStrImmediate("Not "); //may have been header only
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
      //80,                 //URL.port = 80;
      "/",                //strcpy(URL.path = "/");
      "?q=",              //strcpy(URL.postpath = "?q=");
   };
      
   uint16_t UWPCharNum = strlen(URL.postpath);
   
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
         URL.postpath[UWPCharNum++] = NextChar;      
      }
      else
      {
         //encode character (%xx hex val)
         URL.postpath[UWPCharNum++] = '%';
         URL.postpath[UWPCharNum++] = HexChar[NextChar >> 4];
         URL.postpath[UWPCharNum++] = HexChar[NextChar & 0x0f];
      }
   }
   
   WebConnect(&URL, true);
}

void DownloadFile(stcURLParse *DestURL)
{  // Modifies (decodes) FileName

   char FileName[MaxURLPathSize]; //local copy for decoded version

   const char* ptrOrigFilename = strrchr(DestURL->path, '/'); //pointer to nav orig path/file name, find last slash
   if (ptrOrigFilename == NULL) ptrOrigFilename = DestURL->path; //use the whole thing if no slash
   else ptrOrigFilename++; //skip the slash

   //copy/decode special chars
   uint16_t NewCharNum = 0;
   uint16_t OrigCharNum = 0;
   while(ptrOrigFilename[OrigCharNum])
   {
      uint8_t NextChar;
      if(CheckAndDecode(ptrOrigFilename+OrigCharNum, &NextChar)) OrigCharNum+=3;
      else NextChar = ptrOrigFilename[OrigCharNum++];
      
      FileName[NewCharNum++] = NextChar;
      //OrigCharNum++;
   }
   FileName[NewCharNum] = 0;

   SendASCIIStrImmediate("Filename: \"");
   SendASCIIStrImmediate(FileName);
   SendASCIIStrImmediate("\"\r");

   char FileNamePath[TxMsgMaxSize+MaxURLPathSize];
   FS *sourceFS;
   uint8_t USB_SD = EEPROM.read(eepAdDLPathSD_USB);
   EEPreadStr(eepAdDLPath, FileNamePath); 
   
   SendASCIIStrImmediate("Path: ");

   if(USB_SD == Drive_SD)
   {
      if (!SD.begin(BUILTIN_SDCARD))  // refresh, takes 3 seconds for fail/unpopulated, 20-200mS populated
      {
         SendASCIIErrorStrImmediate("No SD card");  
         return;      
      }
      SendASCIIStrImmediate("SD:");
      sourceFS = &SD;
   }
   else
   {
      SendASCIIStrImmediate("USB:");
      sourceFS = &firstPartition;      
   }
   
   SendASCIIStrImmediate(FileNamePath);
   SendASCIIStrImmediate("\r");

   strcat(FileNamePath, FileName); //add filename to dest path

   if (sourceFS->exists(FileNamePath))
   {
      SendASCIIErrorStrImmediate("File already exists");  
      return;      
   }
   
   if (!WebConnect(DestURL, false)) return;

   if (!client.connected())    
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

bool ValidModifier(const char cMod)
{
   char ValidMods[] = "dfr ";
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

FLASHMEM bool DLExtension(const char * Extension)
{
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

void ModWebConnect(stcURLParse *DestURL, char cMod, bool AddToHist)
{  //Do WebConnect, apply Modifier argument
   //assumes char already qualified via ValidModifier()
   
   if (cMod == ' ' || cMod == 0) //modifier not specified, check for auto-download
   {
      char * Extension = strrchr(DestURL->path, '.');
      Printf_dbg("*--Ext: ");
      if (Extension != NULL)
      {
         Extension++; //spip the '.'
         Printf_dbg("%s\n", Extension);
         
         if(DLExtension(Extension)) cMod = 'd';
      }
      else {Printf_dbg("none\n");}
   }
   
   switch (cMod)
   {
      case 'd': //download
         DownloadFile(DestURL);
         while (client.available()) client.read(); //clear client buffer
         client.stop();  //in case of unfinished/error, don't read it in as text
         break;
         
      case 'f':  //filter via FrogFind
         if(isURLFiltered(DestURL)) WebConnect(DestURL, AddToHist); //go now if already filtered
         else 
         {  //make filtered URL & connect
            stcURLParse URL =
            {
               "www.frogfind.com",     //strcpy(URL.host = "www.frogfind.com");
               //80,                     //URL.port = 80;
               "/read.php",            //strcpy(URL.path = "/read.php?a=http://");
               "?a=http://"            //URL.postpath
            };
                  
            strcat(URL.postpath, DestURL->host);
            strcat(URL.postpath, DestURL->path);
            strcat(URL.postpath, DestURL->postpath);
            //write back filtered URL in case it's pointing to a history queue item (ie 'rf' command) 
            //  so link patchs will be relative to correct server
            memcpy(DestURL, &URL, sizeof(stcURLParse)); 
            
            WebConnect(&URL, AddToHist);
         }
         break;
         
      case 'r': //Raw, no filterring
         if(!isURLFiltered(DestURL)) WebConnect(DestURL, AddToHist); //go now if already raw
         else
         {  //strip off frogfind
            char * ptrURL = strstr(DestURL->postpath, "http");  // "/read.php?a=http://"
            if (ptrURL == NULL)
            {
               SendASCIIErrorStrImmediate("Could not remove filter");
               WebConnect(DestURL, AddToHist);
            }
            else
            {
               stcURLParse URL;
               ParseURL(ptrURL, URL);
               
               //write back unfiltered URL in case it's pointing to a history queue item (ie 'rr' command) 
               //  so link patchs will be relative to correct server
               memcpy(DestURL, &URL, sizeof(stcURLParse)); 
                  
               WebConnect(&URL, AddToHist);  
            }
         }
         break;
      default:   
         WebConnect(DestURL, AddToHist); //no mod, default to reader, prev defined filterring
         break;
   }
}

void ProcessBrowserCommand()
{
   char* CmdMsg = TxMsg; //local pointer for manipulation
   
   
   if(strcmp(CmdMsg, "p") ==0) // Previous web page
   {
      if (PrevURLQueueNum == 0) PrevURLQueueNum = NumPrevURLQueues - 1; //wrap around bottom
	   else  PrevURLQueueNum--;
      
      Printf_dbg("PrevURL# %d\n", PrevURLQueueNum);
      WebConnect(PrevURLQueue[PrevURLQueueNum], false);
   }
   
   else if(*CmdMsg == 'b') // Bookmark Commands
   {
      RxQueueHead = RxQueueTail = 0; //dump the queue         
      CmdMsg++; //past the 'b'
      
      if (*CmdMsg == 0) //no modifier
      {  //print bookmark list via buffer
         char buf[eepBMURLSize];
         
         AddRawStrToRxQueue("<br><b>Saved Bookmarks:</b>"); 
         for (uint8_t BMNum=0; BMNum < eepNumBookmarks; BMNum++)
         {
            AddRawStrToRxQueue("<li><a href=\"");
            EEPreadStr(eepAdBookmarks+BMNum*(eepBMTitleSize+eepBMURLSize)+eepBMTitleSize,buf); //URL
            Printf_dbg("BM#%d- %s", BMNum, buf);
            AddRawStrToRxQueue(buf);
            AddRawStrToRxQueue("\">");
            EEPreadStr(eepAdBookmarks+BMNum*(eepBMTitleSize+eepBMURLSize),buf); //Title
            Printf_dbg("- %s\n", buf);
            AddRawStrToRxQueue(buf);         
            AddRawStrToRxQueue("</a>");
         }
         AddRawStrToRxQueue("<eoftag>");
      }
      
      else if(*CmdMsg >= '1' && *CmdMsg <= '9')
      {  //jump to bookmark
         stcURLParse URL;
         char buf[eepBMURLSize];
         uint8_t BMNum = *CmdMsg - '1'; //zero based
         
         EEPreadStr(eepAdBookmarks+BMNum*(eepBMTitleSize+eepBMURLSize)+eepBMTitleSize, buf); //URL
         ParseURL(buf, URL);
         WebConnect(&URL, true);
      }
      
      else if(*CmdMsg == 's' && *(CmdMsg+1) >= '1' && *(CmdMsg+1) <= '9')
      {  //set bookmark # to current
   
         //re-encode to maximize eeprom usage, but could be too long...
         char strURL[MaxURLHostSize+MaxURLPathSize+MaxURLPathSize+12]; //   +"HTTP:// & :Prt"
         
         sprintf(strURL, "http://%s%s%s",
            PrevURLQueue[PrevURLQueueNum]->host,
            //PrevURLQueue[PrevURLQueueNum]->port,
            PrevURLQueue[PrevURLQueueNum]->path,
            PrevURLQueue[PrevURLQueueNum]->postpath);
         
         if (strlen(strURL) >= eepBMURLSize)
         {
            SendASCIIErrorStrImmediate("URL too long");
            return;  //early return, may remain paused            
         }
         CmdMsg++;
         uint8_t BMNum = *CmdMsg - '1'; //zero based
         while(!ReadyToSendRx()) CheckRxNMITimeout(); //Let any outstanding NMIs clear before EEPROM writes (resource hog)
         EEPwriteStr(eepAdBookmarks+BMNum*(eepBMTitleSize+eepBMURLSize), CurrPageTitle); //Title
         EEPwriteStr(eepAdBookmarks+BMNum*(eepBMTitleSize+eepBMURLSize)+eepBMTitleSize, strURL); //URL
         
         //Send confirmation
         AddRawStrToRxQueue("<br><b>Bookmark #"); 
         AddRawCharToRxQueue(*CmdMsg);
         AddRawStrToRxQueue(" updated to:</b><br>\"");
         AddRawStrToRxQueue(CurrPageTitle);
         AddRawStrToRxQueue("\" at<br>");
         AddRawStrToRxQueue(strURL);
         AddRawStrToRxQueue("<eoftag>");
      }
      else
      {
         SendASCIIErrorStrImmediate("Unk Bookmark Mod");
         return;  //early return, may remain paused
      }
   }   
   
   else if(*CmdMsg == 'r') // Reload web page
   {
      CmdMsg++; //past the 'r'
      if (!ValidModifier(*CmdMsg)) return; //early return, may remain paused
      
      Printf_dbg("CurrURL# %d\n", PrevURLQueueNum);
      ModWebConnect(PrevURLQueue[PrevURLQueueNum], *CmdMsg, false);
   }
   
   else if(*CmdMsg >= '0' && *CmdMsg <= '9') //Hyperlink #
   {
      uint8_t CmdMsgVal = atoi(CmdMsg);
      
      if (CmdMsgVal > 0 && CmdMsgVal <= UsedPageLinkBuffs)
      {
         while (*CmdMsg >='0' && *CmdMsg <='9') CmdMsg++;  //move pointer past numbers
         if (!ValidModifier(*CmdMsg)) return; //early return, may remain paused
         
         //we have a valid link # to follow...
         stcURLParse URL;
         
         ParseURL(PageLinkBuff[CmdMsgVal-1], URL); //zero based

         if(URL.host[0] == 0) //relative path, use same server/port, append path
         {         
            if (URL.path[0] == 0)
            {
               SendASCIIErrorStrImmediate("Empty Link");
               return;  //early return, may remain paused
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
            //URL.port = PrevURLQueue[PrevURLQueueNum]->port;
            strcpy(URL.host, PrevURLQueue[PrevURLQueueNum]->host);
            //leave postpath data in-tact
         }
         ModWebConnect(&URL, *CmdMsg, true);
      }
      else
      {
         SendASCIIErrorStrImmediate("Link# Unknown");
         return;  //early return, may remain paused
      }
   }
   
   else if(*CmdMsg == 'u') //URL
   {
      CmdMsg++; //past the 'u'
      char Mod = *CmdMsg;
      if (!ValidModifier(Mod)) return; //early return, may remain paused
      if (Mod) CmdMsg++; //past the Mod or first space
      while(*CmdMsg==' ') CmdMsg++;  //Allow for more spaces after command
      
      stcURLParse URL;
      char httpServer[MaxTagSize] = "http://";
      strcat(httpServer, CmdMsg);
      ParseURL(httpServer, URL);
      ModWebConnect(&URL, Mod, true);
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
      uint8_t USB_SD;
      FS *sourceFS;
      
      CmdMsg++; //past the 'd'
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
         if (!SD.begin(BUILTIN_SDCARD))
         {
            SendASCIIErrorStrImmediate("SD not present");
            return;  //early return, may remain paused
         }
         sourceFS = &SD; 
      }
      else
      {
         SendASCIIErrorStrImmediate("SD: or USB: missing");
         return;  //early return, may remain paused
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
         return;  //early return, may remain paused
      }
      else
      {
         SendASCIIErrorStrImmediate("Path not found");
         return;  //early return, may remain paused
      }
   }
   
   else if(strcmp(CmdMsg, "x") ==0) //Exit browse mode
   {
      while (client.available()) client.read(); //clear client buffer
      client.stop();
      BrowserMode = false;
      RxQueueHead = RxQueueTail = 0; //dump the queue
      AddToPETSCIIStrToRxQueueLN("\rBrowser mode exit");
   }

   else if(*CmdMsg != 0) //something there, but not recognized command
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
