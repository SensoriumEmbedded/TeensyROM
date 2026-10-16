// SPDX-License-Identifier: MIT

// A USB mouse on the TR's USB host port, read through registers that a RAM expansion handler
//  serves from the IO page it leaves free: $DF00 beside GeoRAM, $DE00 beside the REU.
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

static const char USBMouse_ID[] = "TRM";
static volatile int32_t USBMouse_Counts[2];   // USB counts, written only by the main loop
static int32_t USBMouse_StepsRead[2];         // written only by the IO ISR, after InitUSBMouse
static volatile uint8_t USBMouse_Buttons;

static int32_t USBMouse_Steps(uint8_t Axis)
{
   return USBMouse_Counts[Axis] >> USBMouse_StepShift;  // floors, so no wider dead zone at 0
}

void PollUSBMouse()
{
   if (!usbHostMouse.available()) return;
   USBMouse_Counts[0] += usbHostMouse.getMouseX();
   USBMouse_Counts[1] += usbHostMouse.getMouseY();
   USBMouse_Buttons = usbHostMouse.getButtons();
   usbHostMouse.mouseDataClear();
}

// Movement from before the handler started (at the TR menu) is not the C64's to see
FLASHMEM void InitUSBMouse()
{
   for (uint8_t Axis = 0; Axis < 2; Axis++) USBMouse_StepsRead[Axis] = USBMouse_Steps(Axis);
}

void USBMouseIORead(uint8_t Address)
{
   if (Address < 2)
   {
      int32_t Step = USBMouse_Steps(Address) - USBMouse_StepsRead[Address];
      if (Step > USBMouse_MaxStep) Step = USBMouse_MaxStep;
      else if (Step < -USBMouse_MaxStep) Step = -USBMouse_MaxStep;
      USBMouse_StepsRead[Address] += Step;
      DataPortWriteWaitLog((uint8_t)Step);
   }
   else if (Address == USBMouse_RegButtons) DataPortWriteWaitLog(USBMouse_Buttons);
   else if (Address < USBMouse_RegID + sizeof(USBMouse_ID) - 1)
      DataPortWriteWaitLog(USBMouse_ID[Address - USBMouse_RegID]);
}
