
//Compile with 816MHz (overclock) option set

#include "TeensyROM.h"

uint8_t IO1_RAM[256];

void setup() 
{
   Serial.begin(115200);
   Serial.println(F("File: " __FILE__ ));
   Serial.println(F("Date: " __DATE__ ));
   Serial.println(F("Time: " __TIME__ ));

   pinMode(DataCEn_PIN, OUTPUT);  
   DataBufDisable; //buffer disabled
   for(uint8_t PinNum=0; PinNum<sizeof(AddrBusPins); PinNum++) pinMode(AddrBusPins[PinNum], INPUT); 
   SetDataPortWrite; //default to output (for C64 Read)
   
   pinMode(IO1n_PIN, INPUT);  
   pinMode(PHI2_PIN, INPUT);  
   pinMode(R_Wn_PIN, INPUT);  
   //pinMode(IN2_PIN, INPUT_PULLUP);  
 
   attachInterrupt( digitalPinToInterrupt( PHI2_PIN ), isrPHI2, RISING );

   Serial.print("TeensyROM is on-line\n");
} 
  
void loop()
{

}

FASTRUN void isrPHI2()
{
   register uint32_t StartCycles = ARM_DWT_CYCCNT;  //mark isr start "time"
   register uint32_t GPIO_6 = ReadGPIO6; //Address bus and R/*W are valid on Phi2 rising, Read now:
   
   //if (!GP_R_Wn(GPIO_6)) //low (Write)
   //{
   //   //SetDataPortRead; //set data ports to inputs
   //   //DataBufEnable; //enable external buffer
   //}
   
   register uint16_t Address = GP_Address(GPIO_6); //parse out address
   
 	Wait_nS(StartCycles, nS_PLAprop); 
   
   GPIO_6 = ReadGPIO6; //read it again
   if (!GP_IO1n(GPIO_6)) //DExx address space
   {
      if (GP_R_Wn(GPIO_6)) //High (Read)
      {
         DataPortWriteWait(IO1_RAM[Address & 0xFF], StartCycles);  
         //Serial.printf("Rd %d from %d\n", IO1_RAM[Address & 0xFF], Address);
      }
      else  //write
      {
         IO1_RAM[Address & 0xFF] = DataPortWaitRead(StartCycles); 
         Serial.printf("Wr %d to %04x\n", IO1_RAM[Address & 0xFF], Address);
      }
      
   }
   //DataBufDisable;
   //SetDataPortWrite; //(default, not needed) set data ports to outputs (default)
   
   //now the time-sensitive work is done, have a few hundred nS until the next interrupt...
   
}



