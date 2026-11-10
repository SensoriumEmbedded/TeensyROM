// SPDX-License-Identifier: MIT
// c128_prg_harness: C128Prg.ino as HandleExecution calls it, driven from stdin one command a line:
//   host c64|c128      the machine the menu reported
//   prg HEX...         the selected PRG's bytes, load address first
//   size N             the PRG grows to N bytes, zero-filled
//   classify           prints 1 if RunsInC128Mode takes the PRG, else 0
//   launch             runs LaunchC128BasicPrg and prints what it left behind
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define FLASHMEM
#define PROGMEM
#define PSTR(s) (s)
#define SetGameDeassert (Game = false)
#define SetExROMDeassert (ExROM = false)
#define RAM_ImageSize (128 * 1024)

static bool Game = true, ExROM = true, IsC128, BootComplete, doReset;
static unsigned HandlerSelects;
static std::string LastMessage;
static uint8_t RAM_Image[RAM_ImageSize], *XferImage;
static uint32_t XferSize, StreamOffsetAddr = 0xffffffff;
static const unsigned char *LOROM_Image, *HIROM_Image;
static uint16_t LOROM_Mask = 0x1fff, HIROM_Mask = 0x1fff;

static bool HostIsC128() { return IsC128; }
static void IOHandlerSelectInit() { HandlerSelects++; }
static void SendMsgPrintfln(const char *Format, ...)
{
   char Text[128];
   va_list Args;
   va_start(Args, Format);
   vsnprintf(Text, sizeof Text, Format, Args);
   va_end(Args);
   LastMessage = Text;
}

#include "../C128Prg.ino"

int main()
{
   std::vector<uint8_t> Prg;
   char Line[4096];
   while (fgets(Line, sizeof Line, stdin))
   {
      char *Arg = strchr(Line, ' ');
      if (Arg) *Arg++ = 0;
      else Line[strcspn(Line, "\r\n")] = 0;
      if (!strcmp(Line, "host")) IsC128 = !strncmp(Arg, "c128", 4);
      else if (!strcmp(Line, "prg"))
      {
         Prg.clear();
         for (char *Byte = strtok(Arg, " \r\n"); Byte; Byte = strtok(NULL, " \r\n")) Prg.push_back(strtoul(Byte, NULL, 16));
      }
      else if (!strcmp(Line, "size")) Prg.resize(strtoul(Arg, NULL, 10));
      else if (!strcmp(Line, "classify")) printf("%d\n", RunsInC128Mode(Prg.data(), Prg.size()));
      else if (!strcmp(Line, "launch"))
      {
         memcpy(RAM_Image, Prg.data(), Prg.size());
         LaunchC128BasicPrg(RAM_Image, Prg.size());
         const unsigned char *End = LOROM_Image ? LOROM_Image + C128PRG_END_PARAM : NULL;
         printf("reset=%d boot=%d selects=%u game=%d exrom=%d roml=%s romh=%s end=%04x xfer=%s:%u:%u masks=%04x:%04x msg=%s\n",
                doReset, BootComplete, HandlerSelects, Game, ExROM,
                LOROM_Image ? "boot" : "none", HIROM_Image ? "boot" : "none", End ? End[0] | End[1] << 8 : 0,
                XferImage == RAM_Image ? "prg" : "none", XferSize, StreamOffsetAddr, LOROM_Mask, HIROM_Mask,
                LastMessage.c_str());
      }
      else
      {
         fprintf(stderr, "unknown command %s\n", Line);
         return 1;
      }
   }
   return 0;
}
