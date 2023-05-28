// MIT License
// 
// Copyright (c) 2023 Travis Smith
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software 
// and associated documentation files (the "Software"), to deal in the Software without 
// restriction, including without limitation the rights to use, copy, modify, merge, publish, 
// distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom 
// the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or 
// substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING 
// BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


enum IOHandlers //Synch order/qty with TblSpecialIO & IOHandlerName (below)
{
   IOH_None,
   IOH_MIDI_Datel,        // always first of 
   IOH_MIDI_Sequential,   //   4 MIDI options
   IOH_MIDI_Passport,     //   in this order...
   IOH_MIDI_NamesoftIRQ,  //   See SetMIDIRegAddrs
   IOH_Debug,
   IOH_TeensyROM, 
   IOH_SwiftLink,
   IOH_EpyxFastLoad,
   
   IOH_Num_Handlers       //always last
};

const char IOHandlerName[][20] =
{
   "None              ", // IOH_None,
   "MIDI:Datel/Siel   ", // IOH_MIDI_Datel,      
   "MIDI:Sequential   ", // IOH_MIDI_Sequential, 
   "MIDI:Passport/Sent", // IOH_MIDI_Passport,   
   "MIDI:Namesoft IRQ ", // IOH_MIDI_NamesoftIRQ,
   "Debug             ", // IOH_Debug,
   "TeensyROM         ", // IOH_TeensyROM, 
   "SwiftLink         ", // IOH_SwiftLink,
   "Epyx Fast Load    ", // IOH_EpyxFastLoad,
};

//these must match enum IOHandlers/IOHandlerName MIDI order/qty starting at IOH_MIDI_Datel
const uint8_t MidiRegs[][4] = {
   //wControl, rStatus, wTransmit, rReceive $de00+
   4,6,5,7,  // Datel/Siel
   0,2,1,3,  // Sequential
   8,8,9,9,  // Passport/Sent
   0,2,1,3,  // Namesoft IRQ (same as seq, no NMI)
};

#define BigBufSize        5000
#define NumMIDIControls   16  //must be power of 2, may want to do this differently?

//see https://codebase64.org/doku.php?id=base:c64_midi_interfaces
// 6580 ACIA interface emulation
//rIORegMIDIStatus:
#define MIDIStatusIRQReq  0x80   // Interrupt Request
#define MIDIStatusDCD     0x04   // Data Carrier Detect (Ready to receive Tx data)
#define MIDIStatusTxRdy   0x02   // Transmit Data Register Empty (Ready to receive Tx data)
#define MIDIStatusRxFull  0x01   // Receive Data Register Full (Rx Data waiting to be read)


// 6551 ACIA interface emulation
//register locations (IO1, DExx)
#define IORegSwiftData    0x00   // Swift Emulation Data Reg
#define IORegSwiftStatus  0x01   // Swift Emulation Status Reg
#define IORegSwiftCommand 0x02   // Swift Emulation Command Reg
#define IORegSwiftControl 0x03   // Swift Emulation Control Reg

//status reg flags
#define SwiftStatusIRQ     0x80   // high if ACIA caused interrupt;
#define SwiftStatusDSR     0x40   // reflects state of DSR line
#define SwiftStatusDCD     0x20   // reflects state of DCD line
#define SwiftStatusTxEmpty 0x10   // high if xmit-data register is empty
#define SwiftStatusRxFull  0x08   // high if receive-data register full
#define SwiftStatusErrOver 0x04   // high if overrun error
#define SwiftStatusErrFram 0x02   // high if framing error
#define SwiftStatusErrPar  0x01   // high if parity error

//command reg flags
#define SwiftCmndRxIRQEn   0x02   // low if Rx IRQ enabled


#define EpyxMaxCycleCount  512 //Numer for C64 clock cycles to disable Epyx
