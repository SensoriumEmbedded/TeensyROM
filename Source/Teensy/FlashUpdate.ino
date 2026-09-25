
// Adapted from:

//******************************************************************************
// FlasherX -- firmware OTA update via Intel Hex file over serial or SD stream
// https://github.com/joepasquariello/FlasherX
//******************************************************************************
//
// Based on Flasher3 (Teensy 3.x) and Flasher4 (Teensy 4.x) by Jon Zeeff
//
// Jon Zeeff 2016, 2019, 2020 This code is in the public domain.
// Please retain my name in distributed copies, and let me know about any bugs
//
// I, Jon Zeeff, give no warranty, expressed or implied for this software and/or
// documentation provided, including, without limitation, warranty of
// merchantability and fitness for a particular purpose.
//
// WARNING: You can brick your Teensy with incorrect flash erase/write, such as
// incorrect flash config (0x400-40F). This code may or may not prevent that.

// 10/09/22 (v2.3) JWP - option for reading hex file from serial or SD
//   - move hex file support functions to new file FXUtil.cpp
//   - update_firmware() now takes two Stream* arguments ("in" and "out")
//   - FlasherX.ino lets user choose between hex file via serial or SD
// 09/01/22 (v2.2) JWP - change FlashTxx from CPP to C file
//   - rename FlashTxx.cpp to FlashTxx.c (resolve link error when calling from C)
//   - FlasherX.ino place #include "FlashTxx.h" inside extern "C" block
// 01/07/22 (v2.1) JWP - use TD 1.56 core functions for T4x wait/write/erase
//   - FlashTxx.h update FLASH_SIZE for Teensy Micromod from 8 to 16 MB
//   - option to artificially increase code size via const array (in flash)
// 11/18/21 JWP - bug fix in file FlashTXX.cpp
//   - fix logic in while loop in flash_block_write() in FlashTXX
// 10/27/21 JWP - add support for Teensy Micromod
//   - define macros for TEENSY_MICROMOD w/ same values as for TEENSY40
//   - update FLASH_SIZE for T4.1 and TMM from 2MB to 8MB
// JWP - merge of Flasher3/4 and new features
//   - FLASH buffer dynamically sized from top of existing code to FLASH_RESERVE
//   - optional RAM buffer option for T4.x via macro RAM_BUFFER_SIZE > 0
//   - Stream* (USB or UART) and buffer addr/size set at run-time
//   - incorporate Frank Boesing's FlashKinetis routines for T3.x
//   - add support for Teensy 4.1 and Teensy LC
//    This code is released into the public domain.
// JWP - Joe Pasquariello - modifications for T3.5 and T3.6 in Dec 2020
//    This code is released into the public domain
// Deb Hollenback at GiftCoder -- Modifications for teensy 3.5/3/6
//    This code is released into the public domain.
//    see https://forum.pjrc.com/threads/43165-Over-the-Air-firmware-updates-changes-for-flashing-Teensy-3-5-amp-3-6
// Jon Zeeff modifications
//    see https://forum.pjrc.com/threads/29607-Over-the-air-updates
// Original by Niels A. Moseley, 2015.
//    This code is released into the public domain.
//    https://namoseley.wordpress.com/2015/02/04/freescale-kinetis-mk20dx-series-flash-erasing/


#define FLASH_RESERVE_STOCK (0x40000) // 256k reserved space at top of flash 
#ifdef VM_EXTENSIONS_ENABLED
   // The extension host slot (VMHostABI.h) sits directly below those 256k. The updater
   // stages into and erases everything under FLASH_RESERVE, so the slot is reserved too,
   // or an update would erase the installed host. 640k in all; checked below.
   #define FLASH_RESERVE     (FLASH_RESERVE_STOCK + VM_HOST_SLOT_BYTES)
#else
   #define FLASH_RESERVE     FLASH_RESERVE_STOCK
#endif
#ifdef Fab04_Features
   #define FLASH_ID         "fw_t41_teensyromplus_sensorium" // target ID to match, must be a unique to previous   
#else
   #define FLASH_ID         "fw_t41_teensyrom_sensorium_v3" // target ID to match, must be a superset of previous #define FLASH_ID         "fw_t41_teensyrom_sensorium_v3" // target ID to match, must be a superset of previous
   #define FLASH_ID_ORIG    "fw_t41_teensyrom_sensorium" // target ID to match for old fab 0.2x FW
#endif

#include <SD.h>
#include "Flash/FXUtil.h"		// read_ascii_line(), hex file support
#include "Flash/FXUtil.cpp"
extern "C" {
  #include "Flash/FlashTxx.h"		// TLC/T3x/T4x/TMM flash primitives
  #include "Flash/FlashTxx.c"
}

#ifdef VM_EXTENSIONS_ENABLED
   // FLASH_RESERVE covers the slot only if the slot ends exactly where the stock 256k
   // begins. Moving the slot without that fails here, rather than leaving an updater that
   // erases the installed host or a slot overlapping the EEPROM emulation.
   static_assert(FLASH_BASE_ADDR + FLASH_SIZE - FLASH_RESERVE_STOCK == VM_HOST_SLOT_LIMIT,
                 "the extension host slot must end where the stock flash reserve begins");
#endif


void DoFlashUpdate(FS *sourceFS, const char *FilePathName)
{
   uint32_t buffer_addr, buffer_size;

   //Serial.printf( "target = %s (%dK flash in %dK sectors)\n", FLASH_ID, FLASH_SIZE/1024, FLASH_SECTOR_SIZE/1024);
   
   // create flash buffer to hold new firmware
   SendMsgPrintfln("Create buffer ");
   if (firmware_buffer_init( &buffer_addr, &buffer_size ) != FLASH_BUFFER_TYPE) 
   {
     SendMsgFailed();
     return;
   }
   SendMsgOK();
   
   SendMsgPrintfln("%s Buffer = %1luK of %1dK total\r\n(%08lX - %08lX)", 
      IN_FLASH(buffer_addr) ? "Flash" : "RAM", buffer_size/1024, FLASH_SIZE/1024, 
      buffer_addr, buffer_addr + buffer_size);
  
   //Already initialized to get to this point...
   //SendMsgPrintfln( "SD initialization " );
   //if (!SD.begin( BUILTIN_SDCARD )) 
   //{
   //   SendMsgFailed();
   //   return;
   //}
   //SendMsgOK();

   SendMsgPrintfln("Open: %s%s ", sourceFS==&SD ? "SD" : "USB", FilePathName); 

   File hexFile = sourceFS->open(FilePathName, FILE_READ );
      
   if (!hexFile) {
      SendMsgFailed();
      return;
   }
   SendMsgOK();
   
   // read hex file, write new firmware to flash, clean up, reboot
   update_firmware( &hexFile, &Serial, buffer_addr, buffer_size );
  
   // return from update_firmware() means error or user abort, so clean up and
   // reboot to ensure that static vars get boot-up initialized before retry(? nah)
   SendMsgPrintfln( "Erasing Flash buffer ");  
   firmware_buffer_free( buffer_addr, buffer_size );
   SendMsgOK();
   
   //SendMsgPrintfln( "Rebooting  Teensy");  
   //REBOOT;
}

bool isFab2x()
{
   // Determines if this is a fab 0.2x PCB by reading the Dot_Clock input
   //  (only connectedon fab 0.2, debug on fab 0.3)
   
   uint32_t Highs = 0, Lows = 0;
   
   pinMode(DotClk_Debug_PIN, INPUT_PULLUP);  //p28 is Dot_Clk input (unused) on fab 0.2x
   
   uint32_t StartmS = millis();
   while(millis()-StartmS<100)
   {
      if (ReadDotClkDebug) Highs++;
      else Lows++;
   }
   
   Printf_dbg("\nDotClk/Debug pin: %lu Highs, %lu Lows\n", Highs, Lows); 
   //fab 0.3:  4121505 to 4153250 Highs, 0 Lows
   //fab 0.2x: 2230311 to 2296610 Highs, 1990311-2052164 Lows
   return(Highs>1000000 && Lows>1000000);
}

#ifdef VM_EXTENSIONS_ENABLED
// Writing an extension host into the slot from a .TRH file on SD or USB.
// VMHostInstall.h decides what happens in what order and is covered natively
// by vm/tests/host_install_test.cpp; everything here is the device behind it.

// The core's FlexSPI primitives hold interrupts off for one operation and
// re-enable them on the way out, so nothing here may assume they stay off
// across a call.
struct VmSlotFlash
{
   bool erase(uint32_t sector)
   {
      if (sector >= VM_HOST_SECTORS) return false;

      void *addr = (void *)(VM_HOST_SLOT_BASE + sector * VM_HOST_SECTOR_BYTES);
      eepromemu_flash_erase_sector(addr);
      arm_dcache_delete(addr, VM_HOST_SECTOR_BYTES);
      return flash_sector_not_erased((uint32_t)addr) == 0;
   }

   bool program(uint32_t offset, const uint8_t *data, uint32_t n)
   {
      if (offset + n > VM_HOST_SLOT_BYTES) return false;

      eepromemu_flash_write((void *)(VM_HOST_SLOT_BASE + offset), data, n);
      return memcmp(map(offset), data, n) == 0;
   }

   // The slot is XIP and cacheable, so a read-back has to come from the part.
   const uint8_t *map(uint32_t offset)
   {
      void *sector = (void *)(VM_HOST_SLOT_BASE + (offset & ~(VM_HOST_SECTOR_BYTES - 1)));
      arm_dcache_delete(sector, VM_HOST_SECTOR_BYTES);
      return (const uint8_t *)(VM_HOST_SLOT_BASE + offset);
   }
};

struct VmTrhFile
{
   File file;

   bool seek(uint32_t offset) { return file.seek(offset); }
   bool read(void *dst, uint32_t n) { return file.read(dst, n) == (int)n; }
};

static const char *HostInstallWhy(VmInstallStatus status)
{
   switch (status)
   {
      case VmInstallStatus::Ok:             return "ok";
      case VmInstallStatus::ShortFile:      return "file too short";
      case VmInstallStatus::BadMagic:       return "not a TRH package";
      case VmInstallStatus::BadFormat:      return "package format unsupported";
      case VmInstallStatus::BadHeader:      return "package header bad";
      case VmInstallStatus::BadHeaderCrc:   return "package header CRC bad";
      case VmInstallStatus::WrongSlot:      return "package targets elsewhere";
      case VmInstallStatus::BadLength:      return "package length wrong";
      case VmInstallStatus::ReadError:      return "read error";
      case VmInstallStatus::BadPayloadCrc:  return "payload CRC bad";
      case VmInstallStatus::MirrorMismatch: return "header disagrees w/ image";
      case VmInstallStatus::WrongAbi:       return "built for another ABI";
      case VmInstallStatus::NotBootable:    return "image would not start";
      case VmInstallStatus::EraseFailed:    return "erase failed";
      case VmInstallStatus::ProgramFailed:  return "write failed";
      case VmInstallStatus::VerifyFailed:   return "verify failed";
   }
   return "refused";
}

// The install ends in a reboot either way, so its outcome travels in the
// VmFail record the main image collects on the way back up.
// The C64 runs from cartridge ROM served by isrPHI2, and a sector erase stalls this
// core for up to 400 mS with interrupts off. Stop the 6510 first, then stop answering
// it. Both the install and the removal erase, so both come through here.
// MessageSeen is what the caller's warning returned: true only if the C64 actually read
// it. False means nothing was drawn, so there is nothing on screen to give time for.
static void StopServingTheC64(bool MessageSeen)
{
   // Blank the screen while the bus is still ours to drive. DEN=0 stops VIC-II fetches,
   // so the frozen menu does not sit on screen for the whole erase -- and it has to
   // happen before the reset assert below, because after that there is no 6510 to run
   // the DMA handshake. Common_Defs.h refuses an extensions build without
   // Fab04_FullDMACapable, so the callers' messages can promise the blank on any board
   // that can show one; the only case they cannot cover is a C64 that is not running,
   // which has no screen to blank and no clock to blank it with. Skip it there rather
   // than spend the DMA waits' own timeouts on a handshake that cannot happen -- on a bus
   // that has already stopped WaitForDMAState ends them by itself (~20 mS), so most of what
   // skipping saves is latency.  Not all of it: DMATransferISR's edge waits are unbounded
   // (DMAControl.ino, "two of the three things"), so a transfer begun on a bus that stops
   // part way through wedges the board, and not starting one is the only thing that helps.
   if (C64IsClockingPHI2())
   {
      // Long enough to read the two lines the caller just printed -- but only when they
      // were printed. The C64 writes rsContinue the instant PrintSerialString returns
      // (MainMenu.asm, WaitForTRMain) and SendMsgSerialStringBuf returns on that, so
      // without a pause the blank would land within a frame of the message appearing and
      // "Do not power off" would never be legible. That warning guards the one action
      // that can leave the slot half erased.
      //
      // WaitForTRMain is the only thing that answers, though, and the C64 is in it only
      // while waiting on a command it issued itself. Reached over USB or TCP -- the
      // HostRemoveToken path, where the C64 is sitting in its idle menu loop -- the send
      // times out after 3 s and draws nothing, and pausing here would add two more
      // seconds of a stale menu for a warning that does not exist. The operator on that
      // path is at the computer, and the host tool tells them there.
      if (MessageSeen) delay(2000);

      uint8_t BlankD011 = 0x00;
      PerformDMA(DMA_WRITE, 0xD011, &BlankD011, 1, DMA_ADDR_INCREMENT);
      CloseDMA();
   }

   SetResetAssert;
   delay(20);
   detachInterrupt(digitalPinToInterrupt(PHI2_PIN));
   detachInterrupt(digitalPinToInterrupt(Menu_Btn_In_PIN));
#ifdef Fab04_BiDirReset
   detachInterrupt(digitalPinToInterrupt(BiDir_Reset_PIN));
#endif
   NVIC_DISABLE_IRQ(IRQ_ENET);
   NVIC_DISABLE_IRQ(IRQ_PIT);
}

static uint8_t HostInstallCode(VmInstallStatus status)
{
   switch (status)
   {
      case VmInstallStatus::Ok:            return VmFail::Installed;
      case VmInstallStatus::ReadError:     return VmFail::InstallRead;
      case VmInstallStatus::EraseFailed:   return VmFail::InstallErase;
      case VmInstallStatus::ProgramFailed: return VmFail::InstallProgram;
      case VmInstallStatus::VerifyFailed:  return VmFail::InstallVerify;
      default:                             return VmFail::InstallFailed;
   }
}

void DoHostInstall(FS *sourceFS, const char *FilePathName)
{
   static uint8_t staging[VM_HOST_SECTOR_BYTES];

   VmTrhFile package{sourceFS->open(FilePathName, FILE_READ)};
   if (!package.file)
   {
      SendMsgPrintfln("%s\r\nwould not open", FilePathName);
      return;
   }

   VmTrhHeader header;
   VmHostCandidate candidate;
   const uint32_t FileBytes = (uint32_t)package.file.size();

   // The length is judged before the header is read: a file shorter than the
   // header cannot supply one, and a short read there would otherwise blame
   // the card for a file that is only too small.
   VmInstallResult checked{VmInstallStatus::ShortFile, FileBytes};
   if (FileBytes >= VM_TRH_HEADER_BYTES)
   {
      checked = package.read(&header, sizeof header)
         ? vm_trh_valid(header, FileBytes)
         : VmInstallResult{VmInstallStatus::ReadError, 0};
   }
   if (checked) checked = vm_host_scan(package, header, staging, candidate);

   // Everything up to here only reads, so a refusal is an ordinary message
   // with the C64 still running.
   if (!checked)
   {
      package.file.close();
      SendMsgPrintfln("Host package refused:\r\n%s ($%lx)",
         HostInstallWhy(checked.status), (unsigned long)checked.detail);
      return;
   }

   char HostName[VmBootImage::nameBytes];
   VmBootImage::displayName(HostName, sizeof HostName, &candidate.id);

   // Before the reset assert: on fab 0.4 that pulls the pin isrExtResetDetect
   // watches, and the resulting BtnPressed ends the wait for the C64 to read.
   const bool Warned = SendMsgPrintfln("Installing host %s.\r\nDo not power off. Up to 45s,\r\nscreen will be blank.", HostName);

   StopServingTheC64(Warned);

   VmSlotFlash slot;
   const VmInstallResult done = vm_host_install(slot, package, header, candidate, staging);

   VmFail::set(HostInstallCode(done.status), done.detail);
   RebootTR();
   while (true) ;
}

// Removing a host opens the way an install does -- clear the tag so the slot stops
// reading as a host -- and then erases the rest of the slot too. vm_host_slot_valid
// would decide the same without it, but firmware without the extension loader does
// not: it reserves only the top 256K, and a payload left directly below that is
// where its update buffer search stops. See vm_host_remove().
//
// For the same reason this runs for a slot that holds anything, not only for a host:
// an install that failed part way leaves bytes that are no host to this firmware and
// are still in that search's way, and this is the one thing that clears them.
void DoHostUninstall()
{
   const bool Installed = VmBootImage::installed();
   if (!Installed && VmBootImage::blank())
   {
      SendMsgPrintfln("No extension host is installed.");
      return;
   }

   // Before the reset assert, for the reason DoHostInstall gives above.
   bool Warned;
   if (Installed)
   {
      VmHostId id{};
      char HostName[VmBootImage::nameBytes];
      VmBootImage::displayName(HostName, sizeof HostName, VmBootImage::identity(id) ? &id : nullptr);
      Warned = SendMsgPrintfln("Removing host %s.\r\nDo not power off. Up to 45s,\r\nscreen will be blank.", HostName);
   }
   else Warned = SendMsgPrintfln("Clearing the extension slot.\r\nDo not power off. Up to 45s,\r\nscreen will be blank.");

   StopServingTheC64(Warned);

   VmSlotFlash slot;
   const VmInstallResult done = vm_host_remove(slot);

   // The tag is cleared before any sector is erased, so an erase that fails still leaves
   // a slot that no longer reads as a host. Removal is a question about the slot, not
   // about the last operation: reporting the status here would claim the host is still
   // installed while the next boot finds nothing, and the menu would agree with the boot
   // rather than with the message. Ask the slot. The status still rides in the detail,
   // so a removal that left some of the payload in flash says so.
   const bool gone = !vm_host_installed(slot);
   VmFail::set(gone ? VmFail::Removed : VmFail::RemoveFailed, (uint32_t)done.status);
   RebootTR();
   while (true) ;
}
#endif
