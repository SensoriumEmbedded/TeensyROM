// SPDX-License-Identifier: MIT

// A USB mouse on the TR's USB host port, read through registers the GeoRAM handler serves
//  at $DF00, in the part of the IO2 page that GeoRAM leaves free.
//  It cannot be a 1351: the SID's POT lines are not on the cartridge port, so only software
//  with a TR driver sees it (tools/geos).
//
//    +0     X movement since the last read of +0, signed, right is positive
//    +1     Y movement since the last read of +1, signed, down is positive
//    +2     buttons, 1 = pressed: bit 0 left, bit 1 right, bit 2 middle
//    +3..5  "TRM", for a driver to find the base it is at

#define USBMouse_RegButtons  2
#define USBMouse_RegID       3
#define USBMouse_StepShift   2     // USB mice count ~1000 per inch; GEOS's screen is 320 wide
#define USBMouse_MaxStep     127
#define USBMouse_ReadyByte   0xff
#define USBMouse_ReadyNext   0x100

static const char USBMouse_ID[] = "TRM";
static volatile int32_t USBMouse_Counts[2];   // USB counts, written only by the main loop
static volatile int32_t USBMouse_StepsRead[2];  // written only by the IO ISR, after InitUSBMouse
static volatile uint8_t USBMouse_Buttons;
// The next read's byte, kept ready so a read drives the bus before any arithmetic. The bits
//  above it count reads, so the main loop can tell that a read slipped in while it computed.
static volatile uint32_t USBMouse_Ready[2];

static int32_t USBMouse_Steps(uint8_t Axis)
{
   return USBMouse_Counts[Axis] >> USBMouse_StepShift;  // floors, so no wider dead zone at 0
}

static uint8_t USBMouse_NextStep(uint8_t Axis)
{
   int32_t Step = USBMouse_Steps(Axis) - USBMouse_StepsRead[Axis];
   if (Step > USBMouse_MaxStep) Step = USBMouse_MaxStep;
   else if (Step < -USBMouse_MaxStep) Step = -USBMouse_MaxStep;
   return (uint8_t)Step;
}

static void USBMouse_UpdateReady()
{
   for (uint8_t Axis = 0; Axis < 2; Axis++)
   {
      uint32_t Ready = __atomic_load_n(&USBMouse_Ready[Axis], __ATOMIC_RELAXED);
      uint32_t Next;
      do Next = (Ready & ~USBMouse_ReadyByte) | USBMouse_NextStep(Axis);
      while (!__atomic_compare_exchange_n(&USBMouse_Ready[Axis], &Ready, Next, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED));
   }
}

FLASHMEM void PollUSBMouse()
{
   // MouseController is also a BTHIDInput, so its own bool conversion is ambiguous
   if (!static_cast<USBHIDInput &>(usbHostMouse))
   {
      // unplugging leaves the library's last report in place, for a replug to deliver
      usbHostMouse.mouseDataClear();
      USBMouse_Buttons = 0;
      return;
   }
   if (!usbHostMouse.available()) return;
   // the USB ISR writes each report straight into the controller, and mouseDataClear()
   //  would drop one that lands between these reads and the clear
   NVIC_DISABLE_IRQ(IRQ_USBHS);
   // the Cortex-M7 can still take the IRQ after the NVIC write until a DSB and ISB
   asm volatile("dsb\n\tisb" ::: "memory");
   int32_t X = usbHostMouse.getMouseX();
   int32_t Y = usbHostMouse.getMouseY();
   uint8_t Buttons = usbHostMouse.getButtons();
   usbHostMouse.mouseDataClear();
   asm volatile("" ::: "memory");
   NVIC_ENABLE_IRQ(IRQ_USBHS);
   USBMouse_Counts[0] += X;
   USBMouse_Counts[1] += Y;
   USBMouse_Buttons = Buttons;
   USBMouse_UpdateReady();
}

// Movement from before the handler started (at the TR menu) is not the C64's to see
FLASHMEM void InitUSBMouse()
{
   usbHostMouse.mouseDataClear();
   for (uint8_t Axis = 0; Axis < 2; Axis++)
   {
      USBMouse_StepsRead[Axis] = USBMouse_Steps(Axis);
      USBMouse_Ready[Axis] = 0;
   }
}

void USBMouseIORead(uint8_t Address)
{
   if (Address < 2)
   {
      uint32_t Ready = USBMouse_Ready[Address];
      DataPortWriteWaitLog((uint8_t)Ready);
      USBMouse_StepsRead[Address] += (int8_t)Ready;
      USBMouse_Ready[Address] = (Ready & ~USBMouse_ReadyByte) + USBMouse_ReadyNext + USBMouse_NextStep(Address);
   }
   else if (Address == USBMouse_RegButtons) DataPortWriteWaitLog(USBMouse_Buttons);
   else if (Address < USBMouse_RegID + sizeof(USBMouse_ID) - 1)
      DataPortWriteWaitLog(USBMouse_ID[Address - USBMouse_RegID]);
}
