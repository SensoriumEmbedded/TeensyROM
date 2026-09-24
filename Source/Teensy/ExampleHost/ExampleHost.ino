// SPDX-License-Identifier: MIT
//
// An example third-party extension host: the smallest program that is a host
// rather than a sketch that happens to be in the right place.
//
// It does nothing useful on purpose. What it demonstrates is the contract, and
// the contract is four things -- the descriptor, the marker, the record, and the
// way back. Everything past those four is your program, and this repo has no
// opinion about it. docs/Architecture/Extension-Hosts.md walks through each one
// and explains why it is there; this file is the same story in code.
//
// Build and install:
//
//   node tools/build-firmware.mjs --target tr-plus --host-sketch Source/Teensy/ExampleHost
//   node tools/build-host-package.mjs --hex build/firmware/TeensyROM+_<ver>_ExampleHost_full.hex
//   python3 tools/bench/hostinstall.py build/firmware/EXAMPLE.TRH
//
// A --host-sketch build is named for its sketch directory, not the shipping name, so
// this hex cannot be mistaken for -- or overwrite -- the release image.
//
// What you should see: the Installed Extensions page names "Example", launching
// an extension blinks the LED four times with the C64 held in reset, and the
// menu comes back saying `Extension: extension host returned ($50/$4)`.

#include <SD.h>
#include <EEPROM.h>
#include "Min_TeensyROM.h"
#include "Common/Common_Defs.h"
#include "Common/Menu_Regs.h"
#include "Common/DriveDirLoad.h"
#include "Common/IOHandlers.h"
#include "Common/VMHostABI.h"
#include "Common/VMFail.h"

// The shared minimal sources this sketch is overlaid onto declare these extern
// and expect exactly one definition. A host that never serves the C64 still has
// to supply them, because those sources compile either way.
uint8_t RAM_Image[RAM_ImageSize];
volatile uint8_t BtnPressed = false;
volatile uint8_t EmulateVicCycles = false;
uint8_t CurrentIOHandler = IOH_None;
StructMenuItem DriveDirMenu;
char DriveDirPath[MaxPathLength];
uint16_t LOROM_Mask, HIROM_Mask;
Stream *CmdChannel = &Serial;

#include "Common/ISRs.c"

// ---------------------------------------------------------------- descriptor
//
// 1 of 4. The main image reads this out of flash without booting the host, to
// name it on the Installed Extensions page and to decide whether a module's
// required services exist. It lands at VM_HOST_ID_OFFSET because
// extensionLinkerScript() in tools/lib/extension-image.mjs puts the .vmhostid
// section there; nothing in this file chooses the address.
//
// `services` is 0 because this host publishes no module services at all. That
// is a legal answer, and it is the honest one: a module asking for any service
// is refused against this host rather than crashing inside it. `host_bytes` is
// 0 for the same reason -- there is no VmHost table to describe the size of.
// `name` is 12 bytes and need not be terminated.
__attribute__((used, section(".vmhostid")))
const VmHostId vmHostId = { VM_HOSTID_MAGIC, VM_ABI, 0, 0, "Example", 0 };

// The detail this host reports alongside its status code. Free for a host's own
// use -- the menu prints whatever is here and attaches no meaning to it.
static constexpr uint32_t BlinkCount = 4;

// ------------------------------------------------------------- the way back
//
// 4 of 4. RebootTR() is a raw MCU reset, which leaves the boot indicator at
// whatever was last written. Minimal writes VM_BOOT_FROM_MIN there before it
// jumps here, so a host that never touches the byte is already right. A host
// that writes VM_BOOT_SKIP_MIN of its own has replaced it -- VMBoot.ino does,
// to clear the flag in case power is lost mid-run -- and the main image reads
// VM_BOOT_SKIP_MIN as a cold power up and re-runs the user's autolaunch file,
// sending the machine somewhere the user did not ask to go. Writing
// VM_BOOT_FROM_MIN here makes the answer right either way.
FLASHMEM void ReturnToMenu(uint8_t code, uint32_t detail)
{
   VmFail::set(code, detail);   // 3 of 4: the record, read by the main image on the way up
   EEPROM.write(VM_EEP_BOOTIND_ADDR, VM_BOOT_FROM_MIN);
   delay(10);                   // let the EEPROM write complete before the reset
   RebootTR();
   while (true) ;               // RebootTR does not return; say so to the compiler
}

void setup()
{
   // The C64 is held in reset from here until something deasserts it. This host
   // never does: it owns the machine for as long as it runs, and hands it back
   // by resetting. A host that serves the C64 deasserts here and takes over the
   // bus instead -- see VMBoot.ino, which does exactly that.
   SetLEDOn;
   for (uint8_t PinNum = 0; PinNum < sizeof(OutputPins); PinNum++) pinMode(OutputPins[PinNum], OUTPUT);
   SetDMADeassert;
   SetIRQDeassert;
   SetNMIDeassert;
   SetResetAssert;
   pinMode(Menu_Btn_In_PIN, INPUT_PULLUP);

   // ------------------------------------------------------------- the marker
   //
   // 2 of 4. Being in the slot is not authorization to run: the marker is what
   // says an extension was selected rather than a cartridge. Minimal tests it
   // itself before it jumps, along with the button and the EEPROM magic, so the
   // three checks below are a second look at what minimal already agreed to --
   // cheap, and the right three to take if a host takes any. The marker is
   // never consumed, so it is safe to read and wrong to rely on alone: only the
   // boot indicator says this boot is the one that selected an extension.
   //
   // Checking the boot indicator the way MinimalBoot.ino does would be a
   // mistake here, and a quiet one: minimal has already replaced
   // VM_BOOT_EXECUTE_MIN with VM_BOOT_FROM_MIN by the time a host sees it, so
   // that test never passes and every launch falls straight through to the main
   // app -- which looks from the C64 exactly like a host that failed.
   if (ReadButton == 0) runMainTRApp();   // button held: escape to the menu

   uint32_t magic = 0;
   EEPROM.get(VM_EEP_MAGIC_ADDR, magic);
   if (magic != VM_EEP_MAGIC) runMainTRApp();   // EEPROM not initialised by the main image

   char marker[5]{};
   for (uint8_t i = 0; i < 4; i++) marker[i] = EEPROM.read(VM_EEP_BOOTNAME_ADDR + i);
   if (strcmp(marker, VM_HOST_MARKER) != 0) runMainTRApp();

   // Entered on purpose. From here the machine is ours.
   for (uint32_t i = 0; i < BlinkCount; i++)
   {
      SetLEDOff; delay(150);
      SetLEDOn;  delay(150);
   }

   ReturnToMenu(VmFail::HostReturned, BlinkCount);
}

void loop()
{
   // Unreachable: setup() either jumps to the main app or resets. A host that
   // runs continuously does its work here instead.
}

// The shared minimal sources call these; MinimalBoot.ino, which this sketch
// replaced, is where they live in an ordinary build. Same bodies, because a
// host that changed them would be changing what the main image reads back.
void EEPwriteNBuf(uint16_t addr, const uint8_t* buf, uint8_t len)
{
   while (len--) EEPROM.write(addr+len, buf[len]);
}

void EEPwriteStr(uint16_t addr, const char* buf)
{
   EEPwriteNBuf(addr, (uint8_t*)buf, strlen(buf)+1); //include terminator
}

void EEPreadNBuf(uint16_t addr, uint8_t* buf, uint16_t len)
{
   while (len--) buf[len] = EEPROM.read(addr+len);
}

void EEPreadStr(uint16_t addr, char* buf)
{
   uint16_t CharNum = 0;

   do
   {
      buf[CharNum] = EEPROM.read(addr+CharNum);
   } while (buf[CharNum++] !=0); //end on termination, but include it in buffer
}
