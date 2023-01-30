
//Compile with 816MHz (overclock) option set

#include "ROM_Images.h"
#include "TeensyROM.h"

uint8_t IO2_RAM[256];
uint8_t RAM_Image[65536];
volatile uint8_t doReset = true;
volatile uint8_t ResetBtnPressed = false;
volatile uint8_t DisablePhi2ISR = false;
static const unsigned char *HIROM_Image = NULL;
static const unsigned char *LOROM_Image = NULL;

void setup() 
{
   Serial.begin(115200);
   Serial.println(F("File: " __FILE__ ));
   Serial.println(F("Date: " __DATE__ ));
   Serial.println(F("Time: " __TIME__ ));
   Serial.print(F_CPU_ACTUAL);
   Serial.println(" Hz");
   
   DataBufDisable; //buffer disabled
   for(uint8_t PinNum=0; PinNum<sizeof(OutputPins); PinNum++) pinMode(OutputPins[PinNum], OUTPUT); 
   SetDataPortDirOut; //default to output (for C64 Read)
   SetDMADeassert;
   SetNMIDeassert;
   SetIRQDeassert;
   SetUpMainMenuROM();
   SetLEDOn;
   SetDebugDeassert;
   SetResetDeassert;
  
   for(uint8_t PinNum=0; PinNum<sizeof(InputPins); PinNum++) pinMode(InputPins[PinNum], INPUT); 
   pinMode(Reset_Btn_In_PIN, INPUT_PULLUP);  //also makes it Schmitt triggered (PAD_HYS)
   pinMode(PHI2_PIN, INPUT_PULLUP);   //also makes it Schmitt triggered (PAD_HYS)
   attachInterrupt( digitalPinToInterrupt(Reset_Btn_In_PIN), isrResetBtn, FALLING );
   attachInterrupt( digitalPinToInterrupt(PHI2_PIN), isrPHI2, RISING );
   
   Serial.print("TeensyROM 0.01 is on-line\n");
   //go directly to BASIC:
      //SetGameDeassert;
      //SetExROMDeassert;      
      //LOROM_Image = NULL;
      //HIROM_Image = NULL;  
      //DisablePhi2ISR = true;
      //SetLEDOff;

} 
     
void loop()
{

   if (ResetBtnPressed)
   {
      Serial.println("Reset Button detected"); 
      SetLEDOn;
      ResetBtnPressed=false;
      SetDebugDeassert;
      SetUpMainMenuROM(); //back to main menu
      DisablePhi2ISR=false;
      doReset=true;
   }
   
   if (doReset)
   {
      Serial.print("Resetting C64..."); 
      SetResetAssert; 
      delay(50); 
      while(ReadResetButton==0); //avoid self reset detection
      SetResetDeassert; //if self monitorring, place before while loop
      SetDebugDeassert;
      Serial.println("Done");
      doReset=false;
      ResetBtnPressed = false;
   }
  
   if (Serial.available())
   {
      uint8_t inByte = Serial.read();
      if (inByte == 0x64) //command from app
      {
         inByte = Serial.read();
         switch (inByte)
         {
            case 0x55:
               Serial.println("TeensyROM Ready!");
               break;
            case 0xAA:
               ReceiveFile();        
               break;
            case 0xEE:
               Serial.println("Reset cmd received");
               SetUpMainMenuROM();
               doReset = true;
               break;
            default:
               Serial.printf("Unk: %02x\n", inByte); 
               break;
         }
        
      }
   }
}

void SetUpMainMenuROM()
{
   SetGameDeassert;
   SetExROMAssert;
   LOROM_Image = TeensyROMC64_bin;
   HIROM_Image = NULL;
}

void ReceiveFile()
{ 
      //   App: SendFileToken 0x64AA
      //Teensy: ack 0x6464
      //   App: Length(2), CS(2), file(length)
      //Teensy: Pass 0x6480 or Fail 0x9b7f

      //send file token has been received, only 2 byte responses until final response
   
   Serial.write(0x64);  //ack
   Serial.write(0x64);  
   
   if(!SerialAvailabeTimeout()) return;
   uint16_t len = Serial.read();
   len = len + 256 * Serial.read();
   if(!SerialAvailabeTimeout()) return;
   uint16_t CheckSum = Serial.read();
   CheckSum = CheckSum + 256 * Serial.read();
   
   uint16_t bytenum = 0;
   while(bytenum < len)
   {
      if(!SerialAvailabeTimeout()) return;
      RAM_Image[bytenum] = Serial.read();
      CheckSum-=RAM_Image[bytenum++];
   }  

   if (CheckSum!=0)
   {  //Failed
     Serial.write(0x9B);  
     Serial.write(0x7F);  
     Serial.printf("Failed! Len:%d, RCS:%d\n", len, CheckSum);
     return;
   }   

   //success!
   Serial.write(0x64);  
   Serial.write(0x80);  
   Serial.println("Yay!");
   
   //launch prg
   
   
   
}

bool SerialAvailabeTimeout()
{
   uint32_t StartTOMillis = millis();
   
   while(!Serial.available() && (millis() - StartTOMillis) < SerialTimoutMillis); // timeout loop
   if (Serial.available()) return(true);
   
   Serial.write(0x9B);  
   Serial.write(0x7F);  
   Serial.print("Timeout!\n");  
   return(false);
}