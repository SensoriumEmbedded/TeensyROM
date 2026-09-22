// SPDX-License-Identifier: MIT
#pragma once

// Whether a storage mutation changes what the loaded directory listing shows,
// and whether that listing is one the firmware can rebuild at all. The remote
// file commands ask before marking the listing stale and the menu asks before
// acting on the mark, so both sides read the same answer.
namespace LoadedListing {

// The remote protocol's storage byte: zero is the USB drive, anything else SD.
static FLASHMEM uint8_t storageDevice(uint32_t storageType)
{
   return storageType ? rmtSD : rmtUSBDrive;
}

// DriveDirPath carries a trailing '*' for a mounted disk image, whose contents
// are entries inside a file rather than a directory the drive can re-read.
static FLASHMEM bool reloadable(uint8_t loadedDevice, const char *loadedDirectory)
{
   if (loadedDevice != rmtSD && loadedDevice != rmtUSBDrive) return false;
   if (!loadedDirectory || loadedDirectory[0] != '/') return false;

   return loadedDirectory[strlen(loadedDirectory) - 1] != '*';
}

static FLASHMEM bool insideDirectory(const char *path, const char *directory)
{
   const size_t depth = strlen(directory);

   if (strncasecmp(path, directory, depth) != 0) return false;
   if (directory[depth - 1] == '/') return path[depth] != 0;

   return path[depth] == '/' && path[depth + 1] != 0;
}

static FLASHMEM bool invalidatedBy(const char *path, uint8_t device,
                                   uint8_t loadedDevice, const char *loadedDirectory)
{
   if (!path || path[0] != '/' || device != loadedDevice) return false;
   if (!reloadable(loadedDevice, loadedDirectory)) return false;

   return insideDirectory(path, loadedDirectory);
}

}
