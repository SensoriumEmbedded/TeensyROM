// A GEOS menu item: finds the user's boot disk image in the launcher's folder on the SD card and puts
// the TR input driver on a copy of it, leaving the original untouched.

#include "GEOS/GeosTargets.h"

#define GEOS_TEMP_NAME     "PATCHING.TMP"
#define GEOS_COPY_TAG      "-TR"
#define GEOS_EXT_LENGTH    4
#define GEOS_MAX_IMAGES    8

static const char *const GeosImageExtensions[] = { ".d64", ".d71", ".d81" };

FLASHMEM static bool GeosFileRead(void *Context, uint32_t Offset, uint8_t *Block)
{
   File *Image = (File *)Context;
   return Image->seek(Offset) && Image->read(Block, GEOS_SECTOR_SIZE) == GEOS_SECTOR_SIZE;
}

FLASHMEM static bool GeosFileWrite(void *Context, uint32_t Offset, const uint8_t *Block)
{
   File *Image = (File *)Context;
   return Image->seek(Offset) && Image->write(Block, GEOS_SECTOR_SIZE) == GEOS_SECTOR_SIZE;
}

FLASHMEM static struct GeosDisk GeosDiskOn(const struct GeosLauncher *Launcher, File *Image)
{
   struct GeosDisk Disk = {};
   uint64_t Size = Image->size();

   Disk.Format = Size <= UINT32_MAX ? GeosFormatForSize(Size) : NULL;
   Disk.Target = &Launcher->Target;
   Disk.Context = Image;
   Disk.Read = GeosFileRead;
   Disk.Write = GeosFileWrite;
   return Disk;
}

FLASHMEM static bool GeosHasImageExtension(const char *Name)
{
   size_t Length = strlen(Name);

   if (Length <= GEOS_EXT_LENGTH) return false;
   for (const char *Extension : GeosImageExtensions)
   {
      if (!strcasecmp(Name + Length - GEOS_EXT_LENGTH, Extension)) return true;
   }
   return false;
}

FLASHMEM static bool GeosIsPatchedCopy(const char *Name)
{
   size_t Base = strlen(Name) - GEOS_EXT_LENGTH, TagLength = strlen(GEOS_COPY_TAG);
   return Base > TagLength && !strncasecmp(Name + Base - TagLength, GEOS_COPY_TAG, TagLength);
}

FLASHMEM static void GeosPath(char *Path, size_t Size, const struct GeosLauncher *Launcher, const char *Name)
{
   snprintf(Path, Size, "%s/%s", Launcher->Folder, Name);
}

FLASHMEM static void GeosCopyPath(char *Path, size_t Size, const struct GeosLauncher *Launcher, const char *Name)
{
   int Base = strlen(Name) - GEOS_EXT_LENGTH;
   snprintf(Path, Size, "%s/%.*s" GEOS_COPY_TAG "%s", Launcher->Folder, Base, Name, Name + Base);
}

// The originals in the folder, listed before anything is written to it
FLASHMEM static uint8_t GeosListImages(const struct GeosLauncher *Launcher, char Names[][MaxItemNameLength])
{
   File Dir = SD.open(Launcher->Folder);
   uint8_t Count = 0;

   if (!Dir) return 0;
   for (File Entry = Dir.openNextFile(); Entry && Count < GEOS_MAX_IMAGES; Entry = Dir.openNextFile())
   {
      const char *Name = Entry.name();
      if (Entry.isDirectory() || !GeosHasImageExtension(Name) || GeosIsPatchedCopy(Name)) continue;
      if (strlen(Name) >= MaxItemNameLength) continue;
      strcpy(Names[Count++], Name);
   }
   Dir.close();
   return Count;
}

FLASHMEM static bool GeosInspectFile(const struct GeosLauncher *Launcher, const char *Path, struct GeosBootDisk *Found,
                                     char *Error, size_t ErrorSize)
{
   File Image = SD.open(Path, FILE_READ);
   struct GeosDisk Disk;
   bool Inspected;

   if (!Image) return false;
   Disk = GeosDiskOn(Launcher, &Image);
   Inspected = GeosInspect(&Disk, Found);
   if (!Inspected && Error) snprintf(Error, ErrorSize, "%s", Disk.Error);
   Image.close();
   return Inspected;
}

FLASHMEM static bool GeosCopyFile(const char *From, const char *To)
{
   File Source = SD.open(From, FILE_READ), Target;
   uint8_t Buffer[GEOS_SECTOR_SIZE * 4];
   bool Copied = true;
   int Length;

   SD.remove(To);
   Target = SD.open(To, FILE_WRITE);
   if (!Source || !Target) Copied = false;
   while (Copied && (Length = Source.read(Buffer, sizeof Buffer)) > 0)
   {
      Copied = Target.write(Buffer, Length) == (size_t)Length;
   }
   Source.close();
   Target.close();
   return Copied;
}

FLASHMEM static struct GeosDate GeosToday()
{
   return (struct GeosDate){ (uint8_t)(year() % 100), (uint8_t)month(), (uint8_t)day(), (uint8_t)hour(), (uint8_t)minute() };
}

FLASHMEM static bool GeosPatchFile(const struct GeosLauncher *Launcher, const char *Path, char *Error, size_t ErrorSize)
{
   File Image = SD.open(Path, FILE_WRITE_BEGIN);
   struct GeosDisk Disk;
   struct GeosDate Today = GeosToday();
   bool Patched;

   if (!Image) return false;
   Disk = GeosDiskOn(Launcher, &Image);
   Patched = GeosInstallDriver(&Disk, &Today);
   if (!Patched) snprintf(Error, ErrorSize, "%s", Disk.Error);
   Image.close();
   return Patched;
}

// Patches a temporary copy and renames it into place, so a failure never leaves a half-patched copy behind
FLASHMEM static bool GeosPatchCopy(const struct GeosLauncher *Launcher, const char *Source, const char *Copy)
{
   char Temp[MaxNamePathLength], Error[GEOS_ERROR_SIZE] = "the SD card could not be written";

   GeosPath(Temp, sizeof Temp, Launcher, GEOS_TEMP_NAME);
   if (!GeosCopyFile(Source, Temp) || !GeosPatchFile(Launcher, Temp, Error, sizeof Error) ||
       (SD.exists(Copy) && !SD.remove(Copy)) || !SD.rename(Temp, Copy))
   {
      SD.remove(Temp);
      SendMsgPrintfln("Could not add the mouse driver:\r\n%s", Error);
      return false;
   }
   return true;
}

// Whether the image is a boot disk the TR can now start, saying what it found
FLASHMEM static bool GeosSetUpImage(const struct GeosLauncher *Launcher, const char *Name)
{
   char Source[MaxNamePathLength], Copy[MaxNamePathLength], Error[GEOS_ERROR_SIZE];
   struct GeosBootDisk Found;

   GeosPath(Source, sizeof Source, Launcher, Name);
   GeosCopyPath(Copy, sizeof Copy, Launcher, Name);
   if (!GeosInspectFile(Launcher, Source, &Found, Error, sizeof Error))
   {
      SendMsgPrintfln("%s:\r\n%s", Name, Error);
      return false;
   }
   if (!Found.HasDeskTop) return false;

   SendMsgPrintfln("%s boot disk:\r\n%s", Launcher->Title, Name);
   if (Found.HasDriver) return true;
   if (GeosInspectFile(Launcher, Copy, &Found, NULL, 0) && Found.HasDriver) return true;
   if (!GeosPatchCopy(Launcher, Source, Copy)) return false;
   SendMsgPrintfln("Added the TR USB mouse driver to\r\n%s", Copy);
   return true;
}

FLASHMEM void LaunchGeos(const struct GeosLauncher *Launcher)
{
   char Names[GEOS_MAX_IMAGES][MaxItemNameLength];
   uint8_t Count, Ready = 0;

   if (!SDFullInit())
   {
      SendMsgPrintfln("%s starts from your boot disk\r\nimage on the SD card. Insert the card.", Launcher->Title);
      return;
   }
   Count = GeosListImages(Launcher, Names);
   for (uint8_t i = 0; i < Count; i++) Ready += GeosSetUpImage(Launcher, Names[i]);
   if (Ready) return;
   SendMsgPrintfln("Copy your %s boot disk image\r\n(.D64, .D71 or .D81) to %s\r\non the SD card, then select this again.",
                   Launcher->Title, Launcher->Folder);
}
