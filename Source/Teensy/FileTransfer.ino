
bool NDJSONformat;

FLASHMEM bool GetPathParameter(char FileNamePath[])
{
    uint16_t CharNum = 0;
    char currentChar;

    while (CharNum < MaxNamePathLength)
    {
        if (!SerialAvailabeTimeout())
        {
            SendU16(FailToken);
            CmdChannel->print("Timed out getting path param!\n");
            return false;
        }
        currentChar = CmdChannel->read();

        if (currentChar == 0) break;

        FileNamePath[CharNum] = currentChar;

        CharNum++;
    }
    FileNamePath[CharNum] = 0;

    if (CharNum == MaxNamePathLength)
    {
        SendU16(FailToken);
        CmdChannel->print("Path too long!\n");
        return false;
    }
    return true;
}

FLASHMEM FS* GetStorageDevice(uint32_t storageType)
{
    if (!storageType) return &firstPartition;

    if (!SD.begin(BUILTIN_SDCARD))
    {
        SendU16(FailToken);
        CmdChannel->printf("Specified storage device was not found: %u\n", storageType);
        return nullptr;
    }
    return &SD;
}

FLASHMEM bool GetFileStream(uint32_t SD_nUSB, char FileNamePath[], FS* sourceFS, File& file)
{
    if (sourceFS->exists(FileNamePath))
    {
        SendU16(FailToken);
        CmdChannel->printf("File already exists.\n");
        return false;
    }
    file = sourceFS->open(FileNamePath, FILE_WRITE);

    int retry = 0;

    while(retry < 3 && !file)
    {
      file = sourceFS->open(FileNamePath, FILE_WRITE);
      retry++;
    }

    if (!file)
    {
        SendU16(FailToken);
        CmdChannel->printf("Could not open for write: %s:%s\n", (SD_nUSB ? "SD" : "USB"), FileNamePath);
        return false;
    }
    return true;
}

FLASHMEM bool EnsureDirectory(const char* path, FS& fs)
{
    const char* lastSlash = strrchr(path, '/');

    if (lastSlash == path || lastSlash == nullptr)
    {
        return true;
    }

    char prevChar = *lastSlash;
    *(char*)lastSlash = '\0';

    bool result = true;

    if (!fs.exists(path))
    {
        if (!EnsureDirectory(path, fs))
        {
            result = false;
        }
        else
        {
            result = fs.mkdir(path);
        }
    }
    *(char*)lastSlash = prevChar;

    return result;
}

FLASHMEM bool ReceiveFileData(File& file, uint32_t len, uint32_t& checksum)
{
    uint32_t bytenum = 0;
    uint8_t byteIn;

    while (bytenum < len)
    {
        if (!SerialAvailabeTimeout())
        {
            SendU16(FailToken);
            CmdChannel->printf("Rec %lu of %lu bytes\n", bytenum, len);
            file.close();
            return false;
        }
        file.write(byteIn = CmdChannel->read());
        checksum -= byteIn;
        bytenum++;
    }  
    file.close();

    checksum &= 0xffff;
    if (checksum != 0)
    {
        SendU16(FailToken);
        CmdChannel->printf("CS Failed! RCS:%lu\n", checksum);        
        return false;
    }
    return true;
}

// Command: 
// Post File to target directory and storage type on TeensyROM.
// Automatically creates target directory if missing.
//
// Workflow:
// Receive <-- Post File Token 0x64BB 
// Send --> AckToken 0x64CC
// Receive <-- Length(4), Checksum(2), SD_nUSB(1) Destination Path(MaxNameLength, null terminator)
// Send --> 0x64CC on Pass,  0x9b7f on Fail 
// Receive <-- File(length)
// Send --> AckToken 0x64CC on Pass,  0x9b7f on Fail
//
// Notes: Once Post File Token Received, responses are 2 bytes in length
FLASHMEM void PostFileCommand()
{
    SendU16(AckToken);

    uint32_t fileLength, checksum, storageType;
    char FileNamePath[MaxNamePathLength];

    if (!GetUInt(&fileLength, 4))
    {
        SendU16(FailToken);
        CmdChannel->println("Error receiving file length value!");
        return;
    }

    if (!GetUInt(&checksum, 2))
    {
        SendU16(FailToken);
        CmdChannel->println("Error receiving checksum value!");
        return;
    }

    if (!GetUInt(&storageType, 1))
    {
        SendU16(FailToken);
        CmdChannel->println("Error receiving storage type value!");
        return;
    }

    if (!GetPathParameter(FileNamePath)) return;

    FS* sourceFS = GetStorageDevice(storageType);

    if (!sourceFS) return;

    if (!EnsureDirectory(FileNamePath, *sourceFS))
    {
        SendU16(FailToken);
        CmdChannel->printf("Failed to ensure directory for: %s\n", FileNamePath);
        return;
    }

    File fileStream;

    if (!GetFileStream(storageType, FileNamePath, sourceFS, fileStream)) return;

    SendU16(AckToken);

    if (!ReceiveFileData(fileStream, fileLength, checksum)) return;

    SendU16(AckToken);
}

FLASHMEM void SendDirInfo(const char *itemName, const char *directoryPath)
{
    if (NDJSONformat)
    {
       CmdChannel->print(F("{\"type\":\"dir\",\"name\":\""));
       CmdChannel->print(itemName);
       CmdChannel->print(F("\"}\r\n"));
    }
    else
    {
       CmdChannel->print(F("[Dir]{\"Name\":\""));
       CmdChannel->print(itemName);
       CmdChannel->print(F("\",\"Path\":\""));
       CmdChannel->print(directoryPath);
       CmdChannel->print('/');
       CmdChannel->print(itemName);
       CmdChannel->print(F("\"}[/Dir]"));
    }
}

FLASHMEM void SendFileInfo(const char *itemName, const char *directoryPath, uint32_t size)
{
    if (NDJSONformat)
    {
       CmdChannel->print(F("{\"type\":\"file\",\"name\":\""));
       CmdChannel->print(itemName);
       CmdChannel->print(F("\",\"size\":"));
       CmdChannel->print(size);
       CmdChannel->print(F("}\r\n"));
    }
    else
    {
       CmdChannel->print(F("[File]{\"Name\":\""));
       CmdChannel->print(itemName);
       CmdChannel->print(F("\",\"Size\":"));
       CmdChannel->print(size);
       CmdChannel->print(F(",\"Path\":\""));
       CmdChannel->print(directoryPath);
       CmdChannel->print('/');
       CmdChannel->print(itemName);
       CmdChannel->print(F("\"}[/File]"));
    }
}

FLASHMEM void SendTRDirectory(const char *dirName, StructMenuItem *TROMMenu, uint16_t numItems)
{
   //Assumes path will be max 1 dir deep
   for(uint16_t itemNum=0; itemNum < numItems; itemNum++)
   {
      if(TROMMenu[itemNum].ItemType == rtDirectory)
      {
         if(strcmp(TROMMenu[itemNum].Name, UpDirString)!=0) // Skip Up Dir entries
         {
            SendDirInfo(TROMMenu[itemNum].Name, dirName);
            SendTRDirectory(TROMMenu[itemNum].Name, (StructMenuItem*)TROMMenu[itemNum].Code_Image, TROMMenu[itemNum].Size/sizeof(StructMenuItem));
         }
      }
      else
      {
         SendFileInfo(TROMMenu[itemNum].Name, dirName, TROMMenu[itemNum].Size);  //TROMMenu[itemNum].ItemType
      }
   }
}

FLASHMEM bool SendPagedDirectoryContents(FS& fileStream, const char* directoryPath, int skip, int take)
{
    File directory = fileStream.open(directoryPath);

    File directoryItem = directory.openNextFile();

    int currentCount = 0;
    int pageCount = 0;

    while (directoryItem && pageCount < take)
    {
        currentCount++;

        const char* itemName = directoryItem.name();

        if (currentCount >= skip)
        {
            pageCount++;

            if (directoryItem.isDirectory()) 
            {
               SendDirInfo(itemName, directoryPath);
            }
            else 
            {
               SendFileInfo(itemName, directoryPath, directoryItem.size());
            }
        }
        directoryItem.close();
        directoryItem = directory.openNextFile();
    }

    if (directoryItem) {
        directoryItem.close();
    }
    directory.close();

    return true;
}


// Command: 
// List Directory Contents on TeensyROM given a take and skip value
// to faciliate batch processing.
//
// Workflow: (TR POV)
// Receive <-- List Directory Token 0x64DD/0x64DE
// Send --> AckToken 0x64CC
// Receive <-- Storage Type(1), skip(2), take(2), Destination Path(MaxNameLength, null terminator)
//        Storage Types: (RegMenuTypes)
//           USBDrive  = 0
//           SD        = 1
//           Teensy    = 2
// Send --> AckToken 0x64CC on successful check of directory existence, 0x9b7f on Fail
// Send --> StartDirectoryListToken 0x5A5A or FailToken 0x9b7f
// Send --> Write content as json
// Send --> EndDirectoryListToken 0xA5A5,  0x9b7f on Fail
FLASHMEM void GetDirectoryCommand(bool SetNDJSONformat)
{
    const uint16_t StartDirectoryListToken = 0x5A5A;
    const uint16_t EndDirectoryListToken = 0xA5A5;
    NDJSONformat = SetNDJSONformat;
    
    SendU16(AckToken);

    uint32_t storageType, skip, take;
    char path[MaxNamePathLength];

    if (!GetUInt(&storageType, 1))
    {
        SendU16(FailToken);
        CmdChannel->println("Error receiving storage type value! (Error 1)");
        return;
    }
    if (storageType >= rmtNumTypes)
    {
        SendU16(FailToken);
        CmdChannel->println("Invalid storage type value! (Error 1.5)");
        return;
    }
    if (!GetUInt(&skip, 2))
    {
        SendU16(FailToken);
        CmdChannel->println("Error receiving skip value! (Error 2)");
        return;
    }
    if (!GetUInt(&take, 2))
    {
        SendU16(FailToken);
        CmdChannel->println("Error receiving take value! (Error 3)");
        return;
    }
    if (!GetPathParameter(path))
    {
        SendU16(FailToken);
        CmdChannel->println("Error receiving path value! (Error 4)");
        return;
    }

    if (storageType == rmtTeensy)
    {
       SendU16(AckToken);//Ack the Destination Path (path), regardless; also ignores skip & take
       SendU16(StartDirectoryListToken);
       
       SendTRDirectory("", TeensyROMMenu, sizeof(TeensyROMMenu)/sizeof(TeensyROMMenu[0]));
       
       SendU16(EndDirectoryListToken);
       return;
    }

    FS* sourceFS = GetStorageDevice(storageType);

    if (!sourceFS) return;

    File dir = sourceFS->open(path);

    if (!dir)
    {
        SendU16(FailToken);
        CmdChannel->println("Directory not found. (Error 5)");
        return;
    }
    if (!dir.isDirectory())
    {
        SendU16(FailToken);
        CmdChannel->println("Path is not a directory. (Error 6)");
        dir.close();
        return;
    }
    dir.close();

    SendU16(AckToken);

    SendU16(StartDirectoryListToken);

    if (!SendPagedDirectoryContents(*sourceFS, path, skip, take)) return;

    SendU16(EndDirectoryListToken);
}

FLASHMEM bool CopyFile(const char* sourcePath, const char* destinationPath, FS& fs)
{
    File sourceFile = fs.open(sourcePath, FILE_READ);

    if (!sourceFile)
    {
        SendU16(FailToken);
        CmdChannel->printf("Failed to open source file: %s\n", sourcePath);
        return false;
    }
    fs.remove(destinationPath);
    File destinationFile = fs.open(destinationPath, FILE_WRITE);

    if (!destinationFile)
    {
        SendU16(FailToken);
        CmdChannel->printf("Failed to open destination file: %s\n", destinationPath);
        sourceFile.close();
        return false;
    }
    while (sourceFile.available())
    {
        uint8_t buf[64];
        size_t len = sourceFile.read(buf, sizeof(buf));
        destinationFile.write(buf, len);
    }
    sourceFile.close();
    destinationFile.close();

    return true;
}


// Command: 
// Copies a file from one folder to the other in the USB/SD storage.
// If the file with the same name already exists at the destination, 
// it will be overwritten.
//
// Workflow:
// Receive <-- Post File Token 0x64FF 
// Send --> AckToken 0x64CC
// Receive <-- Source Path(MaxNameLength, null terminator), Destinationn Path(MaxNameLength, null terminator)
// Send --> 0x64CC on Pass,  0x9b7f on Fail 
//
// Notes: Once Copy File Token Received, responses are 2 bytes in length
FLASHMEM void CopyFileCommand()
{
    SendU16(AckToken);

    uint32_t storageType;
    char sourcePath[MaxNamePathLength];
    char destinationPath[MaxNamePathLength];

    if (!GetUInt(&storageType, 1))
    {
        SendU16(FailToken);
        CmdChannel->println("Error receiving storage type value!");
        return;
    }

    if (!GetPathParameter(sourcePath)) return;

    if (!GetPathParameter(destinationPath)) return;

    FS* sourceFS = GetStorageDevice(storageType);

    if (!sourceFS) return;

    if (!EnsureDirectory(destinationPath, *sourceFS))
    {
        SendU16(FailToken);
        CmdChannel->printf("Failed to ensure directory for: %s\n", destinationPath);
        return;
    }

    if (!CopyFile(sourcePath, destinationPath, *sourceFS)) return;

    SendU16(AckToken);
}

FLASHMEM void DeleteFile(const char* filePath, FS& fileSystem)
{
    if (!fileSystem.exists(filePath))
    {
        SendU16(FailToken);
        CmdChannel->printf("File not found: %s\n", filePath);
        return;
    }

    if (fileSystem.remove(filePath))
    {
        SendU16(AckToken);
        return;
    }
    else
    {
        SendU16(FailToken);
        CmdChannel->printf("Failed to delete file: %s\n", filePath);
        return;
    }
}

// Command: 
// Delete a file from the specified storage device on TeensyROM.
//
// Workflow:
// Receive <-- DeleteFileToken (0x64CF) 
// Send --> AckToken 0x64CC
// Receive <-- SD_nUSB(1), File Path(MaxNameLength, null terminator)
// Send --> 0x64CC on Pass, 0x9b7f on Fail 
//
// Notes: Once Delete File Token Received, responses are 2 bytes in length
FLASHMEM void DeleteFileCommand()
{
    SendU16(AckToken);

    uint32_t storageType;
    char filePath[MaxNamePathLength];

    if (!GetUInt(&storageType, 1))
    {
        SendU16(FailToken);
        CmdChannel->println("Error receiving storage type!");
        return;
    }

    if (!GetPathParameter(filePath))
    {
        SendU16(FailToken);
        CmdChannel->println("Error receiving path!");
        return;
    }

    FS* sourceFS = GetStorageDevice(storageType);
    if (!sourceFS)
    {
        SendU16(FailToken);
        CmdChannel->println("Error getting storage device!");
    }

    DeleteFile(filePath, *sourceFS);
}

FLASHMEM bool SendFileData(File& file, uint32_t len) {
    uint32_t bytenum = 0;
    while (bytenum < len) {
        if (CmdChannel->availableForWrite()) {
            CmdChannel->write(file.read());
            bytenum++;
        } else {
            SendU16(FailToken);
            CmdChannel->printf("Send %lu of %lu bytes\n", bytenum, len);
            return false;
        }
    }
    return true;
}

FLASHMEM uint32_t CalculateChecksum(File& file) {
    uint32_t checksum = 0;
    while (file.available()) {
        checksum += file.read();
    }
    return checksum & 0xffff;
}

// Command: 
// Retrieve File from specified directory and storage type on TeensyROM.
// 
// Workflow:
// Receive <-- Get File Token (e.g., 0x64B0)
// Send --> AckToken 0x64CC
// Receive <-- SD_nUSB(1) Source Path(MaxNameLength, null terminator)
// Send --> 0x64CC on Pass, 0x9b7f on Fail
// Arduino reads the file from storage
// Send --> File Length(4), Checksum(2)
// Send --> File Data in chunks
// Send --> AckToken 0x64CC on successful check of storage availability, 0x9b7f on Fail
// Send --> AckToken 0x64CC on successful completion, 0x9b7f on Fail
//
// Notes: Once Get File Token is received, initial responses are 2 bytes in length. File data is sent in subsequent operations. Checksum is calculated for file validation.
FLASHMEM void GetFileCommand() {
    SendU16(AckToken);

    uint32_t storageType;
    char filePath[MaxNamePathLength];

    if (!GetUInt(&storageType, 1))
    {
        SendU16(FailToken);
        CmdChannel->println("Error receiving storage type! (Error 1)");
        return;
    }

    if (!GetPathParameter(filePath))
    {
        SendU16(FailToken);
        CmdChannel->println("Error receiving path! (Error 2)");
        return;
    }

    FS* sourceFS = GetStorageDevice(storageType);

    if (sourceFS == nullptr || !sourceFS->exists("/")) 
    {
      SendU16(FailToken);
      CmdChannel->println("Storage device unavailable. (Error 3)");
      return;
    }

    if (!sourceFS->exists(filePath)) 
    {
      SendU16(FailToken);
      CmdChannel->println("File not found. (Error 4)");
      return;
    }

    File fileStream = sourceFS->open(filePath, FILE_READ);
    
    if (!fileStream) 
    {
        SendU16(FailToken);
        CmdChannel->println("Failed to open file. (Error 5)");
        return;
    }
     SendU16(AckToken);

    uint32_t fileLength = fileStream.size();
    SendU32(fileLength);

    uint32_t checksum = CalculateChecksum(fileStream);
    SendU32(checksum);
    fileStream.seek(0);

    if (!SendFileData(fileStream, fileLength)) 
    {
        fileStream.close();
        return;
    }
    fileStream.close();
    
    SendU16(AckToken);
}
