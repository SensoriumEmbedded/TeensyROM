#ifndef USBFormatter
#define USBFormatter

#include "../USBHost_t36.h"  // TeensyROM: was "USBHost_t36.h", found only via the library's own include path

class USBFilesystemFormatter
{
public:
  bool format(USBFilesystem &fs, uint8_t fat_type, uint8_t* secBuf, print_t* pr);
  bool formatFAT(USBDrive &dev, USBFilesystem &fs, uint8_t part, uint8_t fat_type, uint8_t* secBuf, print_t* pr);
  bool formatExFAT(USBDrive &dev, USBFilesystem &fs, uint8_t part, uint8_t fat_type, uint8_t* secBuf, print_t* pr);

private:
  bool makeFat16(USBDrive &m_dev);
  bool makeFat32(USBDrive &m_dev);
  bool writeFatMbr(USBDrive &m_dev);
  bool initFatDir(USBDrive &m_dev, uint8_t fatType, uint32_t sectorCount);
  void initPbs();
  void setWriteSandBox(uint32_t min_sector, uint32_t max_sector) {
    m_minSector = min_sector;
    m_maxSector = max_sector;
  }
  bool writeSector(USBDrive &m_dev, uint32_t sector, const uint8_t* src);

  bool writeExFatMbr(USBDrive &m_dev);
  bool syncUpcase(USBDrive &m_dev);
  bool writeUpcaseByte(USBDrive &m_dev, uint8_t b);
  bool writeUpcase(USBDrive &m_dev, uint32_t sector);
  bool writeUpcaseUnicode(USBDrive &m_dev, uint16_t unicode);
    
  uint32_t m_capacityMB;
  uint32_t m_dataStart;
  uint32_t m_fatSize;
  uint32_t m_fatStart;
  uint32_t m_relativeSectors;
  uint32_t m_sectorCount;
  uint32_t m_totalSectors;
  print_t* m_pr;
  uint8_t* m_secBuf;
  uint16_t m_reservedSectorCount;
  uint8_t m_partType;
  uint8_t m_sectorsPerCluster;
  uint8_t m_part;
  uint32_t m_mbrLBA = 0;
  uint8_t m_mbrPart;
  uint32_t m_part_relativeSectors;
  uint32_t m_minSector = 0;
  uint32_t m_maxSector = (uint32_t)-1;

  uint32_t volumeLength;
  uint32_t partitionOffset;
  uint32_t m_upcaseSector;
  uint32_t m_upcaseChecksum;
  uint32_t m_upcaseSize;
  uint32_t bitmapSize;

};
#endif  // USBFormatter_h
