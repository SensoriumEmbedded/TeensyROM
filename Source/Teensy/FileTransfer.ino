
#define AckToken                0x64CC
#define FailToken               0x9B7F

FLASHMEM bool GetPathParameter(char FileNamePath[]) 
{
    uint16_t CharNum = 0;
    char currentChar;

    while (CharNum < MaxNamePathLength)
    {
        if (!SerialAvailabeTimeout())
        {
            Serial.print("Timed out getting path param!\n");
            return false;
        }
        currentChar = Serial.read();

        if (currentChar == 0) break;

        FileNamePath[CharNum] = currentChar;

        CharNum++;
    }
    FileNamePath[CharNum] = 0;

    if (CharNum == MaxNamePathLength)
    {
        Serial.print("Path too long!\n");
        return false;
    }
    return true;
}

FLASHMEM FS* GetStorageDevice(uint32_t storageType)
{
    if (!storageType) return &firstPartition;

    if (!SD.begin(BUILTIN_SDCARD)) 
    {            
        Serial.printf("Specified storage device was not found: %u\n", storageType);
        return nullptr;
    }
    return &SD;
}

FLASHMEM bool GetFileStream(uint32_t SD_nUSB, char FileNamePath[], FS* sourceFS, File& file)
{
    if (sourceFS->exists(FileNamePath))
    {
        SendU16(FailToken);
        Serial.printf("File already exists.\n");
        return false;
    }
    file = sourceFS->open(FileNamePath, FILE_WRITE);

    if (!file)
    {
        SendU16(FailToken);
        Serial.printf("Could not open for write: %s:%s\n", (SD_nUSB ? "SD" : "USB"), FileNamePath);
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

FLASHMEM bool ReceiveFileData(File& file, uint32_t len, uint32_t& CheckSum)
{
    uint32_t bytenum = 0;
    uint8_t ByteIn;

    while (bytenum < len)
    {
        if (!SerialAvailabeTimeout())
        {
            SendU16(FailToken);
            Serial.printf("Rec %lu of %lu bytes\n", bytenum, len);
            file.close();
            return false;
        }
        file.write(ByteIn = Serial.read());
        CheckSum -= ByteIn;
        bytenum++;
    }
    file.close();

    CheckSum &= 0xffff;
    if (CheckSum != 0)
    {
        SendU16(FailToken);
        Serial.printf("CS Failed! RCS:%lu\n", CheckSum);
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
        Serial.println("Error receiving file length value!");
        return;
    }

    if (!GetUInt(&checksum, 2))
    {
        SendU16(FailToken);
        Serial.println("Error receiving checksum value!");
        return;
    }

    if (!GetUInt(&storageType, 1))
    {
        SendU16(FailToken);
        Serial.println("Error receiving storage type value!");
        return;
    }

    if (!GetPathParameter(FileNamePath)) 
    {
        SendU16(FailToken);
        return;
    }
    
    FS* sourceFS = GetStorageDevice(storageType);

    if (!sourceFS)
    {        
        SendU16(FailToken);
        Serial.println("Unable to get storage device!");
        return;
    }

    if (!EnsureDirectory(FileNamePath, *sourceFS))
    {
      SendU16(FailToken);
      Serial.printf("Failed to ensure directory for: %s\n", FileNamePath);
      return;
    }
   
    File fileStream;

    if (!GetFileStream(storageType, FileNamePath, sourceFS, fileStream)) return;
   
   SendU16(AckToken);
  
   if (!ReceiveFileData(fileStream, fileLength, checksum)) return;
   
   SendU16(AckToken);
}

FLASHMEM bool SendPagedDirectoryContents(FS& fileStream, const char* directoryPath, int skip, int take)
{
    File directory = fileStream.open(directoryPath);
    
    File directoryItem = directory.openNextFile();

    int currentCount = 0; 
    int pageCount = 0; 

    while(directoryItem && pageCount < take)
    { 
      currentCount++;
      
      const char* itemName = directoryItem.name();

      if(currentCount >= skip)
      {
        pageCount++;
        
        if (directoryItem.isDirectory())
        {
            Serial.print(F("[Dir]{\"Name\":\""));
            Serial.print(itemName);
            Serial.print(F("\",\"Path\":\""));
            Serial.print(directoryPath);
            Serial.print('/');
            Serial.print(itemName);
            Serial.print(F("\"}[/Dir]"));
        }
        else
        {
            Serial.print(F("[File]{\"Name\":\""));
            Serial.print(itemName);
            Serial.print(F("\",\"Size\":"));
            Serial.print(directoryItem.size());
            Serial.print(F(",\"Path\":\""));
            Serial.print(directoryPath);
            Serial.print('/');
            Serial.print(itemName);
            Serial.print(F("\"}[/File]"));
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
// Workflow:
// Receive <-- List Directory Token 0x64DD 
// Send --> AckToken 0x64CC
// Receive <-- SD_nUSB(1), Destination Path(MaxNameLength, null terminator), sake(1), skip(1)
// Send --> StartDirectoryListToken 0x5A5A or FailToken 0x9b7f
// Send --> Write content as json
// Send --> EndDirectoryListToken 0xA5A5,  0x9b7f on Fail
FLASHMEM void GetDirectoryCommand()
{
    const uint16_t StartDirectoryListToken = 0x5A5A;
    const uint16_t EndDirectoryListToken = 0xA5A5;

    SendU16(AckToken);

    uint32_t storageType, skip, take;
    char path[MaxNamePathLength];

    if (!GetUInt(&storageType, 1))
    {
        SendU16(FailToken);
        Serial.println("Error receiving storage type value!");
        return;
    }
    if (!GetUInt(&skip, 2))
    {
        SendU16(FailToken);
        Serial.println("Error receiving skip value!");
        return;
    }
    if (!GetUInt(&take, 2))
    {
        SendU16(FailToken);
        Serial.println("Error receiving take value!");
        return;
    }
    if (!GetPathParameter(path)) 
    {
        SendU16(FailToken);
        Serial.println("Error receiving path value!");
        return;
    }

    FS* sourceFS = GetStorageDevice(storageType);

    if (!sourceFS)
    {        
        SendU16(FailToken);
        Serial.println("Unable to get storage device!");
        return;
    }

    SendU16(StartDirectoryListToken);  

    if (!SendPagedDirectoryContents(*sourceFS, path, skip, take)) return;

    SendU16(EndDirectoryListToken);
}
