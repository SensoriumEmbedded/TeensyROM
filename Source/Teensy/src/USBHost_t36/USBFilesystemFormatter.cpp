#include <Arduino.h>
#include "utility/USBFilesystemFormatter.h"

#ifndef DBG_FAIL_MACRO
#define DBG_FAIL_MACRO
#endif
uint16_t toUpcase(uint16_t chr);

//=============================================
#define DBG_FILE "USBFilesystemFormatter.cpp"
//Set to 0 for debug info
//#define DBG_Print  0
#if defined(DBG_Print)
#define DBGPrintf Serial.printf
#else
void inline DBGPrintf(...) {};
#endif

#define PRINT_FORMAT_PROGRESS 1
#if !PRINT_FORMAT_PROGRESS
#define writeMsg(str)
void inline writeMsgF(...) {};
#elif defined(__AVR__)
#define writeMsg(str) if (m_pr) m_pr->print(F(str))
#define writeMsgF if (m_pr) m_pr->printf
#else  // PRINT_FORMAT_PROGRESS
#define writeMsg(str) if (m_pr) m_pr->write(str)
#define writeMsgF if (m_pr) m_pr->printf
#endif  // PRINT_FORMAT_PROGRESS

//=============================================
// Set nonzero to use calculated CHS in MBR.  Should not be required.
#define USE_LBA_TO_CHS 1
// Constants for file system structure optimized for flash.
uint16_t const BU16 = 128;
uint16_t  BU32 = 8192;
// Assume 512 byte sectors.
const uint16_t BYTES_PER_SECTOR = 512;
const uint16_t SECTORS_PER_MB = 0X100000/BYTES_PER_SECTOR;
const uint16_t FAT16_ROOT_ENTRY_COUNT = 512;
const uint16_t FAT16_ROOT_SECTOR_COUNT =
               32*FAT16_ROOT_ENTRY_COUNT/BYTES_PER_SECTOR;
			   
//Support for exFat formatting
const uint32_t BOOT_BACKUP_OFFSET = 12;
const uint16_t SECTOR_MASK = BYTES_PER_SECTOR - 1;
const uint8_t  BYTES_PER_SECTOR_SHIFT = 9;
const uint16_t MINIMUM_UPCASE_SKIP = 512;
const uint32_t BITMAP_CLUSTER = 2;
const uint32_t UPCASE_CLUSTER = 3;
const uint32_t ROOT_CLUSTER = 4;

//=============================================
bool USBFilesystemFormatter::format(USBFilesystem &fs, uint8_t fat_type, uint8_t* secBuf, print_t* pr)
{
  // We can extract the device and partition from the fs object. 
  DBGPrintf("\n### USBFilesystemFormatter::formatFAT called\n");
  DBGPrintf("\tFattype: FS:%u Opt:%u\n", fs.mscfs.fatType(), fat_type);
  if (fat_type == 0) fat_type = fs.mscfs.fatType();
  switch (fat_type) {
    case FAT_TYPE_FAT16:
    case FAT_TYPE_FAT32:
    //case FAT_TYPE_FAT12
      return formatFAT(*fs.device, fs, fs.partition, fat_type, secBuf, pr);
    case FAT_TYPE_EXFAT:
      return formatExFAT(*fs.device, fs, fs.partition, fat_type, secBuf, pr);
  }
  return false;
}

//====================================================
bool USBFilesystemFormatter::formatFAT(USBDrive &dev, USBFilesystem &fs, uint8_t part, uint8_t fat_type, uint8_t* secBuf, print_t* pr)
{
  DBGPrintf("\n### USBFilesystemFormatter::formatFAT called\n");

  bool rtn;
  m_secBuf = secBuf;
  m_pr = pr;
  writeMsg("Begin format Fat File system\n");
  //m_dev = partVol.blockDevice();
  m_part = part-1;  // convert to 0 biased. 
  
  uint32_t firstLBA;  //comes from getpartitioninfo in pfslib
  uint32_t sectorCount;
  uint32_t mbrLBA; //comes from getpartitioninfo in pfslib
  uint8_t mbrPart;
  int mbrType;
  char volName[32];

  //findPartion using partition number non-zero biased - it does the zero biasing in the function
  int vt = dev.findPartition(part, mbrType, firstLBA, sectorCount, mbrLBA, mbrPart);

  DBGPrintf("Part:%u vt:%u first:%u, count:%u MBR:%u MBR Part:%u MBR Type: %u\n", part, vt, firstLBA, sectorCount, mbrLBA, mbrPart, mbrType);

  if (vt == 0) return false;    // got an invalid volume

  // yes could have used some of the directly...
  m_sectorCount = sectorCount;
  m_part_relativeSectors = firstLBA;
  m_mbrLBA = mbrLBA;
  m_mbrPart = mbrPart;

  m_capacityMB = (m_sectorCount + SECTORS_PER_MB - 1)/SECTORS_PER_MB;
  writeMsgF("Capacity in MB: %u\n", m_capacityMB);

//  m_capacityMB = (uint32_t) (fs.totalSize()/1000000);
  
  if(m_capacityMB > 32768) {
	  writeMsgF("Volume is greater(%u) than 32MB, Need to format as exFAT!!\n", m_capacityMB);
	  return false;
  }
  
  bool has_volume_label = fs.mscfs.getVolumeLabel(volName, sizeof(volName));

  if (has_volume_label) {
   writeMsgF("Volume name:(%s)\n", volName);
  }
  DBGPrintf("\nPFsFatFormatter::format................");
  DBGPrintf("Sector Count: %d, Sectors/MB: %d\n", m_sectorCount, SECTORS_PER_MB);
  DBGPrintf("Partition Capacity (MB): %d\n", m_capacityMB);
  DBGPrintf("Partition Capacity - from fs (MB): %d\n", (uint32_t) (fs.totalSize()/1000000));
  DBGPrintf("Fat Type: %d\n", fs.mscfs.fatType());
  DBGPrintf("    m_dataStart:%u\n", fs.mscfs.dataStartSector());
  DBGPrintf("    m_sectorsPerCluster:%u\n",fs.mscfs.sectorsPerCluster());
  DBGPrintf("    m_relativeSectors:%u\n", m_part_relativeSectors);
  DBGPrintf("    Fat start sector:%u\n", fs.mscfs.fatStartSector());
  DBGPrintf("    Cluster Count:%u\n", fs.mscfs.clusterCount());
  DBGPrintf("    Bytes per Cluster:%u\n", fs.mscfs.bytesPerCluster());
  DBGPrintf("\n");
  
  if (m_capacityMB <= 6) {
    writeMsg("Card is too small.\r\n");
    return false;
  } else if (m_capacityMB <= 16) {
    m_sectorsPerCluster = 2;
  } else if (m_capacityMB <= 32) {
    m_sectorsPerCluster = 4;
  } else if (m_capacityMB <= 64) {
    m_sectorsPerCluster = 8;
  } else if (m_capacityMB <= 128) {
    m_sectorsPerCluster = 16;
  } else if (m_capacityMB <= 1024) {
    m_sectorsPerCluster = 32;
  } else if (m_capacityMB <= 32768) {
    m_sectorsPerCluster = 64;
  } else {
    // SDXC cards
    m_sectorsPerCluster = 128;
  }
  

  //rtn = m_sectorCount < 0X400000 ? makeFat16() :makeFat32();
  
  if(fat_type == 16 && m_sectorCount < 0X400000 ) {
    writeMsg("format makeFAT16\r\n");  
    rtn = makeFat16(dev);
  } else if(fat_type == 32) {
    writeMsg("format makeFAT32\r\n");  
    rtn = makeFat32(dev);
  } else {
    rtn = false;
  }

  return rtn;
}

//====================================================================================
bool USBFilesystemFormatter::makeFat16(USBDrive &m_dev) {
  
  DBGPrintf(" MAKEFAT16\n");
  uint32_t nc;
  uint32_t r;
  PbsFat_t* pbs = reinterpret_cast<PbsFat_t*>(m_secBuf);
  
  for (m_dataStart = 2*BU16; ; m_dataStart += BU16) {
    nc = (m_sectorCount - m_dataStart)/m_sectorsPerCluster;
    m_fatSize = (nc + 2 + (BYTES_PER_SECTOR/2) - 1)/(BYTES_PER_SECTOR/2);
    r = BU16 + 1 + 2*m_fatSize + FAT16_ROOT_SECTOR_COUNT;
    if (m_dataStart >= r) {
      m_relativeSectors = m_dataStart - r + BU16;
      break;
    }
  }

  //nc = (m_sectorCount - m_dataStart)/m_sectorsPerCluster;
  //m_fatSize = (nc + 2 + (BYTES_PER_SECTOR/2) - 1)/(BYTES_PER_SECTOR/2);
  
  DBGPrintf("m_relativeSectors: %u, m_fatSize: %u, m_dataStart: %u\n",m_relativeSectors, m_fatSize, m_dataStart) ;
  
  // check valid cluster count for FAT16 volume
  if (nc < 4085 || nc >= 65525) {
    writeMsg("Bad cluster count\r\n");
    return false;
  }
  m_reservedSectorCount = 1;
  m_fatStart = m_relativeSectors + m_reservedSectorCount;
  m_totalSectors = nc*m_sectorsPerCluster
                   + 2*m_fatSize + m_reservedSectorCount + 32;
  if (m_totalSectors < 65536) {
    m_partType = 0X04;
  } else {
    m_partType = 0X06;
  }

  //Added to keep relative sectors straight
  m_relativeSectors = m_part_relativeSectors;
  m_fatStart = m_relativeSectors + m_reservedSectorCount;
    m_dataStart= m_fatStart + 2 * m_fatSize + FAT16_ROOT_SECTOR_COUNT;
  m_totalSectors = m_sectorCount;

  DBGPrintf("partType: %d, m_relativeSectors: %u, fatStart: %u, fatDatastart: %u, totalSectors: %u\n", m_partType, m_relativeSectors, m_fatStart, m_dataStart, m_totalSectors);

  // write MBR
  writeMsg("Writing MBR...");
  if (!writeFatMbr(m_dev)) {
    return false;
  }

  initPbs();
  setLe16(pbs->bpb.bpb16.rootDirEntryCount, FAT16_ROOT_ENTRY_COUNT);
  setLe16(pbs->bpb.bpb16.sectorsPerFat16, m_fatSize);
  pbs->bpb.bpb16.physicalDriveNumber = 0X80;
  pbs->bpb.bpb16.extSignature = EXTENDED_BOOT_SIGNATURE;
  setLe32(pbs->bpb.bpb16.volumeSerialNumber, 1234567);
  
  for (size_t i = 0; i < sizeof(pbs->bpb.bpb16.volumeLabel); i++) {
    pbs->bpb.bpb16.volumeLabel[i] = ' ';
  }
  pbs->bpb.bpb16.volumeType[0] = 'F';
  pbs->bpb.bpb16.volumeType[1] = 'A';
  pbs->bpb.bpb16.volumeType[2] = 'T';
  pbs->bpb.bpb16.volumeType[3] = '1';
  pbs->bpb.bpb16.volumeType[4] = '6';
  if (!writeSector(m_dev, m_relativeSectors, m_secBuf)) {
    return false;
  }
  
  return initFatDir(m_dev, 16, m_dataStart - m_fatStart);
  
}

bool USBFilesystemFormatter::makeFat32(USBDrive &m_dev) {
  DBGPrintf(" MAKEFAT32\n");
  uint32_t nc;
  uint32_t r;
  
  PbsFat_t* pbs = reinterpret_cast<PbsFat_t*>(m_secBuf);
  FsInfo_t* fsi = reinterpret_cast<FsInfo_t*>(m_secBuf);
  
  m_relativeSectors = BU32;
  for (m_dataStart = 2*BU32; ; m_dataStart += BU32) {
    nc = (m_sectorCount - m_dataStart)/m_sectorsPerCluster;
    m_fatSize = (nc + 2 + (BYTES_PER_SECTOR/4) - 1)/(BYTES_PER_SECTOR/4);
    r = m_relativeSectors + 9 + 2*m_fatSize;
    DBGPrintf("m_dataStart: %u, m_fatSize: %u, r: %u\n", m_dataStart, m_fatSize, r);
    if (m_dataStart >= r) {
      break;
    }
  }
  
    //nc = (m_sectorCount - m_dataStart)/m_sectorsPerCluster;
    //m_fatSize = (nc + 2 + (BYTES_PER_SECTOR/4) - 1)/(BYTES_PER_SECTOR/4);
    DBGPrintf("    m_part: %d\n", m_part);
    DBGPrintf("    m_sectorCount: %d\n", m_sectorCount);
    DBGPrintf("    m_dataStart: %d\n", m_dataStart);
    DBGPrintf("    m_sectorsPerCluster: %d\n", m_sectorsPerCluster);
    DBGPrintf("    nc: %d\n", nc);
    DBGPrintf("    m_fatSize: %d\n", m_fatSize);

  // error if too few clusters in FAT32 volume
  if (nc < 65525) {
    writeMsg("Bad cluster count\r\n");
    return false;
  }
  m_reservedSectorCount = m_dataStart - m_relativeSectors - 2*m_fatSize;
  m_fatStart = m_relativeSectors + m_reservedSectorCount;
  m_totalSectors = nc*m_sectorsPerCluster + m_dataStart - m_relativeSectors;
  // type depends on address of end sector
  // max CHS has lba = 16450560 = 1024*255*63
  if ((m_relativeSectors + m_totalSectors) <= 16450560) {
    // FAT32 with CHS and LBA
    m_partType = 0X0B;
  } else {
    // FAT32 with only LBA
    m_partType = 0X0C;
  }
  
  //Write MBR
  //Added to keep relative sectors straight
  m_relativeSectors = m_part_relativeSectors;
  m_fatStart = m_relativeSectors + m_reservedSectorCount;
  m_dataStart = m_relativeSectors + m_dataStart;
  m_totalSectors = m_sectorCount;
  
  DBGPrintf("[makeFat32] partType: %d, m_relativeSectors: %u, fatStart: %u, fatDatastart: %u, totalSectors: %u\n", m_partType, m_relativeSectors, m_fatStart, m_dataStart, m_totalSectors);

  if (!writeFatMbr(m_dev)) {
    writeMsg("Failed to write MBR!!");
    return false;
  }
  
  initPbs();  
  setLe32(pbs->bpb.bpb32.sectorsPerFat32, m_fatSize);
  setLe32(pbs->bpb.bpb32.fat32RootCluster, 2);
  setLe16(pbs->bpb.bpb32.fat32FSInfoSector, 1);
  setLe16(pbs->bpb.bpb32.fat32BackBootSector, 6);
  pbs->bpb.bpb32.physicalDriveNumber = 0X80;
  pbs->bpb.bpb32.extSignature = EXTENDED_BOOT_SIGNATURE;
  setLe32(pbs->bpb.bpb32.volumeSerialNumber, 1234567);
  for (size_t i = 0; i < sizeof(pbs->bpb.bpb32.volumeLabel); i++) {
    pbs->bpb.bpb32.volumeLabel[i] = ' ';
  }
  pbs->bpb.bpb32.volumeType[0] = 'F';
  pbs->bpb.bpb32.volumeType[1] = 'A';
  pbs->bpb.bpb32.volumeType[2] = 'T';
  pbs->bpb.bpb32.volumeType[3] = '3';
  pbs->bpb.bpb32.volumeType[4] = '2';

  writeMsg("Writing Partition Boot Sector\n");
  
  if (!writeSector(m_dev, m_relativeSectors, m_secBuf)  ||
      !writeSector(m_dev, m_relativeSectors + 6, m_secBuf)) {
    return false;
  }
  
  // write extra boot area and backup
  memset(m_secBuf, 0 , BYTES_PER_SECTOR);
  setLe32(fsi->trailSignature, FSINFO_TRAIL_SIGNATURE);
  if (!writeSector(m_dev, m_relativeSectors + 2, m_secBuf)  ||
      !writeSector(m_dev, m_relativeSectors + 8, m_secBuf)) {
    return false;
  }
  // write FSINFO sector and backup
  setLe32(fsi->leadSignature, FSINFO_LEAD_SIGNATURE);
  setLe32(fsi->structSignature, FSINFO_STRUCT_SIGNATURE);
  setLe32(fsi->freeCount, 0XFFFFFFFF);
  setLe32(fsi->nextFree, 0XFFFFFFFF);
  writeMsg("Writing FSInfo Sector\n");
  if (!writeSector(m_dev, m_relativeSectors + 1, m_secBuf)  ||
      !writeSector(m_dev, m_relativeSectors + 7, m_secBuf)) {
    return false;
  }
  writeMsg("Writing FAT\n");
  return initFatDir(m_dev, 32, 2*m_fatSize + m_sectorsPerCluster);

}

//------------------------------------------------------------------------------
bool USBFilesystemFormatter::writeFatMbr(USBDrive &m_dev) {
  if (m_mbrLBA == 0xFFFFFFFFUL) {
    DBGPrintf("    writeMBR - GPT entry so dont update\n");
    return true;
  }
  memset(m_secBuf, 0, BYTES_PER_SECTOR);
  
  // The relative sectors stuff is setup based off of the logicalMBR...
  uint32_t relativeSectors = m_relativeSectors - m_mbrLBA;

  MbrSector_t* mbr = reinterpret_cast<MbrSector_t*>(m_secBuf);
  MbrPart_t *pt = &mbr->part[m_mbrPart];
  if (!m_dev.readSector(m_mbrLBA, m_secBuf)) {
    writeMsg("Didn't read MBR Sector !!!\n");
    return false;
  }

  DBGPrintf("(writeFatMbr)[m_capacityMB,m_relativeSectors,relativeSectors,relativeSectors + m_totalSectors -1] %u, %u, %u, %u\n",m_capacityMB,m_relativeSectors,relativeSectors,relativeSectors + m_totalSectors -1);
#if USE_LBA_TO_CHS
  lbaToMbrChs(pt->beginCHS, m_capacityMB, m_relativeSectors);
  lbaToMbrChs(pt->endCHS, m_capacityMB,
              m_relativeSectors + m_totalSectors -1);
#else  // USE_LBA_TO_CHS
  pt->beginCHS[0] = 1;
  pt->beginCHS[1] = 1;
  pt->beginCHS[2] = 0;
  pt->endCHS[0] = 0XFE;
  pt->endCHS[1] = 0XFF;
  pt->endCHS[2] = 0XFF;
#endif  // USE_LBA_TO_CHS

  pt->type = m_partType;
  setLe32(pt->relativeSectors, relativeSectors);
  setLe32(pt->totalSectors, m_totalSectors);
  setLe16(mbr->signature, MBR_SIGNATURE);
  return writeSector(m_dev, m_mbrLBA, m_secBuf);

}

//-----------------------------------------------------------------------------

#define CSECTORS_PER_WRITE 32
bool USBFilesystemFormatter::initFatDir(USBDrive &m_dev, uint8_t fatType, uint32_t sectorCount) {
  DBGPrintf("PFsFatFormatter::initFatDir(%u, %u)\n", fatType, sectorCount);
  size_t n;
  uint32_t fat_sector = 1;
  DBGPrintf("Writing FAT ");
  if (sectorCount >= CSECTORS_PER_WRITE) {
    uint8_t *large_buffer_alloc = (uint8_t *)malloc(BYTES_PER_SECTOR * CSECTORS_PER_WRITE + 32);

    if (large_buffer_alloc) {
      uint8_t *large_buffer = (uint8_t *)(((uintptr_t)large_buffer_alloc + 31) & ~((uintptr_t)(31)));
      DBGPrintf("\tbuffer:%p alligned:%p\n", large_buffer_alloc, large_buffer);
      memset(large_buffer, 0, BYTES_PER_SECTOR * CSECTORS_PER_WRITE);
      uint32_t sectors_remaining = sectorCount;
      uint32_t loops_per_dot = sectorCount/(32*CSECTORS_PER_WRITE);
      uint32_t loop_count = 0;
      while (sectors_remaining >= CSECTORS_PER_WRITE) {
        if (!m_dev.writeSectors(m_fatStart + fat_sector, large_buffer, CSECTORS_PER_WRITE)) {
           return false;
        }
        fat_sector += CSECTORS_PER_WRITE;
        sectors_remaining -= CSECTORS_PER_WRITE;
        if (++loop_count == loops_per_dot) {
          DBGPrintf(".");
          loop_count = 0;
        }
      }
      if (sectors_remaining) {
        if (!m_dev.writeSectors(m_fatStart + fat_sector, large_buffer, sectors_remaining)) {
           return false;
        }
        fat_sector += sectors_remaining;
      }
      free(large_buffer_alloc);
    }
  }
  if (fat_sector < sectorCount) {
    memset(m_secBuf, 0, BYTES_PER_SECTOR);
    for (; fat_sector < sectorCount; fat_sector++) {
      if (!writeSector(m_dev, m_fatStart + fat_sector, m_secBuf)) {
         return false;
      }
      if ((fat_sector%(sectorCount/32)) == 0) {
        DBGPrintf(".");
      }
    }
  }
  DBGPrintf("\r\n");
  // Allocate reserved clusters and root for FAT32.
  m_secBuf[0] = 0XF8;
  n = fatType == 16 ? 4 : 12;
  for (size_t i = 1; i < n; i++) {
    m_secBuf[i] = 0XFF;
  }
  return writeSector(m_dev, m_fatStart, m_secBuf) &&
         writeSector(m_dev, m_fatStart + m_fatSize, m_secBuf);
}

//------------------------------------------------------------------------------
void USBFilesystemFormatter::initPbs() {
  PbsFat_t* pbs = reinterpret_cast<PbsFat_t*>(m_secBuf);
  memset(m_secBuf, 0, BYTES_PER_SECTOR);
  
  pbs->jmpInstruction[0] = 0XEB;
  pbs->jmpInstruction[1] = 0X76;
  pbs->jmpInstruction[2] = 0X90;
  for (uint8_t i = 0; i < sizeof(pbs->oemName); i++) {
    pbs->oemName[i] = ' ';
  }
  setLe16(pbs->bpb.bpb16.bytesPerSector, BYTES_PER_SECTOR);
  pbs->bpb.bpb16.sectorsPerCluster = m_sectorsPerCluster;
  setLe16(pbs->bpb.bpb16.reservedSectorCount, m_reservedSectorCount);
  pbs->bpb.bpb16.fatCount = 2;
  // skip rootDirEntryCount
  // skip totalSectors16
  pbs->bpb.bpb16.mediaType = 0XF8;
  // skip sectorsPerFat16
  // skip sectorsPerTrack
  // skip headCount
  setLe32(pbs->bpb.bpb16.hidddenSectors, m_relativeSectors);
  setLe32(pbs->bpb.bpb16.totalSectors32, m_totalSectors);
  // skip rest of bpb
  setLe16(pbs->signature, PBR_SIGNATURE);
}
//------------------------------------------------------------------------------

bool USBFilesystemFormatter::writeSector(USBDrive &m_dev, uint32_t sector, const uint8_t* src) {
  // sandbox support
  if ((sector < m_minSector) || (sector > m_maxSector)) {
    DBGPrintf("!!! Sandbox Error: %u <= %u <= %u - Press any key to continue\n", 
      m_minSector, sector, m_maxSector);
    while (Serial.read() == -1);
    while (Serial.read() != -1) ;
  }

  return m_dev.writeSector(sector, src);
}

//===============================================================
//exFat Formatting
//===============================================================
bool USBFilesystemFormatter::formatExFAT(USBDrive &dev, USBFilesystem &fs, uint8_t part, uint8_t fat_type, uint8_t* secBuf, print_t* pr) {
  DBGPrintf("\n### USBFilesystemFormatter::formatExFAT called\n");

  ExFatPbs_t* pbs;
  DirUpcase_t* dup;
  DirBitmap_t* dbm;
  DirLabel_t* label;
  uint32_t bitmapSize;
  uint32_t checksum = 0;
  uint32_t clusterCount;
  uint32_t clusterHeapOffset;
  uint32_t fatLength;
  uint32_t fatOffset;
  uint32_t m;
  uint32_t ns;
  uint32_t sector;
  uint32_t sectorsPerCluster;
  uint32_t sectorCount;
  uint8_t sectorsPerClusterShift;
  uint8_t vs;

  uint32_t firstLBA;
  uint32_t mbrLBA; 
  uint8_t mbrPart;
  int mbrType;
  char volName[32];

  m_secBuf = secBuf;
  m_pr = pr;
  writeMsg("Begin format ExFat File system\n");
  //m_dev = partVol.blockDevice();
  //m_part = partVol.part()-1;  // convert to 0 biased. 
  m_part = part-1;  // convert to 0 biased. 

  int vt = dev.findPartition(part, mbrType, firstLBA, sectorCount, mbrLBA, mbrPart);

  DBGPrintf("Part:%u vt:%u first:%u, count:%u MBR:%u MBR Part:%u Type:%u\n", part, (uint8_t)vt, firstLBA, sectorCount, mbrLBA, mbrPart, mbrType);

  if (vt == 0) return false;    // got an invalid volume

  // yes could have used some of the directly...
  m_relativeSectors = firstLBA;
  m_sectorCount = sectorCount;
  m_mbrLBA = mbrLBA;
  m_mbrPart = mbrPart;
  
  bool has_volume_label = fs.mscfs.getVolumeLabel(volName, sizeof(volName));
  if (has_volume_label) {
   writeMsgF("Volume name:(%s)\n", volName);
  }

  #if defined(DBG_PRINT)
	DBGPrintf("    m_sectorsPerCluster:%u\n", fs.mscfs.sectorsPerCluster());
	DBGPrintf("    m_relativeSectors:%u\n", m_relativeSectors);
	DBGPrintf("    m_fatStartSector: %u\n", fs.mscfs.fatStartSector());
	DBGPrintf("    m_fatType: %d\n", fs.mscfs.fatType());
	DBGPrintf("    m_clusterCount: %u\n", fs.mscfs.clusterCount());
	DBGPrintf("    m_totalSectors: %u\n", sectorCount);
	DBGPrintf("\n");
  #endif
  
  // Min size is 512 MB
  if (sectorCount < 0X100000) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  
  //BYTES_PER_SECTOR = partVol.getExFatVol()->bytesPerSector();
  //SECTOR_MASK = BYTES_PER_SECTOR - 1;
  //ROOT_CLUSTER = partVol.getExFatVol()->rootDirectoryCluster();
  pbs = reinterpret_cast<ExFatPbs_t*>(secBuf);
  if (!dev.readSector(firstLBA, m_secBuf)) {
    DBG_FAIL_MACRO;
    goto fail;    
  }
  
  // Determine partition layout.
  for (m = 1, vs = 0; m && sectorCount > m; m <<= 1, vs++) {}
  sectorsPerClusterShift = vs < 29 ? 8 : (vs - 11)/2;
  //DBGPrintf("Calculate sectorsPerClusterShift = %u\n", vs < 29 ? 8 : (vs - 11)/2);
  
  //1 << n is the same as raising 2 to the power n
  sectorsPerCluster = 1UL << sectorsPerClusterShift;
  //DBGPrintf("Calculated sectorsPerCluster = %u\n",  1UL << sectorsPerClusterShift);

  //The FatLength field shall describe the length, in sectors, of each FAT table
  //   At least (ClusterCount + 2) * 2^2/ 2^BytesPerSectorShift rounded up to the nearest integer
  //   At most (ClusterHeapOffset - FatOffset) / NumberOfFats rounded down to the nearest integer
  fatLength = 1UL << (vs < 27 ? 13 : (vs + 1)/2);  //original
  //DBGPrintf("Calculated fatLength1 = %u\n",1UL << (vs < 27 ? 13 : (vs + 1)/2));

  //The ClusterCount field shall describe the number of clusters the Cluster Heap contains
  //   (VolumeLength - ClusterHeapOffset) / 2^SectorsPerClusterShift rounded down to the nearest integer, which is exactly the number of clusters which can fit between the beginning of the Cluster Heap and the end of the volume
  //   232- 11, which is the maximum number of clusters a FAT can describe
  clusterCount = (sectorCount - 4*fatLength) >> sectorsPerClusterShift;  //original
  //DBGPrintf("Calculated clusterCount = %u\n", (sectorCount - 4*fatLength) >> sectorsPerClusterShift);
  
  //The ClusterHeapOffset field shall describe the volume-relative sector offset of the Cluster Heap
  //   At least FatOffset + FatLength * NumberOfFats, to account for the sectors all the preceding regions consume
  //   At most 2^32- 1 or VolumeLength - (ClusterCount * 2^SectorsPerClusterShift), whichever calculation is less
  clusterHeapOffset = 2*fatLength;  //original
  //DBGPrintf("\tclusterHeapOffset: %u %u\n",getLe32(pbs->bpb.clusterHeapOffset), clusterHeapOffset);

  //The FatOffset field shall describe the volume-relative sector offset of the First FAT
  //   At least 24, which accounts for the sectors the Main Boot and Backup Boot regions consume
  //   At most ClusterHeapOffset - (FatLength * NumberOfFats), which accounts for the sectors the Cluster Heap consumes
  fatOffset = fatLength;
  //DBGPrintf("Calculated fatOffset = %u\n", clusterHeapOffset - fatLength);
  
  //The PartitionOffset field shall describe the media-relative sector offset of the partition which hosts the given exFAT volume
  partitionOffset = m_relativeSectors;
  
  
  //The VolumeLength field shall describe the size of the given exFAT volume in sectors
  //   At least 2^20/ 2^BytesPerSectorShift, which ensures the smallest volume is no less than 1MB
  //   At most 264- 1, the largest value this field can describe
  volumeLength = sectorCount;
  
  #if defined(DBG_PRINT)
    DBGPrintf("VS: %d\n", vs);
    DBGPrintf("sectorsPerClusterShift: %u,\n sectorsPerCluster: %u,\n fatLength %u\n", sectorsPerClusterShift, sectorsPerCluster, fatLength);
    DBGPrintf("fatOffset: %u,\n partitionOffset: %u\n", fatOffset, partitionOffset);
  	DBGPrintf("clusterHeapOffset: %u,\n clusterCount: %u,\n volumeLength %u\n", clusterHeapOffset, clusterCount, volumeLength);

  	DBGPrintf("cluster 2 bitmap: %u\n",partitionOffset + clusterHeapOffset);
  	DBGPrintf("Up case table: %u\n",partitionOffset + clusterHeapOffset + sectorsPerCluster);

// =============================== DEBUG
  pbs = reinterpret_cast<ExFatPbs_t*>(secBuf);
  if (m_dev->readSector(firstLBA, m_secBuf)) {
    DBGPrintf("\n *** PBS data ***\n");
    DBGPrintf("\tFirstLBA: %u\n", firstLBA);
    DBGPrintf("\tpartitionOffset: %llu %u\n",getLe64(pbs->bpb.partitionOffset), partitionOffset);
    DBGPrintf("\tvolumeLength: %llu %u\n",getLe64(pbs->bpb.volumeLength), volumeLength);
    DBGPrintf("\tfatOffset: %u %u\n",getLe32(pbs->bpb.fatOffset), fatOffset);
    DBGPrintf("\tfatLength: %u %u\n",getLe32(pbs->bpb.fatLength), fatLength);
    DBGPrintf("\tclusterHeapOffset: %u %u\n",getLe32(pbs->bpb.clusterHeapOffset), clusterHeapOffset);
    DBGPrintf("\tclusterCount: %u %u\n",getLe32(pbs->bpb.clusterCount), clusterCount);
    DBGPrintf("\trootDirectoryCluster: %u %u\n",getLe32(pbs->bpb.rootDirectoryCluster), ROOT_CLUSTER);
    DBGPrintf("\tvolumeSerialNumber: %u %u\n",getLe32(pbs->bpb.volumeSerialNumber), sectorCount);
    DBGPrintf("\tfileSystemRevision: %u %u\n",getLe16(pbs->bpb.fileSystemRevision), 0X100);
    DBGPrintf("\tvolumeFlags: %u %u\n",getLe16(pbs->bpb.volumeFlags), 0);

    //Serial.println("*** Hit any key to continue $ to abort");
    //int ch;
    //while((ch=Serial.read()) == -1);
    //while(Serial.read() != -1);
    //if (ch == '$') { DBGPrintf("*** Aborted ***"); return false; }
  }
  #endif

//--------------------  WRITE MBR ----------

  if (!writeExFatMbr(dev)) {
    DBG_FAIL_MACRO;
    goto fail;
  }  

  // Debug Set the sand box 
  setWriteSandBox(firstLBA, firstLBA + sectorCount - 1);

  writeMsg( "Writing Partition Boot Sector\n");
  // Partition Boot sector.
  memset(secBuf, 0, BYTES_PER_SECTOR);
  pbs = reinterpret_cast<ExFatPbs_t*>(secBuf);

  pbs->jmpInstruction[0] = 0XEB;
  pbs->jmpInstruction[1] = 0X76;
  pbs->jmpInstruction[2] = 0X90;
  pbs->oemName[0] = 'E';
  pbs->oemName[1] = 'X';
  pbs->oemName[2] = 'F';
  pbs->oemName[3] = 'A';
  pbs->oemName[4] = 'T';
  pbs->oemName[5] = ' ';
  pbs->oemName[6] = ' ';
  pbs->oemName[7] = ' ';
  setLe64(pbs->bpb.partitionOffset, partitionOffset);
  setLe64(pbs->bpb.volumeLength, volumeLength);
  setLe32(pbs->bpb.fatOffset, fatOffset);
  setLe32(pbs->bpb.fatLength, fatLength);
  setLe32(pbs->bpb.clusterHeapOffset, clusterHeapOffset);
  setLe32(pbs->bpb.clusterCount, clusterCount);
  setLe32(pbs->bpb.rootDirectoryCluster, ROOT_CLUSTER);
  setLe32(pbs->bpb.volumeSerialNumber, sectorCount);
    
  setLe16(pbs->bpb.fileSystemRevision, 0X100);
  setLe16(pbs->bpb.volumeFlags, 0);
  pbs->bpb.bytesPerSectorShift = BYTES_PER_SECTOR_SHIFT;
  pbs->bpb.sectorsPerClusterShift = sectorsPerClusterShift;
  pbs->bpb.numberOfFats = 1;
  pbs->bpb.driveSelect = 0X80;
  pbs->bpb.percentInUse = 0;

  // Fill boot code like official SDFormatter.
  for (size_t i = 0; i < sizeof(pbs->bootCode); i++) {
    pbs->bootCode[i] = 0XF4;
  }
  setLe16(pbs->signature, PBR_SIGNATURE);
  for (size_t i = 0; i < BYTES_PER_SECTOR; i++) {
    if (i == offsetof(ExFatPbs_t, bpb.volumeFlags[0]) ||
        i == offsetof(ExFatPbs_t, bpb.volumeFlags[1]) ||
        i == offsetof(ExFatPbs_t, bpb.percentInUse)) {
      continue;
    }
    checksum = exFatChecksum(checksum, secBuf[i]);
  }
    
  sector = partitionOffset;
#if defined(DBG_PRINT)
  DBGPrintf("\tWriting Sector: %d\n", sector-partitionOffset);
#endif
  if (!writeSector(dev, sector, secBuf)  ||
      !writeSector(dev, sector + BOOT_BACKUP_OFFSET , secBuf)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
    
    
  writeMsg( "Write eight Extended Boot Sectors\n");
  sector++;
#if defined(DBG_PRINT)
  DBGPrintf("\tWriting Sector: %d\n", sector-partitionOffset);
#endif
  // Write eight Extended Boot Sectors.
  memset(secBuf, 0, BYTES_PER_SECTOR);
  setLe16(pbs->signature, PBR_SIGNATURE);
  for (int j = 0; j < 8; j++) {
    for (size_t i = 0; i < BYTES_PER_SECTOR; i++) {
      checksum = exFatChecksum(checksum, secBuf[i]);
    }
    if (!writeSector(dev, sector, secBuf)  ||
        !writeSector(dev, sector + BOOT_BACKUP_OFFSET , secBuf)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    sector++;
  }
    
  writeMsg( "Write OEM Parameter Sector and reserved sector\n");
#if defined(DBG_PRINT)
  DBGPrintf("\tWriting Sector: %d\n", sector-partitionOffset);
#endif
  // Write OEM Parameter Sector and reserved sector.
  memset(secBuf, 0, BYTES_PER_SECTOR);
  for (int j = 0; j < 2; j++) {
    for (size_t i = 0; i < BYTES_PER_SECTOR; i++) {
      checksum = exFatChecksum(checksum, secBuf[i]);
    }
    if (!writeSector(dev, sector, secBuf)  ||
        !writeSector(dev, sector + BOOT_BACKUP_OFFSET , secBuf)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    sector++;
  }
  
  writeMsg( "Write Boot CheckSum Sector\n");
#if defined(DBG_PRINT)
  DBGPrintf("\tWriting Sector: %d\n", sector-partitionOffset);
#endif
  // Write Boot CheckSum Sector.
  for (size_t i = 0; i < BYTES_PER_SECTOR; i += 4) {
    setLe32(secBuf + i, checksum);
  }
  if (!writeSector(dev, sector, secBuf)  ||
      !writeSector(dev, sector + BOOT_BACKUP_OFFSET , secBuf)) {
    DBG_FAIL_MACRO;
    goto fail;
  }

  // Initialize FAT.
  writeMsg( "Writing exFAT ");
  sector = partitionOffset + fatOffset;
  
  //The + 2 is because the first two entries in a FAT do not represent clusters.
  //      The FatEntry[0] field shall describe the media type in the first byte (the lowest order byte) and shall contain FFh 
  //      in the remaining three bytes. The media type (the first byte) should be F8h
  //Media type is generally ignored so my bug of not setting it correctly was not caught earlier.
  //      The FatEntry[1] field only exists due to historical precedence and does not describe anything of interest.
  //The )*4 is because entries are four bytes. The expression rounds up to a whole number of sectors.
  ns = ((clusterCount + 2)*4 + BYTES_PER_SECTOR - 1)/BYTES_PER_SECTOR;
#if defined(DBG_PRINT)
  DBGPrintf("\tWriting Sector: %d, ns: %u\n", sector-partitionOffset, ns);
#endif
  memset(secBuf, 0, BYTES_PER_SECTOR);
  // Allocate two reserved clusters, bitmap, upcase, and root clusters.
	  secBuf[0] = 0XF8;
	  for (size_t i = 1; i < 20; i++) {
		secBuf[i] = 0XFF;
	  }
  for (uint32_t i = 0; i < ns; i++) {
    if (i%(ns/32) == 0) {
      writeMsg( ".");
    }
    if (!writeSector(dev, sector + i, secBuf)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (i == 0) {
      memset(secBuf, 0, BYTES_PER_SECTOR);
    }
  }
  writeMsg( "\r\n");
  
  //==================================================================
  writeMsg( "Write cluster two, bitmap\n");

  // Write cluster two, bitmap.
  sector = partitionOffset + clusterHeapOffset;
  // The 7)/8 converts clusterCount to bytes rounded up to whole bytes.
  bitmapSize = (clusterCount + 7)/8;  
  ns = (bitmapSize + BYTES_PER_SECTOR - 1)/BYTES_PER_SECTOR;
#if defined(DBG_PRINT)
  DBGPrintf("\tWriting Sector: %d\n", sector-partitionOffset);
  DBGPrintf("sectorsPerCluster: %d, bitmapSize: %d, ns: %d\n", sectorsPerCluster, bitmapSize, ns);
#endif
  if (ns > sectorsPerCluster) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  memset(secBuf, 0, BYTES_PER_SECTOR);
  // Allocate clusters for bitmap, upcase, and root.
  secBuf[0] = 0X7;
  for (uint32_t i = 1; i < ns; i++) {
    if (!writeSector(dev, sector + i, secBuf)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (i == 0) {
      secBuf[0] = 0;
    }
  }
  
 
  // Write cluster three, upcase table.
  writeMsg( "Writing upcase table\r\n");
#if defined(DBG_PRINT)
  DBGPrintf("\tWriting Sector: %d\n", clusterHeapOffset + sectorsPerCluster);
#endif
  if (!writeUpcase(dev, partitionOffset + clusterHeapOffset + sectorsPerCluster)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (m_upcaseSize > BYTES_PER_SECTOR*sectorsPerCluster) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  
  // Initialize first sector of root.
  writeMsg( "Writing root\r\n");
  ns = sectorsPerCluster;
  sector = partitionOffset + clusterHeapOffset + 2*sectorsPerCluster;
#if defined(DBG_PRINT)
  DBGPrintf("\tWriting 1st Sector of root: %d\n", clusterHeapOffset + 2*sectorsPerCluster);
#endif
  memset(secBuf, 0, BYTES_PER_SECTOR);

  // Unused Label entry.
  label = reinterpret_cast<DirLabel_t*>(secBuf);
  label->type = EXFAT_TYPE_LABEL & 0X7F;

  // bitmap directory  entry.
  dbm = reinterpret_cast<DirBitmap_t*>(secBuf + 32);
  dbm->type = EXFAT_TYPE_BITMAP;
  setLe32(dbm->firstCluster, BITMAP_CLUSTER);
  setLe64(dbm->size, bitmapSize);

  // upcase directory entry.
  dup = reinterpret_cast<DirUpcase_t*>(secBuf +64);
  dup->type = EXFAT_TYPE_UPCASE;
  setLe32(dup->checksum, m_upcaseChecksum);
  setLe32(dup->firstCluster, UPCASE_CLUSTER);
  setLe64(dup->size, m_upcaseSize);

  // Write root, cluster four.
  for (uint32_t i = 0; i < ns; i++) {
    if (!writeSector(dev, sector + i, secBuf)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (i == 0) {
      memset(secBuf, 0, BYTES_PER_SECTOR);
    }
  }
  writeMsg( "Format done\r\n");
  // what happens if I tell the partion to begin again?
  //partVol.begin(dev, true, m_part+1);  // need to 1 bias again...
  //DBGPrintf("free clusters after begin on partVol: %u\n", partVol.freeClusterCount());
  if (has_volume_label) {
    writeMsg( "Set Volume Label\n");
    fs.mscfs.setVolumeLabel(volName);
  }
  dev.syncDevice();
  setWriteSandBox(0, 0xffffffff);
  return true;

 fail:
  writeMsg( "Format failed\r\n");
  setWriteSandBox(0, 0xffffffff);
  return false;
}

//------------------------------------------------------------------------------
bool USBFilesystemFormatter::writeExFatMbr(USBDrive &m_dev) {
  // make Master Boot Record.  Use fake CHS.
  if (m_mbrLBA == 0xFFFFFFFFUL) {
    DBGPrintf("    writeMBR - GPT entry so dont update\n");
    return true;
  }
  memset(m_secBuf, 0, BYTES_PER_SECTOR);

  // Need to handle EXT wmere MBR may be EXtended Boot record
  MbrSector_t* mbr = reinterpret_cast<MbrSector_t*>(m_secBuf);
  MbrPart_t *pt = &mbr->part[m_mbrPart];
  
  if (!m_dev.readSector(m_mbrLBA, m_secBuf)) writeMsg("DIDN't GET SECTOR BUFFER");

  
  pt->beginCHS[0] = 0x20;
  pt->beginCHS[1] = 0x21;
  pt->beginCHS[2] = 0;
  pt->type = 7;
  pt->endCHS[0] = 0XFE;
  pt->endCHS[1] = 0XFF;
  pt->endCHS[2] = 0XFF;
  setLe32(pt->relativeSectors, partitionOffset - m_mbrLBA);  // should be relative to the start...
  setLe32(pt->totalSectors, volumeLength);
  setLe16(mbr->signature, MBR_SIGNATURE);
  //DBGPrintf("    m_relativeSectors:%u\n", getLe32(pt->relativeSectors));
  //DBGPrintf("    m_totalSectors:%u\n", getLe32(pt->totalSectors));
  
  return writeSector(m_dev, m_mbrLBA, m_secBuf);

}

//------------------------------------------------------------------------------
bool USBFilesystemFormatter::syncUpcase(USBDrive &m_dev) {
  uint16_t index = m_upcaseSize & SECTOR_MASK;
  if (!index) {
    return true;
  }
  for (size_t i = index; i < BYTES_PER_SECTOR; i++) {
    m_secBuf[i] = 0;
  }
  return writeSector(m_dev, m_upcaseSector, m_secBuf);
}

//------------------------------------------------------------------------------
bool USBFilesystemFormatter::writeUpcaseByte(USBDrive &m_dev, uint8_t b) {
  uint16_t index = m_upcaseSize & SECTOR_MASK;
  m_secBuf[index] = b;
  m_upcaseChecksum = exFatChecksum(m_upcaseChecksum, b);
  m_upcaseSize++;
  if (index == SECTOR_MASK) {
    return writeSector(m_dev, m_upcaseSector++, m_secBuf);
  }
  return true;
}
//------------------------------------------------------------------------------
bool USBFilesystemFormatter::writeUpcaseUnicode(USBDrive &m_dev, uint16_t unicode) {
  return writeUpcaseByte(m_dev, unicode) && writeUpcaseByte(m_dev, unicode >> 8);
}
//------------------------------------------------------------------------------
bool USBFilesystemFormatter::writeUpcase(USBDrive &m_dev, uint32_t sector) {
  uint32_t n;
  uint32_t ns;
  uint32_t ch = 0;
  uint16_t uc;

  m_upcaseSize = 0;
  m_upcaseChecksum = 0;
  m_upcaseSector = sector;

  while (ch < 0X10000) {
    uc = toUpcase(ch);
    if (uc != ch) {
      if (!writeUpcaseUnicode(m_dev, uc)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      ch++;
    } else {
      for (n = ch + 1; n < 0X10000 && n == toUpcase(n); n++) {}
      ns = n - ch;
      if (ns >= MINIMUM_UPCASE_SKIP) {
        if (!writeUpcaseUnicode(m_dev, 0XFFFF) || !writeUpcaseUnicode(m_dev, ns)) {
          DBG_FAIL_MACRO;
          goto fail;
        }
        ch = n;
      } else {
        while (ch < n) {
          if (!writeUpcaseUnicode(m_dev, ch++)) {
            DBG_FAIL_MACRO;
            goto fail;
          }
        }
      }
    }
  }
  if (!syncUpcase(m_dev)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return true;

 fail:
  return false;
}
