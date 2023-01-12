
//Compile with 816MHz (overclock) option set

#include "TeensyROM.h"

void setup() 
{
   Serial.begin(115200);
   Serial.println(F("File: " __FILE__ ));
   Serial.println(F("Date: " __DATE__ ));
   Serial.println(F("Time: " __TIME__ ));
  
   pinMode(A0_PIN, INPUT);  
   pinMode(A1_PIN, INPUT);  
   pinMode(A2_PIN, INPUT);  
   pinMode(A3_PIN, INPUT);  

   pinMode(DataCEn_PIN, OUTPUT);  
   DataBufDisable; //buffer disabled
   SetDataPortWrite; //default to output (for C64 Read)
   
   pinMode(IO1n_PIN, INPUT);  
   pinMode(PHI2_PIN, INPUT);  
   pinMode(R_Wn_PIN, INPUT);  
   //pinMode(IN2_PIN, INPUT_PULLUP);  
 
   attachInterrupt( digitalPinToInterrupt( PHI2_PIN ), isrPHI2, RISING );

   Serial.print("IO Benchmark test\n");
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
         //SetDataPortWrite; //(default, not needed) set data ports to outputs (default)
         SetDataPortOutWait(Address, StartCycles);  
         //Serial.printf("Read %d from ", Address);
      }
      else  //write
      {
         //read data bus
         register uint8_t DataIn = DataPortRead(StartCycles); 
         Serial.printf("Wrote %d to %d\n", DataIn, Address);
      }
      
      //Serial.println(Address);   
      //Serial.print(" IO1 ");
   }
   //DataBufDisable;
   //SetDataPortWrite; //(default, not needed) set data ports to outputs (default)
   
}



