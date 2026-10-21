// A GEOS menu item: finds the user's boot disk image in the launcher's folder on the SD card and puts
// the TR input driver on a copy of it, leaving the original untouched.

#include "GEOS/GeosBoot.h"
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

FLASHMEM static void GeosCopyName(char *CopyName, size_t Size, const char *Name)
{
   int Base = strlen(Name) - GEOS_EXT_LENGTH;
   snprintf(CopyName, Size, "%.*s" GEOS_COPY_TAG "%s", Base, Name, Name + Base);
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
      if (Entry.isDirectory() || IsDotfile(Name) || !GeosHasImageExtension(Name) || GeosIsPatchedCopy(Name)) continue;
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

   if (!Image)
   {
      if (Error) snprintf(Error, ErrorSize, "the image could not be opened");
      return false;
   }
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
   uint64_t Total = 0;
   bool Copied = true;
   int Length;

   SD.remove(To);
   Target = SD.open(To, FILE_WRITE);
   if (!Source || !Target) Copied = false;
   while (Copied && (Length = Source.read(Buffer, sizeof Buffer)) > 0)
   {
      Copied = Target.write(Buffer, Length) == (size_t)Length;
      Total += Length;
   }
   Copied = Copied && Total == Source.size();
   Source.close();
   Target.close();
   return Copied;
}

// TimeLib's year() and friends run from boot with no sync provider set, so the RTC is read directly
FLASHMEM static struct GeosDate GeosToday()
{
   int8_t Timezone = (int8_t)IO1[rwRegTimezone];
   tmElements_t Now;

   breakTime(Teensy3Clock.get() + (int16_t)Timezone * 30 * 60, Now);
   return (struct GeosDate){ (uint8_t)(tmYearToCalendar(Now.Year) % 100), Now.Month, Now.Day, Now.Hour, Now.Minute };
}

FLASHMEM static bool GeosPatchFile(const struct GeosLauncher *Launcher, const char *Path, char *Error, size_t ErrorSize)
{
   File Image = SD.open(Path, FILE_WRITE_BEGIN);
   struct GeosDisk Disk;
   struct GeosDate Today = GeosToday();
   bool Patched;

   if (!Image)
   {
      snprintf(Error, ErrorSize, "the image could not be opened");
      return false;
   }
   Disk = GeosDiskOn(Launcher, &Image);
   Patched = GeosInstallDriver(&Disk, &Today);
   if (!Patched) snprintf(Error, ErrorSize, "%s", Disk.Error);
   Image.close();
   return Patched;
}

// Patches a temporary copy and renames it into place, so a failure never leaves a half-patched copy behind
FLASHMEM static bool GeosPatchCopy(const struct GeosLauncher *Launcher, const char *Name, const char *Source,
                                   const char *Copy, bool Patch)
{
   char Temp[MaxNamePathLength], Error[GEOS_ERROR_SIZE] = "the SD card could not be written";

   GeosPath(Temp, sizeof Temp, Launcher, GEOS_TEMP_NAME);
   if (!GeosCopyFile(Source, Temp) || (Patch && !GeosPatchFile(Launcher, Temp, Error, sizeof Error)) ||
       (SD.exists(Copy) && !SD.remove(Copy)) || !SD.rename(Temp, Copy))
   {
      SD.remove(Temp);
      SendMsgPrintfln("Could not add the mouse driver to\r\na copy of %s:\r\n%s", Name, Error);
      return false;
   }
   return true;
}

FLASHMEM static bool GeosReady(const struct GeosLauncher *Launcher, const char *Name)
{
   SendMsgPrintfln("%s boot disk:\r\n%s", Launcher->Title, Name);
   return true;
}

FLASHMEM static bool GeosUse(const struct GeosLauncher *Launcher, const char *Name, char *BootName)
{
   strcpy(BootName, Name);
   return GeosReady(Launcher, Name);
}

// Whether the image is a boot disk, set up or not: only with none does the user need telling where to copy one.
// BootName gets the disk to start from: the original when it is set up and only the setup is wanted, else a copy,
// since GEOS saves to the disk it starts from
FLASHMEM static bool GeosSetUpImage(const struct GeosLauncher *Launcher, const char *Name, char *BootName)
{
   char Source[MaxNamePathLength], CopyName[MaxItemNameLength + sizeof GEOS_COPY_TAG], Copy[MaxNamePathLength];
   char Error[GEOS_ERROR_SIZE];
   struct GeosBootDisk Found;
   bool NeedsDriver;

   GeosPath(Source, sizeof Source, Launcher, Name);
   GeosCopyName(CopyName, sizeof CopyName, Name);
   GeosPath(Copy, sizeof Copy, Launcher, CopyName);
   if (!GeosInspectFile(Launcher, Source, &Found, Error, sizeof Error))
   {
      SendMsgPrintfln("%s:\r\n%s", Name, Error);
      return false;
   }
   if (!Found.HasDeskTop) return false;
   if (Found.HasDriver && !GeosBoots(Launcher)) return GeosUse(Launcher, Name, BootName);
   NeedsDriver = !Found.HasDriver;

   if (SD.exists(Copy))
   {
      // The copy is the disk GEOS saves the user's work to: one that cannot be read is left alone, not replaced
      if (!GeosInspectFile(Launcher, Copy, &Found, Error, sizeof Error))
      {
         SendMsgPrintfln("%s:\r\n%s", CopyName, Error);
         return true;
      }
      if (Found.HasDriver) return GeosUse(Launcher, CopyName, BootName);
      if (Found.HasDeskTop)
      {
         if (!GeosPatchFile(Launcher, Copy, Error, sizeof Error))
         {
            SendMsgPrintfln("Could not add the mouse driver to\r\n%s:\r\n%s", CopyName, Error);
            return true;
         }
         return GeosUse(Launcher, CopyName, BootName);
      }
   }
   if (!GeosPatchCopy(Launcher, Name, Source, Copy, NeedsDriver)) return true;
   SendMsgPrintfln(NeedsDriver ? "Added the TR USB mouse driver to\r\na copy of %s" : "Made a copy of %s\r\nto start from", Name);
   return GeosUse(Launcher, CopyName, BootName);
}

FLASHMEM static bool GeosStart(const struct GeosLauncher *Launcher, const char *Name, StructMenuItem *Item)
{
   char Path[MaxNamePathLength];
   struct GeosDisk Disk;
   uint32_t Length = 0;
   struct GeosDate Now = GeosToday();
   bool Changed, Built;
   File Image;

   GeosPath(Path, sizeof Path, Launcher, Name);
   Image = SD.open(Path, FILE_WRITE_BEGIN);
   if (!Image)
   {
      SendMsgPrintfln("%s\r\ncould not be opened", Name);
      return false;
   }
   Disk = GeosDiskOn(Launcher, &Image);
   Built = GeosBuildBootPrg(&Disk, RAM_Image, RAM_ImageSize, &Length) && GeosDisableConfigure(&Disk, &Changed);
   if (Built) GeosSetClock(RAM_Image, Length, &Now);
   if (!Built) SendMsgPrintfln("%s:\r\n%s", Name, Disk.Error);
   Image.close();
   if (!Built) return false;
   if (!TRBlockMount(Path))
   {
      SendMsgPrintfln("%s\r\ncould not be mounted", Name);
      return false;
   }
   Item->ItemType = rtFilePrg;
   Item->Code_Image = RAM_Image;
   Item->Size = Length;
   return true;
}

FLASHMEM bool LaunchGeos(const struct GeosLauncher *Launcher, StructMenuItem *Item)
{
   char Names[GEOS_MAX_IMAGES][MaxItemNameLength], BootName[MaxItemNameLength + sizeof GEOS_COPY_TAG] = "";
   uint8_t Count;
   bool BootDiskFound = false;

   if (!SDFullInit())
   {
      SendMsgPrintfln("%s starts from your boot disk\r\nimage on the SD card. Insert the card.", Launcher->Title);
      return false;
   }
   Count = GeosListImages(Launcher, Names);
   for (uint8_t i = 0; i < Count && !(GeosBoots(Launcher) && *BootName); i++) BootDiskFound |= GeosSetUpImage(Launcher, Names[i], BootName);
   if (!BootDiskFound)
   {
      SendMsgPrintfln("Copy your %s boot disk image\r\n(.D64, .D71 or .D81) to %s\r\non the SD card, then select this again.",
                      Launcher->Title, Launcher->Folder);
      return false;
   }
   return GeosBoots(Launcher) && *BootName && GeosStart(Launcher, BootName, Item);
}
