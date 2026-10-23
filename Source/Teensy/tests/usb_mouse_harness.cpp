// SPDX-License-Identifier: MIT
// usb_mouse_harness: the USB mouse registers, driven from stdin one command a line,
// as the USB host, the main loop and the IO ISR would drive them:
//   report X Y B   the USB host delivers a report into the mouse controller
//   unplug, plug   the mouse leaves or returns
//   poll           the main loop's PollUSBMouse
//   init           InitUSBMouse, as the handler starts
//   r OFFSET       an IO read, printing the byte driven or "open"
//   race AXIS      at the next poll's atomic write of AXIS's ready word, an IO read of
//                  that axis lands first and prints "race BYTE"
//   spurious AXIS  the next compare-exchange on AXIS's ready word fails without a reason,
//                  as a weak one may
#include <cstdint>
#include <cstdio>
#include <cstring>

#define FLASHMEM
#define NVIC_DISABLE_IRQ(Irq)
#define NVIC_ENABLE_IRQ(Irq)
#define IRQ_USBHS 0
// Cortex-M barriers do not assemble for the host: asm volatile(...) becomes asm("")
#define volatile(...) ("")

struct USBHIDInput
{
   bool Plugged = true;
   explicit operator bool() { return Plugged; }
};

static struct MouseController : USBHIDInput
{
   int X = 0, Y = 0;
   uint8_t Buttons = 0;
   bool Available = false;
   bool available() { return Available; }
   int getMouseX() { return X; }
   int getMouseY() { return Y; }
   uint8_t getButtons() { return Buttons; }
   void mouseDataClear() { X = Y = 0; Buttons = 0; Available = false; }
} usbHostMouse;

static int Driven;
static void DataPortWriteWaitLog(uint8_t Data) { Driven = Data; }

static int RaceAxis = -1, SpuriousAxis = -1;
static void LandRace(volatile uint32_t *Word);
static bool FailSpuriously(volatile uint32_t *Word, uint32_t *Expected);

template <typename... Rest>
static bool RacedCompareExchange(volatile uint32_t *Word, uint32_t *Expected, Rest... rest)
{
   LandRace(Word);
   if (FailSpuriously(Word, Expected)) return false;
   return __atomic_compare_exchange_n(Word, Expected, rest...);
}

template <typename... Rest>
static void RacedStore(volatile uint32_t *Word, Rest... rest)
{
   LandRace(Word);
   __atomic_store_n(Word, rest...);
}

#define __atomic_compare_exchange_n RacedCompareExchange
#define __atomic_store_n RacedStore
#include "../MinimalBoot/Common/IO_Handlers/USBMouse.c"
#undef __atomic_compare_exchange_n
#undef __atomic_store_n
#undef volatile

static int Read(unsigned Offset)
{
   Driven = -1;
   USBMouseIORead(Offset);
   return Driven;
}

static void LandRace(volatile uint32_t *Word)
{
   if (RaceAxis < 0 || Word != &USBMouse_Ready[RaceAxis]) return;
   unsigned Axis = RaceAxis;
   RaceAxis = -1;
   printf("race %02x\n", Read(Axis));
}

static bool FailSpuriously(volatile uint32_t *Word, uint32_t *Expected)
{
   if (SpuriousAxis < 0 || Word != &USBMouse_Ready[SpuriousAxis]) return false;
   SpuriousAxis = -1;
   *Expected = *Word;
   return true;
}

int main()
{
   char Line[64];
   while (fgets(Line, sizeof Line, stdin))
   {
      int X, Y;
      unsigned Value;
      if (sscanf(Line, "report %d %d %x", &X, &Y, &Value) == 3)
      {
         usbHostMouse.X = X;
         usbHostMouse.Y = Y;
         usbHostMouse.Buttons = Value;
         usbHostMouse.Available = true;
      }
      else if (!strncmp(Line, "unplug", 6)) usbHostMouse.Plugged = false;
      else if (!strncmp(Line, "plug", 4)) usbHostMouse.Plugged = true;
      else if (!strncmp(Line, "poll", 4)) PollUSBMouse();
      else if (!strncmp(Line, "init", 4)) InitUSBMouse();
      else if (sscanf(Line, "race %u", &Value) == 1) RaceAxis = Value;
      else if (sscanf(Line, "spurious %u", &Value) == 1) SpuriousAxis = Value;
      else if (sscanf(Line, "r %x", &Value) == 1)
      {
         int Byte = Read(Value);
         if (Byte < 0) printf("open\n");
         else printf("%02x\n", Byte);
      }
      else
      {
         fprintf(stderr, "unknown command: %s", Line);
         return 2;
      }
   }
   if (RaceAxis >= 0 || SpuriousAxis >= 0)
   {
      fprintf(stderr, "an armed race or spurious failure never happened\n");
      return 1;
   }
   return 0;
}
