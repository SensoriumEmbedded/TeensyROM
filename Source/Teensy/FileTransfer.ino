

FLASHMEM bool GetPathParameter(char FileNamePath[])
{
    uint16_t CharNum = 0;
    char currentChar;

    while (CharNum < MaxNamePathLength)
    {
        if (!SerialAvailabeTimeout())
        {
            SendU16(FailToken);
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
        SendU16(FailToken);
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
        SendU16(FailToken);
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

    int retry = 0;

    while(retry < 3 && !file)
    {
      file = sourceFS->open(FileNamePath, FILE_WRITE);
      retry++;
    }

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

FLASHMEM bool ReceiveFileData(File& file, uint32_t len, uint32_t& checksum)
{
    uint32_t bytenum = 0;
    uint8_t byteIn;

    while (bytenum < len)
    {
        if (!SerialAvailabeTimeout())
        {
            SendU16(FailToken);
            Serial.printf("Rec %lu of %lu bytes\n", bytenum, len);
            file.close();
            return false;
        }
        file.write(byteIn = Serial.read());
        checksum -= byteIn;
        bytenum++;
    }  
    file.close();

    checksum &= 0xffff;
    if (checksum != 0)
    {
        SendU16(FailToken);
        Serial.printf("CS Failed! RCS:%lu\n", checksum);        
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

    if (!GetPathParameter(FileNamePath)) return;

    FS* sourceFS = GetStorageDevice(storageType);

    if (!sourceFS) return;

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

    while (directoryItem && pageCount < take)
    {
        currentCount++;

        const char* itemName = directoryItem.name();

        if (currentCount >= skip)
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

    if (!sourceFS) return;

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
        Serial.printf("Failed to open source file: %s\n", sourcePath);
        return false;
    }

    File destinationFile = fs.open(destinationPath, FILE_WRITE);
    if (!destinationFile)
    {
        SendU16(FailToken);
        Serial.printf("Failed to open destination file: %s\n", destinationPath);
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
// Copies a command from one folder to the other in the USB/SD storage
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
        Serial.println("Error receiving storage type value!");
        return;
    }

    if (!GetPathParameter(sourcePath)) return;

    if (!GetPathParameter(destinationPath)) return;

    FS* sourceFS = GetStorageDevice(storageType);

    if (!sourceFS) return;

    if (!EnsureDirectory(destinationPath, *sourceFS))
    {
        SendU16(FailToken);
        Serial.printf("Failed to ensure directory for: %s\n", destinationPath);
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
        Serial.printf("File not found: %s\n", filePath);
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
        Serial.printf("Failed to delete file: %s\n", filePath);
        return;
    }
}

// Command: 
// Delete a file from the specified storage device on TeensyROM.
//
// Workflow:
// Receive <-- Delete File Token (e.g., 0x64EE) 
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
        Serial.println("Error receiving storage type!");
        return;
    }

    if (!GetPathParameter(filePath))
    {
        SendU16(FailToken);
        Serial.println("Error receiving path!");
        return;
    }

    FS* sourceFS = GetStorageDevice(storageType);
    if (!sourceFS)
    {
        SendU16(FailToken);
        Serial.println("Error getting storage device!");
    }

    DeleteFile(filePath, *sourceFS);
}

FLASHMEM bool SendFileData(File& file, uint32_t len) {
    uint32_t bytenum = 0;
    while (bytenum < len) {
        if (Serial.availableForWrite()) {
            Serial.write(file.read());
            bytenum++;
        } else {
            SendU16(FailToken);
            Serial.printf("Send %lu of %lu bytes\n", bytenum, len);
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
        Serial.println("Error receiving storage type!");
        return;
    }

    if (!GetPathParameter(filePath))
    {
        SendU16(FailToken);
        Serial.println("Error receiving path!");
        return;
    }

    FS* sourceFS = GetStorageDevice(storageType);
    
    if (!sourceFS) return;

    File fileStream = sourceFS->open(filePath, FILE_READ);
    
    if (!fileStream) {
        SendU16(FailToken);
        Serial.printf("Failed to open file: %s\n", filePath);
        return;
    }

    uint32_t fileLength = fileStream.size();
    SendU32(fileLength);

    uint32_t checksum = CalculateChecksum(fileStream);
    SendU32(checksum);
    fileStream.seek(0);

    if (!SendFileData(fileStream, fileLength)) {
        fileStream.close();
        return;
    }
    fileStream.close();
    
    SendU16(AckToken);
}
