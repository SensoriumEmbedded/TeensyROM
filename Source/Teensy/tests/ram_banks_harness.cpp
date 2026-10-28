// SPDX-License-Identifier: MIT
// ram_banks_harness: one save of RAM expansion banks to an in-memory SD card, with a computer
// that writes every bank between file writes unless its bus is held, as it does through the IO
// handlers while GeoRAM or the REU is installed. Arguments:
//   numbered|replacing   SaveRAMBanks (the REU's save) or SaveRAMBanksReplacing (GeoRAM's)
//   [holdfails]          HoldC64 cannot stop the computer
//   [writefails]         the card refuses the second bank's write
//   [paused]             the host has paused the computer before the save
// Prints "consistent" when the saved file is the banks as they were when the save began, else
// "torn"; "held" if the computer is still stopped afterwards; then each file left on the card.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#define FLASHMEM
#define Fab04_FullDMACapable
#define MaxPathLength 256
#define RAM_ImageSize 0
#define FILE_READ 0
#define FILE_WRITE 1
#define SetLEDOn do {} while (0)
#define SetLEDOff do {} while (0)

static struct
{
   template <typename... Args> void printf(const char *Format, Args... args) { std::fprintf(stderr, Format, args...); }
   void println(const char *Line) { std::fprintf(stderr, "%s\n", Line); }
} Serial;

static uint32_t Now;
static uint32_t millis() { return Now++; }

enum RegMenuTypes { rmtSD, rmtUSBDrive, rmtTeensy };
static uint8_t NumCrtChips;

static constexpr int NumBanks = 4;
static uint8_t *Bank[NumBanks];
static bool Held, HoldFails, WriteFails;
static int WritesSeen;
bool isFrozen;

static void ComputerRuns()
{
   if (Held) return;
   for (int Num = 0; Num < NumBanks; Num++) Bank[Num][0]++;
}

struct File
{
   std::vector<uint8_t> *Data = nullptr;
   size_t Pos = 0;
   explicit operator bool() const { return Data != nullptr; }
   size_t write(const uint8_t *Buf, size_t Len)
   {
      ComputerRuns();
      if (WriteFails && ++WritesSeen == 2) return 0;
      Data->insert(Data->end(), Buf, Buf + Len);
      return Len;
   }
   int read(uint8_t *Buf, size_t Len)
   {
      size_t Got = std::min(Len, Data->size() - Pos);
      std::memcpy(Buf, Data->data() + Pos, Got);
      Pos += Got;
      return (int)Got;
   }
   int available() { return (int)(Data->size() - Pos); }
   uint32_t size() { return (uint32_t)Data->size(); }
   void close() { Data = nullptr; }
};

static struct FS
{
   std::map<std::string, std::vector<uint8_t>> Files;
   File open(const char *Name, int Mode)
   {
      File Opened;
      if (Mode == FILE_WRITE) Opened.Data = &Files[Name];
      else if (Files.count(Name)) Opened.Data = &Files[Name];
      return Opened;
   }
   bool exists(const char *Name) { return Files.count(Name) != 0; }
   bool remove(const char *Name) { return Files.erase(Name) != 0; }
   bool rename(const char *From, const char *To)
   {
      if (!exists(From) || exists(To)) return false;
      Files[To] = Files[From];
      Files.erase(From);
      return true;
   }
} Card;

static RegMenuTypes RegMenuTypeFromFileName(char **ptrptrFileName)
{
   *ptrptrFileName += strlen("SD:");
   return rmtSD;
}
static bool SDFullInit() { return true; }
static bool USBFileSystemWait() { return true; }
static FS *FSfromSourceID(RegMenuTypes) { return &Card; }
static void FreeDriveDirMenu() {}
uint8_t RAM_Image[1];
uint8_t *ptrRAM_ImageEnd = RAM_Image;

bool HoldC64()
{
   if (HoldFails) return false;
   Held = true;
   return true;
}

bool CloseDMA()
{
   Held = false;
   return true;
}

// RAMBanks.c casts pointers to uint32_t, as wide as a pointer on the Teensy and not on the host
#define uint32_t uintptr_t
#include "../MinimalBoot/Common/IO_Handlers/RAMBanks.c"
#undef uint32_t

int main(int argc, char **argv)
{
   if (argc < 2) return 2;
   const bool Replacing = !strcmp(argv[1], "replacing");
   for (int Arg = 2; Arg < argc; Arg++)
   {
      HoldFails |= !strcmp(argv[Arg], "holdfails");
      WriteFails |= !strcmp(argv[Arg], "writefails");
      isFrozen |= !strcmp(argv[Arg], "paused");
   }

   std::vector<uint8_t> AtStart;
   for (int Num = 0; Num < NumBanks; Num++)
   {
      Bank[Num] = (uint8_t *)malloc(RAM_Bank_Size);
      memset(Bank[Num], Num + 1, RAM_Bank_Size);
      AtStart.insert(AtStart.end(), Bank[Num], Bank[Num] + RAM_Bank_Size);
   }
   Card.Files["georam.ram"] = std::vector<uint8_t>(3, 0xee);
   Held = isFrozen;

   if (Replacing) SaveRAMBanksReplacing(Bank, NumBanks, "SD:georam.ram");
   else SaveRAMBanks(Bank, NumBanks, "SD:reu.reu");

   const char *Saved = Replacing ? "georam.ram" : "reu000.reu";
   if (Card.exists(Saved) && Card.Files[Saved].size() == AtStart.size())
      std::puts(Card.Files[Saved] == AtStart ? "consistent" : "torn");
   if (Held) std::puts("held");
   for (auto &Entry : Card.Files) std::printf("%s %zu\n", Entry.first.c_str(), Entry.second.size());
   return 0;
}
