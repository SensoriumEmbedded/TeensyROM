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


//IO1 Handler for <Template> _________________________________________________________________________________________

__attribute__(( always_inline )) inline void IO1Hndlr_Template(uint8_t Address, bool R_Wn)
{
   uint8_t Data;
   
   if (R_Wn) //IO1 Read  -------------------------------------------------
   {
      switch(Address)
      {
         //case rIORegAddrMIDIStatus:
         //   DataPortWriteWait(rIORegMIDIStatus);  
         //   TraceLogAddValidData(rIORegMIDIStatus);
         //   break;
         //default: //used for all other IO1 reads
         //   DataPortWriteWait(IO1[Address]); 
         //   break;
      }
   }
   else  // IO1 write    -------------------------------------------------
   {
      Data = DataPortWaitRead();
      switch(Address)
      {
         //case wIORegAddrMIDIControl:
         //   break;
      }
      TraceLogAddValidData(Data);
   }
}
