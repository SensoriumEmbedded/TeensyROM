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


enum IO1Handlers //Synch order/qty with TblSpecialIO & SetMIDIRegs
{
   IO1H_None,
   IO1H_MIDI_Datel,        // always first of 
   IO1H_MIDI_Sequential,   //   4 MIDI options
   IO1H_MIDI_Passport,     //   ...
   IO1H_MIDI_NamesoftIRQ,  //   in this order
   IO1H_Debug,
   IO1H_TeensyROM, 
   IO1H_SwiftLink,
   IO1H_Num_Handlers       //always last
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
#define IORegSwiftData    0x00   // Swift Emulation Data Reg
#define IORegSwiftStatus  0x01   // Swift Emulation Status Reg
#define IORegSwiftCommand 0x02   // Swift Emulation Command Reg
#define IORegSwiftControl 0x03   // Swift Emulation Control Reg
#define IORegSwiftBaud    0x07   // Swift Emulation Baud Reg(?)

