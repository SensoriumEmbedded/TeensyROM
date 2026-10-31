// SPDX-License-Identifier: MIT
// reu_trpage_harness: the REU's IO2 page as IOH_REU_TRPage serves it, driven from stdin one command a line:
//   init 0         the handler starts, with no RAM to give the REU
//   banks N        the REU has N banks (0: none could be allocated)
//   r OFFSET       an IO2 read, printing the byte driven or "open"
#include <cstdint>
#include <cstdio>
#include <cstring>

#define FLASHMEM
#define RAM_Bank_Size 0x2000
#define MaxPathLength 256
#define SetIRQAssert
#define SetIRQDeassert
#define SetDMAAssert
#define SetDMADeassert
#define ReadGPIO6 0
#define GP6_Phi2(Port) ((Port) & 0)
#define ARM_DWT_CYCCNT 0u
#define WaitUntil_nS(nS)
#define GPIO6_ISR Discard
#define CORE_PIN1_BITMASK 0
#define NVIC_CLEAR_PENDING(Irq)
#define IRQ_GPIO6789 0

struct StructMenuItem;
enum DMA_Trans_RnW { DMA_READ, DMA_WRITE };
enum DMA_Addr_Mode { DMA_ADDR_INCREMENT, DMA_ADDR_FIXED };
struct stcIOHandlers
{
   char Name[20];
   void (*InitHndlr)();
   void (*IO1Hndlr)(uint8_t Address, bool R_Wn);
   void (*IO2Hndlr)(uint8_t Address, bool R_Wn);
   void (*ROMLHndlr)(uint32_t Address);
   void (*ROMHHndlr)(uint32_t Address);
   void (*PollingHndlr)();
   void (*CycleHndlr)();
};
static struct { template <typename... Args> void printf(const char *, Args...) {} } Serial;

static uint32_t Discard, StartCycCnt, LastCycCnt;
static uint32_t micros() { return 0; }
static const int eepAdREUFilename = 0;
static void EEPreadStr(int, char *Out) { *Out = 0; }
static bool (*fBusSnoop)(uint16_t Address, bool R_Wn), (*PendingfBusSnoop)(uint16_t Address, bool R_Wn);
void (*fSpecialBtnChange)(bool Up_nDn);
bool DMA_RnW, DMA_FixC64Addr;
uint32_t DMA_Length, DMA_Count, DMA_StartAddr;
StructMenuItem *DriveDirMenu;
bool DMAByte(uint8_t *) { return true; }
static uint8_t AllocRAMBanks(uint8_t **, uint8_t, uint8_t) { return 0; }
static void FreeRAMBanks(uint8_t **, uint8_t *Count) { *Count = 0; }
static void LoadRAMBanks(uint8_t **, uint8_t, const char *) {}
static void SaveRAMBanks(uint8_t **, uint8_t, const char *) {}
static void InitUSBMouse() {}
static void InitTRBlock() {}
static void PollUSBMouse() {}
static void PollTRBlock() {}
static void TRPageIO(uint8_t, bool) {}

static int Driven;
static void DataPortWriteWaitLog(uint8_t Data) { Driven = Data; }
static uint8_t DataPortWaitRead() { return 0; }

#include "../MinimalBoot/Common/IO_Handlers/IOH_REU.c"

int main()
{
   char Command[16];
   unsigned Value;

   while (scanf("%15s %x", Command, &Value) == 2)
   {
      if (!strcmp(Command, "init")) IOHndlr_REU_TRPage.InitHndlr();
      else if (!strcmp(Command, "banks")) NumREU_Banks = Value;
      else if (!strcmp(Command, "r"))
      {
         Driven = -1;
         IOHndlr_REU_TRPage.IO2Hndlr(Value, true);
         if (Driven < 0) printf("open\n");
         else printf("%02x\n", Driven);
      }
      else return 1;
   }
   return 0;
}
