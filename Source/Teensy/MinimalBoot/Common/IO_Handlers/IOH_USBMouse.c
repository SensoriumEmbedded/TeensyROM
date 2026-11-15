// SPDX-License-Identifier: MIT

// The TR's USB mouse alone (USBMouse.c has the registers), for a program that needs nothing beside it.
//  In IO1: a driver that probes as tools/geos does looks at $DE00 when no block device answers at $DF10.

void IO1Hndlr_USBMouse(uint8_t Address, bool R_Wn);

stcIOHandlers IOHndlr_USBMouse =
{
  "USB Mouse",          //Name of handler
  &InitUSBMouse,        //Called once at handler startup
  &IO1Hndlr_USBMouse,   //IO1 R/W handler
  NULL,                 //IO2 R/W handler
  NULL,                 //ROML Read handler, in addition to any ROM data sent
  NULL,                 //ROMH Read handler, in addition to any ROM data sent
  &PollUSBMouse,        //Polled in main routine
  NULL,                 //called at the end of EVERY c64 cycle
};

void IO1Hndlr_USBMouse(uint8_t Address, bool R_Wn)
{
   if (R_Wn) USBMouseIORead(Address);   // it drives the bus whatever R/W says
}
